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

#include <gio/gio.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib-object.h>

struct _PrefObject {
	GObject parent;
};

#define PREF_TYPE_OBJECT (pref_object_get_type())
G_DECLARE_FINAL_TYPE(PrefObject, pref_object, PREF, OBJECT, GObject)

#define MAX_DBUS_REPLY_STR_LEN	100

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

gboolean thd_dbus_interface_get_sensor_temperature(PrefObject *obj, int index,
		unsigned int *temperature, GError **error);

gboolean thd_dbus_interface_get_zone_count(PrefObject *obj, int *status,
		GError **error);

gboolean thd_dbus_interface_get_zone_information(PrefObject *obj, gint index,
		gchar **zone_out, gint *sensor_count, gint *trip_count, gint *bound,
		GError **error);

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
#ifndef GDBUS
#include "thd_dbus_interface.h"
#endif

// DBUS Related functions

// Dbus object initialization
static void pref_object_init(PrefObject *obj) {
	g_assert(obj != NULL);
}

// Dbus object class initialization
static void pref_object_class_init(PrefObjectClass *_class) {
	g_assert(_class != NULL);

#ifndef GDBUS
	dbus_g_object_type_install_info(PREF_TYPE_OBJECT,
			&dbus_glib_thd_dbus_interface_object_info);
#endif
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

gboolean (*thd_dbus_exit_callback)(void);
gboolean thd_dbus_interface_terminate(PrefObject *obj, GError **error) {
	thd_engine->thd_engine_terminate();
	if (thd_dbus_exit_callback)
		thd_dbus_exit_callback();

	return TRUE;
}

gboolean thd_dbus_interface_reinit(PrefObject *obj, GError **error) {
	bool exclusive_control = false;

	if (thd_engine->get_control_mode() == EXCLUSIVE)
		exclusive_control = true;

	std::string config_file = thd_engine->get_config_file();
	const char *conf_file = NULL;
	if (!config_file.empty())
		conf_file = config_file.c_str();

	thd_engine->thd_engine_terminate();
	sleep(1);
	delete thd_engine;
	sleep(2);


	if (thd_engine_create_default_engine(true, exclusive_control,
			conf_file) != THD_SUCCESS) {
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
		gchar **zone_out, gint *sensor_count, gint *trip_count, gint *bound,
		GError **error) {
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
	*bound = (gint) zone->zone_active_status();
	return TRUE;
}

gboolean thd_dbus_interface_get_zone_sensor_at_index(PrefObject *obj,
		gint zone_index, gint sensor_index, gchar **sensor_out,
		GError **error) {
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
	thd_log_debug("thd_dbus_interface_get_zone_sensor_at_index %d\n",
			zone_index);

	cthd_zone *zone = thd_engine->user_get_zone(zone_index);
	if (!zone)
		return FALSE;

	cthd_trip_point *trip = zone->get_trip_at_index(trip_index);
	if (!trip)
		return FALSE;

	*temp = trip->get_trip_temp();
	*trip_type = trip->get_trip_type();
	*sensor_id = trip->get_sensor_id();
	*cdev_size = trip->get_cdev_count();

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

	// Using a device in /etc is a security issue
	if ((strlen(path) >= strlen("/etc")) && !strncmp(path, "/etc",
			strlen("/etc")))
			return FALSE;

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
	g_assert(obj != NULL);

	// Using a device in /etc is a security issue
	if ((strlen(path) >= strlen("/etc")) && !strncmp(path, "/etc",
			strlen("/etc")))
			return FALSE;

	return thd_dbus_interface_add_cooling_device(obj, cdev_name, path,
			min_state, max_state, step, error);

}

gboolean thd_dbus_interface_get_sensor_temperature(PrefObject *obj, int index,
		unsigned int *temperature, GError **error) {
	int ret;

	ret = thd_engine->get_sensor_temperature(index, temperature);

	if (ret == THD_SUCCESS)
		return TRUE;
	else
		return FALSE;
}

#ifdef GDBUS
#pragma GCC diagnostic push

static GDBusInterfaceVTable interface_vtable;
extern gint own_id;

static GDBusNodeInfo *
thd_dbus_load_introspection(const gchar *filename, GError **error)
{
	g_autoptr(GBytes) data = NULL;
	g_autofree gchar *path = NULL;

	path = g_build_filename("/org/freedesktop/thermald", filename, NULL);
	data = g_resources_lookup_data(path, G_RESOURCE_LOOKUP_FLAGS_NONE, error);
	if (data == NULL)
		return NULL;

	return g_dbus_node_info_new_for_xml((gchar *)g_bytes_get_data(data, NULL), error);
}


static void
thd_dbus_handle_method_call(GDBusConnection       *connection,
			    const gchar           *sender,
			    const gchar           *object_path,
			    const gchar           *interface_name,
			    const gchar           *method_name,
			    GVariant              *parameters,
			    GDBusMethodInvocation *invocation,
			    gpointer               user_data)
{
	PrefObject *obj = PREF_OBJECT(user_data);
	g_autoptr(GError) error = NULL;
	
	thd_log_debug("Dbus method called %s %s.\n", interface_name, method_name);

	if (g_strcmp0(method_name, "AddCoolingDevice") == 0) {
		g_autofree gchar *cdev_name = NULL;
		g_autofree gchar *path = NULL;
		gint min_state;
		gint max_state;
		gint step;

		g_variant_get(parameters, "(ssiii)", &cdev_name, &path, &min_state,
			      &max_state, &step);

		thd_dbus_interface_add_cooling_device(obj, cdev_name, path, min_state,
						      max_state, step, &error);

		if (error) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}
		g_dbus_method_invocation_return_value(invocation, NULL);
		return;
	}

	if (g_strcmp0(method_name, "AddSensor") == 0) {
		g_autofree gchar *sensor_name = NULL;
		g_autofree gchar *path = NULL;

		g_variant_get(parameters, "(ss)", &sensor_name, &path);

		thd_dbus_interface_add_sensor(obj, sensor_name, path, &error);

		if (error) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}
		g_dbus_method_invocation_return_value(invocation, NULL);
		return;
	}

	if (g_strcmp0(method_name, "AddTripPoint") == 0) {
		g_autofree gchar *zone_name = NULL;
		guint trip_point_temp;
		g_autofree gchar *trip_point_sensor = NULL;
		g_autofree gchar *trip_point_cdev = NULL;

		g_variant_get(parameters, "(suss)", &zone_name, &trip_point_temp,
			       &trip_point_sensor, &trip_point_cdev);

		thd_dbus_interface_add_trip_point(obj, zone_name, &error);
		
		if(error) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}
		g_dbus_method_invocation_return_value(invocation, NULL);
		return;
	}

	if (g_strcmp0(method_name, "AddVirtualSensor") == 0) {
		g_autofree gchar *sensor_name = NULL;
		g_autofree gchar *dep_sensor = NULL;
		gdouble slope;
		gdouble intercept;

		g_variant_get(parameters, "(ssdd)", &sensor_name, &dep_sensor,
			      &slope, &intercept);

		thd_dbus_interface_add_virtual_sensor(obj, sensor_name, dep_sensor,
						      slope, intercept, &error);
		
		if (error) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}
		g_dbus_method_invocation_return_value(invocation, NULL);
		return;
	}

	if (g_strcmp0(method_name, "AddZonePassive") == 0) {
		g_autofree gchar *zone_name = NULL;
		gint trip_point_temp;
		g_autofree gchar *trip_point_sensor = NULL;
		g_autofree gchar *trip_point_cdev = NULL;

		g_variant_get(parameters, "(siss)", &zone_name, &trip_point_temp,
			      &trip_point_sensor, &trip_point_cdev);
		
		thd_dbus_interface_add_zone_passive(obj, zone_name, trip_point_temp,
						    trip_point_sensor, trip_point_cdev, &error);

		if (error) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}
		g_dbus_method_invocation_return_value(invocation, NULL);
		return;
	}

	if (g_strcmp0(method_name, "DeleteTripPoint") == 0) {
		g_autofree gchar *zone_name = NULL;
		guint trip_point_temp;

		g_variant_get(parameters, "(su)", &zone_name, &trip_point_temp);

		thd_dbus_interface_delete_trip_point(obj, zone_name, &error);
		
		if (error) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}
		g_dbus_method_invocation_return_value(invocation, NULL);
		return;
	}

	if (g_strcmp0(method_name, "DeleteZone") == 0) {
		g_autofree gchar *zone_name = NULL;

		g_variant_get(parameters, "(&s)", &zone_name);

		thd_dbus_interface_delete_zone(obj, zone_name, &error);

		if (error) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}
		g_dbus_method_invocation_return_value(invocation, NULL);
		return;
	}

	if (g_strcmp0(method_name, "DisableCoolingDevice") == 0) {
		g_autofree gchar *cdev_name = NULL;

		g_variant_get(parameters, "(s)", &cdev_name);

		thd_dbus_interface_disable_cooling_device(obj, cdev_name, &error);

		if (error) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}
		g_dbus_method_invocation_return_value(invocation, NULL);
		return;
	}

	if (g_strcmp0(method_name, "GetCdevCount") == 0) {
		gint count;
	
		thd_dbus_interface_get_cdev_count(obj, &count, &error);
		if (error) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}
		
		g_dbus_method_invocation_return_value (invocation,
						      g_variant_new("(u)", count));
		return;
	}

	if (g_strcmp0(method_name, "GetCdevInformation") == 0) {
		gint ret;
		gint index;
		g_autofree gchar *cdev_out = NULL;
		gint min_state;
		gint max_state;
		gint curr_state;

		g_variant_get(parameters, "(u)", &index);

		ret = thd_dbus_interface_get_cdev_information(obj, index, &cdev_out,
							      &min_state, &max_state,
							      &curr_state, &error);

		if (error || !ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("(siii)", cdev_out,
								    min_state, max_state,
								    curr_state));
		return;
	}

	if (g_strcmp0(method_name, "GetCurrentPreference") == 0) {
		gboolean ret;
		g_autofree gchar *cur_pref = NULL;

		ret = thd_dbus_interface_get_current_preference(obj, &cur_pref,
								&error);
		
		if (error || !ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("(s)", cur_pref));
		return;
	}

	if (g_strcmp0(method_name, "GetSensorCount") == 0) {
		gboolean ret;
		gint count;

		ret = thd_dbus_interface_get_sensor_count(obj, &count, &error);

		if (error || !ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("(u)", count));
		return;
	}

	if (g_strcmp0(method_name, "GetSensorInformation") == 0) {
		gboolean ret;
		gint index;
		g_autofree gchar *sensor_out = NULL;
		g_autofree gchar *path = NULL;
		gint temp;

		g_variant_get(parameters, "(u)", &index);

		ret = thd_dbus_interface_get_sensor_information(obj, index, &sensor_out,
								&path, &temp, &error);

		if (error || !ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("(ssi)", sensor_out,
								    path, temp));
		return;
	}

	if (g_strcmp0(method_name, "GetSensorTemperature") == 0) {
		gboolean ret;
		gint index;
		guint temperature;

		g_variant_get(parameters, "(u)", &index);

		ret = thd_dbus_interface_get_sensor_temperature(obj, index, &temperature,
								&error);

		if (error || !ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("(u)", temperature));
		return;
	}

	if (g_strcmp0(method_name, "GetZoneCount") == 0) {
		gboolean ret;
		gint count;

		ret = thd_dbus_interface_get_zone_count(obj, &count, &error);

		if (error || !ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("(u)", count));
		return;
	}

	if (g_strcmp0(method_name, "GetZoneInformation") == 0) {
		gboolean ret;
		gint index;
		g_autofree gchar *zone_out = NULL;
		gint sensor_count;
		gint trip_count;
		gint bound;

		g_variant_get(parameters, "(u)", &index);

		ret =  thd_dbus_interface_get_zone_information(obj, index, &zone_out,
							       &sensor_count, &trip_count,
							       &bound, &error);
		
		if (error || !ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("(siii)", zone_out,
								     sensor_count, trip_count,
								     bound));

		return;
	}

	if (g_strcmp0(method_name, "GetZoneSensorAtIndex") == 0) {
		gboolean ret;
		gint zone_index;
		gint sensor_index;
		g_autofree gchar *sensor_out = NULL;

		g_variant_get(parameters, "(uu)", &zone_index, &sensor_index);

		ret = thd_dbus_interface_get_zone_sensor_at_index(obj, zone_index,
								  sensor_index, &sensor_out,
								  &error);

		if (error || !ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("(s)", sensor_out));
		return;
	}

	if (g_strcmp0(method_name, "GetZoneStatus") == 0) {
		gboolean ret;
		g_autofree gchar *zone_name = NULL;
		gint status;

		g_variant_get(parameters, "(s)", &zone_name);
	
		ret = thd_dbus_interface_get_zone_status(obj, zone_name, &status,
							 &error);

		if (error || !ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		g_dbus_method_invocation_return_value(invocation,
						      g_variant_new("(i)", status));
		return;
	}

	if (g_strcmp0(method_name, "GetZoneTripAtIndex") == 0) {
		gboolean ret;
		gint zone_index;
		gint trip_index;
		gint temp;
		gint trip_type;
		gint sensor_id;
		gint cdev_size;
		g_autoptr(GArray) cdev_ids = NULL;
		g_autoptr(GVariantBuilder) builder = NULL;
		GVariant **tmp;
		GVariant *array = NULL;


		g_variant_get(parameters, "(uu)", &zone_index, &trip_index);

		ret = thd_dbus_interface_get_zone_trip_at_index(obj, zone_index, trip_index,
							        &temp, &trip_type, &sensor_id,
							        &cdev_size, &cdev_ids, &error);

		if (error || !ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		builder = g_variant_builder_new(G_VARIANT_TYPE("(iiiiai)"));
		g_variant_builder_add_value(builder, g_variant_new_int32(temp));
		g_variant_builder_add_value(builder, g_variant_new_int32(trip_type));
		g_variant_builder_add_value(builder, g_variant_new_int32(sensor_id));
		g_variant_builder_add_value(builder, g_variant_new_int32(cdev_size));
		
		tmp = (GVariant **) g_malloc0(cdev_size * sizeof(GVariant *));
		for (int i = 0; i < cdev_size; i++) {
			tmp[i] = g_variant_new_int32(g_array_index(cdev_ids, gint, i));
		}
		array = g_variant_new_array(G_VARIANT_TYPE_INT32, tmp, cdev_size);
		g_variant_builder_add_value(builder, array);

		g_dbus_method_invocation_return_value(invocation,
						      g_variant_builder_end(builder));
		g_free(tmp);

		return;
	}

	if (g_strcmp0(method_name, "Reinit") == 0) {
		thd_dbus_interface_reinit(obj, &error);

		g_dbus_method_invocation_return_value(invocation,
						      NULL);
		return;
	}

	if (g_strcmp0(method_name, "SetCurrentPreference") == 0) {
		gboolean ret;
		g_autofree gchar *pref = NULL;

		g_variant_get(parameters, "(s)", &pref);

		ret = thd_dbus_interface_set_current_preference(obj, pref, &error);

		if (!ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		g_dbus_method_invocation_return_value(invocation,
						      NULL);

		return;
	}

	if (g_strcmp0(method_name, "SetUserMaxTemperature") == 0) {
		gboolean ret;
		g_autofree gchar *zone_name = NULL;
		guint user_set_point_in_milli_degree_celsius;

		g_variant_get(parameters, "(su)", &zone_name,
			      &user_set_point_in_milli_degree_celsius);
		
		ret = thd_dbus_interface_set_user_max_temperature(obj, zone_name,
								  user_set_point_in_milli_degree_celsius,
								  &error);
		
		if (!ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}
		return;
	}

	if (g_strcmp0(method_name, "SetUserPassiveTemperature") == 0) {
		gboolean ret;
		g_autofree gchar *zone_name = NULL;
		guint user_set_point_in_milli_degree_celsius;

		g_variant_get(parameters, "(su)", &zone_name,
			      &user_set_point_in_milli_degree_celsius);

		ret = thd_dbus_interface_set_user_passive_temperature(obj, zone_name,
								      user_set_point_in_milli_degree_celsius,
								      &error);
		if (!ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		g_dbus_method_invocation_return_value(invocation, NULL);
		return;
	}

	if (g_strcmp0(method_name, "SetZoneStatus") == 0) {
		gboolean ret;
		g_autofree gchar *zone_name = NULL;
		gint status;

		g_variant_get(parameters, "(si)", &zone_name, &status);

		ret = thd_dbus_interface_set_zone_status(obj, zone_name, status,
							 &error);

		if (!ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		g_dbus_method_invocation_return_value(invocation, NULL);
		return;
	}

	if (g_strcmp0(method_name, "Terminate") == 0) {
		g_dbus_method_invocation_return_value(invocation, NULL);
		thd_dbus_interface_terminate(obj, &error);
		return;
	}

	if (g_strcmp0(method_name, "UpdateCoolingDevice") == 0) {
		gboolean ret;
		g_autofree gchar *cdev_name = NULL;
		g_autofree gchar *path = NULL;
		gint min_state;
		gint max_state;
		gint step;

		g_variant_get(parameters, "(ssiii)", &cdev_name, &path, &min_state,
			      &max_state, &step);

		ret = thd_dbus_interface_update_cooling_device(obj,
							       cdev_name, path, min_state, max_state,
							       step, &error);
		
		if (!ret) {
			g_dbus_method_invocation_return_gerror(invocation, error);
			return;
		}

		g_dbus_method_invocation_return_value(invocation, NULL);
		return;
	}

	g_set_error(&error,
		    G_DBUS_ERROR,
		    G_DBUS_ERROR_UNKNOWN_METHOD,
		    "no such method %s",
		    method_name);
	g_dbus_method_invocation_return_gerror(invocation, error);
}

static GVariant *
thd_dbus_handle_get_property(GDBusConnection  *connection,
			     const gchar      *sender,
			     const gchar      *object_path,
			     const gchar      *interface_name,
			     const gchar      *property_name,
			     GError          **error,
			     gpointer          user_data)
{
	return NULL;
}

static gboolean
thd_dbus_handle_set_property(GDBusConnection  *connection,
			     const gchar      *sender,
			     const gchar      *object_path,
			     const gchar      *interface_name,
			     const gchar      *property_name,
			     GVariant         *value,
			     GError          **error,
			     gpointer          user_data) {
	return TRUE;
}


static void
thd_dbus_on_bus_acquired(GDBusConnection *connection,
			 const gchar     *name,
			 gpointer         user_data) {
	guint registration_id;
	GDBusProxy *proxy_id = NULL;
	GError *error = NULL;
	GDBusNodeInfo *introspection_data = NULL;

	if (user_data == NULL) {
		thd_log_error("user_data is NULL\n");
		return;
	}

	introspection_data = thd_dbus_load_introspection("src/thd_dbus_interface.xml",
							 &error);
	if (error != NULL) {
		thd_log_error("Couldn't create introspection data: %s:\n",
			      error->message);
		return;
	}

	registration_id = g_dbus_connection_register_object(connection,
							    "/org/freedesktop/thermald",
							    introspection_data->interfaces[0],
							    &interface_vtable,
							    user_data,
							    NULL,
							    &error);

	proxy_id = g_dbus_proxy_new_sync(connection,
					 G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
					 NULL,
					 "org.freedesktop.DBus",
					 "/org/freedesktop/DBus",
					 "org.freedesktop.DBus",
					 NULL,
					 &error);
	g_assert(registration_id > 0);
	g_assert(proxy_id != NULL);
}

static void
thd_dbus_on_name_acquired(GDBusConnection *connection,
			  const gchar     *name,
			  gpointer         user_data) {
}

static void
thd_dbus_on_name_lost(GDBusConnection *connection,
		      const gchar     *name,
		      gpointer         user_data)
{
	g_warning("Lost the name %s\n", name);
	exit(1);
}


// Set up Dbus server with GDBus
int thd_dbus_server_init(gboolean (*exit_handler)(void)) {
	PrefObject *value_obj;

	value_obj = PREF_OBJECT(g_object_new(PREF_TYPE_OBJECT, NULL));
	if (value_obj == NULL) {
		thd_log_error("Failed to create one Value instance:\n");
		return THD_FATAL_ERROR;
	}

	thd_dbus_exit_callback = exit_handler;

	interface_vtable.method_call = thd_dbus_handle_method_call;
	interface_vtable.get_property = thd_dbus_handle_get_property;
	interface_vtable.set_property = thd_dbus_handle_set_property;

	own_id = g_bus_own_name(G_BUS_TYPE_SYSTEM,
				"org.freedesktop.thermald",
				G_BUS_NAME_OWNER_FLAGS_REPLACE,
				thd_dbus_on_bus_acquired,
				thd_dbus_on_name_acquired,
				thd_dbus_on_name_lost,
				g_object_ref(value_obj),
				NULL);
	
	return THD_SUCCESS;
}
#pragma GCC diagnostic pop
#else
// Setup dbus server
int thd_dbus_server_init(gboolean (*exit_handler)(void)) {
	DBusGConnection *bus;
	DBusGProxy *bus_proxy;
	GError *error = NULL;
	guint result;
	PrefObject *value_obj;

	thd_dbus_exit_callback = exit_handler;

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
#endif
