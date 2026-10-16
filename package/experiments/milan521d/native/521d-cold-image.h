/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-classification.h"
typedef struct {
  int32_t classification_status,mode,apply_mask,selected,quality,coverage;
  uint8_t image[GX521D_PIXELS];
} Gx521dColdImage;
/* Image-only composition for one live frame after fresh setup. This excludes
 * warm history, post-render state updates, extraction auxiliary data and the
 * vendor exported API's complete status/diagnostic contract. No device access. */
int gx521d_cold_image(const uint16_t background[GX521D_PIXELS],
    const uint16_t raw[GX521D_PIXELS],Gx521dColdImage *out);
