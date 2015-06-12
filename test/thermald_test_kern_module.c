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
 * To build add
 * obj-m  += thermald_test_kern_module.o in drivers/thermal/Makefile
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>

#define SENSOR_COUNT	10
#define CDEV_COUNT	10

struct thermald_sensor {
	struct thermal_zone_device *tzone;
};

struct thermald_cdev {
        struct thermal_cooling_device *cdev;
        unsigned long max_state;
	unsigned long curr_state;
};

static struct thermald_sensor *sensors[SENSOR_COUNT];
static struct thermald_cdev *cdevs[CDEV_COUNT];

static int sys_get_curr_temp(struct thermal_zone_device *tzd,
				unsigned long *temp)
{
	*temp = 10000;

	return 0;
}

static int sys_get_trip_temp(struct thermal_zone_device *tzd,
                             int trip, unsigned long *temp)
{
	*temp = 40000;

	return 0;
}

static int sys_get_trip_type(struct thermal_zone_device *thermal,
                int trip, enum thermal_trip_type *type)
{
	*type = THERMAL_TRIP_PASSIVE;

	return 0;
}

static struct thermal_zone_device_ops tzone_ops = {
	.get_temp = sys_get_curr_temp,
	.get_trip_temp = sys_get_trip_temp,
	.get_trip_type = sys_get_trip_type,
};

static struct thermald_sensor *create_test_tzone(int id)
{
	struct thermald_sensor *sensor;
	char name[20];

	sensor = kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return NULL;

	snprintf(name, sizeof(name), "thd_test_%d", id);
	sensor->tzone = thermal_zone_device_register(name, 1, 0, sensor,
						     &tzone_ops,
                                                     NULL, 0, 0);
	if (IS_ERR(sensor->tzone)) {
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

static struct thermald_cdev *create_test_cdev(int id)
{
	struct thermald_cdev *cdev;
	char name[20];

	cdev = kzalloc(sizeof(*cdev), GFP_KERNEL);
	if (!cdev)
		return NULL;

	snprintf(name, sizeof(name), "thd_cdev_%d", id);
	cdev->cdev = thermal_cooling_device_register(name, cdev, &cdev_ops);
	if (IS_ERR(cdev->cdev)) {
		kfree(cdev);
		return NULL;
	}
	cdev->max_state = 10;

	return cdev;
}

static void destroy_test_cdev(struct thermald_cdev *cdev)
{
	if (!cdev)
		return;

	thermal_cooling_device_unregister(cdev->cdev);
	kfree(cdev);
}


static int sensor_temp;
static ssize_t sensor_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf)
{
	return sprintf(buf, "%d\n", sensor_temp);
}

static ssize_t sensor_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	sscanf(buf, "%d", &sensor_temp);

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
	sscanf(buf, "%d", &control_state);

	return count;
}

static struct kobj_attribute sensor_attribute =
		__ATTR(sensor_temp, 0644, sensor_show, sensor_store);
static struct kobj_attribute control_attribute =
		__ATTR(control_state, 0644, control_show, control_store);

static struct attribute *thermald_attrs[] = {
	&sensor_attribute.attr,
	&control_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = thermald_attrs,
};

static struct kobject *thermal_control_kobj;

static int __init thermald_init(void)
{
	int i;
	int ret;

	thermal_control_kobj = kobject_create_and_add("thermald_test",
						      kernel_kobj);
	if (!thermal_control_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(thermal_control_kobj, &attr_group);
	if (ret) {
		kobject_put(thermal_control_kobj);
		return ret;
	}

	for (i = 0; i < SENSOR_COUNT; ++i) {
		sensors[i] = create_test_tzone(i);
	}
	for (i = 0; i < CDEV_COUNT; ++i) {
		cdevs[i] = create_test_cdev(i);
	}

	return 0;
}

static void __exit thermald_exit(void)
{
	int i;

	for (i = 0; i < SENSOR_COUNT; ++i) {
		destroy_test_tzone(sensors[i]);
	}
	for (i = 0; i < CDEV_COUNT; ++i) {
		destroy_test_cdev(cdevs[i]);
	}

	kobject_put(thermal_control_kobj);
}

module_init(thermald_init)
module_exit(thermald_exit)

MODULE_LICENSE("GPL v2");
