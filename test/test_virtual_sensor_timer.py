#!/usr/bin/env python3
"""Exercise the real virtual-sensor source with fake sensor/engine interfaces.

Usage: python3 test_virtual_sensor_timer.py /path/to/thermal_daemon
Requires a C++11 compiler. No daemon, root access, sysfs or hardware is used.
The temporary build directory is removed on success and failure.
"""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile


SENSOR_HEADER = r'''
#pragma once
#include <pthread.h>
#include <unistd.h>
#include <string>
#include <utility>
#define THD_SUCCESS 0
#define THD_ERROR -1
#define thd_log_info(...) ((void)0)
#define thd_log_debug(...) ((void)0)
#define thd_log_warn(...) ((void)0)
class cthd_sensor {
protected:
    std::string type_str;
    bool virtual_sensor = false;
public:
    cthd_sensor(int, std::string, std::string name): type_str(std::move(name)) {}
    virtual ~cthd_sensor() = default;
    virtual unsigned int read_temperature() { return 41000; }
    virtual const std::string& get_sensor_type() { return type_str; }
    virtual void sensor_dump() {}
};
'''

ENGINE_HEADER = r'''
#pragma once
#include "thd_sensor.h"
class cthd_engine {
public:
    cthd_sensor *source;
    cthd_sensor *search_sensor(const std::string&) { return source; }
};
extern cthd_engine *thd_engine;
'''

TEST_SOURCE = r'''
#include "thd_sensor_virtual.h"
#include "thd_engine.h"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

using Clock = std::chrono::steady_clock;
using Milliseconds = std::chrono::milliseconds;

class FakeSensor: public cthd_sensor {
public:
    std::atomic<int> reads{0};
    std::mutex mutex;
    std::condition_variable ready;
    bool block = false;
    FakeSensor(): cthd_sensor(0, "none", "FAKE") {}
    unsigned int read_temperature() override {
        std::unique_lock<std::mutex> lock(mutex);
        ++reads;
        ready.wait(lock, [this] { return !block; });
        return 41000;
    }
};

cthd_engine *thd_engine;

static void wait_for_read(FakeSensor &source, int previous) {
    auto deadline = Clock::now() + std::chrono::seconds(2);
    while (source.reads.load() == previous && Clock::now() < deadline)
        std::this_thread::sleep_for(Milliseconds(1));
    assert(source.reads.load() > previous);
}

int main() {
    FakeSensor source;
    cthd_engine engine;
    engine.source = &source;
    thd_engine = &engine;
    std::string link("FAKE");
    cthd_sensor_virtual sensor(1, "VIRTUAL", link, 1.0, 0.0);
    polling_table_entry entry{0, 60};
    sensor.update_polling_table(entry);
    sensor.disable_periodic_timer();

    auto start = Clock::now();
    for (int i = 0; i < 50; ++i) {
        int previous = source.reads.load();
        sensor.enable_periodic_timer();
        wait_for_read(source, previous);
        assert(sensor.read_temperature() == 41000);
        std::this_thread::sleep_for(Milliseconds(2));
        sensor.disable_periodic_timer();
        sensor.disable_periodic_timer();
        previous = source.reads.load();
        std::this_thread::sleep_for(Milliseconds(2));
        assert(source.reads.load() == previous);
    }
    auto elapsed = std::chrono::duration_cast<Milliseconds>(Clock::now() - start).count();
    assert(elapsed < 3000);
    std::cout << "PASS: 50 start/stop cycles, 60-second period, " << elapsed << " ms\n";

    // Stop while sampling is blocked: the notification can precede wait_for().
    {
        std::lock_guard<std::mutex> lock(source.mutex);
        source.block = true;
    }
    int previous = source.reads.load();
    sensor.enable_periodic_timer();
    wait_for_read(source, previous);
    std::thread stopper([&] { sensor.disable_periodic_timer(); });
    auto deadline = Clock::now() + std::chrono::seconds(2);
    while (!sensor.stop_polling.load() && Clock::now() < deadline)
        std::this_thread::sleep_for(Milliseconds(1));
    assert(sensor.stop_polling.load());
    {
        std::lock_guard<std::mutex> lock(source.mutex);
        source.block = false;
    }
    source.ready.notify_all();
    stopper.join();
    std::cout << "PASS: stop before polling wait is not lost\n";

    // Normal timeout still samples, and the wait does not busy-loop.
    sensor.polling_table[0].sample_period = 1;
    previous = source.reads.load();
    sensor.enable_periodic_timer();
    sensor.enable_periodic_timer();
    wait_for_read(source, previous);
    std::this_thread::sleep_for(Milliseconds(1150));
    sensor.disable_periodic_timer();
    int sampled = source.reads.load() - previous;
    assert(sampled >= 2 && sampled <= 3);
    std::cout << "PASS: periodic sampling and repeated enable/disable\n";

    start = Clock::now();
    {
        cthd_sensor_virtual scoped(2, "SCOPED", link, 1.0, 0.0);
        scoped.update_polling_table(entry);
        previous = source.reads.load();
        scoped.enable_periodic_timer();
        wait_for_read(source, previous);
        std::this_thread::sleep_for(Milliseconds(2));
    }
    assert(Clock::now() - start < std::chrono::seconds(1));
    std::cout << "PASS: destruction joins the polling thread promptly\n";
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repository", type=Path)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="thermald-timer-test-") as directory:
        work = Path(directory)
        for name in ("thd_sensor_virtual.cpp", "thd_sensor_virtual.h"):
            shutil.copyfile(args.repository / "src" / name, work / name)
        (work / "thd_sensor.h").write_text(SENSOR_HEADER)
        (work / "thd_engine.h").write_text(ENGINE_HEADER)
        (work / "test.cpp").write_text(TEST_SOURCE)
        command = shlex.split(os.environ.get("CXX", "c++"))
        command += ["-std=c++11", "-O1", "-g", "-Wall", "-Wextra", "-pthread"]
        if args.sanitize:
            command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        command += ["thd_sensor_virtual.cpp", "test.cpp", "-o", "test"]
        subprocess.run(command, cwd=work, check=True, timeout=60)
        try:
            subprocess.run([str(work / "test")], check=True, timeout=8)
        except subprocess.TimeoutExpired as error:
            raise SystemExit("FAIL: polling thread did not stop within 8 seconds") from error


if __name__ == "__main__":
    main()
