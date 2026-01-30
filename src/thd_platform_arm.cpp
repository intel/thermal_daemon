/*
 * thd_platform_arm.cpp: ARM platform-specific functionality implementation
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

#include "thd_platform_arm.h"
#include "thd_common.h"

int cthd_platform_arm::check_cpu_id_arm(bool &proc_list_matched) {

    // For ARM, we assume the platform is supported
    proc_list_matched = true;

    return THD_SUCCESS;
}
