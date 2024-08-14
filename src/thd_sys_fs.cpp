/*
 * thd_sys_fs.cpp: sysfs class implementation
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

#include "thd_sys_fs.h"
#include "thd_common.h"
#include <stdlib.h>

int csys_fs::write(const std::string &path, const std::string &buf) {
	std::string p = base_path + path;
	int fd = ::open(p.c_str(), O_WRONLY);
	if (fd < 0) {
		thd_log_info("sysfs write failed %s\n", p.c_str());
		return -errno;
	}
	int ret = ::write(fd, buf.c_str(), buf.size());
	if (ret < 0) {
		ret = -errno;
		thd_log_info("sysfs write failed %s\n", p.c_str());
	}
	close(fd);

	return ret;
}

int csys_fs::write(const std::string &path, unsigned int position, unsigned
long long data) {
	std::string p = base_path + path;
	int fd = ::open(p.c_str(), O_WRONLY);
	if (fd < 0) {
		thd_log_info("sysfs write failed %s\n", p.c_str());
		return -errno;
	}
	if (::lseek(fd, position, SEEK_CUR) == -1) {
		thd_log_info("sysfs write failed %s\n", p.c_str());
		close(fd);
		return -errno;
	}
	int ret = ::write(fd, &data, sizeof(data));
	if (ret < 0)
		thd_log_info("sysfs write failed %s\n", p.c_str());
	close(fd);

	return ret;
}

int csys_fs::write(const std::string &path, unsigned int data) {
	std::ostringstream os;
	os << data;
	return csys_fs::write(path, os.str());
}

int csys_fs::read(const std::string &path, char *buf, int len) {
	if (!buf)
		return -EINVAL;

	std::string p = base_path + path;
	int fd = ::open(p.c_str(), O_RDONLY);
	size_t curr_len = len;
	if (fd < 0) {
		thd_log_info("sysfs read failed %s\n", p.c_str());
		return -errno;
	}
	while (curr_len > 0) {
		ssize_t ret = ::read(fd, buf, curr_len);
		if (ret <= 0 || ret > len || ret >= INT_MAX) {
			thd_log_info("sysfs read failed %s\n", p.c_str());
			close(fd);
			return -1;
		}
		buf += (int) ret;
		curr_len -= ret;
	}
	close(fd);

	return len;
}

int csys_fs::read(const std::string &path, unsigned int position, char *buf,
		int len) {
	std::string p = base_path + path;
	int fd = ::open(p.c_str(), O_RDONLY);
	if (fd < 0) {
		thd_log_info("sysfs read failed %s\n", p.c_str());
		return -errno;
	}
	if (::lseek(fd, position, SEEK_CUR) == -1) {
		thd_log_info("sysfs read failed %s\n", p.c_str());
		close(fd);
		return -errno;
	}
	int ret = ::read(fd, buf, len);
	if (ret < 0)
		thd_log_info("sysfs read failed %s\n", p.c_str());
	close(fd);

	return ret;
}

int csys_fs::read(const std::string &path, int *ptr_val) {
	std::string p = base_path + path;
	char str[16];
	int ret;

	int fd = ::open(p.c_str(), O_RDONLY);
	if (fd < 0) {
		thd_log_info("sysfs open failed %s\n", p.c_str());
		return -errno;
	}
	ret = ::read(fd, str, sizeof(str));
	if (ret > 0)
		*ptr_val = atoi(str);
	else
		thd_log_info("sysfs read failed %s\n", p.c_str());
	close(fd);

	return ret;
}

int csys_fs::read(const std::string &path, unsigned long *ptr_val) {
	std::string p = base_path + path;
	char str[32];
	int ret;

	int fd = ::open(p.c_str(), O_RDONLY);
	if (fd < 0) {
		thd_log_info("sysfs read failed %s\n", p.c_str());
		return -errno;
	}
	ret = ::read(fd, str, sizeof(str));
	if (ret > 0)
		*ptr_val = atol(str);
	else
		thd_log_info("sysfs read failed %s\n", p.c_str());
	close(fd);

	return ret;
}

int csys_fs::read(const std::string &path, std::string &buf) {
	std::string p = base_path + path;
	int ret = 0;

#ifndef ANDROID
	try {
#endif
		std::ifstream f(p.c_str(), std::fstream::in);
		if (f.fail()) {
			thd_log_info("sysfs read failed %s\n", p.c_str());
			return -EINVAL;
		}
		f >> buf;
		if (f.bad()) {
			thd_log_info("sysfs read failed %s\n", p.c_str());
			ret = -EIO;
		}
		f.close();
#ifndef ANDROID
	} catch (...) {
		thd_log_info("csys_fs::read exception %s\n", p.c_str());

		ret = -EIO;
	}
#endif
	return ret;
}

bool csys_fs::exists(const std::string &path) {
	struct stat s;

	return (bool) (stat((base_path + path).c_str(), &s) == 0);
}

size_t csys_fs::size(const std::string &path) {
	struct stat s;

	if (stat((base_path + path).c_str(), &s) == 0)
		return s.st_size;

	return 0;
}

int csys_fs::create() {

	int fd = ::open(base_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
	if (fd < 0) {
		thd_log_info("sysfs create failed %s\n", base_path.c_str());
		return -errno;
	}
	close(fd);
	return 0;
}

bool csys_fs::exists() {
	return csys_fs::exists("");
}

mode_t csys_fs::get_mode(const std::string &path) {
	struct stat s;

	if (stat((base_path + path).c_str(), &s) == 0)
		return s.st_mode;
	else
		return 0;
}

int csys_fs::read_symbolic_link_value(const std::string &path, char *buf,
		int len) {
	std::string p = base_path + path;
	int ret = ::readlink(p.c_str(), buf, len);
	if (ret < 0) {
		*buf = '\0';
		thd_log_info("read_symbolic_link %s\n", path.c_str());
		return -errno;
	}
	buf[ret] = '\0';
	return 0;
}
