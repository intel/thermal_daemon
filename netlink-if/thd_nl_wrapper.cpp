/*
 * thd_nl_wrapper.cpp: Netlink wrapper class implementation
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

#include "thd_nl_wrapper.h"
#include "thd_engine.h"

__u32 cthd_nl_wrapper::nl_mgrp(__u32 group)
{
	if (group > 31) {
		fprintf(stderr, "Use setsockopt for this group %d\n", group);
		exit(-1);
	}
	return group ? (1 << (group - 1)) : 0;
}


int cthd_nl_wrapper::print_ctrl_cmds(FILE * fp, struct rtattr *arg, __u32 ctrl_ver)
{
	struct rtattr *tb[CTRL_ATTR_OP_MAX + 1];

	if (arg == NULL)
		return -1;

	parse_rtattr_nested(tb, CTRL_ATTR_OP_MAX, arg);
	if (tb[CTRL_ATTR_OP_ID]) {
		__u32 *id = (__u32*)RTA_DATA(tb[CTRL_ATTR_OP_ID]);
		fprintf(fp, " ID-0x%x ", *id);
	}
	return 0;
}

int cthd_nl_wrapper::print_ctrl_grp(FILE * fp, struct rtattr *arg, __u32 ctrl_ver)
{
        struct rtattr *tb[CTRL_ATTR_MCAST_GRP_MAX + 1];

        if (arg == NULL)
                return -1;

        parse_rtattr_nested(tb, CTRL_ATTR_MCAST_GRP_MAX, arg);
        if (tb[2]) {
                __u32 *id = (__u32*)RTA_DATA(tb[CTRL_ATTR_MCAST_GRP_ID]);
                fprintf(fp, " ID: 0x%x ", *id);
        }
        if (tb[1]) {
              	char *name = (char*)RTA_DATA(tb[CTRL_ATTR_MCAST_GRP_NAME]);
                fprintf(fp, " Name: %s ", name);
        }

        return 0;
}

int cthd_nl_wrapper::get_ctrl_grp_id(struct rtattr *arg)
{
        struct rtattr *tb[CTRL_ATTR_MCAST_GRP_MAX + 1];
	char *name;

        if (arg == NULL)
                return -1;

        parse_rtattr_nested(tb, CTRL_ATTR_MCAST_GRP_MAX, arg);

        if (!tb[1] || !tb[2])
		return -1;

 	name = (char*)RTA_DATA(tb[CTRL_ATTR_MCAST_GRP_NAME]);

	if (strcmp(name, ACPI_EVENT_MCAST_GROUP_NAME))
        	return -1;

	acpi_event_mcast_group_id =
		*((__u32 *)RTA_DATA(tb[CTRL_ATTR_MCAST_GRP_ID]));

        return 0;
}


int cthd_nl_wrapper::genl_get_mcast_group_id(struct nlmsghdr *n)
{
        struct rtattr *tb[CTRL_ATTR_MAX + 1];
        struct genlmsghdr *ghdr = (struct genlmsghdr *)NLMSG_DATA(n);
        int len = n->nlmsg_len;
        struct rtattr *attrs;
	__u32 ctrl_v = 0x1;

        if (n->nlmsg_type != GENL_ID_CTRL) {
                fprintf(stderr, "Not a controller message, nlmsg_len=%d "
                        "nlmsg_type=0x%x\n", n->nlmsg_len, n->nlmsg_type);
                return 0;
        }

        if (ghdr->cmd != CTRL_CMD_GETFAMILY &&
            ghdr->cmd != CTRL_CMD_DELFAMILY &&
            ghdr->cmd != CTRL_CMD_NEWFAMILY &&
            ghdr->cmd != CTRL_CMD_NEWMCAST_GRP &&
            ghdr->cmd != CTRL_CMD_DELMCAST_GRP) {
                fprintf(stderr, "Unkown controller command %d\n", ghdr->cmd);
                return 0;
        }

        len -= NLMSG_LENGTH(GENL_HDRLEN);

        if (len < 0) {
                fprintf(stderr, "wrong controller message len %d\n", len);
                return -1;
        }

        attrs = (struct rtattr *)((char *)ghdr + GENL_HDRLEN);
        parse_rtattr(tb, CTRL_ATTR_MAX, attrs, len);

	if (tb[CTRL_ATTR_FAMILY_ID])
		acpi_event_family_id =
			*((__u32 *)RTA_DATA(tb[CTRL_ATTR_FAMILY_ID]));

        if (tb[CTRL_ATTR_MCAST_GROUPS]) {
                struct rtattr *tb2[GENL_MAX_FAM_GRPS + 1];
                int i;

                parse_rtattr_nested(tb2, GENL_MAX_FAM_GRPS,
                                        tb[CTRL_ATTR_MCAST_GROUPS]);

                for (i = 0; i < GENL_MAX_FAM_GRPS; i++)
                        if (tb2[i])
                                if (!get_ctrl_grp_id(tb2[i]))
        				return 0;
	}

        return -1;
}

int cthd_nl_wrapper::genl_print_ctrl_message(const struct sockaddr_nl *who,
				   struct nlmsghdr *n, void *arg)
{
	struct rtattr *tb[CTRL_ATTR_MAX + 1];
	struct genlmsghdr *ghdr = (struct genlmsghdr *)NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr *attrs;
	FILE *fp = (FILE *) arg;
	__u32 ctrl_v = 0x1;

	if (n->nlmsg_type != GENL_ID_CTRL) {
		fprintf(stderr, "Not a controller message, nlmsg_len=%d "
			"nlmsg_type=0x%x\n", n->nlmsg_len, n->nlmsg_type);
		return 0;
	}

	if (ghdr->cmd != CTRL_CMD_GETFAMILY &&
	    ghdr->cmd != CTRL_CMD_DELFAMILY &&
	    ghdr->cmd != CTRL_CMD_NEWFAMILY &&
	    ghdr->cmd != CTRL_CMD_NEWMCAST_GRP &&
	    ghdr->cmd != CTRL_CMD_DELMCAST_GRP) {
		fprintf(stderr, "Unkown controller command %d\n", ghdr->cmd);
		return 0;
	}

	len -= NLMSG_LENGTH(GENL_HDRLEN);

	if (len < 0) {
		fprintf(stderr, "wrong controller message len %d\n", len);
		return -1;
	}

	attrs = (struct rtattr *)((char *)ghdr + GENL_HDRLEN);
	parse_rtattr(tb, CTRL_ATTR_MAX, attrs, len);


	if (tb[CTRL_ATTR_FAMILY_NAME]) {
		char *name = (char*)RTA_DATA(tb[CTRL_ATTR_FAMILY_NAME]);
		fprintf(fp, "\nName: %s\n", name);
	}

	if (tb[CTRL_ATTR_FAMILY_ID]) {
		__u16 *id = (__u16*)RTA_DATA(tb[CTRL_ATTR_FAMILY_ID]);
		fprintf(fp, "\tID: 0x%x ", *id);
	}

	if (tb[CTRL_ATTR_VERSION]) {
		__u32 *v = (__u32*)RTA_DATA(tb[CTRL_ATTR_VERSION]);
		fprintf(fp, " Version: 0x%x ", *v);
		ctrl_v = *v;
	}
	if (tb[CTRL_ATTR_HDRSIZE]) {
		__u32 *h = (__u32*)RTA_DATA(tb[CTRL_ATTR_HDRSIZE]);
		fprintf(fp, " header size: %d ", *h);
	}
	if (tb[CTRL_ATTR_MAXATTR]) {
		__u32 *ma = (__u32*)RTA_DATA(tb[CTRL_ATTR_MAXATTR]);
		fprintf(fp, " max attribs: %d ", *ma);
	}
	/* end of family definitions .. */
	fprintf(fp, "\n");
	if (tb[CTRL_ATTR_OPS]) {
		struct rtattr *tb2[GENL_MAX_FAM_OPS];
		int i = 0;
		parse_rtattr_nested(tb2, GENL_MAX_FAM_OPS, tb[CTRL_ATTR_OPS]);
		fprintf(fp, "\tcommands supported: \n");
		for (i = 0; i < GENL_MAX_FAM_OPS; i++) {
			if (tb2[i]) {
				fprintf(fp, "\t\t#%d: ", i);
				if (0 > print_ctrl_cmds(fp, tb2[i], ctrl_v)) {
					fprintf(fp, "Error printing command\n");
				}
				/* for next command */
				fprintf(fp, "\n");
			}
		}

		/* end of family::cmds definitions .. */
		fprintf(fp, "\n");
	}

	if (tb[CTRL_ATTR_MCAST_GROUPS]) {
		struct rtattr *tb2[GENL_MAX_FAM_GRPS + 1];
		int i;

		parse_rtattr_nested(tb2, GENL_MAX_FAM_GRPS,
					tb[CTRL_ATTR_MCAST_GROUPS]);
		fprintf(fp, "\tmulticast groups: \n");
		for (i = 0; i < GENL_MAX_FAM_GRPS; i++) {
			if (tb2[i]) {
				fprintf(fp, "\t#%d\t", i);
				if (0 > print_ctrl_grp(fp, tb2[i], ctrl_v))
					fprintf(fp,
						"Error printing multicast groups\n");
				/* for next multicast group */
				fprintf(fp, "\n");
			}
		}
		/* end of family::mcgrp definitions */
		fprintf(fp, "\n");
	}

	fflush(fp);
	return 0;
}

int cthd_nl_wrapper::genl_print_acpi_event_message(const struct sockaddr_nl *who,
					 struct nlmsghdr *n, void *arg)
{
	struct rtattr *tb[ACPI_GENL_ATTR_MAX + 1];
	struct genlmsghdr *ghdr = (struct genlmsghdr *)NLMSG_DATA(n);
	FILE *fp = (FILE *) arg;
	int len = n->nlmsg_len;
	struct rtattr *attrs;

	if (n->nlmsg_type != acpi_event_family_id) {
		fprintf(stderr, "Not a acpi event message, nlmsg_len=%d "
			"nlmsg_type=0x%x\n", n->nlmsg_len, n->nlmsg_type);
		return 0;
	}

	len -= NLMSG_LENGTH(GENL_HDRLEN);

	if (len < 0) {
		fprintf(stderr, "wrong controller message len %d\n", len);
		return -1;
	}

	attrs = (struct rtattr *)((char *)ghdr + GENL_HDRLEN);
	parse_rtattr(tb, ACPI_GENL_ATTR_MAX, attrs, len);

	if (tb[ACPI_GENL_ATTR_EVENT]) {
		struct acpi_genl_event *event =
		    (struct acpi_genl_event *)RTA_DATA(tb[ACPI_GENL_ATTR_EVENT]);
		fprintf(fp, "\n%20s %15s %08x %08x\n", event->device_class,
			event->bus_id, event->type, event->data);
		fflush(fp);
		return 0;
	}

	return -1;

}

int cthd_nl_wrapper::genl_parse_event_message(const struct sockaddr_nl *who,
		 struct nlmsghdr *n, void *arg)
{
	struct rtattr *tb[ACPI_GENL_ATTR_MAX + 1];
	struct genlmsghdr *ghdr = (struct genlmsghdr *)NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr *attrs;

	if (n->nlmsg_type != acpi_event_family_id) {
		fprintf(stderr, "Not a acpi event message, nlmsg_len=%d "
			"nlmsg_type=0x%x\n", n->nlmsg_len, n->nlmsg_type);
		return 0;
	}

	len -= NLMSG_LENGTH(GENL_HDRLEN);

	if (len < 0) {
		fprintf(stderr, "wrong controller message len %d\n", len);
		return -1;
	}

	attrs = (struct rtattr *)((char *)ghdr + GENL_HDRLEN);
	parse_rtattr(tb, ACPI_GENL_ATTR_MAX, attrs, len);

	if (tb[ACPI_GENL_ATTR_EVENT]) {
		struct acpi_genl_event *event =
		    (struct acpi_genl_event *)RTA_DATA(tb[ACPI_GENL_ATTR_EVENT]);

		if (!strcmp(event->device_class, "thermal_zone")) {
			thermal_zone_notify_t msg;
			char *zone_str;
			msg.zone = 0;
			if (!strncmp(event->bus_id, "LNXTHERM:", strlen("LNXTHERM:")))
			{
				zone_str = event->bus_id + strlen("LNXTHERM:");
				sscanf(zone_str, "%d", &msg.zone);
				thd_log_debug("matched %s\n", zone_str);
			}
			msg.type = event->type;
			msg.data = event->data;
			thd_engine->send_message(THERMAL_ZONE_NOTIFY, sizeof(msg), (unsigned char*)&msg);

			thd_log_debug("%15s[zone %d] %08x %08x\n", event->bus_id,
					msg.zone, event->type, event->data);
		}
		return 0;
	}

	return -1;

}

int cthd_nl_wrapper::genl_get_family_status(char *family_name, int type)
{
	struct rtnl_handle rth;
	struct nlmsghdr *nlh;
	struct genlmsghdr *ghdr;
	int ret = -1;
	struct {
		struct nlmsghdr n;
		char buf[4096];
	} req;

	memset(&req, 0, sizeof(req));

	nlh = &req.n;
	nlh->nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_type = GENL_ID_CTRL;

	ghdr = (struct genlmsghdr *)NLMSG_DATA(&req.n);
	ghdr->cmd = CTRL_CMD_GETFAMILY;

	/* send CTRL_CMD_GET_FAMILY message to controller */
	if (rtnl_open_byproto(&rth, 0, NETLINK_GENERIC) < 0) {
		fprintf(stderr, "Cannot open generic netlink socket\n");
		exit(1);
	}

	addattr_l(nlh, 128, CTRL_ATTR_FAMILY_NAME,
		  family_name, strlen(family_name) + 1);

	if (rtnl_talk(&rth, nlh, 0, 0, nlh, NULL, NULL) < 0) {
		fprintf(stderr, "Error talking to the kernel\n");
		goto ctrl_done;
	}

	if (type) {
		if (genl_get_mcast_group_id(nlh))
			fprintf(stderr,
				"Failed to get acpi_event multicast group\n");
	} else
		if (genl_print_ctrl_message(NULL, nlh, (void *)stdout) < 0)
			fprintf(stderr,
				"Failed to print acpi_event family status\n");


	ret = 0;
      ctrl_done:
	rtnl_close(&rth);
	return ret;
}

int _genl_parse_acpi_event_message(const struct sockaddr_nl *who,
					 struct nlmsghdr *n, void *arg)
{
	cthd_nl_wrapper *obj = (cthd_nl_wrapper *)arg;
	if (obj)
		obj->genl_parse_event_message(who,
					 n, NULL);
}

int cthd_nl_wrapper::genl_acpi_family_open(cthd_engine *engine)
{
	if (genl_get_family_status(ACPI_EVENT_FAMILY_NAME, 1) < 0)
		return -1;

	if (rtnl_open_byproto
	    (&rt_handle, nl_mgrp(acpi_event_mcast_group_id), NETLINK_GENERIC) < 0) {
		fprintf(stderr, "Canot open generic netlink socket\n");
		return -1;
	}
	thd_engine = engine;
	return 0;
}

int cthd_nl_wrapper::genl_message_indication()
{
	if (rtnl_read(&rt_handle, _genl_parse_acpi_event_message, (void *)this) <
	    0)
		return -1;

	return 0;
}
