// SPDX-License-Identifier: GPL-2.0+
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
#include "../vdec/esparser-padding.h"

int main(void)
{
	u8 buffer[8192], copy[8192];
	unsigned long capacities[] = { 0, 64, 4096, 4607 };

	for (unsigned int i = 0; i < sizeof(capacities) / sizeof(capacities[0]); i++) {
		memset(buffer, 0xa5, sizeof(buffer));
		memcpy(copy, buffer, sizeof(buffer));
		assert(esparser_pad_buffer(buffer, 1, capacities[i]) == -ENOSPC);
		assert(memcmp(buffer, copy, sizeof(buffer)) == 0);
	}
	assert(esparser_pad_buffer(buffer, UINT32_MAX, sizeof(buffer)) == -ENOSPC);
	assert(memcmp(buffer, copy, sizeof(buffer)) == 0);
	assert(esparser_pad_buffer(NULL, 100, sizeof(buffer)) == -ENOSPC);
	assert(esparser_pad_buffer(buffer, 3, 4608) == 4093);
	assert(buffer[2] == 0xa5 && buffer[3] == 0 && buffer[4096] == 0);
	assert(buffer[4098] == 1 && buffer[4099] == 0xff);
	assert(buffer[4607] == 0 && buffer[4608] == 0xa5);
	memset(buffer, 0xa5, sizeof(buffer));
	assert(esparser_pad_buffer(buffer, 7680, 8192) == 0);
	assert(buffer[7679] == 0xa5 && buffer[7682] == 1 && buffer[8191] == 0);
	return 0;
}
