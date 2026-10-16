/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-preprocess.h"
#include <stddef.h>
#define GX521D_CALIDATA_BYTES 0x224b0
/* Exact active type17 export fields. Legacy coefficient blocks and the full
 * classifier-containing span remain opaque; this is not a fingerprint template.
 * This codec does not claim full runtime context restoration. */
typedef struct {
  uint16_t calibration[GX521D_PIXELS],setup[GX521D_PIXELS];
  uint8_t coefficients[0x800],parameters[0x4a40],classifier[0xa000];
  int32_t sample_count,retained_flag;
} Gx521dCalidata;
_Static_assert(sizeof(Gx521dCalidata)==82504,"Type17 calibration field layout");
/* Save preserves gaps/padding in the caller buffer, as the vendor does. Load
 * validates both active-map CRCs and the version before modifying destination.
 * Returns0,0x80 invalid content,0x81 invalid pointer/short buffer. */
int gx521d_calidata_store(const Gx521dCalidata *state,uint8_t *blob,size_t bytes);
int gx521d_calidata_load(Gx521dCalidata *state,const uint8_t *blob,size_t bytes);
