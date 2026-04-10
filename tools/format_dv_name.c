/*
 * thd_engine.cpp: thermal engine class implementation
 *
 * Copyright (C) 2026 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 or later as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 * Author Name <Srinivas.Pandruvada@linux.intel.com>
 */
 
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#define DMI_ID_PATH "/sys/class/dmi/id"

int read_dmi_attribute(const char *attr, char *out, size_t out_size)
{
	FILE *fp;
	char path[PATH_MAX];

	if (!attr || !out || out_size == 0) {
		errno = EINVAL;
		return -1;
	}

	if (snprintf(path, sizeof(path), "%s/%s", DMI_ID_PATH, attr) >= (int)sizeof(path)) {
		errno = ENAMETOOLONG;
		return -1;
	}

	fp = fopen(path, "r");
	if (!fp) {
		return -1;
	}

	if (!fgets(out, (int)out_size, fp)) {
		int saved = errno ? errno : EIO;

		fclose(fp);
		errno = saved;
		return -1;
	}

	fclose(fp);

	out[strcspn(out, "\n")] = '\0';
	return 0;
}

// From
// https://elixir.bootlin.com/linux/v6.19.8/source/tools/pcmcia/crc32hash.c

static unsigned int crc32(unsigned char const *p, unsigned int len)
{
	int i;
	unsigned int crc = 0;
	while (len--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
	}
	return crc;
}

int main(void)
{
	char sys_vendor[256];
	char product_name[256];
	char product_sku[256];
	char product_family[256];
	char file_name[256];
		
	if (read_dmi_attribute("sys_vendor", sys_vendor, sizeof(sys_vendor)) == 0) {
		printf("SYS_VENDOR	 : %s\n", sys_vendor);
	} else {
		perror("Failed to read SYS_VENDOR");
	}

	if (read_dmi_attribute("product_name", product_name, sizeof(product_name)) == 0) {
		printf("PRODUCT_NAME : %s\n", product_name);
	} else {
		perror("Failed to read PRODUCT_NAME");
	}

	if (read_dmi_attribute("product_family", product_family, sizeof(product_family)) == 0) {
		printf("PRODUCT_FAMILY : %s\n", product_family);
	} else {
		perror("Failed to read PRODUCT_FAMILY");
	}

	if (read_dmi_attribute("product_sku", product_sku, sizeof(product_sku)) == 0) {
		printf("PRODUCT_SKU	: %s\n", product_sku);
	} else {
		perror("Failed to read PRODUCT_SKU");
	}

	//fomrmat:
	// dtt_data_vault_${SYS_VENDOR_CRC32}_${PRODUCT_FAMILY_CRC32}_${PRODUCT_NAME_CRC32}_${PRODUCT_SKU_CRC32}.bin 

	snprintf(file_name, sizeof(file_name), "dtt_data_vault_%u_%u_%u_%u.bin", crc32(sys_vendor, strlen(sys_vendor)),
	crc32(product_family, strlen(product_family)), crc32(product_name, strlen(product_name)), crc32(product_sku, strlen(product_sku))
	); 
	
	printf("File name:%s\n", file_name);
	
	return 0;
}
