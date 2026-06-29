/*
 * thd_cdev_spel_restore.cpp: SPEL power limit restoration on exit
 * Copyright (c) 2026 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * Author Name <priyansh.jain@oss.qualcomm.com>
 *
 */

#include "thd_cdev_spel.h"
#include "thermald.h"
#include <vector>
#include <mutex>
#include <cstdlib>

// Global registry of SPEL devices that need restoration on exit
namespace {
	struct spel_restore_info {
		std::string sysfs_path;
		int constraint_index;
		int power_limit;
		int time_window;
	};

	std::vector<spel_restore_info> restore_registry;
	std::mutex registry_mutex;
	bool atexit_registered = false;

	// Called by atexit() - restores all SPEL power limits
	void restore_spel_limits() {
		std::vector<spel_restore_info> registry_copy;
		{
			std::lock_guard<std::mutex> lock(registry_mutex);
			registry_copy = restore_registry;
		}

		thd_log_info("Restoring %zu SPEL power limits on exit\n",
			registry_copy.size());

		for (const auto& info : registry_copy) {
			csys_fs sysfs(info.sysfs_path);

			// Restore power limit
			std::ostringstream power_limit_path;
			power_limit_path << "constraint_" << info.constraint_index
				<< "_power_limit_uw";
			if (sysfs.exists(power_limit_path.str())) {
				sysfs.write(power_limit_path.str(), info.power_limit);
				thd_log_info("  Restored SPEL power limit=%d uW for %s constraint_%d\n",
					info.power_limit, info.sysfs_path.c_str(),
					info.constraint_index);
			}

			// Restore time window
			std::ostringstream time_window_path;
			time_window_path << "constraint_" << info.constraint_index
				<< "_time_window_us";
			if (sysfs.exists(time_window_path.str())) {
				sysfs.write(time_window_path.str(), info.time_window);
				thd_log_info("  Restored SPEL time window=%d us for %s constraint_%d\n",
					info.time_window, info.sysfs_path.c_str(),
					info.constraint_index);
			}
		}
	}
}

// Register a SPEL device for restoration on exit.
// Called from update() after successful initialization so that the
// unconstrained (min_state) power limit and time window are saved as
// the restoration target.
void cthd_sysfs_cdev_spel::register_for_restoration() {
	std::lock_guard<std::mutex> lock(registry_mutex);

	// Register atexit handler on first call
	if (!atexit_registered) {
		if (std::atexit(restore_spel_limits) != 0) {
			thd_log_warn("Failed to register SPEL power limit restoration handler\n");
			return;
		}
		atexit_registered = true;
		thd_log_info("Registered SPEL power limit restoration handler\n");
	}

	const std::string sysfs_path = cdev_sysfs.get_base_path();

	// Avoid duplicate registrations for the same domain/constraint
	for (const auto &existing : restore_registry) {
		if (existing.sysfs_path == sysfs_path
				&& existing.constraint_index == constraint_index) {
			return;
		}
	}

	const int power_limit = spel_read_power_limit();
	const int time_window = spel_read_time_window();
	if (power_limit < 0 || time_window < 0) {
		thd_log_warn(
			"Failed to read initial SPEL state for %s constraint_%d, "
			"skipping restoration registration\n",
			sysfs_path.c_str(), constraint_index);
		return;
	}

	spel_restore_info info;
	info.sysfs_path = sysfs_path;
	info.constraint_index = constraint_index;
	info.power_limit = power_limit;
	info.time_window = time_window;

	restore_registry.push_back(info);

	thd_log_info("Registered SPEL %s constraint_%d: power_limit=%d uW, "
		"time_window=%d us\n",
		sysfs_path.c_str(), constraint_index,
		info.power_limit, info.time_window);
}
