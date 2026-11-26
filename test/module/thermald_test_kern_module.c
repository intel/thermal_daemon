/*
 * Test module to test thermald
 *
 * Copyright (C) 2015, Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/*
 * Build with "make -C test/module" (out of tree) or add
 * obj-m  += thermald_test_kern_module.o in drivers/thermal/Makefile
 *
 * Without parameters this creates the zones thd_test_0..9,
 * gddv_test_target_0/1 and the cooling devices thd_cdev_0..9,
 * gddv_test_source_0/1, as before.
 *
 * Parameters:
 *   zone_name=NAME      create only one zone called NAME (legacy)
 *   zone_names=A,B,...  also create zones with these types, e.g. the
 *                       participant names used in a test GDDV (SEN2, VR1)
 *   cdev_names=A,B,...  also create cooling devices with these types
 *                       (max state 100), e.g. intel_powerclamp, which
 *                       thermald's dynamic zones for virtual sensors need
 *   rapl=1              register fake "intel-rapl" and "intel-rapl-mmio"
 *                       powercap control types, each with a "package-0"
 *                       zone (long_term + short_term constraints)
 *   rapl_max_uw, rapl_pl1_uw, rapl_pl2_uw, rapl_demand_uw, rapl_locked,
 *   rapl_lock_errno     initial fake RAPL state, see MODULE_PARM_DESC
 *
 * Runtime control in /sys/kernel/thermald_test/:
 *   zone_temp       write "<zone type> <millicelsius>", read: all zones
 *   rapl_demand_uw  package power the simulated workload asks for
 *   rapl_locked     1: power limit writes fail with -rapl_lock_errno
 *   rapl_log        read: RAPL writes from the read cursor on, one per
 *                   line, "<seq> <ms> <domain> <attr> <value> <ret>"
 *                   (at most one page per read). Write a number to move
 *                   the cursor to that seq, or "clear" to drop the log.
 *   sensor_temp, control_state: unused, kept for old scripts
 *
 * Package power is min(rapl_demand_uw, lowest enabled PL1 of all fake
 * domains), like hardware where the lower of the MSR and MMIO limit
 * wins, and drives energy_uj of both domains.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/thermal.h>
#include <linux/moduleparam.h>
#include <linux/powercap.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/mutex.h>

static char *zone_name = "none";
module_param(zone_name, charp, 0644);
MODULE_PARM_DESC(zone_name, "Add a test zone name");

#define MAX_NAMED_ZONES	16
static char *zone_names[MAX_NAMED_ZONES];
static int nr_zone_names;
module_param_array(zone_names, charp, &nr_zone_names, 0444);
MODULE_PARM_DESC(zone_names, "Extra thermal zone types to create");

#define MAX_NAMED_CDEVS	8
static char *cdev_names[MAX_NAMED_CDEVS];
static int nr_cdev_names;
module_param_array(cdev_names, charp, &nr_cdev_names, 0444);
MODULE_PARM_DESC(cdev_names, "Extra cooling device types to create");

static bool rapl;
module_param(rapl, bool, 0444);
MODULE_PARM_DESC(rapl, "Register fake intel-rapl and intel-rapl-mmio domains");

static ulong rapl_max_uw = 64000000;
module_param(rapl_max_uw, ulong, 0444);
MODULE_PARM_DESC(rapl_max_uw, "constraint_*_max_power_uw (default 64 W)");

static ulong rapl_pl1_uw = 28000000;
module_param(rapl_pl1_uw, ulong, 0444);
MODULE_PARM_DESC(rapl_pl1_uw, "Power-on PL1 (default 28 W)");

static ulong rapl_pl2_uw = 64000000;
module_param(rapl_pl2_uw, ulong, 0444);
MODULE_PARM_DESC(rapl_pl2_uw, "Power-on PL2 (default 64 W)");

static ulong rapl_demand_uw = 45000000;
module_param(rapl_demand_uw, ulong, 0444);
MODULE_PARM_DESC(rapl_demand_uw, "Initial package power demand (default 45 W)");

static bool rapl_locked;
module_param(rapl_locked, bool, 0444);
MODULE_PARM_DESC(rapl_locked, "Power limit writes fail (BIOS locked)");

static int rapl_lock_errno = ENODATA;
module_param(rapl_lock_errno, int, 0444);
MODULE_PARM_DESC(rapl_lock_errno, "errno returned while locked (default ENODATA)");

#define SENSOR_COUNT	12
#define CDEV_COUNT	12

struct thermald_sensor {
	struct thermal_zone_device *tzone;
	struct thermal_trip *trips;
	int num_trips;
	int temp;
	char name[THERMAL_NAME_LENGTH];
};

struct thermald_cdev {
        struct thermal_cooling_device *cdev;
        unsigned long max_state;
	unsigned long curr_state;
};

static struct thermald_sensor *sensors[SENSOR_COUNT];
static struct thermald_sensor *named_sensors[MAX_NAMED_ZONES];
static struct thermald_cdev *cdevs[CDEV_COUNT];
static struct thermald_cdev *named_cdevs[MAX_NAMED_CDEVS];

static int sys_get_curr_temp(struct thermal_zone_device *tzd,
				int *temp)
{
	struct thermald_sensor *sensor = thermal_zone_device_priv(tzd);

	*temp = READ_ONCE(sensor->temp);

	return 0;
}

static struct thermal_zone_device_ops tzone_ops = {
	.get_temp = sys_get_curr_temp,
};

static int init_trips(struct thermald_sensor *sensor, int num_trips)
{
	struct thermal_trip *trips;
	int i;

	sensor->trips = NULL;
	sensor->num_trips = 0;
	if (!num_trips)
		return 0;

	trips = kcalloc(num_trips, sizeof(*trips), GFP_KERNEL);
	if (!trips)
		return -ENOMEM;

	for (i = 0; i < num_trips; ++i) {
		trips[i].temperature = 40000;
		trips[i].type = THERMAL_TRIP_PASSIVE;
	}

	sensor->trips = trips;
	sensor->num_trips = num_trips;

	return 0;
}

static struct thermald_sensor *create_test_tzone(int id, const char *_name,
						 int num_trips)
{
	struct thermald_sensor *sensor;

	sensor = kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return NULL;

	if (init_trips(sensor, num_trips)) {
		kfree(sensor);
		return NULL;
	}

	sensor->temp = 10000;
	if (!_name)
		snprintf(sensor->name, sizeof(sensor->name), "thd_test_%d", id);
	else
		strscpy(sensor->name, _name, sizeof(sensor->name));

	sensor->tzone = thermal_zone_device_register_with_trips(sensor->name,
			sensor->trips, sensor->num_trips,
			sensor, &tzone_ops,
			NULL, 0, 0);

	if (IS_ERR(sensor->tzone)) {
		kfree(sensor->trips);
		kfree(sensor);
		return NULL;
	}

	return sensor;
}

static void destroy_test_tzone(struct thermald_sensor *sensor)
{
	if (!sensor)
		return;

	thermal_zone_device_unregister(sensor->tzone);
	kfree(sensor->trips);
	kfree(sensor);
}

static int get_max_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	struct thermald_cdev *pcdev = cdev->devdata;

	*state = pcdev->max_state;

	return 0;
}

static int get_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long *state)
{
	struct thermald_cdev *pcdev = cdev->devdata;

	*state = pcdev->curr_state;

	return 0;
}

static int set_cur_state(struct thermal_cooling_device *cdev,
			 unsigned long state)
{
	struct thermald_cdev *pcdev = cdev->devdata;

	pcdev->curr_state = state;

	return 0;
}

static const struct thermal_cooling_device_ops cdev_ops = {
	.get_max_state = get_max_state,
	.get_cur_state = get_cur_state,
	.set_cur_state = set_cur_state,
};

static struct thermald_cdev *create_test_cdev(int id, const char *_name,
					      int max_state)
{
	struct thermald_cdev *cdev;
	char name[THERMAL_NAME_LENGTH];

	cdev = kzalloc(sizeof(*cdev), GFP_KERNEL);
	if (!cdev)
		return NULL;

	if (!_name)
		snprintf(name, sizeof(name), "thd_cdev_%d", id);
	else
		strscpy(name, _name, sizeof(name));
	cdev->max_state = max_state;
	cdev->cdev = thermal_cooling_device_register(name, cdev, &cdev_ops);
	if (IS_ERR(cdev->cdev)) {
		kfree(cdev);
		return NULL;
	}

	return cdev;
}

static void destroy_test_cdev(struct thermald_cdev *cdev)
{
	if (!cdev)
		return;

	thermal_cooling_device_unregister(cdev->cdev);
	kfree(cdev);
}

static struct thermald_sensor *find_sensor(const char *name)
{
	int i;

	for (i = 0; i < SENSOR_COUNT; ++i)
		if (sensors[i] && !strcmp(sensors[i]->name, name))
			return sensors[i];
	for (i = 0; i < MAX_NAMED_ZONES; ++i)
		if (named_sensors[i] && !strcmp(named_sensors[i]->name, name))
			return named_sensors[i];

	return NULL;
}

/* Fake RAPL */

enum { RAPL_MSR, RAPL_MMIO, RAPL_DOMAINS };
#define RAPL_CONSTRAINTS	2	/* 0: long_term (PL1), 1: short_term (PL2) */
#define RAPL_MAX_ENERGY_UJ	262143328850ULL

static const char * const rapl_type_names[RAPL_DOMAINS] = {
	"intel-rapl", "intel-rapl-mmio"
};
static const char * const rapl_constraint_names[RAPL_CONSTRAINTS] = {
	"long_term", "short_term"
};

struct fake_rapl {
	struct powercap_zone zone;	/* static storage, never freed */
	struct powercap_control_type *ctype;
	int index;
	bool registered;
	bool enabled;
	u64 limit_uw[RAPL_CONSTRAINTS];
	u64 window_us[RAPL_CONSTRAINTS];
	u64 energy_uj;
};

static struct fake_rapl rapl_domains[RAPL_DOMAINS];
static DEFINE_MUTEX(rapl_lock);
static ktime_t rapl_last_update;

#define RAPL_LOG_SIZE	1024
struct rapl_log_entry {
	u64 seq;
	u64 ms;
	int domain;
	char attr[24];
	u64 value;
	int ret;
};
static struct rapl_log_entry rapl_log[RAPL_LOG_SIZE];
static u64 rapl_log_seq;	/* entries ever logged since last clear */
static u64 rapl_log_cursor;	/* first seq returned by a read */

static void rapl_log_write(int domain, const char *attr, u64 value, int ret)
{
	struct rapl_log_entry *e = &rapl_log[rapl_log_seq % RAPL_LOG_SIZE];

	e->seq = rapl_log_seq++;
	e->ms = ktime_to_ms(ktime_get());
	e->domain = domain;
	strscpy(e->attr, attr, sizeof(e->attr));
	e->value = value;
	e->ret = ret;
}

/* Package power the hardware would draw now, called with rapl_lock held */
static u64 rapl_package_power_uw(void)
{
	u64 power = READ_ONCE(rapl_demand_uw);
	int i;

	for (i = 0; i < RAPL_DOMAINS; ++i) {
		struct fake_rapl *d = &rapl_domains[i];

		if (d->registered && d->enabled && d->limit_uw[0] < power)
			power = d->limit_uw[0];
	}

	return power;
}

/* Advance all energy counters to now, called with rapl_lock held */
static void rapl_update_energy(void)
{
	ktime_t now = ktime_get();
	u64 dt_us = ktime_to_us(ktime_sub(now, rapl_last_update));
	u64 delta_uj = div_u64(rapl_package_power_uw() * dt_us, 1000000);
	int i;

	for (i = 0; i < RAPL_DOMAINS; ++i) {
		struct fake_rapl *d = &rapl_domains[i];

		d->energy_uj = (d->energy_uj + delta_uj) % RAPL_MAX_ENERGY_UJ;
	}
	rapl_last_update = now;
}

static struct fake_rapl *to_fake(struct powercap_zone *zone)
{
	return container_of(zone, struct fake_rapl, zone);
}

static int rapl_get_max_energy_range_uj(struct powercap_zone *zone, u64 *val)
{
	*val = RAPL_MAX_ENERGY_UJ;
	return 0;
}

static int rapl_get_energy_uj(struct powercap_zone *zone, u64 *val)
{
	mutex_lock(&rapl_lock);
	rapl_update_energy();
	*val = to_fake(zone)->energy_uj;
	mutex_unlock(&rapl_lock);

	return 0;
}

static int rapl_set_enable(struct powercap_zone *zone, bool mode)
{
	struct fake_rapl *d = to_fake(zone);

	mutex_lock(&rapl_lock);
	rapl_update_energy();
	d->enabled = mode;
	rapl_log_write(d->index, "enabled", mode, 0);
	mutex_unlock(&rapl_lock);

	return 0;
}

static int rapl_get_enable(struct powercap_zone *zone, bool *mode)
{
	*mode = READ_ONCE(to_fake(zone)->enabled);
	return 0;
}

static int rapl_zone_release(struct powercap_zone *zone)
{
	/* Zones are static storage */
	return 0;
}

static const struct powercap_zone_ops rapl_zone_ops = {
	.get_max_energy_range_uj = rapl_get_max_energy_range_uj,
	.get_energy_uj = rapl_get_energy_uj,
	.set_enable = rapl_set_enable,
	.get_enable = rapl_get_enable,
	.release = rapl_zone_release,
};

static int rapl_set_power_limit_uw(struct powercap_zone *zone, int cid, u64 val)
{
	struct fake_rapl *d = to_fake(zone);
	char attr[24];
	int ret = 0;

	snprintf(attr, sizeof(attr), "constraint_%d_power", cid);

	mutex_lock(&rapl_lock);
	if (READ_ONCE(rapl_locked)) {
		ret = -rapl_lock_errno;
	} else {
		rapl_update_energy();
		d->limit_uw[cid] = val;
	}
	rapl_log_write(d->index, attr, val, ret);
	mutex_unlock(&rapl_lock);

	return ret;
}

static int rapl_get_power_limit_uw(struct powercap_zone *zone, int cid, u64 *val)
{
	mutex_lock(&rapl_lock);
	*val = to_fake(zone)->limit_uw[cid];
	mutex_unlock(&rapl_lock);

	return 0;
}

static int rapl_set_time_window_us(struct powercap_zone *zone, int cid, u64 val)
{
	struct fake_rapl *d = to_fake(zone);
	char attr[24];

	snprintf(attr, sizeof(attr), "constraint_%d_window", cid);

	mutex_lock(&rapl_lock);
	d->window_us[cid] = val;
	rapl_log_write(d->index, attr, val, 0);
	mutex_unlock(&rapl_lock);

	return 0;
}

static int rapl_get_time_window_us(struct powercap_zone *zone, int cid, u64 *val)
{
	*val = READ_ONCE(to_fake(zone)->window_us[cid]);
	return 0;
}

static int rapl_get_max_power_uw(struct powercap_zone *zone, int cid, u64 *val)
{
	*val = rapl_max_uw;
	return 0;
}

static const char *rapl_get_constraint_name(struct powercap_zone *zone, int cid)
{
	return rapl_constraint_names[cid];
}

static const struct powercap_zone_constraint_ops rapl_constraint_ops = {
	.set_power_limit_uw = rapl_set_power_limit_uw,
	.get_power_limit_uw = rapl_get_power_limit_uw,
	.set_time_window_us = rapl_set_time_window_us,
	.get_time_window_us = rapl_get_time_window_us,
	.get_max_power_uw = rapl_get_max_power_uw,
	.get_name = rapl_get_constraint_name,
};

static void fake_rapl_exit(void)
{
	int i;

	for (i = RAPL_DOMAINS - 1; i >= 0; --i) {
		struct fake_rapl *d = &rapl_domains[i];

		if (d->registered)
			powercap_unregister_zone(d->ctype, &d->zone);
		if (!IS_ERR_OR_NULL(d->ctype))
			powercap_unregister_control_type(d->ctype);
		d->registered = false;
		d->ctype = NULL;
	}
}

static int fake_rapl_init(void)
{
	int i;

	rapl_last_update = ktime_get();

	for (i = 0; i < RAPL_DOMAINS; ++i) {
		struct fake_rapl *d = &rapl_domains[i];
		struct powercap_zone *zone;

		d->index = i;
		d->enabled = true;
		d->limit_uw[0] = rapl_pl1_uw;
		d->limit_uw[1] = rapl_pl2_uw;
		d->window_us[0] = 28000000;
		d->window_us[1] = 2440;

		d->ctype = powercap_register_control_type(NULL,
				rapl_type_names[i], NULL);
		if (IS_ERR(d->ctype)) {
			pr_err("thermald_test: can't register %s: %ld\n",
				rapl_type_names[i], PTR_ERR(d->ctype));
			goto err;
		}

		zone = powercap_register_zone(&d->zone, d->ctype, "package-0",
				NULL, &rapl_zone_ops, RAPL_CONSTRAINTS,
				&rapl_constraint_ops);
		if (IS_ERR(zone)) {
			pr_err("thermald_test: can't register %s zone: %ld\n",
				rapl_type_names[i], PTR_ERR(zone));
			goto err;
		}
		d->registered = true;
	}

	return 0;

err:
	fake_rapl_exit();
	return -ENODEV;
}

/* Control files in /sys/kernel/thermald_test */

static int sensor_temp;
static ssize_t sensor_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf)
{
	return sprintf(buf, "%d\n", sensor_temp);
}

static ssize_t sensor_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	if (kstrtoint(buf, 0, &sensor_temp))
		return -EINVAL;

	return count;
}

static int control_state;
static ssize_t control_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf)
{
	return sprintf(buf, "%d\n", control_state);
}

static ssize_t control_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	if (kstrtoint(buf, 0, &control_state))
		return -EINVAL;

	return count;
}

static ssize_t zone_temp_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	int i, len = 0;

	for (i = 0; i < SENSOR_COUNT; ++i)
		if (sensors[i])
			len += sysfs_emit_at(buf, len, "%s %d\n",
					sensors[i]->name, READ_ONCE(sensors[i]->temp));
	for (i = 0; i < MAX_NAMED_ZONES; ++i)
		if (named_sensors[i])
			len += sysfs_emit_at(buf, len, "%s %d\n",
					named_sensors[i]->name,
					READ_ONCE(named_sensors[i]->temp));

	return len;
}

static ssize_t zone_temp_store(struct kobject *kobj, struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	struct thermald_sensor *sensor;
	char name[THERMAL_NAME_LENGTH];
	int temp;

	if (sscanf(buf, "%19s %d", name, &temp) != 2)
		return -EINVAL;

	sensor = find_sensor(name);
	if (!sensor)
		return -ENOENT;

	WRITE_ONCE(sensor->temp, temp);
	thermal_zone_device_update(sensor->tzone, THERMAL_EVENT_UNSPECIFIED);

	return count;
}

static ssize_t rapl_demand_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return sysfs_emit(buf, "%lu\n", READ_ONCE(rapl_demand_uw));
}

static ssize_t rapl_demand_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	ulong val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	mutex_lock(&rapl_lock);
	rapl_update_energy();
	WRITE_ONCE(rapl_demand_uw, val);
	mutex_unlock(&rapl_lock);

	return count;
}

static ssize_t rapl_locked_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return sysfs_emit(buf, "%d\n", READ_ONCE(rapl_locked));
}

static ssize_t rapl_locked_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	WRITE_ONCE(rapl_locked, val);

	return count;
}

static ssize_t rapl_log_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	u64 first, seq;
	int len = 0;

	mutex_lock(&rapl_lock);
	first = rapl_log_seq > RAPL_LOG_SIZE ? rapl_log_seq - RAPL_LOG_SIZE : 0;
	if (rapl_log_cursor > first)
		first = rapl_log_cursor;
	for (seq = first; seq < rapl_log_seq; ++seq) {
		struct rapl_log_entry *e = &rapl_log[seq % RAPL_LOG_SIZE];
		int n;

		n = snprintf(buf + len, PAGE_SIZE - len, "%llu %llu %s %s %llu %d\n",
			     e->seq, e->ms, rapl_type_names[e->domain], e->attr,
			     e->value, e->ret);
		if (n >= PAGE_SIZE - len)
			break;	/* page full: move the cursor and read again */
		len += n;
	}
	mutex_unlock(&rapl_lock);

	return len;
}

static ssize_t rapl_log_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	u64 cursor;

	mutex_lock(&rapl_lock);
	if (sysfs_streq(buf, "clear")) {
		rapl_log_seq = 0;
		rapl_log_cursor = 0;
	} else if (!kstrtou64(buf, 0, &cursor)) {
		rapl_log_cursor = cursor;
	} else {
		mutex_unlock(&rapl_lock);
		return -EINVAL;
	}
	mutex_unlock(&rapl_lock);

	return count;
}

static struct kobj_attribute sensor_attribute =
		__ATTR(sensor_temp, 0644, sensor_show, sensor_store);
static struct kobj_attribute control_attribute =
		__ATTR(control_state, 0644, control_show, control_store);
static struct kobj_attribute zone_temp_attribute =
		__ATTR(zone_temp, 0644, zone_temp_show, zone_temp_store);
static struct kobj_attribute rapl_demand_attribute =
		__ATTR(rapl_demand_uw, 0644, rapl_demand_show, rapl_demand_store);
static struct kobj_attribute rapl_locked_attribute =
		__ATTR(rapl_locked, 0644, rapl_locked_show, rapl_locked_store);
static struct kobj_attribute rapl_log_attribute =
		__ATTR(rapl_log, 0644, rapl_log_show, rapl_log_store);

static struct attribute *thermald_attrs[] = {
	&sensor_attribute.attr,
	&control_attribute.attr,
	&zone_temp_attribute.attr,
	&rapl_demand_attribute.attr,
	&rapl_locked_attribute.attr,
	&rapl_log_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = thermald_attrs,
};

static struct kobject *thermal_control_kobj;

static struct thermald_sensor *test_sensor;

static void destroy_all(void)
{
	int i;

	for (i = 0; i < SENSOR_COUNT; ++i) {
		destroy_test_tzone(sensors[i]);
		sensors[i] = NULL;
	}
	for (i = 0; i < MAX_NAMED_ZONES; ++i) {
		destroy_test_tzone(named_sensors[i]);
		named_sensors[i] = NULL;
	}
	for (i = 0; i < CDEV_COUNT; ++i) {
		destroy_test_cdev(cdevs[i]);
		cdevs[i] = NULL;
	}
	for (i = 0; i < MAX_NAMED_CDEVS; ++i) {
		destroy_test_cdev(named_cdevs[i]);
		named_cdevs[i] = NULL;
	}
}

static int __init thermald_init(void)
{
	int i;
	int ret;

	if (strcmp(zone_name, "none")) {
		pr_info("create a zone:%s\n", zone_name);
		test_sensor = create_test_tzone(0, zone_name, 2);
		if (test_sensor)
			return 0;

		return -EINVAL;
	}

	thermal_control_kobj = kobject_create_and_add("thermald_test",
						      kernel_kobj);
	if (!thermal_control_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(thermal_control_kobj, &attr_group);
	if (ret) {
		kobject_put(thermal_control_kobj);
		return ret;
	}

	for (i = 0; i < SENSOR_COUNT - 2; ++i) {
		sensors[i] = create_test_tzone(i, NULL, 2);
	}
	for (i = 0; i < CDEV_COUNT - 2; ++i) {
		cdevs[i] = create_test_cdev(i, NULL, 10);
	}

	sensors[SENSOR_COUNT - 1] = create_test_tzone(SENSOR_COUNT - 1, "gddv_test_target_0", 2);
	sensors[SENSOR_COUNT - 2] = create_test_tzone(SENSOR_COUNT - 2, "gddv_test_target_1", 2);
	cdevs[CDEV_COUNT - 1] = create_test_cdev(CDEV_COUNT - 1, "gddv_test_source_0", 100000000);
	cdevs[CDEV_COUNT - 2] = create_test_cdev(CDEV_COUNT - 2, "gddv_test_source_1", 100000000);

	/* Named zones have no trips: the GDDV supplies them, as on ACPI */
	for (i = 0; i < nr_zone_names; ++i) {
		named_sensors[i] = create_test_tzone(SENSOR_COUNT + i,
						     zone_names[i], 0);
		if (!named_sensors[i]) {
			pr_err("thermald_test: can't create zone %s\n",
			       zone_names[i]);
			ret = -EINVAL;
			goto err;
		}
	}

	for (i = 0; i < nr_cdev_names; ++i) {
		named_cdevs[i] = create_test_cdev(CDEV_COUNT + i, cdev_names[i], 100);
		if (!named_cdevs[i]) {
			pr_err("thermald_test: can't create cooling device %s\n",
			       cdev_names[i]);
			ret = -EINVAL;
			goto err;
		}
	}

	if (rapl) {
		ret = fake_rapl_init();
		if (ret)
			goto err;
	}

	return 0;

err:
	destroy_all();
	kobject_put(thermal_control_kobj);
	return ret;
}

static void __exit thermald_exit(void)
{
	if (test_sensor) {
		destroy_test_tzone(test_sensor);
		return;
	}

	fake_rapl_exit();
	destroy_all();
	kobject_put(thermal_control_kobj);
}

module_init(thermald_init)
module_exit(thermald_exit)

MODULE_DESCRIPTION("thermald test platform: thermal zones, cooling devices and fake RAPL");
MODULE_LICENSE("GPL v2");
