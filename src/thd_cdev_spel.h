/*
 * cthd_cdev_spel.h: thermal cooling class interface using
 * Qualcomm SPEL
 * Copyright (c) 2026 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * Author Name <priyansh.jain@oss.qualcomm.com>
 *
 */

#ifndef THD_CDEV_SPEL_H_
#define THD_CDEV_SPEL_H_

#include "thd_cdev.h"
#include "thd_sys_fs.h"

class cthd_sysfs_cdev_spel: public cthd_cdev {
protected:
	int domain_id;
	int constraint_index;
	int ppcc_max_pwr;
	int ppcc_min_pwr;
	int ppcc_min_window;
	int ppcc_max_window;
	int ppcc_step_pwr;
	bool constrained;
	int power_on_constraint_pwr;
	int power_on_constraint_time_window;
	int power_on_enable_status;
	std::string device_name;
	std::string domain_type; // "soc" or "sys"
	virtual bool read_ppcc_power_limits();

private:
	int spel_sysfs_valid();
	int spel_read_power_limit();
	int spel_read_power_limit_max();
	int spel_update_power_limit(int power_limit);
	int spel_read_time_window();
	int spel_update_time_window(int time_window);
	int spel_read_enable_status();
	void register_for_restoration();

public:
	cthd_sysfs_cdev_spel(unsigned int _index, int domain, std::string _domain_type) :
            cthd_sysfs_cdev_spel(_index, domain, std::move(_domain_type), 0, "")
	{
	}
	cthd_sysfs_cdev_spel(unsigned int _index, int domain, std::string _domain_type,
			int _constraint_index, std::string control_path) :
			cthd_cdev(_index, std::move(control_path)), domain_id(
					domain), constraint_index(_constraint_index),
			ppcc_max_pwr(0), ppcc_min_pwr(0), ppcc_min_window(0), ppcc_max_window(0),
			ppcc_step_pwr(0), constrained(false),
			power_on_constraint_pwr(0), power_on_constraint_time_window(0),
			power_on_enable_status(0), device_name("SPEL"),
			domain_type(std::move(_domain_type))
	{
	}

	void set_curr_state(int state, int arg) override;
	int get_curr_state() override;
	int get_max_state() override;
	int update() override;
	void set_curr_state_raw(int state, int arg) override;
	void thd_cdev_set_min_state_param(int arg) override;

	std::string get_domain_type() const {
		return domain_type;
	}
};

#endif /* THD_CDEV_SPEL_H_ */
