/*
 * thd_cdev_backlight.cpp: thermal backlight cooling implementation
 *
 * Copyright (C) 2015 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 or later as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 * Author Name <Srinivas.Pandruvada@linux.intel.com>
 *
 */

#include "thd_cdev_backlight.h"
#define MAX_BACKLIGHT_DEV 4

const std::string cthd_cdev_backlight::backlight_devices[MAX_BACKLIGHT_DEV] = {
	            "/sys/class/backlight/intel_backlight/",
	            "/sys/class/backlight/acpi_video0/",
	            "/sys/class/leds/lcd-backlight/",
	            "/sys/class/backlight/lcd-backlight/"
};

cthd_cdev_backlight::cthd_cdev_backlight(unsigned int _index, int _cpu_index):
		cthd_cdev(_index, backlight_devices[0]) {
		int active_device = 0;
		do {
			cdev_sysfs.update_path(backlight_devices[active_device]);
			if (update() == THD_SUCCESS)
				break;
			active_device++;
		}while (active_device <  MAX_BACKLIGHT_DEV);
}

int cthd_cdev_backlight::update() {
	int ret;

	if (cdev_sysfs.exists()) {
		std::string temp_str;

		ret = cdev_sysfs.read("max_brightness", temp_str);
		if (ret < 0)
			return ret;
		std::istringstream(temp_str) >> max_state;
	}

	if (max_state <= 0)
		return THD_ERROR;

	set_inc_dec_value(max_state * (float) 10 / 100);

	return THD_SUCCESS;
}

void cthd_cdev_backlight::set_curr_state(int state, int arg) {
	int ret;

	ret = cdev_sysfs.write("brightness", max_state - state);
	if (ret < 0) {
		thd_log_warn("Failed to write brightness\n");
		return;
	}
	curr_state = state;
}
