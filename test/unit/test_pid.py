from pathlib import Path
import subprocess


def test_pid_elapsed_time():
    subprocess.run([Path(__file__).parent / "build" / "pid_clock"], check=True)
