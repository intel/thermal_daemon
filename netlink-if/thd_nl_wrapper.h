/*
 * thd_nl_wrapper.h: Netlink wrapper class interface
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
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
 * Copied code from acpi_genl implementaion from Zhang Rui <rui.zhang@intel.com>
 */

#ifndef THD_NL_WRAPPER_H
#define THD_NL_WRAPPER_H

#include "thermald.h"
#include "genetlink.h"
#include "libnetlink.h"
#include "acpi_genetlink.h"
#include <linux/netlink.h>

class cthd_engine; // forward declaration

class cthd_nl_wrapper {
private:
	__u16 acpi_event_family_id;
	__u32 acpi_event_mcast_group_id;
	struct rtnl_handle rt_handle;
	cthd_engine *thd_engine;
	
	__u32 nl_mgrp(__u32 group);
	int print_ctrl_cmds(FILE * fp, struct rtattr *arg, __u32 ctrl_ver);
	int print_ctrl_grp(FILE * fp, struct rtattr *arg, __u32 ctrl_ver);
	int get_ctrl_grp_id(struct rtattr *arg);
	int genl_get_mcast_group_id(struct nlmsghdr *n);
	int genl_print_ctrl_message(const struct sockaddr_nl *who,
				   struct nlmsghdr *n, void *arg);
	
	int genl_get_family_status(char *family_name, int type);

public:
	int genl_acpi_family_open(cthd_engine *engine);
	int genl_print_acpi_event_message(const struct sockaddr_nl *who,
					 struct nlmsghdr *n, void *arg);

	int genl_parse_event_message(const struct sockaddr_nl *who,
			 struct nlmsghdr *n, void *arg);

	int rtnl_fd() {return rt_handle.fd;}
	int genl_message_indication();

};

#endif
