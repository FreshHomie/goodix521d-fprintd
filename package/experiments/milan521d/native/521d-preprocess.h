/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include <stdint.h>
#define GX521D_ROWS 64
#define GX521D_COLUMNS 80
#define GX521D_PIXELS (GX521D_ROWS * GX521D_COLUMNS)
#define GX521D_RETRY_RAW 0x29aa
#define GX521D_RETRY_SETUP 0x29bb
/* This is the input/setup stage only, not a complete image renderer. */
int gx521d_prepare_input(uint16_t frame[GX521D_PIXELS], int setup);
int gx521d_make_setup(const uint16_t normalized[GX521D_PIXELS],
                     uint16_t map[GX521D_PIXELS], uint32_t *mean);
typedef struct {
  int32_t count, rows, columns;
  int16_t threshold;
  uint16_t percent;
  uint8_t pixels[GX521D_PIXELS];
} Gx521dMask;
int gx521d_make_mask(const uint16_t normalized_live[GX521D_PIXELS],
                    const uint16_t setup_map[GX521D_PIXELS], Gx521dMask *mask);
int gx521d_prepare_render_mask(const uint16_t normalized_live[GX521D_PIXELS],
                               const Gx521dMask *initial, Gx521dMask *render);
