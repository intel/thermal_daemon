/*
 * thd_cdev_modem.h: thermal modem cooling interface
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

#ifndef THD_CDEV_MODEM_H_
#define THD_CDEV_MODEM_H_

#include "thd_cdev.h"

#define MODEM_INTERFACE "org.ofono.Modem"
#define THERMAL_MANAGEMENT_INTERFACE "org.ofono.sofia3gr.ThermalManagement"

enum modem_throttling_state {
	MODEM_THROTTLING_DISABLED = 0, MODEM_THROTTLING_ENABLED,
};

class cthd_cdev_modem: public cthd_cdev {
private:
	std::string modem_path;
	bool online;
	bool throttling;

	bool is_online(void);
	void set_online_state(bool);
	int is_throttling(void);
	void set_throttling_state(bool);
	void throttle_modem(int state);

public:
	cthd_cdev_modem(unsigned int _index, std::string control_path);
	int get_curr_state(void);
	void set_curr_state(int state, int arg);
	int get_max_state(void);
	int get_modem_property(DBusConnection *conn, const char *interface,
			const char *property, bool *value);
	int update_online_state(DBusConnection *conn);
	int update_throttling_state(DBusConnection *conn);
	int update(void);
	void update_throttling_state(void);
	int is_interface_up(DBusConnection *conn);

	static DBusHandlerResult
	ofono_signal_handler(DBusConnection *conn, DBusMessage *msg,
			void *user_data);

	static int
	parse_ofono_property_changed_signal(DBusMessage *msg, const char *interface,
			const char *signal, const char *property, dbus_bool_t *value);
};
#endif /* THD_CDEV_MODEM_H_ */
