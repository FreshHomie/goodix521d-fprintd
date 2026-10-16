/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-classification.h"
typedef struct Gx521dImageContext Gx521dImageContext;
typedef struct {
  int32_t selected,post_status,classification_status,mode,apply_mask,quality,coverage,quality_status;
  uint8_t image[GX521D_PIXELS];
} Gx521dImageResult;
typedef struct {
  uint16_t live[GX521D_PIXELS],setup[GX521D_PIXELS];
  Gx521dMask initial,mask;
  Gx521dColdGain before,after;
  Gx521dPostRender post;
  Gx521dColdClassification classified;
  Gx521dHistory history;
} Gx521dImageTrace;
/* Default type17/orientation1 image pipeline. No hardware, template or matching
 * operations. Full exported retry/diagnostic/persistence contracts are separate. */
int gx521d_image_context_new(const uint16_t background[GX521D_PIXELS],Gx521dImageContext **out);
void gx521d_image_context_free(Gx521dImageContext *context);
int gx521d_image_step(Gx521dImageContext *context,const uint16_t raw[GX521D_PIXELS],
    Gx521dImageResult *out,Gx521dImageTrace *trace);
/* Default exported-wrapper mode0 resets its entry-selection tag per call. */
void gx521d_image_reset_entry_selection(Gx521dImageContext *context);
