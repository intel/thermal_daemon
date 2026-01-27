/*
 * thd_platform_intel.h: Intel platform-specific functionality
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
 */

#ifndef THD_PLATFORM_INTEL_H_
#define THD_PLATFORM_INTEL_H_

#include <vector>
#include "thd_parse.h"

class cthd_platform_intel {
public:
    // Intel-specific CPU ID checking
    static int check_cpu_id_intel(bool &proc_list_matched);

    // Intel-specific workaround functions
    static void workaround_rapl_mmio_power();

    // Other Intel-specific platform APIs can be added here
};

#endif /* THD_PLATFORM_INTEL_H_ */
