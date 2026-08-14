/*
 * cthd_cdev_rapl_restore.cpp: RAPL power limit restoration on exit
 *	using RAPL
 * Copyright (C) 2026 Intel Corporation. All rights reserved.
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

#include "thd_cdev_rapl.h"
#include "thermald.h"
#include <vector>
#include <mutex>
#include <cstdlib>

// Global registry of RAPL devices that need restoration on exit
namespace {
	struct rapl_restore_info {
		std::string sysfs_path;
		int constraint_index;
		unsigned long long power_limit;
		int time_window;
		int enable_status;
	};

	std::vector<rapl_restore_info> restore_registry;
	std::mutex registry_mutex;
	bool atexit_registered = false;

	// Called by atexit() - restores all RAPL power limits
	void restore_rapl_limits() {
		std::vector<rapl_restore_info> registry_copy;
		{
			std::lock_guard<std::mutex> lock(registry_mutex);
			registry_copy = restore_registry;
		}

		thd_log_info("Restoring %zu RAPL power limits on exit\n",
			registry_copy.size());

		for (const auto& info : registry_copy) {
			csys_fs sysfs(info.sysfs_path);

			// Restore PL1 power limit
			std::ostringstream power_limit_path;
			power_limit_path << "constraint_" << info.constraint_index << "_power_limit_uw";
			if (sysfs.exists(power_limit_path.str())) {
				sysfs.write(power_limit_path.str(), std::to_string(info.power_limit));
				thd_log_info("  Restored PL1=%llu uW for %s\n",
					info.power_limit, info.sysfs_path.c_str());
			}

			// Restore time window
			std::ostringstream time_window_path;
			time_window_path << "constraint_" << info.constraint_index << "_time_window_us";
			if (sysfs.exists(time_window_path.str())) {
				sysfs.write(time_window_path.str(), info.time_window);
				thd_log_info("  Restored PL1 time window=%d for %s\n",
					info.time_window, info.sysfs_path.c_str());
			}

			// Restore enable status
			if (sysfs.exists("enabled")) {
				sysfs.write("enabled", info.enable_status);
				thd_log_info("  Restored PL1 enabled=%d for %s\n",
					info.enable_status, info.sysfs_path.c_str());
			}
		}
	}
}

// Register a RAPL device for restoration on exit
void cthd_sysfs_cdev_rapl::register_for_restoration() {
	std::lock_guard<std::mutex> lock(registry_mutex);

	// Register atexit handler on first call
	if (!atexit_registered) {
		if (std::atexit(restore_rapl_limits) != 0) {
			thd_log_warn("Failed to register RAPL power limit restoration handler\n");
			return;
		}
		atexit_registered = true;
		thd_log_info("Registered RAPL power limit restoration handler\n");
	}

	const std::string sysfs_path = cdev_sysfs.get_base_path();
	for (const auto &existing : restore_registry) {
		if (existing.sysfs_path == sysfs_path
			&& existing.constraint_index == constraint_index) {
			return;
		}
	}

	std::ostringstream power_limit_path;
	power_limit_path << "constraint_" << constraint_index << "_power_limit_uw";

	unsigned long power_limit = 0;
	const int read_power_ret = cdev_sysfs.read(power_limit_path.str(), &power_limit);
	const int time_window = rapl_read_time_window();
	const int enable_status = rapl_read_enable_status();
	if (read_power_ret <= 0 || time_window < 0 || enable_status < 0) {
		thd_log_warn(
			"Failed to read initial RAPL state for %s, skipping restoration registration\n",
			sysfs_path.c_str());
		return;
	}

	rapl_restore_info info;
	info.sysfs_path = sysfs_path;
	info.constraint_index = constraint_index;
	info.power_limit = power_limit;
	info.time_window = time_window;
	info.enable_status = enable_status;

	restore_registry.push_back(info);

	thd_log_info("Registered RAPL %s: PL1=%llu uW, window=%d us, enable=%d\n",
		sysfs_path.c_str(), info.power_limit, info.time_window, info.enable_status);
}
