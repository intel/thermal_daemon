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

bool cthd_kobj_uevent::check_for_event() {
	ssize_t i = 0;
	ssize_t len;
	const char *dev_path = "DEVPATH=";
	unsigned int dev_path_len = strlen(dev_path);
	char buffer[max_buffer_size];

	len = recv(fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
	if (len <= 0)
		return false;
	buffer[len] = '\0';

	while (i < len) {
		if (strlen(buffer + i) > dev_path_len
				&& !strncmp(buffer + i, dev_path, dev_path_len)) {
			if (!strncmp(buffer + i + dev_path_len, device_path,
					strlen(device_path))) {
				return true;
			}
		}
		i += strlen(buffer + i) + 1;
	}

	return false;
}

void cthd_kobj_uevent::register_dev_path(char *path) {
	strncpy(device_path, path, max_buffer_size);
	device_path[max_buffer_size - 1] = '\0';
}
