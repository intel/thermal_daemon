/*
 * thd_dbus_interface.cpp: Thermal Daemon dbus interface
 *
 * Copyright (C) 2014 Intel Corporation. All rights reserved.
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

#include "thermald.h"
#include "thd_preference.h"
#include "thd_engine.h"
#include "thd_engine_default.h"
#include "thd_sensor.h"
#include "thd_zone.h"
#include "thd_trip_point.h"

typedef struct {
	GObject parent;
} PrefObject;

typedef struct {
	GObjectClass parent;
} PrefObjectClass;

GType pref_object_get_type(void);
#define MAX_DBUS_REPLY_STR_LEN	100
#define PREF_TYPE_OBJECT (pref_object_get_type())
G_DEFINE_TYPE(PrefObject, pref_object, G_TYPE_OBJECT)

gboolean thd_dbus_interface_terminate(PrefObject *obj, GError **error);
gboolean thd_dbus_interface_reinit(PrefObject *obj, GError **error);

gboolean thd_dbus_interface_set_current_preference(PrefObject *obj, gchar *pref,
		GError **error);
gboolean thd_dbus_interface_get_current_preference(PrefObject *obj,
		gchar **pref_out, GError **error);
gboolean thd_dbus_interface_set_user_max_temperature(PrefObject *obj,
		gchar *zone_name, unsigned temperature, GError **error);
gboolean thd_dbus_interface_set_user_passive_temperature(PrefObject *obj,
		gchar *zone_name, unsigned int temperature, GError **error);

gboolean thd_dbus_interface_add_sensor(PrefObject *obj, gchar *sensor,
		gchar *path, GError **error);

gboolean thd_dbus_interface_get_sensor_information(PrefObject *obj, gint index,
		gchar **sensor_out, gchar **path, gint *temp, GError **error);

gboolean thd_dbus_interface_add_zone_passive(PrefObject *obj, gchar *zone_name,
		gint trip_temp, gchar *sensor_name, gchar *cdev_name, GError **error);

gboolean thd_dbus_interface_set_zone_status(PrefObject *obj, gchar *zone_name,
		int status, GError **error);

gboolean thd_dbus_interface_get_zone_status(PrefObject *obj, gchar *zone_name,
		int *status, GError **error);

gboolean thd_dbus_interface_delete_zone(PrefObject *obj, gchar *zone_name,
		GError **error);

gboolean thd_dbus_interface_add_virtual_sensor(PrefObject *obj, gchar *name,
		gchar *dep_sensor, double slope, double intercept, GError **error);

gboolean thd_dbus_interface_add_cooling_device(PrefObject *obj,
		gchar *cdev_name, gchar *path, gint min_state, gint max_state,
		gint step, GError **error);

gboolean thd_dbus_interface_update_cooling_device(PrefObject *obj,
		gchar *cdev_name, gchar *path, gint min_state, gint max_state,
		gint step, GError **error);

gboolean thd_dbus_interface_get_sensor_count(PrefObject *obj, int *status,
		GError **error);

gboolean thd_dbus_interface_get_zone_count(PrefObject *obj, int *status,
		GError **error);

gboolean thd_dbus_interface_get_zone_information(PrefObject *obj, gint index,
		gchar **zone_out, gint *sensor_count, gint *trip_count, GError **error);

gboolean thd_dbus_interface_get_zone_sensor_at_index(PrefObject *obj,
		gint zone_index, gint sensor_index, gchar **sensor_out, GError **error);

gboolean thd_dbus_interface_get_zone_trip_at_index(PrefObject *obj,
		gint zone_index, gint trip_index, int *temp, int *trip_type,
		int *sensor_id, int *cdev_size, GArray **cdev_ids, GError **error);

gboolean thd_dbus_interface_get_cdev_count(PrefObject *obj, int *status,
		GError **error);

gboolean thd_dbus_interface_get_cdev_information(PrefObject *obj, gint index,
		gchar **cdev_out, gint *min_state, gint *max_state, gint *curr_state,
		GError **error);

// To be implemented
gboolean thd_dbus_interface_add_trip_point(PrefObject *obj, gchar *name,
		GError **error) {
	return FALSE;
}

gboolean thd_dbus_interface_delete_trip_point(PrefObject *obj, gchar *name,
		GError **error) {
	return FALSE;
}

gboolean thd_dbus_interface_disable_cooling_device(PrefObject *obj, gchar *name,
		GError **error) {
	return FALSE;
}

// This is a generated file, which expects the above prototypes
#include "thd_dbus_interface.h"

// DBUS Related functions

// Dbus object initialization
static void pref_object_init(PrefObject *obj) {
	g_assert(obj != NULL);
}

// Dbus object class initialization
static void pref_object_class_init(PrefObjectClass *_class) {
	g_assert(_class != NULL);

	dbus_g_object_type_install_info(PREF_TYPE_OBJECT,
			&dbus_glib_thd_dbus_interface_object_info);
}

// Callback function called to inform a sent value via dbus
gboolean thd_dbus_interface_set_current_preference(PrefObject *obj, gchar *pref,
		GError **error) {
	int ret;

	thd_log_debug("thd_dbus_interface_set_current_preference %s\n",
			(char*) pref);
	g_assert(obj != NULL);
	cthd_preference thd_pref;
	ret = thd_pref.set_preference((char*) pref);
	thd_engine->send_message(PREF_CHANGED, 0, NULL);

	return ret;
}

// Callback function called to get value via dbus
gboolean thd_dbus_interface_get_current_preference(PrefObject *obj,
		gchar **pref_out, GError **error) {
	thd_log_debug("thd_dbus_interface_get_current_preference\n");
	g_assert(obj != NULL);
	gchar *value_out;
	static char *pref_str;

	pref_str = g_new(char, MAX_DBUS_REPLY_STR_LEN);

	if (!pref_str)
		return FALSE;

	cthd_preference thd_pref;
	value_out = (gchar*) thd_pref.get_preference_cstr();
	if (!value_out) {
		g_free(pref_str);
		return FALSE;
	}
	strncpy(pref_str, value_out, MAX_DBUS_REPLY_STR_LEN);
	free(value_out);
	thd_log_debug("thd_dbus_interface_get_current_preference out :%s\n",
			pref_str);
	*pref_out = pref_str;

	return TRUE;
}

gboolean thd_dbus_interface_terminate(PrefObject *obj, GError **error) {
	thd_engine->thd_engine_terminate();
	return TRUE;
}

gboolean thd_dbus_interface_reinit(PrefObject *obj, GError **error) {
	bool exclusive_control = false;

	thd_engine->thd_engine_terminate();
	sleep(1);
	delete thd_engine;
	sleep(2);
	if (thd_engine->get_control_mode() == EXCLUSIVE)
		exclusive_control = true;
	if (thd_engine_create_default_engine(true, exclusive_control) != THD_SUCCESS) {
		return FALSE;
	}

	return TRUE;
}

gboolean thd_dbus_interface_set_user_max_temperature(PrefObject *obj,
		gchar *zone_name, unsigned int temperature, GError **error) {

	thd_log_debug("thd_dbus_interface_set_user_set_point %s:%d\n", zone_name,
			temperature);

	g_assert(obj != NULL);

	int ret = thd_engine->user_set_max_temp(zone_name, temperature);

	if (ret == THD_SUCCESS)
		thd_engine->send_message(PREF_CHANGED, 0, NULL);
	else
		return FALSE;

	return TRUE;
}

gboolean thd_dbus_interface_set_user_passive_temperature(PrefObject *obj,
		gchar *zone_name, unsigned int temperature, GError **error) {

	thd_log_debug("thd_dbus_interface_set_user_passive_temperature %s:%u\n",
			zone_name, temperature);
	g_assert(obj != NULL);

	int ret = thd_engine->user_set_psv_temp(zone_name, temperature);

	if (ret == THD_SUCCESS)
		thd_engine->send_message(PREF_CHANGED, 0, NULL);
	else
		return FALSE;

	return TRUE;
}

gboolean thd_dbus_interface_add_sensor(PrefObject *obj, gchar *sensor,
		gchar *path, GError **error) {
	int ret;

	g_assert(obj != NULL);

	thd_log_debug("thd_dbus_interface_add_sensor %s:%s\n", (char*) sensor,
			(char *) path);

	ret = thd_engine->user_add_sensor(sensor, path);
	if (ret == THD_SUCCESS)
		return TRUE;
	else
		return FALSE;
}

// Adjust parameters for the following
gboolean thd_dbus_interface_add_virtual_sensor(PrefObject *obj, gchar *name,
		gchar *dep_sensor, double slope, double intercept, GError **error) {

	int ret;

	g_assert(obj != NULL);

	thd_log_debug("thd_dbus_interface_add_sensor %s:%s\n", (char*) name,
			(char *) dep_sensor);

	ret = thd_engine->user_add_virtual_sensor(name, dep_sensor, slope,
			intercept);

	if (ret == THD_SUCCESS)
		return TRUE;
	else
		return FALSE;
}

gboolean thd_dbus_interface_get_sensor_information(PrefObject *obj, gint index,
		gchar **sensor_out, gchar **path, gint *temp, GError **error) {
	char error_str[] = "Invalid Contents";
	char *sensor_str;
	char *path_str;

	thd_log_debug("thd_dbus_interface_get_sensor_information %d\n", index);

	cthd_sensor *sensor = thd_engine->user_get_sensor(index);
	if (!sensor)
		return FALSE;

	sensor_str = g_new(char, MAX_DBUS_REPLY_STR_LEN + 1);
	if (!sensor_str)
		return FALSE;

	path_str = g_new(char, MAX_DBUS_REPLY_STR_LEN + 1);
	if (!path_str) {
		g_free(sensor_str);
		return FALSE;
	}

	strncpy(sensor_str, sensor->get_sensor_type().c_str(),
	MAX_DBUS_REPLY_STR_LEN);
	sensor_str[MAX_DBUS_REPLY_STR_LEN] = '\0';
	strncpy(path_str, sensor->get_sensor_path().c_str(),
	MAX_DBUS_REPLY_STR_LEN);
	path_str[MAX_DBUS_REPLY_STR_LEN] = '\0';
	*temp = (gint) sensor->read_temperature();
	*sensor_out = sensor_str;
	*path = path_str;

	return TRUE;
}

gboolean thd_dbus_interface_get_sensor_count(PrefObject *obj, int *count,
		GError **error) {

	*count = thd_engine->get_sensor_count();

	return TRUE;
}

gboolean thd_dbus_interface_get_zone_count(PrefObject *obj, int *count,
		GError **error) {

	*count = thd_engine->get_zone_count();

	return TRUE;
}

gboolean thd_dbus_interface_get_zone_information(PrefObject *obj, gint index,
		gchar **zone_out, gint *sensor_count, gint *trip_count,
		GError **error) {
	char error_str[] = "Invalid Contents";
	char *zone_str;

	thd_log_debug("thd_dbus_interface_get_zone_information %d\n", index);

	cthd_zone *zone = thd_engine->user_get_zone(index);
	if (!zone)
		return FALSE;

	zone_str = g_new(char, MAX_DBUS_REPLY_STR_LEN + 1);
	if (!zone_str)
		return FALSE;

	strncpy(zone_str, zone->get_zone_type().c_str(),
	MAX_DBUS_REPLY_STR_LEN);
	zone_str[MAX_DBUS_REPLY_STR_LEN] = '\0';
	*zone_out = zone_str;
	*sensor_count = zone->get_sensor_count();
	*trip_count = zone->get_trip_count();

	return TRUE;
}

gboolean thd_dbus_interface_get_zone_sensor_at_index(PrefObject *obj,
		gint zone_index, gint sensor_index, gchar **sensor_out,
		GError **error) {
	char error_str[] = "Invalid Contents";
	char *sensor_str;

	thd_log_debug("thd_dbus_interface_get_zone_sensor_at_index %d\n",
			zone_index);

	cthd_zone *zone = thd_engine->user_get_zone(zone_index);
	if (!zone)
		return FALSE;

	cthd_sensor *sensor = zone->get_sensor_at_index(sensor_index);
	if (!sensor)
		return FALSE;

	sensor_str = g_new(char, MAX_DBUS_REPLY_STR_LEN + 1);
	if (!sensor_str)
		return FALSE;

	strncpy(sensor_str, sensor->get_sensor_type().c_str(),
	MAX_DBUS_REPLY_STR_LEN);
	sensor_str[MAX_DBUS_REPLY_STR_LEN] = '\0';
	*sensor_out = sensor_str;

	return TRUE;
}

gboolean thd_dbus_interface_get_zone_trip_at_index(PrefObject *obj,
		gint zone_index, gint trip_index, int *temp, int *trip_type,
		int *sensor_id, int *cdev_size, GArray **cdev_ids, GError **error) {
	char error_str[] = "Invalid Contents";

	thd_log_debug("thd_dbus_interface_get_zone_sensor_at_index %d\n",
			zone_index);

	cthd_zone *zone = thd_engine->user_get_zone(zone_index);
	if (!zone)
		return FALSE;

	cthd_trip_point *trip = zone->get_trip_at_index(trip_index);

	*temp = trip->get_trip_temp();
	*trip_type = trip->get_trip_type();
	*sensor_id = trip->get_sensor_id();
	*cdev_size = trip->get_cdev_count();
	if (*cdev_size <= 0)
		return TRUE;

	GArray *garray;

	garray = g_array_new(FALSE, FALSE, sizeof(gint));
	for (int i = 0; i < *cdev_size; i++) {
		trip_pt_cdev_t cdev_trip;
		int index;
		cdev_trip = trip->get_cdev_at_index(i);
		index = cdev_trip.cdev->thd_cdev_get_index();
		g_array_prepend_val(garray, index);
	}

	*cdev_ids = garray;

	return TRUE;
}

gboolean thd_dbus_interface_get_cdev_count(PrefObject *obj, int *count,
		GError **error) {

	*count = thd_engine->get_cdev_count();

	return TRUE;
}

gboolean thd_dbus_interface_get_cdev_information(PrefObject *obj, gint index,
		gchar **cdev_out, gint *min_state, gint *max_state, gint *curr_state,
		GError **error) {
	char error_str[] = "Invalid Contents";
	char *cdev_str;

	thd_log_debug("thd_dbus_interface_get_cdev_information %d\n", index);

	cthd_cdev *cdev = thd_engine->user_get_cdev(index);
	if (!cdev)
		return FALSE;

	cdev_str = g_new(char, MAX_DBUS_REPLY_STR_LEN + 1);
	if (!cdev_str)
		return FALSE;

	strncpy(cdev_str, cdev->get_cdev_type().c_str(),
	MAX_DBUS_REPLY_STR_LEN);
	cdev_str[MAX_DBUS_REPLY_STR_LEN] = '\0';
	*cdev_out = cdev_str;
	*min_state = cdev->get_min_state();
	*max_state = cdev->get_max_state();
	*curr_state = cdev->get_curr_state();

	return TRUE;
}

gboolean thd_dbus_interface_add_zone_passive(PrefObject *obj, gchar *zone_name,
		gint trip_temp, gchar *sensor_name, gchar *cdev_name, GError **error) {
	int ret;

	g_assert(obj != NULL);

	thd_log_debug("thd_dbus_interface_add_zone_passive %s\n",
			(char*) zone_name);

	ret = thd_engine->user_add_zone(zone_name, trip_temp, sensor_name,
			cdev_name);
	if (ret == THD_SUCCESS)
		return TRUE;
	else
		return FALSE;
}

gboolean thd_dbus_interface_set_zone_status(PrefObject *obj, gchar *zone_name,
		int status, GError **error) {
	int ret;

	g_assert(obj != NULL);

	thd_log_debug("thd_dbus_interface_set_zone_status %s\n", (char*) zone_name);

	ret = thd_engine->user_set_zone_status(zone_name, status);
	if (ret == THD_SUCCESS)
		return TRUE;
	else
		return FALSE;
}

gboolean thd_dbus_interface_get_zone_status(PrefObject *obj, gchar *zone_name,
		int *status, GError **error) {
	int ret;

	g_assert(obj != NULL);

	thd_log_debug("thd_dbus_interface_set_zone_status %s\n", (char*) zone_name);

	ret = thd_engine->user_get_zone_status(zone_name, status);
	if (ret == THD_SUCCESS)
		return TRUE;
	else
		return FALSE;
}

gboolean thd_dbus_interface_delete_zone(PrefObject *obj, gchar *zone_name,
		GError **error) {
	int ret;

	g_assert(obj != NULL);

	thd_log_debug("thd_dbus_interface_delete_zone %s\n", (char*) zone_name);

	ret = thd_engine->user_delete_zone(zone_name);
	if (ret == THD_SUCCESS)
		return TRUE;
	else
		return FALSE;
}

gboolean thd_dbus_interface_add_cooling_device(PrefObject *obj,
		gchar *cdev_name, gchar *path, gint min_state, gint max_state,
		gint step, GError **error) {
	int ret;

	g_assert(obj != NULL);

	thd_log_debug("thd_dbus_interface_add_cooling_device %s\n",
			(char*) cdev_name);
	ret = thd_engine->user_add_cdev(cdev_name, path, min_state, max_state,
			step);
	if (ret == THD_SUCCESS)
		return TRUE;
	else
		return FALSE;
}

gboolean thd_dbus_interface_update_cooling_device(PrefObject *obj,
		gchar *cdev_name, gchar *path, gint min_state, gint max_state,
		gint step, GError **error) {
	int ret;

	g_assert(obj != NULL);

	return thd_dbus_interface_add_cooling_device(obj, cdev_name, path,
			min_state, max_state, step, error);

}

// Setup dbus server
int thd_dbus_server_init() {
	DBusGConnection *bus;
	DBusGProxy *bus_proxy;
	GError *error = NULL;
	guint result;
	PrefObject *value_obj;

	bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
	if (error != NULL) {
		thd_log_error("Couldn't connect to session bus: %s:\n", error->message);
		return THD_FATAL_ERROR;
	}

	// Get a bus proxy instance
	bus_proxy = dbus_g_proxy_new_for_name(bus, DBUS_SERVICE_DBUS,
			DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);
	if (bus_proxy == NULL) {
		thd_log_error("Failed to get a proxy for D-Bus:\n");
		return THD_FATAL_ERROR;
	}

	thd_log_debug("Registering the well-known name (%s)\n",
	THD_SERVICE_NAME);
	// register the well-known name
	if (!dbus_g_proxy_call(bus_proxy, "RequestName", &error, G_TYPE_STRING,
	THD_SERVICE_NAME, G_TYPE_UINT, 0, G_TYPE_INVALID, G_TYPE_UINT, &result,
			G_TYPE_INVALID)) {
		thd_log_error("D-Bus.RequestName RPC failed: %s\n", error->message);
		return THD_FATAL_ERROR;
	}
	thd_log_debug("RequestName returned %d.\n", result);
	if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		thd_log_error("Failed to get the primary well-known name:\n");
		return THD_FATAL_ERROR;
	}
	value_obj = (PrefObject*) g_object_new(PREF_TYPE_OBJECT, NULL);
	if (value_obj == NULL) {
		thd_log_error("Failed to create one Value instance:\n");
		return THD_FATAL_ERROR;
	}

	thd_log_debug("Registering it on the D-Bus.\n");
	dbus_g_connection_register_g_object(bus, THD_SERVICE_OBJECT_PATH,
			G_OBJECT(value_obj));

	return THD_SUCCESS;
}
