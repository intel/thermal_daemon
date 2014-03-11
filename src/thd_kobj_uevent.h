/*
 * thd_kobj_uevent.h: Get notification from kobj uevent
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
#ifndef THD_KOBJ_UEVENT_H_
#define THD_KOBJ_UEVENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/types.h>
#include <linux/netlink.h>

class cthd_kobj_uevent {
private:
	static const int max_buffer_size = 512;

	struct sockaddr_nl nls;
	int fd;
	char device_path[max_buffer_size];

public:
	cthd_kobj_uevent() {
		fd = 0;
		memset(&nls, 0, sizeof(nls));
		device_path[0] = '\0';
	}
	int kobj_uevent_open();
	void kobj_uevent_close();
	void register_dev_path(char *path);
	bool check_for_event();
}
;

#endif
