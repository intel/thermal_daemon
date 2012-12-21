/*
 * thd_sys_fs.cpp: sysfs class implementation
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
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

int csys_fs::write(const std::string &path, const std::string &buf)
{
    std::string p = base_path + path;
    int fd = ::open(p.c_str(), O_WRONLY);
    if (fd < 0)
        return -errno;

    int ret = ::write(fd, buf.c_str(), buf.size());
    close(fd);

    return ret;
}

int csys_fs::write(const std::string &path, unsigned int data)
{
	std::ostringstream os;
	os << data;
	return csys_fs::write(path, os.str());
}

int csys_fs::read(const std::string &path, char *buf, int len)
{
	std::string p = base_path + path;
	int fd = ::open(p.c_str(), O_RDONLY);
	if (fd < 0)
        	return -errno;

	int ret = ::read(fd, buf, len);
	close(fd);

	return ret;
}

int csys_fs::read(const std::string &path, std::string &buf)
{
	std::string p = base_path + path;

	std::ifstream f(p.c_str(), std::fstream::in);
	if (f.fail())
        	return -EINVAL;

	int ret = 0;
	f >> buf;
	if (f.bad())
        	ret = -EIO;
	f.close();

	return ret;
}

bool csys_fs::exists(const std::string &path)
{
	struct stat s;
	//thd_log_debug("checking for %s\n", (base_path+path).c_str());
	return (bool) (stat((base_path + path).c_str(), &s) == 0);
}

bool csys_fs::exists()
{
	return csys_fs::exists("");
}

int csys_fs::read_symbolic_link_value(const std::string &path, char *buf, int len)
{
	std::string p = base_path + path;
	int ret = ::readlink(p.c_str(), buf, len);
	if (ret < 0)
        	return -errno;
	buf[ret] = '\0';
	return 0;
}

