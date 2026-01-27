Enabling Performance Mode Thermal Tables on MSI Prestige 14 AI+ Flip Evo D3HMTG

Background
These steps are required to use performance mode thermal tables on the MSI laptop
"Prestige 14 AI+ Flip Evo D3HMTG". There is a missing interface to communicate the
power profile manager "performance" setting to the firmware, so thermald is not
informed about the switch of tables.

Note: These steps are NOT required for "balanced" mode of the power profile manager.

Prerequisites
Root/sudo access
The specific MSI laptop model mentioned above

These steps are verified for "Ubuntu 25.10", but should work for other Ubuntu versions.


Steps

1. Verify Model Number
There is an explicit check for the model name to avoid loading on other laptop models.
Verify the model name matches exactly:

cat /sys/class/dmi/id/product_name

Expected output: Prestige 14 AI+ Flip Evo D3HMTG

2. Switch to Performance Mode

Change to performance mode using either method:
Option A - GUI: Use GNOME Power Profile Manager Option B - Command line:

sudo powerprofilesctl set performance

3. Install Thermal Configuration

Copy the thermal configuration file:

sudo cp thermal-conf.xml /etc/thermald/

4. Modify Thermald Service

Remove the --adaptive parameter from the thermald service file:

Edit /usr/lib/systemd/system/thermald.service and change this line:
ExecStart=/usr/sbin/thermald --systemd --dbus-enable --adaptive
to:
ExecStart=/usr/sbin/thermald --systemd --dbus-enable

5. Restart Thermald Service

Reload systemd configuration and restart the service:

sudo systemctl daemon-reload
sudo systemctl restart thermald.service

6. Verify the Configuration

Check that the power limit has been updated correctly:

cat /sys/class/powercap/intel-rapl-mmio/intel-rapl-mmio\:0/constraint_0_power_limit_uw
Expected output: 45000000


Troubleshooting
If step 6 doesn't show the expected value, verify all previous steps were completed correctly.

Check thermald service status: sudo systemctl status thermald.service
Review thermald logs: sudo journalctl -u thermald.service.
Add loglevel=debug in the command line described in step 4 to get debug logs and
repeat step 5.

To get back to balanced mode again, add --adaptive option removed in step 4 and repeat step 5.

