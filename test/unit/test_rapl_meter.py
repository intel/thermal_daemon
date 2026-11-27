from pathlib import Path
import subprocess


def test_package_power_interval(tmp_path):
    package = tmp_path / "pkg"
    package.mkdir()
    (package / "name").write_text("package-0\n")
    (package / "max_energy_range_uj").write_text("1000000000\n")
    subprocess.run([Path(__file__).parent / "build" / "rapl_meter", tmp_path], check=True)
