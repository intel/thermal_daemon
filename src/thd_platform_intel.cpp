/*
 * thd_platform_intel.cpp: Intel platform-specific functionality implementation
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

#include "thd_platform_intel.h"
#include "thd_common.h"
#include "thd_engine.h"
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>

#ifndef ANDROID
#ifdef __x86_64__
#include <cpuid.h>
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#endif

#define BIT_ULL(nr)	(1ULL << (nr))

#ifndef ANDROID
#ifdef __x86_64__
typedef struct {
	unsigned int family;
	unsigned int model;
} supported_ids_t;

static supported_ids_t intel_id_table[] = {
    { 6, 0x2a }, // Sandybridge
    { 6, 0x3a }, // IvyBridge
    { 6, 0x3c }, // Haswell
    { 6, 0x45 }, // Haswell ULT
    { 6, 0x46 }, // Haswell ULT
    { 6, 0x3d }, // Broadwell
    { 6, 0x47 }, // Broadwell-GT3E
    { 6, 0x37 }, // Valleyview BYT
    { 6, 0x4c }, // Brasewell
    { 6, 0x4e }, // skylake
    { 6, 0x5e }, // skylake
    { 6, 0x5c }, // Broxton
    { 6, 0x7a }, // Gemini Lake
    { 6, 0x8e }, // kabylake
    { 6, 0x9e }, // kabylake
    { 6, 0x66 }, // Cannonlake
    { 6, 0x7e }, // Icelake
    { 6, 0x8c }, // Tigerlake_L
    { 6, 0x8d }, // Tigerlake
    { 6, 0xa5 }, // Cometlake
    { 6, 0xa6 }, // Cometlake_L
    { 6, 0xa7 }, // Rocketlake
    { 6, 0x9c }, // Jasper Lake
    { 6, 0x97 }, // Alderlake
    { 6, 0x9a }, // Alderlake
    { 6, 0xb7 }, // Raptorlake
    { 6, 0xba }, // Raptorlake
    { 6, 0xbe }, // Alderlake N
    { 6, 0xbf }, // Raptorlake S
    { 6, 0xaa }, // Mateor Lake L
    { 6, 0xbd }, // Lunar Lake M
    { 6, 0xc6 }, // Arrow Lake
    { 6, 0xc5 }, // Arrow Lake H
    { 6, 0xb5 }, // Arrow Lake U
    { 6, 0xcc }, // Panther Lake L
    { 0, 0 } // Last Invalid entry
};

std::vector<std::string> blocklist_paths {
    /* Some Lenovo machines have in-firmware thermal management,
     * avoid having two entities trying to manage things.
     * We may want to change this to dytc_perfmode once that is
     * widely available. */
    "/sys/devices/platform/thinkpad_acpi/dytc_lapmode",
};
#endif // __x86_64__
#endif

int cthd_platform_intel::check_cpu_id_intel(bool &proc_list_matched) {
#ifndef ANDROID
#ifdef __x86_64__
    unsigned int ebx, ecx, edx, max_level;
    unsigned int fms, family, model, stepping;
	unsigned int genuine_intel = 0;
    int i = 0;
    bool valid = false;

	proc_list_matched = false;
    ebx = ecx = edx = 0;

    __cpuid(0, max_level, ebx, ecx, edx);
    if (ebx == 0x756e6547 && edx == 0x49656e69 && ecx == 0x6c65746e)
        genuine_intel = 1;
    if (genuine_intel == 0) {
        // Simply return without further capability check
        return THD_SUCCESS;
    }
    __cpuid(1, fms, ebx, ecx, edx);
    family = (fms >> 8) & 0xf;
    model = (fms >> 4) & 0xf;
    stepping = fms & 0xf;
    if (family == 6 || family == 0xf)
        model += ((fms >> 16) & 0xf) << 4;

    thd_log_msg(
            "%u CPUID levels; family:model:stepping 0x%x:%x:%x (%u:%u:%u)\n",
            max_level, family, model, stepping, family, model, stepping);

    while (intel_id_table[i].family) {
        if (intel_id_table[i].family == family && intel_id_table[i].model == model) {
            proc_list_matched = true;
            valid = true;
            break;
        }
        i++;
    }
    if (!valid) {
        thd_log_msg(" Need Linux PowerCap sysfs\n");
    }

    for (std::string path : blocklist_paths) {
        struct stat s;

        if (!stat(path.c_str(), &s)) {
            proc_list_matched = false;
            thd_log_warn("[%s] present: Thermald can't run on this platform\n", path.c_str());
            break;
        }
    }
#else
    thd_log_info("Non-x86_64 platform detected in Intel check - skipping CPUID\n");
#endif // __x86_64__
#endif // ANDROID
    return THD_SUCCESS;
}

void cthd_platform_intel::workaround_rapl_mmio_power(void) {
    // First check if workaround is enabled and needed
    extern bool workaround_enabled;
    if (!workaround_enabled)
        return;

    // Check if RAPL MMIO controller is already being used
    extern cthd_engine *thd_engine;
    if (thd_engine) {
        cthd_cdev *cdev = thd_engine->search_cdev("rapl_controller_mmio");
        if (cdev) {
            /* RAPL MMIO is enabled and getting used. No need to disable */
            return;
        } else {
            csys_fs _sysfs("/sys/devices/virtual/powercap/intel-rapl-mmio/intel-rapl-mmio:0/");

            if (_sysfs.exists()) {
                std::stringstream temp_str;

                temp_str << "enabled";
                if (_sysfs.write(temp_str.str(), 0) > 0)
                    return;

                thd_log_debug("Failed to write to RAPL MMIO\n");
            }
        }
    }

#ifndef ANDROID
#ifdef __x86_64__
    int map_fd;
    void *rapl_mem;
    unsigned char *rapl_pkg_pwr_addr;
    unsigned long long pkg_power_limit;

    unsigned int ebx, ecx, edx;
    unsigned int fms, family, model;

    ecx = edx = 0;
    __cpuid(1, fms, ebx, ecx, edx);
    family = (fms >> 8) & 0xf;
    model = (fms >> 4) & 0xf;
    if (family == 6 || family == 0xf)
        model += ((fms >> 16) & 0xf) << 4;

    // Apply for KabyLake only
    if (model != 0x8e && model != 0x9e)
        return;

    map_fd = open("/dev/mem", O_RDWR, 0);
    if (map_fd < 0)
        return;

    rapl_mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, map_fd,
            0xfed15000);
    if (!rapl_mem || rapl_mem == MAP_FAILED) {
        close(map_fd);
        return;
    }

    rapl_pkg_pwr_addr = ((unsigned char *)rapl_mem + 0x9a0);
    pkg_power_limit = *(unsigned long long *)rapl_pkg_pwr_addr;
    *(unsigned long long *)rapl_pkg_pwr_addr = pkg_power_limit
            & ~BIT_ULL(15);

    munmap(rapl_mem, 4096);
    close(map_fd);
#endif // __x86_64__
#endif // ANDROID
}
