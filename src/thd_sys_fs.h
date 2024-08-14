/*
 * thd_sys_fs.h: sysfs class interface
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#ifndef THD_SYS_FS_H_
#define THD_SYS_FS_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

class csys_fs {
private:
	std::string base_path;

public:
	csys_fs() :
			base_path("") {
	}
	;
	csys_fs(const char *path) :
			base_path(path) {
	}

	/* write data to base path (dir) + provided path */
	int write(const std::string &path, const std::string &buf);
	int write(const std::string &path, unsigned int data);
	int write(const std::string &path, unsigned int position,
			unsigned long long data);

	/* read data from base path (dir) + provided path */
	int read(const std::string &path, char *buf, int len);
	int read(const std::string &path, std::string &buf);
	int read(const std::string &path, int *ptr_val);
	int read(const std::string &path, unsigned long *ptr_val);
	int read(const std::string &path, unsigned int position, char *buf,
			int len);

	const char *get_base_path() {
		return base_path.c_str();
	}
	int read_symbolic_link_value(const std::string &path, char *buf, int len);

	bool exists(const std::string &path);
	bool exists();
	size_t size(const std::string &path);
	int create();
	mode_t get_mode(const std::string &path);

	void update_path(std::string path) {
		base_path = std::move(path);
	}
};

#endif /* THD_SYS_FS_H_ */
