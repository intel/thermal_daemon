/*
 * thd_platform_arm.h: ARM platform-specific functionality
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

#ifndef THD_PLATFORM_ARM_H_
#define THD_PLATFORM_ARM_H_

#include "thd_platform.h"

class arm_platform : public cthd_platform {
public:
    arm_platform();
    ~arm_platform() override;

    // Override virtual methods from base class
    void detect_platform() override;
    int check_cpu_id(bool &proc_list_matched) override;
};

#endif /* THD_PLATFORM_ARM_H_ */
