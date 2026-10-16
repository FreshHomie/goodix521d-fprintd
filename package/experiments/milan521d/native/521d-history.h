/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-preprocess.h"
typedef struct {
  int32_t count,mask_threshold,mask_average;
  uint16_t reference[GX521D_PIXELS];
  uint8_t age[GX521D_PIXELS];
} Gx521dHistory;
/* Isolated update owner only. Initialization from retained calibration,
 * persistence and history-dependent classification are separate boundaries. */
void gx521d_history_init(Gx521dHistory *history);
int gx521d_history_update(Gx521dHistory *history,
    const uint16_t difference[GX521D_PIXELS],int sample_count,const Gx521dMask *mask);
