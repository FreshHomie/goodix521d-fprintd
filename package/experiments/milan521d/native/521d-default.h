/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-image.h"
#define GX521D_AUXILIARY_BYTES 0x4c98
typedef struct {
  int32_t status,quality,coverage;
  uint8_t image[GX521D_PIXELS],auxiliary[GX521D_AUXILIARY_BYTES];
} Gx521dDefaultResult;
/* Default flags (type17/orientation1, pixel_cancel disabled), purpose0 and
 * auxiliary output enabled. Full exported wrapper/persistence are separate.
 * Return value reports input/internal errors; status is the final core result. */
int gx521d_default_step(Gx521dImageContext *context,const uint16_t raw[GX521D_PIXELS],
    Gx521dDefaultResult *out,Gx521dImageTrace *trace);
