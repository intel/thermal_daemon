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
#include "thd_common.h"
#include <cstring>
#include <iostream>

// Static member initialization
platform_type_t cthd_platform::detected_platform = PLATFORM_UNKNOWN;
bool cthd_platform::platform_detected = false;
std::string cthd_platform::machine_type = "";

platform_type_t cthd_platform::detect_platform() {
    if (platform_detected) {
        return detected_platform;
    }

    struct utsname sysinfo;
    if (uname(&sysinfo) != 0) {
        thd_log_error("Failed to get system information\n");
        detected_platform = PLATFORM_UNKNOWN;
        platform_detected = true;
        return detected_platform;
    }

    machine_type = std::string(sysinfo.machine);

    thd_log_info("Detected machine architecture: %s\n", machine_type.c_str());

    if (strcmp(sysinfo.machine, "x86_64") == 0) {
        detected_platform = PLATFORM_INTEL_X86;  // Default to Intel for x86_64
        thd_log_info("Intel/AMD x86_64 platform detected\n");
    } else if (strcmp(sysinfo.machine, "aarch64") == 0) {
        detected_platform = PLATFORM_ARM64;
        thd_log_info("ARM64 platform detected\n");
    } else if (strcmp(sysinfo.machine, "arm") == 0) {
        detected_platform = PLATFORM_ARM32;
        thd_log_info("ARM32 platform detected\n");
    } else {
        detected_platform = PLATFORM_OTHER;
        thd_log_info("Other architecture detected: %s\n", sysinfo.machine);
    }

    platform_detected = true;
    return detected_platform;
}

platform_type_t cthd_platform::get_platform() {
    if (!platform_detected) {
        return detect_platform();
    }
    return detected_platform;
}

std::string cthd_platform::get_machine_type() {
    if (!platform_detected) {
        detect_platform();
    }
    return machine_type;
}

bool cthd_platform::is_intel_platform() {
    platform_type_t platform = get_platform();
    return (platform == PLATFORM_INTEL_X86);
}

bool cthd_platform::is_arm_platform() {
    platform_type_t platform = get_platform();
    return (platform == PLATFORM_ARM64 || platform == PLATFORM_ARM32);
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
