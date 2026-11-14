// SPDX-License-Identifier: GPL-2.0+
/* Meson8b JPEG DCT/VLC setup, ported from Hardkernel jpegenc.c
 * at 5aed95d35d252cafc75ce613a3a0052285662de2 (Simon Zheng).
 */
#include "venc.h"
#include "jpeg-header.h"

void venc_jpeg_init(struct venc_ctx *ctx)
{
	struct venc_core *core = ctx->core;
	u8 quant[2][64];
	u32 lut[162];
	unsigned int t, i;

	venc_jpeg_quantize(quant, ctx->jpeg_quality);
	venc_write(core, VLC_PIC_SIZE, ctx->width | (ctx->height << 16));
	/* 4:2:0, mark the last coefficient at each MCU boundary. */
	venc_write(core, VLC_PIC_POSITION, 3 | BIT(4));
	venc_write(core, QDCT_JPEG_X_START_END, (ctx->width - 1) << 16);
	venc_write(core, QDCT_JPEG_Y_START_END, (ctx->height - 1) << 16);
	venc_write(core, QDCT_JPEG_QUANT_ADDR, 0);
	for (t = 0; t < 2; t++)
		for (i = 0; i < 64; i += 2)
			venc_write(core, QDCT_JPEG_QUANT_DATA,
				   venc_jpeg_reciprocal(quant[t][i]) |
				   ((u32)venc_jpeg_reciprocal(quant[t][i + 1]) << 16));

	venc_write(core, VLC_HUFFMAN_ADDR, 0);
	for (t = 0; t < 2; t++) {
		venc_jpeg_huffman(lut, t ? jpeg_chroma_dc_table :
				  jpeg_luma_dc_table, 0);
		for (i = 0; i < 12; i++)
			venc_write(core, VLC_HUFFMAN_DATA, lut[i]);
	}
	venc_write(core, VLC_HUFFMAN_ADDR, 24);
	for (t = 0; t < 2; t++) {
		venc_jpeg_huffman(lut, t ? jpeg_chroma_ac_table :
				  jpeg_luma_ac_table, 1);
		for (i = 0; i < 162; i++)
			venc_write(core, VLC_HUFFMAN_DATA, lut[i]);
	}

	/* Three components, Y 2x2 blocks, U/V 1x1, chroma table 1. */
	venc_write(core, QDCT_JPEG_CTRL, BIT(16) | BIT(15) | BIT(12) |
		   BIT(11) | BIT(8) | BIT(6) | BIT(5) | BIT(4) | BIT(0));
	venc_write(core, VLC_JPEG_CTRL, BIT(29) | BIT(28) | BIT(24) |
		   BIT(21) | BIT(20) | BIT(16) | (3 << 9) | BIT(8));
	venc_write(core, QDCT_MB_CONTROL, BIT(9) | BIT(0));
	venc_write(core, QDCT_MB_CONTROL, BIT(13) | BIT(12));

	venc_jpeg_header(ctx->buf_vaddr + ctx->mem.bitstream_start -
			 ctx->buf_paddr, ctx->width, ctx->height, quant);
	/* JPEG microcode uses scratch 1 as header size, not AVC workspace. */
	venc_write(core, HENC_SCRATCH_1, VENC_JPEG_HEADER_SIZE);
	venc_write(core, VLC_TOTAL_BYTES, 0);
	venc_write(core, VLC_VB_MEM_CTL, BIT(31) | (0x3f << 24) | (0x20 << 16) | 2);
	venc_write(core, VLC_VB_START_PTR,
		   ctx->mem.bitstream_start + VENC_JPEG_HEADER_SIZE);
	venc_write(core, VLC_VB_WR_PTR,
		   ctx->mem.bitstream_start + VENC_JPEG_HEADER_SIZE);
	venc_write(core, VLC_VB_SW_RD_PTR,
		   ctx->mem.bitstream_start + VENC_JPEG_HEADER_SIZE);
	venc_write(core, VLC_VB_END_PTR, ctx->mem.bitstream_end);
	venc_write(core, VLC_VB_CONTROL, 1);
	venc_write(core, VLC_VB_CONTROL, (7 << 3) | BIT(1));
}
