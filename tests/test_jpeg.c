// SPDX-License-Identifier: GPL-2.0+
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#include "../venc/jpeg-header.h"

int main(void)
{
	u8 quant[2][64], previous[2][64], header[VENC_JPEG_HEADER_SIZE + 16];
	u32 lut[162];
	unsigned int quality, t, i, pos, marker;

	memset(previous, 255, sizeof(previous));
	for (quality = 1; quality <= 100; quality++) {
		venc_jpeg_quantize(quant, quality);
		for (t = 0; t < 2; t++)
			for (i = 0; i < 64; i++) {
				assert(quant[t][i] >= 1 && quant[t][i] <= previous[t][i]);
				previous[t][i] = quant[t][i];
			}
	}
	for (i = 1; i <= 255; i++)
		assert(venc_jpeg_reciprocal(i) == (i == 1 ? 65535 : 65536 / i));
	memset(lut, 0xff, sizeof(lut));
	venc_jpeg_huffman(lut, jpeg_luma_dc_table, 0);
	assert(lut[0] == 0x10000 && lut[1] == 0x20002 && lut[11] == 0x801fe);
	for (t = 0; t < 2; t++) {
		memset(lut, 0xff, sizeof(lut));
		venc_jpeg_huffman(lut, t ? jpeg_chroma_ac_table : jpeg_luma_ac_table, 1);
		for (i = 0; i < 162; i++)
			assert(lut[i] != UINT32_MAX && (lut[i] >> 16) < 16);
	}
	venc_jpeg_quantize(quant, 90);
	memset(header, 0xa5, sizeof(header));
	venc_jpeg_header(header, 1920, 1080, quant);
	assert(header[0] == 0xff && header[1] == 0xd8);
	for (i = VENC_JPEG_HEADER_SIZE; i < sizeof(header); i++)
		assert(header[i] == 0xa5);
	pos = 2;
	while (pos < VENC_JPEG_HEADER_SIZE) {
		assert(header[pos++] == 0xff);
		while (header[pos] == 0xff) pos++;
		marker = header[pos++];
		unsigned int length = header[pos] * 256 + header[pos + 1];
		assert(length >= 2 && pos + length <= VENC_JPEG_HEADER_SIZE);
		if (marker == 0xc0) {
			assert(header[pos + 3] * 256 + header[pos + 4] == 1080);
			assert(header[pos + 5] * 256 + header[pos + 6] == 1920);
		}
		if (marker == 0xdb)
			for (i = 0; i < 64; i++)
				assert(header[pos + 3 + i] == quant[0][jpeg_zigzag[i]]);
		pos += length;
	}
	assert(pos == VENC_JPEG_HEADER_SIZE);
	puts("JPEG header, quantizer and Huffman LUT checks passed");
	return 0;
}
