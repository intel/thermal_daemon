/*
 * thd_kobj_uevent.cpp: Get notification from kobj uevent
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#include "thd_kobj_uevent.h"
#include "thd_common.h"

int cthd_kobj_uevent::kobj_uevent_open() {
	memset(&nls, 0, sizeof(struct sockaddr_nl));

	nls.nl_family = AF_NETLINK;
	nls.nl_pid = getpid();
	nls.nl_groups = -1;

	fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (fd < 0)
		return fd;

	if (bind(fd, (struct sockaddr*) &nls, sizeof(struct sockaddr_nl))) {
		thd_log_warn("kob_uevent bin failed\n");
		close(fd);
		return -1;
	}

	return fd;
}

void cthd_kobj_uevent::kobj_uevent_close() {
	close(fd);
}

cthd_kobj_uevent::event_type cthd_kobj_uevent::check_for_event() {
	ssize_t len;
	char buffer[max_buffer_size];

	len = recv(fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
	if (len <= 0)
		return NO_EVENT;
	buffer[len] = '\0';

	return parse_event(buffer, len);
}

cthd_kobj_uevent::event_type cthd_kobj_uevent::parse_event(
		const char *buffer, size_t len) const {
	const char *dev_path = "DEVPATH=";
	const size_t dev_path_len = strlen(dev_path);
	const char *thinkpad_path = "/devices/platform/thinkpad_acpi";
	const char *thermal_zone_prefix = "/devices/virtual/thermal/thermal_zone";
	const char *int3400_name = "NAME=INT3400 Thermal";
	/* THERMAL_EVENT_KEEP_ALIVE from include/linux/thermal.h */
	const char *keep_alive_event = "EVENT=8";
	bool is_thermal_zone_dev = false;
	bool matched_registered_dev = false;
	bool is_int3400 = false;
	bool is_keep_alive = false;
	/*
	 * kobj_uevent_open() subscribes to all netlink groups (nl_groups = -1),
	 * so udevd re-broadcasts each kernel event with a "libudev\0" header.
	 * Ignore the udev duplicate for keep-alive to avoid writing imok twice.
	 */
	bool is_udev = (len >= 8 && !memcmp(buffer, "libudev", 8));
	size_t offset = 0;

	while (offset < len) {
		size_t field_len = strnlen(buffer + offset, len - offset);
		const char *field = buffer + offset;

		if (field_len > dev_path_len
				&& !strncmp(field, dev_path, dev_path_len)) {
			const char *path = field + dev_path_len;
			size_t path_len = field_len - dev_path_len;
			if (path_len == strlen(thinkpad_path)
					&& !memcmp(path, thinkpad_path, path_len))
				return THINKPAD_ACPI_EVENT;
			if (path_len >= strlen(thermal_zone_prefix)
					&& !memcmp(path, thermal_zone_prefix,
							strlen(thermal_zone_prefix)))
				is_thermal_zone_dev = true;
			if (device_path[0] && path_len >= strlen(device_path)
					&& !memcmp(path, device_path, strlen(device_path)))
				matched_registered_dev = true;
		} else if (field_len == strlen(int3400_name)
				&& !memcmp(field, int3400_name, field_len)) {
			is_int3400 = true;
		} else if (field_len == strlen(keep_alive_event)
				&& !memcmp(field, keep_alive_event, field_len)) {
			is_keep_alive = true;
		}
		offset += field_len + 1;
	}

	if (!is_udev && is_thermal_zone_dev && is_int3400 && is_keep_alive)
		return KEEP_ALIVE_EVENT;
	if (matched_registered_dev)
		return THERMAL_EVENT;

	return NO_EVENT;
}

void cthd_kobj_uevent::register_dev_path(char *path) {
	strncpy(device_path, path, max_buffer_size);
	device_path[max_buffer_size - 1] = '\0';
}
