/*
 * udev-helper.cpp: Simple utility to use from udev rules
 *
 * Copyright (C) 2020 Red Hat Inc. All rights reserved.
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
 * Author Name <bberg@redhat.com>
 *
 * This is a utility to use together with udev rules.
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char **argv) {
	/* Currently must be run using the --firmware-managed option,
	 * so this is a bit lazy and doesn't do proper argument parsing
	 * for now. */
	int fd;

	if (argc != 2 || strcmp(argv[1], "--firmware-managed") != 0) {
		fprintf(stderr, "Must be run with --firmware-managed as the sole argument");
		return 1;
	}

	/* Flag by creating the file /run/thermald/firmware-managed */
	if (mkdir("/run/thermald", 0755) < 0) {
		if (errno != EEXIST) {
			fprintf(stderr, "Failed to create directory /run/thermald: %m\n");
			return 1;
		}
	}

	fd = creat("/run/thermald/firmware-managed", 0644);
	if (fd < 0) {
		fprintf(stderr, "Failed to create file /run/thermald/firmware-managed: %m\n");
	}

	close(fd);
}
