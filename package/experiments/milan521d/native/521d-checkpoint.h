/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-image.h"
/* Experimental lossless native runtime checkpoint, not the Windows calidata
 * file format and not an ABI-stable on-disk fingerprint/template format. */
typedef struct {
  uint32_t sample_count,stable_count,update_state,auxiliary_count,ready,initialized;
  uint16_t calibration[GX521D_PIXELS],application_gain[GX521D_PIXELS];
  uint16_t auxiliary_gain[GX521D_PIXELS],secondary_gain[GX521D_PIXELS];
  uint16_t coarse_reference[GX521D_PIXELS/4];
} Gx521dGainCheckpoint;
typedef struct {
  uint32_t version,rows,columns;
  int32_t previous_selected;
  uint16_t setup[GX521D_PIXELS];
  Gx521dGainCheckpoint gain;
  Gx521dHistory history;
} Gx521dImageCheckpoint;
int gx521d_gain_checkpoint(const Gx521dGainContext *context,Gx521dGainCheckpoint *out);
int gx521d_gain_restore(Gx521dGainContext *context,const Gx521dGainCheckpoint *input);
int gx521d_image_checkpoint(const Gx521dImageContext *context,Gx521dImageCheckpoint *out);
int gx521d_image_restore(const Gx521dImageCheckpoint *input,Gx521dImageContext **out);
