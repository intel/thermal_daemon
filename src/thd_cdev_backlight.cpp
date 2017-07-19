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

cthd_cdev_backlight::cthd_cdev_backlight(unsigned int _index, int _cpu_index) :
		cthd_cdev(_index, backlight_devices[0]), ref_backlight_state(0) {
	int active_device = 0;
	do {
		cdev_sysfs.update_path(backlight_devices[active_device]);
		if (update() == THD_SUCCESS)
			break;
		active_device++;
	} while (active_device < MAX_BACKLIGHT_DEV);
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

	// Don't let backlight less than min_backlight_percent percent
	min_back_light = max_state * min_backlight_percent /100;

	return THD_SUCCESS;
}

int cthd_cdev_backlight::map_target_state(int target_valid, int target_state) {
	if (!target_valid)
		return target_state;

	if (target_state > max_state)
		return 0;

	return max_state - target_state;
}

void cthd_cdev_backlight::set_curr_state(int state, int arg) {
	int ret;
	int backlight_val;

	/*
	 * When state > 0, then we need to reduce backlight. But we should start
	 * from backlight which triggered thermal condition. That is here stored
	 * in ref_backlight_state.
	 * When the first state more than 0 is called, then this variable is
	 * updated with the current backlight value. When the state == 0,
	 * it is restored.
	 */

	if (state == 0) {
		if (ref_backlight_state) {

			thd_log_debug("LCD restore original %d\n", ref_backlight_state);
			ret = cdev_sysfs.write("brightness", ref_backlight_state);
			if (ret < 0) {
				thd_log_warn("Failed to write brightness\n");
				return;
			}
			ref_backlight_state = 0;
		}
		curr_state = state;
		return;
	}

	if (ref_backlight_state == 0 && state == inc_dec_val) {
		std::string temp_str;

		// First time enter to throttle LCD after normal or state == 0
		// Store the current backlight
		ret = cdev_sysfs.read("brightness", temp_str);
		if (ret < 0)
			return;

		std::istringstream(temp_str) >> ref_backlight_state;

		thd_log_debug("LCD ref state is %d\n", ref_backlight_state);
	}

	backlight_val = ref_backlight_state - state;

	if (backlight_val <= min_back_light) {
		thd_log_debug("LCD reached min state\n");
		backlight_val = min_back_light;
	}

	ret = cdev_sysfs.write("brightness", backlight_val);
	if (ret < 0) {
		thd_log_warn("Failed to write brightness\n");
		return;
	}

	curr_state = state;
}
