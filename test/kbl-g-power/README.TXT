How to test kbl-g power sharing?

1.
Copy the built thermald to this folder

2.
The minimal kernel version to run this is 5.0 with AMD KBL-G support.
Also KBL-G Linux is not supported by standard distributions like Ubuntu,
So need to add manually.
Refer to
https://github.com/VCDP/KBL-G/wiki/KBL-G-AMD-VEGA-Driver-Getting-Started-Guide

After this verify that you have amdgpu power support
#grep -r amdgpu /sys/class/hwmon/*
It should show that in one of the hwmon folder amdgpu controls are present.

3.
Total Power of the systems which needs to be maintained needs to be
configured.
This can be configured via thermal_conf.xml.
For example the thermal_conf.xml in the attached file has a total power of 100W.

<Power>100000000</Power>

So this line can be changed to whatever the limit is.

Change <Hyst> line if required. This is basically hysteresis to avoid oscillation.

