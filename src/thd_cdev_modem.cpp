/*
 * thd_cdev_modem.cpp: thermal modem cooling implementation
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License
 * version 2 or later as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 *
 * Author Name <Rajagopalx.Aravindan@intel.com>
 *
 */

/*
 * Modem Throttling Levels:
 * 0 - Disabled or No Throttling
 * 1 - Enabled
 */

#include <stdint.h>
#include <dbus/dbus.h>
#include <string.h>

#include "thd_cdev_modem.h"

int cthd_cdev_modem::parse_ofono_property_changed_signal(DBusMessage* msg,
		const char* interface, const char* signal, const char* property,
		dbus_bool_t* new_value) {
	DBusMessageIter iter;
	DBusMessageIter var;

	char *property_name;

	dbus_message_iter_init(msg, &iter);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) {
		thd_log_error("Invalid arguments in %s[%s]", interface, signal);
		return THD_ERROR;
	}

	dbus_message_iter_get_basic(&iter, &property_name);

	if (strlen(property) != strlen(property_name)
			|| strcmp(property, property_name)) {
		thd_log_error("Unsupported property : %s", property_name);
		return THD_ERROR;
	}

	dbus_message_iter_next(&iter);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
		thd_log_error("Invalid arguments in %s[%s(%s)]", interface, signal,
				property);
		return THD_ERROR;
	}

	dbus_message_iter_recurse(&iter, &var);

	if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_BOOLEAN) {
		thd_log_error("Invalid arguments in %s[%s(%s)]", interface, signal,
				property);
		return THD_ERROR;
	}

	dbus_message_iter_get_basic(&var, new_value);

	return THD_SUCCESS;
}

DBusHandlerResult cthd_cdev_modem::ofono_signal_handler(DBusConnection *conn,
		DBusMessage *msg, void *user_data) {
	DBusError error;
	cthd_cdev_modem *cdev_modem = (cthd_cdev_modem *) user_data;

	const char *signal = "PropertyChanged";

	dbus_error_init(&error);

	if (dbus_message_is_signal(msg, THERMAL_MANAGEMENT_INTERFACE, signal)) {
		dbus_bool_t throttling;

		if (parse_ofono_property_changed_signal(msg,
				THERMAL_MANAGEMENT_INTERFACE, signal, "TransmitPowerThrottling",
				&throttling) != THD_SUCCESS)
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

		if (cdev_modem) {
			cdev_modem->set_throttling_state(throttling ? true : false);

			cdev_modem->update_online_state(conn);
			cdev_modem->update_throttling_state(conn);
			thd_log_debug("TransmitPowerThrottling Initiated");
		}

		return DBUS_HANDLER_RESULT_HANDLED;
	} else if (dbus_message_is_signal(msg, MODEM_INTERFACE, signal)) {
		dbus_bool_t online;

		if (parse_ofono_property_changed_signal(msg, MODEM_INTERFACE, signal,
				"Online", &online) != THD_SUCCESS)
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

		if (cdev_modem) {
			cdev_modem->set_online_state(online ? true : false);
			cdev_modem->update_online_state(conn);
			cdev_modem->update_throttling_state(conn);
			thd_log_debug("Modem online Initiated");
		}

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

cthd_cdev_modem::cthd_cdev_modem(unsigned int _index, std::string control_path) :
		cthd_cdev(_index, "") {
	throttling = false;
	online = false;

	if (control_path.length() > 0)
		modem_path.assign(control_path);
	else
		modem_path.assign("/ril_0");
}

int cthd_cdev_modem::get_modem_property(DBusConnection* conn,
		const char* interface, const char* property, bool* value) {
	DBusError error;
	DBusMessage *msg;
	DBusMessage *reply;
	DBusMessageIter array;
	DBusMessageIter dict;
	int rc = THD_ERROR;

	dbus_error_init(&error);

	msg = dbus_message_new_method_call("org.ofono", modem_path.c_str(),
			interface, "GetProperties");
	if (msg == NULL) {
		thd_log_error("Error creating D-Bus message for GetProperties "
				"under %s : %s\n", modem_path.c_str(), error.message);
		return rc;
	}

	reply = dbus_connection_send_with_reply_and_block(conn, msg, 10000, &error);
	if (dbus_error_is_set(&error)) {
		thd_log_error("Error invoking GetProperties under %s : %s\n",
				modem_path.c_str(), error.message);
		dbus_error_free(&error);
		dbus_message_unref(msg);
		return rc;
	}

	dbus_message_unref(msg);

	dbus_message_iter_init(reply, &array);
	if (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_ARRAY) {
		thd_log_error("GetProperties return type not array under %s!\n",
				modem_path.c_str());
		dbus_message_unref(reply);
		return rc;
	}

	dbus_message_iter_recurse(&array, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		DBusMessageIter key, var;

		char *property_name;

		dbus_message_iter_recurse(&dict, &key);

		if (dbus_message_iter_get_arg_type(&key) != DBUS_TYPE_STRING) {
			thd_log_error("GetProperties dict key type not string "
					"under %s!\n", modem_path.c_str());
			dbus_message_unref(reply);
			return rc;
		}

		dbus_message_iter_get_basic(&key, &property_name);

		/* Check if, this is the property we are interested in */
		if (strlen(property) != strlen(property_name)
				|| strcmp(property, property_name)) {
			dbus_message_iter_next(&dict);
			continue;
		}

		dbus_message_iter_next(&key);

		if (dbus_message_iter_get_arg_type(&key) != DBUS_TYPE_VARIANT) {
			thd_log_error("GetProperties dict value type not "
					"variant under %s!\n", modem_path.c_str());
			dbus_message_unref(reply);
			return rc;
		}

		dbus_message_iter_recurse(&key, &var);

		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_BOOLEAN) {
			thd_log_error("GetProperties dict value(1) type not "
					"boolean under %s!\n", modem_path.c_str());
			dbus_message_unref(reply);
			return rc;
		}

		dbus_message_iter_get_basic(&var, value);
		rc = THD_SUCCESS;
		break;
	}

	dbus_message_unref(reply);

	return rc;
}

int cthd_cdev_modem::update_online_state(DBusConnection* conn) {
	bool online_state;

	if (get_modem_property(conn, MODEM_INTERFACE, "Online",
			&online_state) == THD_SUCCESS) {
		online = online_state;
		return THD_SUCCESS;
	}

	return THD_ERROR;
}

int cthd_cdev_modem::update_throttling_state(DBusConnection *conn) {
	bool enabled;

	if (get_modem_property(conn, THERMAL_MANAGEMENT_INTERFACE,
			"TransmitPowerThrottling", &enabled) == THD_SUCCESS) {
		throttling = enabled;
		return THD_SUCCESS;
	}

	return THD_ERROR;
}

int cthd_cdev_modem::update() {
	DBusConnection *conn;
	DBusError error;
	std::string thermal_management_dbus_rule, modem_dbus_rule;

	/* Modem has only 2 throttling states, enabled or disabled */
	min_state = MODEM_THROTTLING_DISABLED;
	max_state = MODEM_THROTTLING_ENABLED;

	dbus_error_init(&error);

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	if (dbus_error_is_set(&error)) {
		thd_log_error("Error connecting to system bus: %s:\n", error.message);
		dbus_error_free(&error);
		return THD_ERROR;
	}

	dbus_connection_setup_with_g_main(conn, NULL);

	/*
	 * Add a match rule as below ...
	 *	Type :	Signal
	 *	From-Interface : org.ofono.Modem
	 *	Signal Name : PropertyChanged
	 *	Property Name : Online
	 */
	modem_dbus_rule.append("type='signal'");
	modem_dbus_rule.append(",path='").append(modem_path).append("'");
	modem_dbus_rule.append(",interface='" MODEM_INTERFACE "'");
	modem_dbus_rule.append(",member='PropertyChanged'");
	modem_dbus_rule.append(",arg0='Online'");

	dbus_bus_add_match(conn, modem_dbus_rule.c_str(), &error);
	if (dbus_error_is_set(&error)) {
		thd_log_error("Error adding D-Bus rule \"%s\" : %s",
				modem_dbus_rule.c_str(), error.message);
		dbus_error_free(&error);
		return THD_ERROR;
	}

	/*
	 * Add a match rule as below ...
	 *	Type :	Signal
	 *	From-Interface : org.ofono.sofia3gr.ThermalManagement
	 *	Signal Name : PropertyChanged
	 *	Property Name : TransmitPowerThrottling
	 */
	thermal_management_dbus_rule.append("type='signal'");
	thermal_management_dbus_rule.append(",path='").append(modem_path).append(
			"'");
	thermal_management_dbus_rule.append(",interface='").append(
			THERMAL_MANAGEMENT_INTERFACE).append("'");
	thermal_management_dbus_rule.append(",member='PropertyChanged'");
	thermal_management_dbus_rule.append(",arg0='TransmitPowerThrottling'");

	dbus_bus_add_match(conn, thermal_management_dbus_rule.c_str(), &error);
	if (dbus_error_is_set(&error)) {
		thd_log_error("Error adding D-Bus rule \"%s\" : %s",
				thermal_management_dbus_rule.c_str(), error.message);
		dbus_error_free(&error);
		return THD_ERROR;
	}

	/* Register a handler for the above added rules */
	dbus_connection_add_filter(conn, ofono_signal_handler, this, NULL);

	return THD_SUCCESS;
}

int cthd_cdev_modem::get_curr_state() {

	if (!online) {
		update_throttling_state();
	}

	if (throttling) {
		curr_state = MODEM_THROTTLING_ENABLED;
		thd_log_debug("Modem currently under throttling\n");
	} else {
		curr_state = MODEM_THROTTLING_DISABLED;
		thd_log_debug("Modem currently not under throttling\n");
	}

	return curr_state;
}

void cthd_cdev_modem::update_throttling_state() {

	DBusConnection *conn;
	DBusError error;

	dbus_error_init(&error);
	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);

	if (dbus_error_is_set(&error)) {
		thd_log_error("Error: %s", error.message);
		dbus_error_free(&error);
		return;
	}

	dbus_connection_setup_with_g_main(conn, NULL);

	if (is_interface_up(conn) == THD_ERROR) {
		thd_log_warn("Thermal Interface not ready\n");
		return;
	}

	update_online_state(conn);
	update_throttling_state(conn);

}

int cthd_cdev_modem::is_interface_up(DBusConnection *conn) {

	DBusError error;
	DBusMessage *msg;
	DBusMessage *reply;
	int rc = THD_ERROR;

	dbus_error_init(&error);

	msg = dbus_message_new_method_call("org.ofono", modem_path.c_str(),
	THERMAL_MANAGEMENT_INTERFACE, "GetProperties");

	if (msg == NULL) {
		thd_log_error("Error creating D-Bus message for GetProperties "
				"under %s : %s\n", modem_path.c_str(), error.message);
		return rc;
	}

	reply = dbus_connection_send_with_reply_and_block(conn, msg, 10000, &error);

	if (dbus_error_is_set(&error)) {
		dbus_error_free(&error);
		dbus_message_unref(msg);
		return rc;
	}

	dbus_message_unref(msg);
	dbus_message_unref(reply);

	return THD_SUCCESS;
}

void cthd_cdev_modem::set_curr_state(int state, int arg) {

	DBusConnection *conn;
	DBusError error;

	dbus_error_init(&error);
	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);

	if (dbus_error_is_set(&error)) {
		thd_log_error("Error : %s", error.message);
		dbus_error_free(&error);
		return;
	}

	dbus_connection_setup_with_g_main(conn, NULL);

	update_online_state(conn);

	switch (state) {
	case MODEM_THROTTLING_ENABLED:
		if (!online)
			thd_log_debug("Modem not yet online, hence "
					"ignoring throttle request\n");
		else if (throttling)
			thd_log_debug("Modem already throttled, hence "
					"ignoring throttle request\n");
		else {
			thd_log_debug("Initiating modem throttling\n");
			throttle_modem(state);
			update_throttling_state(conn);
		}
		break;

	case MODEM_THROTTLING_DISABLED:
		if (!online)
			thd_log_debug("Modem not yet online, hence "
					"ignoring de-throttle request\n");
		else if (!throttling)
			thd_log_debug("Modem already de-throttled, hence "
					"ignoring de-throttle request\n");
		else {
			thd_log_debug("Initiating modem de-throttling\n");
			throttle_modem(state);
			update_throttling_state(conn);
		}
		break;
	}
}

int cthd_cdev_modem::get_max_state() {
	return max_state;
}

int cthd_cdev_modem::is_throttling() {
	return throttling;
}

void cthd_cdev_modem::set_throttling_state(bool enabled) {
	throttling = enabled;
}

bool cthd_cdev_modem::is_online() {
	return online;
}

void cthd_cdev_modem::set_online_state(bool on) {
	online = on;
}

void cthd_cdev_modem::throttle_modem(int state) {
	DBusConnection *conn;
	DBusError error;

	DBusMessage *msg;
	DBusMessageIter iter;
	DBusMessageIter var;

	const char *property = "TransmitPowerThrottling";

	dbus_bool_t enable;

	char var_sig[] = { DBUS_TYPE_BOOLEAN, 0 };

	dbus_error_init(&error);

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	if (dbus_error_is_set(&error)) {
		thd_log_error("Couldn't connect to system bus: %s:\n", error.message);
		dbus_error_free(&error);
		return;
	}

	msg = dbus_message_new_method_call("org.ofono", modem_path.c_str(),
	THERMAL_MANAGEMENT_INTERFACE, "SetProperty");
	if (msg == NULL) {
		thd_log_error("Couldn't create D-Bus message for SetProperty "
				"under %s : %s\n", modem_path.c_str(), error.message);
		return;
	}

	enable = (state == MODEM_THROTTLING_ENABLED) ? true : false;

	dbus_message_iter_init_append(msg, &iter);

	if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &property)) {
		thd_log_error("Error populating modem %s arguments: %s\n",
				enable ? "throttle" : "de-throttle", error.message);
		return;
	}

	dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, var_sig, &var);

	if (!dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &enable)) {
		thd_log_error("Error populating modem %s arguments: %s\n",
				enable ? "throttle" : "de-throttle", error.message);
		return;
	}

	dbus_message_iter_close_container(&iter, &var);

	// send message
	if (!dbus_connection_send(conn, msg, NULL)) {
		thd_log_error("Error sending modem throttle message to %s !\n",
				modem_path.c_str());
		return;
	}

	dbus_connection_flush(conn);
	dbus_message_unref(msg);
}
