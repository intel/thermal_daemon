/*
 * thd_platform.h: Platform detection and abstraction layer
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

#ifndef THD_PLATFORM_H_
#define THD_PLATFORM_H_

#include <string>
#include <sys/utsname.h>

typedef enum {
    PLATFORM_UNKNOWN = 0,
    PLATFORM_INTEL_X86,
    PLATFORM_OTHER
} platform_type_t;

class cthd_platform {
private:
    static platform_type_t detected_platform;
    static bool platform_detected;
    static std::string machine_type;

public:
    static platform_type_t detect_platform();
    static platform_type_t get_platform();
    static std::string get_machine_type();
    static bool is_intel_platform();
    static void dump_platform_info();
};

#endif /* THD_PLATFORM_H_ */
