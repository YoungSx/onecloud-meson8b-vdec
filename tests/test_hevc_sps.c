// SPDX-License-Identifier: GPL-2.0+
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
#include "../vdec/hevc-sps.h"

int main(int argc, char **argv)
{
	FILE *file;
	u8 *data;
	long size;
	struct hevc_sps_geometry g;
	unsigned int i, end, found = 0;

	assert(argc == 2);
	file = fopen(argv[1], "rb"); assert(file);
	fseek(file, 0, SEEK_END); size = ftell(file); rewind(file);
	data = malloc(size); assert(data);
	assert(fread(data, 1, size, file) == (size_t)size); fclose(file);
	for (i = 2; i + 2 < (unsigned long)size; i++) {
		if (data[i - 2] || data[i - 1] || data[i] != 1 ||
		    ((data[i + 1] >> 1) & 63) != 33) continue;
		for (end = i + 4; end + 2 < (unsigned long)size; end++)
			if (!data[end] && !data[end + 1] && data[end + 2] == 1) break;
		if (end + 2 >= (unsigned long)size) end = size;
		assert(hevc_parse_sps_geometry(data + i + 1, end - i - 1, &g) == 0);
		printf("coded %u x %u visible %u x %u dpb %u reorder %u\n",
		       g.coded_width,g.coded_height,g.width,g.height,g.dpb,g.reorder);
		assert(g.coded_width==1920 && g.coded_height==1088);
		assert(g.width==1920 && g.height==1080 && g.dpb>=g.reorder+1);
		for (unsigned int length=0; length<20; length++)
			assert(hevc_parse_sps_geometry(data+i+1,length,&g)!=0);
		found++;
	}
	assert(found);
	u8 zeros[512]={0};
	assert(hevc_parse_sps_geometry(zeros,sizeof(zeros),&g)!=0);
	zeros[0]=66;zeros[1]=1;
	assert(hevc_parse_sps_geometry(zeros,sizeof(zeros),&g)!=0);
	free(data);
	return 0;
}
