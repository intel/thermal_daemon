from pathlib import Path
import subprocess


def test_pid_elapsed_time():
    subprocess.run([Path(__file__).parent / "build" / "pid_clock"], check=True)


def test_pid_zone_reset():
    subprocess.run([Path(__file__).parent / "build" / "pid_device"], check=True)


def test_pid_band_release():
    subprocess.run([Path(__file__).parent / "build" / "pid_device", "band-release"], check=True)


def test_large_pid_delta():
    subprocess.run([Path(__file__).parent / "build" / "pid_device", "large-delta"], check=True)
