#ifndef __ACPI_GENETLINK_H__
#define __ACPI_GENETLINK_H__ 1

#include <linux/types.h>

struct acpi_genl_event {
        char device_class[20];
        char bus_id[15];
        __u32 type;
        __u32 data;
};

/* attributes of acpi_genl_family */
enum {
        ACPI_GENL_ATTR_UNSPEC,
        ACPI_GENL_ATTR_EVENT,   /* ACPI event info needed by user space */
        __ACPI_GENL_ATTR_MAX,
};
#define ACPI_GENL_ATTR_MAX (__ACPI_GENL_ATTR_MAX - 1)

/* commands supported by the acpi_genl_family */
enum {
        ACPI_GENL_CMD_UNSPEC,
        ACPI_GENL_CMD_EVENT,    /* kernel->user notifications for ACPI events */        __ACPI_GENL_CMD_MAX,
};
#define ACPI_GENL_CMD_MAX (__ACPI_GENL_CMD_MAX - 1)
#define GENL_MAX_FAM_OPS        256
#define GENL_MAX_FAM_GRPS       256

#define ACPI_EVENT_FAMILY_NAME		"acpi_event"
#define ACPI_EVENT_MCAST_GROUP_NAME	"acpi_mc_group"

#endif
