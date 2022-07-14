#!/usr/bin/env python3

import gi
gi.require_version('GUdev', '1.0')
from gi.repository import GUdev
import os
import sys
import fcntl
import subprocess
import struct
import signal
import base64
import ioctls

ioctl_devs = [
        '/dev/acpi_thermal_rel'
    ]

# Base paths to look into
base_paths = [
        # Thermal zones and cooling devices
        "/sys/class/thermal",
        # Platform drivers (including vendor specific ones,
        # even if we don't really need them currently).
        "/sys/devices/platform",
        # Sensors
        "/sys/class/hwmon",
        # CPU information (e.g. pstate)
        "/sys/devices/system/cpu",
        "/sys/devices/virtual/powercap",
        "/sys/devices/virtual/thermal",
        "/sys/devices/LNXSYSTM:00/",
    ]

assert(len(sys.argv) == 2)
output = sys.argv[1]
os.mkdir(output)

# TODO: input event for SW_TABLET_MODE (and SW_LID)

paths = []

c = GUdev.Client()
# Just use all devices
for dev in c.query_by_subsystem():
    path = dev.get_sysfs_path()
    for base in base_paths:
        if path.startswith(base):
            break
    else:
        continue

    paths.append(path)

for dev in ioctl_devs:
    paths.append(dev)

# First run umockdev-record to dump the device information
recording = subprocess.run(
        [
            "umockdev-record",
            *paths
        ],
        stdout=subprocess.PIPE,
    )

open(os.path.join(output, "devices"), "bw").write(recording.stdout)

# Then, we need to record some IOCTLs
for dev in ioctl_devs:
    fd = os.open(dev, os.O_RDWR)

    if dev == '/dev/acpi_thermal_rel':
        atr = open(os.path.join(output, "acpi_thermal_rel"), "w")

        try:
            count = bytearray(struct.pack('l', 0))
            fcntl.ioctl(fd, ioctls.ACPI_THERMAL_GET_ART_COUNT, count, True)
            count = struct.unpack('l', count)[0]

            length = bytearray(struct.pack('l', 0))
            fcntl.ioctl(fd, ioctls.ACPI_THERMAL_GET_ART_LEN, length, True)
            length = struct.unpack('l', length)[0]

            art = bytearray(length)
            fcntl.ioctl(fd, ioctls.ACPI_THERMAL_GET_ART, art, True)

            atr.write(f"{count} {base64.b64encode(art).decode('ascii')}\n")
        except OSError as e:
            if e.errno != 19:
                raise
            atr.write(f"-19\n")

        try:
            count = bytearray(struct.pack('l', 0))
            fcntl.ioctl(fd, ioctls.ACPI_THERMAL_GET_TRT_COUNT, count, True)
            count = struct.unpack('l', count)[0]

            length = bytearray(struct.pack('l', 0))
            fcntl.ioctl(fd, ioctls.ACPI_THERMAL_GET_TRT_LEN, length, True)
            length = struct.unpack('l', length)[0]

            trt = bytearray(length)
            fcntl.ioctl(fd, ioctls.ACPI_THERMAL_GET_TRT, trt, True)

            atr.write(f"{count} {base64.b64encode(trt).decode('ascii')}\n")
        except OSError as e:
            if e.errno != 19:
                raise
            atr.write(f"-19\n")
    else:
        raise AssertionError(f"No code to store ioctl information for device {dev}")

