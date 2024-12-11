/*
 * thd_thermal_netlink.cpp: Get notification from thermal genl
 * netlinks
 *
 * Copyright (C) 2024 Intel Corporation. All rights reserved.
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

#include <mutex>
#include <netlink/socket.h>
#include <netlink/netlink.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>

#include "thermal.h"
#include "thd_common.h"

static struct nl_sock *nl_sock;

static int connect()
{
	struct nl_sock *socket;
	int ret;

	socket = nl_socket_alloc();
	if (!socket)
		return THD_ERROR;

	ret = genl_connect(socket);
	if (ret) {
		nl_socket_free(socket);
		return THD_ERROR;
	}

	nl_sock = socket;

	return THD_SUCCESS;
}

static int handle_event(struct nl_msg *n, void *arg)
{
	struct nlmsghdr *nlh = nlmsg_hdr(n);
	struct genlmsghdr *genlhdr = genlmsg_hdr(nlh);
	struct nlattr *attrs[THERMAL_GENL_ATTR_MAX + 1];
	int ret;

	ret = genlmsg_parse(nlh, 0, attrs, THERMAL_GENL_ATTR_MAX, NULL);

	thd_log_debug("Received event %d parse_rer:%d\n", genlhdr->cmd, ret);

	return 0;
}

void netlink_free(void)
{
	if (nl_sock) {
		nl_close(nl_sock);
		nl_socket_free(nl_sock);
	}
}

void netlink_receive(void)
{
	int err = 0;

	while (!err)
		err = nl_recvmsgs_default(nl_sock);
}

int netlink_init(int *fd)
{
	int family;
	int group;
	int ret;

	if (connect()) {
		thd_log_info("Failed to connect\n");
		return THD_ERROR;
	}

	try {
		family = genl_ctrl_resolve(nl_sock, THERMAL_GENL_FAMILY_NAME);
		if (family < 0) {
			thd_log_info("Failed to resolve family\n");
			throw(THD_ERROR);
		}

		nl_socket_disable_seq_check(nl_sock);

		group = genl_ctrl_resolve_grp(nl_sock, THERMAL_GENL_FAMILY_NAME,
		THERMAL_GENL_EVENT_GROUP_NAME);
		if (group < 0) {
			thd_log_info("Failed to resolve multicast group\n");
			throw(THD_ERROR);
		}

		/* Join the multicast group. */
		if ((ret = nl_socket_add_membership(nl_sock, group) < 0)) {
			thd_log_info("failed to join multicast group: %s\n", strerror(-ret));
			throw(THD_ERROR);
		}

		ret = nl_socket_modify_cb(nl_sock, NL_CB_VALID, NL_CB_CUSTOM,
				handle_event, NULL);
		if (ret) {
			thd_log_info("Failed to set callback %s\n", strerror(-ret));
			throw(THD_ERROR);
		}

		nl_socket_set_nonblocking(nl_sock);

		*fd = nl_socket_get_fd(nl_sock);
	} catch (int code) {
		thd_log_info("Thermal Netlink Error\n");
		nl_close(nl_sock);
		nl_socket_free(nl_sock);
		nl_sock = NULL;
		return THD_ERROR;
	}

	return THD_SUCCESS;
}
