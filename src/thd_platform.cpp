/*
 * thd_platform.cpp: Platform detection and abstraction layer implementation
 *
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
 * Author Name <priyansh.jain@oss.qualcomm.com>
 */

#include "thd_platform.h"
#include "thd_platform_intel.h"
#include "thd_platform_arm.h"
#include "thd_common.h"
#include <cstring>
#include <iostream>

cthd_platform::cthd_platform() : detected_platform(PLATFORM_UNKNOWN), machine_type("") {
}

cthd_platform::~cthd_platform() {
}

void cthd_platform::detect_platform() {
    struct utsname sysinfo;
    if (uname(&sysinfo) != 0) {
        thd_log_error("Failed to get system information\n");
        detected_platform = PLATFORM_UNKNOWN;
        return;
    }

    machine_type = std::string(sysinfo.machine);
    thd_log_info("Detected machine architecture: %s\n", machine_type.c_str());

    if (strcmp(sysinfo.machine, "x86_64") == 0) {
        detected_platform = PLATFORM_INTEL_X86;
    } else if (strcmp(sysinfo.machine, "aarch64") == 0) {
        detected_platform = PLATFORM_ARM64;
    } else if (strcmp(sysinfo.machine, "arm") == 0) {
        detected_platform = PLATFORM_ARM32;
    } else {
        detected_platform = PLATFORM_OTHER;
    }
}

platform_type_t cthd_platform::get_platform() {
    return detected_platform;
}

std::string cthd_platform::get_machine_type() {
    return machine_type;
}

int cthd_platform::check_cpu_id(bool &proc_list_matched) {
    // Base implementation - to be overridden by derived classes
    proc_list_matched = false;
    return THD_SUCCESS;
}

void cthd_platform::workaround_rapl_mmio_power() {
    // Base implementation - to be overridden by derived classes
    // No workaround needed for generic platform
}

void cthd_platform::dump_platform_info() {
    platform_type_t platform = get_platform();

    thd_log_info("=== Platform Information ===\n");
    thd_log_info("Machine Type: %s\n", get_machine_type().c_str());

    switch (platform) {
        case PLATFORM_INTEL_X86:
            thd_log_info("Platform: Intel x86/x86_64\n");
            break;
        case PLATFORM_ARM64:
            thd_log_info("Platform: ARM64 (aarch64)\n");
            break;
        case PLATFORM_ARM32:
            thd_log_info("Platform: ARM32\n");
            break;
        case PLATFORM_OTHER:
            thd_log_info("Platform: Other (%s)\n", get_machine_type().c_str());
            break;
        case PLATFORM_UNKNOWN:
        default:
            thd_log_info("Platform: Unknown\n");
            break;
    }
    thd_log_info("============================\n");
}

std::unique_ptr<cthd_platform> cthd_platform::create_platform() {
    // Detect platform architecture using uname
    struct utsname sysinfo;
    if (uname(&sysinfo) != 0) {
        thd_log_error("Failed to get system information\n");
        return std::unique_ptr<cthd_platform>(new cthd_platform());
    }

    // Create appropriate platform instance based on architecture
    if (strcmp(sysinfo.machine, "x86_64") == 0) {
        thd_log_info("Creating Intel platform instance\n");
        return std::unique_ptr<cthd_platform>(new intel_platform());
    } else if (strcmp(sysinfo.machine, "aarch64") == 0 || strcmp(sysinfo.machine, "arm") == 0) {
        thd_log_info("Creating ARM platform instance\n");
        return std::unique_ptr<cthd_platform>(new arm_platform());
    } else {
        thd_log_info("Creating generic platform instance for %s\n", sysinfo.machine);
        return std::unique_ptr<cthd_platform>(new cthd_platform());
    }
}
