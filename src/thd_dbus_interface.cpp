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

typedef struct {
	GObject parent;
} PrefObject;

typedef struct {
	GObjectClass parent;
} PrefObjectClass;

GType pref_object_get_type(void);
#define MAX_DBUS_REPLY_STR_LEN	20
#define PREF_TYPE_OBJECT (pref_object_get_type())
G_DEFINE_TYPE(PrefObject, pref_object, G_TYPE_OBJECT)

gboolean thd_dbus_interface_calibrate(PrefObject *obj, GError **error);
gboolean thd_dbus_interface_terminate(PrefObject *obj, GError **error);
gboolean thd_dbus_interface_set_current_preference(PrefObject *obj, gchar *pref,
		GError **error);
gboolean thd_dbus_interface_get_current_preference(PrefObject *obj,
		gchar **pref_out, GError **error);
gboolean thd_dbus_interface_set_user_max_temperature(PrefObject *obj,
		gchar *zone_name, gchar *temperature, GError **error);
gboolean thd_dbus_interface_set_user_passive_temperature(PrefObject *obj,
		gchar *zone_name, gchar *temperature, GError **error);
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

gboolean thd_dbus_interface_calibrate(PrefObject *obj, GError **error) {
//	thd_engine->thd_engine_calibrate();
	return TRUE;
}

gboolean thd_dbus_interface_terminate(PrefObject *obj, GError **error) {
	thd_engine->thd_engine_terminate();
	return TRUE;
}

gboolean thd_dbus_interface_set_user_max_temperature(PrefObject *obj,
		gchar *zone_name, gchar *temperature, GError **error) {
	thd_log_debug("thd_dbus_interface_set_user_set_point %s:%s\n", zone_name,
			temperature);
	g_assert(obj != NULL);
	cthd_preference thd_pref;
	if (thd_engine->thd_engine_set_user_max_temp(zone_name,
			(char*) temperature) == THD_SUCCESS)
		thd_engine->send_message(PREF_CHANGED, 0, NULL);

	return TRUE;
}

gboolean thd_dbus_interface_set_user_passive_temperature(PrefObject *obj,
		gchar *zone_name, gchar *temperature, GError **error) {
	thd_log_debug("thd_dbus_interface_set_user_passive_temperature %s:%s\n",
			zone_name, temperature);
	g_assert(obj != NULL);
	cthd_preference thd_pref;
	if (thd_engine->thd_engine_set_user_psv_temp(zone_name,
			(char*) temperature) == THD_SUCCESS)
		thd_engine->send_message(PREF_CHANGED, 0, NULL);

	return TRUE;
}

static GMainLoop *g_main_loop;

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
