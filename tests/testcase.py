#!/usr/bin/env python3

import os
import sys
import unittest
import dbusmock
import fcntl
import io
import re
import gi
import time
import threading
import subprocess
import signal
import ioctls
import ctypes
import struct
import base64
gi.require_version('UMockdev', '1.0')
from gi.repository import UMockdev

from output_checker import OutputChecker

# Use installed version as a fallback, but try to find a local copy
thermald='thermald'
if 'TOP_BUILD_DIR' in os.environ:
    thermald=os.path.join(os.environ['TOP_BUILD_DIR'], 'thermald')
elif os.path.exists('./thermald'):
    thermald='./thermald'

THERMALD=[thermald, '--no-daemon', '--test-mode', '--loglevel=debug']

class TestCase(dbusmock.DBusTestCase):

    @classmethod
    def setUpClass(cls):
        super().setUpClass()

        # Python does not seem to pick up the environment change otherwise
        os.environ['LD_PRELOAD'] = 'libumockdev-preload.so.0'

    def setUp(self):
        self.acpi_art = (-19, b'')
        self.acpi_trt = (-19, b'')

        # We create a new testbed each time, as clear() does not clean up
        # completely currently (this.dev_fd) is not cleaned.
        self.testbed = UMockdev.Testbed.new()
        self.addCleanup(self.__delattr__, "testbed")

        self.acpi_thermal_rel_handler = UMockdev.IoctlBase()
        self.acpi_thermal_rel_handler.connect("handle-ioctl", self.acpi_thermal_rel_ioctl)

        self.testbed.attach_ioctl('/dev/acpi_thermal_rel', self.acpi_thermal_rel_handler)
        self.addCleanup(self.testbed.detach_ioctl, '/dev/acpi_thermal_rel')

        # Python does not pick up the environment change otherwise
        os.environ['UMOCKDEV_DIR'] = self.testbed.get_root_dir()

        super().setUp()

    def acpi_thermal_rel_ioctl(self, handler, client):
        request = client.get_request()

        if request in (ioctls.ACPI_THERMAL_GET_ART_COUNT, ioctls.ACPI_THERMAL_GET_ART_LEN, ioctls.ACPI_THERMAL_GET_ART):
            if self.acpi_art[0] < 0:
                client.complete(-1, -self.acpi_art[0])
                return True

            arg = client.get_arg()

            if request == ioctls.ACPI_THERMAL_GET_ART_COUNT:
                i = struct.pack('l', self.acpi_art[0])
                data = arg.resolve(0, len(i));
                data.update(0, i)
            elif request == ioctls.ACPI_THERMAL_GET_ART_LEN:
                i = struct.pack('l', len(self.acpi_art[1]))
                data = arg.resolve(0, len(i));
                data.update(0, i)
            elif request == ioctls.ACPI_THERMAL_GET_ART:
                data = arg.resolve(0, len(self.acpi_art[1]))
                data.update(0, self.acpi_art[1])

            client.complete(0, 0)
            return True

        elif request in (ioctls.ACPI_THERMAL_GET_TRT_COUNT, ioctls.ACPI_THERMAL_GET_TRT_LEN, ioctls.ACPI_THERMAL_GET_TRT):
            if self.acpi_trt[0] < 0:
                client.complete(-1, -self.acpi_trt[0])
                return True

            arg = client.get_arg()

            if request == ioctls.ACPI_THERMAL_GET_TRT_COUNT:
                i = struct.pack('l', self.acpi_trt[0])
                data = arg.resolve(0, len(i));
                data.update(0, i)
            elif request == ioctls.ACPI_THERMAL_GET_TRT_LEN:
                i = struct.pack('l', len(self.acpi_trt[1]))
                data = arg.resolve(0, len(i));
                data.update(0, i)
            elif request == ioctls.ACPI_THERMAL_GET_TRT:
                data = arg.resolve(0, len(self.acpi_trt[1]))
                data.update(0, self.acpi_trt[1])

            client.complete(0, 0)
            return True

        return False

    def load_hwconfig(self, hw_config):
        self.testbed.add_from_file(os.path.join(os.path.dirname(__file__), hw_config, "devices"))
        self.addCleanup(self.testbed.clear)

        with open(os.path.join(os.path.dirname(__file__), hw_config, "acpi_thermal_rel")) as f:
            art = f.readline().split(' ')
            trt = f.readline().split(' ')
            self.acpi_art = int(art[0]), base64.b64decode(art[1] if len(art) == 2 else b'')
            self.acpi_trt = int(trt[0]), base64.b64decode(trt[1] if len(trt) == 2 else b'')

    def thermald_start(self, adaptive=True, ignore_cpuid_check=False):
        self.thermald = None

        env = os.environ.copy()
        # Any environment to set for thermald?

        argv = THERMALD[:]
        if adaptive:
            argv.append('--adaptive')
        if ignore_cpuid_check:
            argv.append('--ignore-cpuid-check')

        valgrind = os.getenv('VALGRIND')
        self.valgrind = False
        if valgrind is not None:
            argv.insert(0, 'valgrind')
            argv.insert(1, '--leak-check=full')
            if os.path.exists(valgrind):
                argv.insert(2, '--suppressions=%s' % valgrind)
            self.valgrind = True

        strace = os.getenv('STRACE')
        self.strace = False
        if strace is not None and strace != '0':
            argv.insert(0, 'strace')
            argv.insert(1, '-ff')
            self.strace = True

        self.thermald_output = OutputChecker()
        print(f"Running thermald: {argv}")
        self.thermald = subprocess.Popen(argv,
                                         env=env,
                                         stdout=self.thermald_output.fd,
                                         stderr=subprocess.STDOUT)
        self.thermald_output.writer_attached()

    def thermald_stop(self, retcode=0, timeout=5):
        real_pid = self.thermald.pid
        if self.strace or self.valgrind:
            c = open(f"/proc/{real_pid}/task/{real_pid}/children")
            real_pid = int(c.read())
            c.close()

        os.kill(real_pid, signal.SIGTERM)
        try:
            self.thermald.wait(timeout)
            self.assertEqual(self.thermald.returncode, retcode)
            self.thermald_output.assert_closed()

        except subprocess.TimeoutExpired:
            os.kill(real_pid, signal.SIGABRT)
            self.thermald.wait(timeout)
            self.thermald_output.assert_closed()
            raise AssertionError("Thermald had to be killed") from None

    def test_x1c7_lapmode(self):
        self.load_hwconfig("x1c7")

        self.thermald_start(ignore_cpuid_check=False)
        self.thermald_output.check_line(b'/sys/devices/platform/thinkpad_acpi/dytc_lapmode', timeout=5)
        self.thermald_output.check_line(b'Unsupported cpu model or platform', timeout=5)

        # XXX: At some point I thought we set this to 2, but seems it is still explicitly 0 in some cases
        self.thermald_stop(retcode=0)

    def test_x1c7_non_adaptive(self):
        self.load_hwconfig("x1c7")

        self.thermald_start(adaptive=False, ignore_cpuid_check=True)

        # In non-adaptive mode, the ART/TRT data is read
        # ART data fails to read, TRT data is found
        self.thermald_output.check_line(b'failed to GET COUNT on /dev/acpi_thermal_rel', timeout=1)
        self.thermald_output.check_line(b'TRT count 1', timeout=1)
        self.thermald_output.check_line_re(b'TRT 0:.*SRC B0D4.*TGT B0D4.*INF 18.*SMPL 50', timeout=1)

        # INT3400 UUIDs
        self.thermald_output.check_line(b'uuid: 3A95C389-E4B8-4629-A526-C52C88626BAE', timeout=1)
        self.thermald_output.check_line(b'uuid: 63BE270F-1C11-48FD-A6F7-3AF253FF3E2D', timeout=1)
        self.thermald_output.check_line(b'uuid: 9E04115A-AE87-4D1C-9500-0F3E340BFE75', timeout=1)

        self.thermald_output.check_line(b'thd_engine_thread begin', timeout=1)
        # We sleep 4 seconds now, wait for a bit longer
        self.thermald_output.check_line(b'poll exit 0 polls_fd event 0 0', timeout=6)

        # Mainloop appears running, quit
        self.thermald_stop()

    def test_x1c7_adaptive(self):
        self.load_hwconfig("x1c7")

        self.thermald_start(adaptive=True, ignore_cpuid_check=True)

        # Some of the adaptive information
        self.thermald_output.check_line(b'..ppcc dump begin..', timeout=5)
        self.thermald_output.check_line(b'Name :IETM.D0', timeout=1)
        self.thermald_output.check_line(b'Name :0x0', timeout=1)
        self.thermald_output.check_line(b'Name :0xd78', timeout=1)
        self.thermald_output.check_line(b'Name :0xd78_1', timeout=1)

        self.thermald_output.check_line(b'..apat dump begin..', timeout=1)
        self.thermald_output.check_line(b'target_id:1 name:0x4 participant:\_SB_.PCI0.B0D4 domain:9 code:PL2PowerLimit argument:5000', timeout=1)
        self.thermald_output.check_line(b'target_id:2 name:0xC', timeout=1)
        self.thermald_output.check_line(b'target_id:3 name:0x8s', timeout=1)
        self.thermald_output.check_line(b'target_id:11 name:0x8', timeout=1)
        self.thermald_output.check_line(b'target_id:4 name:0x1s', timeout=1)
        self.thermald_output.check_line(b'target_id:12 name:0x1', timeout=1)
        self.thermald_output.check_line(b'target_id:5 name:0xD78s', timeout=1)
        self.thermald_output.check_line(b'target_id:13 name:0xD78', timeout=1)
        self.thermald_output.check_line(b'target_id:10 name:0xD1234', timeout=1)
        self.thermald_output.check_line(b'target_id:6 name:0x0', timeout=1)

        # APCT has 9 condition sets
        self.thermald_output.check_line(b'..apct dump begin..', timeout=1)
        self.thermald_output.check_line(b'condition_set 9', timeout=1)

        # IDSP has 3 UUIDs
        self.thermald_output.check_line(b'..idsp dump begin..', timeout=1)
        self.thermald_output.check_line(b'3A95C389-E4B8-4629-A526-C52C88626BAE', timeout=1)
        self.thermald_output.check_line(b'63BE270F-1C11-48FD-A6F7-3AF253FF3E2D', timeout=1)
        self.thermald_output.check_line(b'9E04115A-AE87-4D1C-9500-0F3E340BFE75', timeout=1)


        self.thermald_output.check_line(b'thd_engine_thread begin', timeout=5)
        # We sleep 4 seconds now, wait for a bit longer
        self.thermald_output.check_line(b'poll exit 0 polls_fd event 0 0', timeout=6)

        # Mainloop appears running, quit
        self.thermald_stop()


def list_tests():
    import unittest_inspector
    return unittest_inspector.list_tests(sys.modules[__name__])

if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] == "list-tests":
        for machine, human in list_tests():
            print("%s %s" % (machine, human), end="\n")
        sys.exit(0)

    prog = unittest.main(verbosity=2, exit=False)
    if prog.result.errors or prog.result.failures:
        sys.exit(1)

    # Translate to skip error
    if prog.result.testsRun == len(prog.result.skipped):
        sys.exit(77)

    sys.exit(0)

