/* SPDX-License-Identifier: GPL-2.0 */
#ifndef MESON8B_RUNTIME_H
#define MESON8B_RUNTIME_H

#include <linux/types.h>
#include <linux/io.h>

/* Importing this symbol prevents removing the overlay before its drivers. */
bool meson8b_runtime_ready(void);

/* VDEC1/HCODEC share one DOS resource and top-level control registers. */
void __iomem *meson8b_runtime_dos_base(void);
void meson8b_runtime_update_dos_bits(u32 reg, u32 mask, u32 value);

#endif
