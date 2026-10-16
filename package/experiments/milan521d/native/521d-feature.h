/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-preprocess.h"
#define GX521D_FEATURE_BITMAP_BYTES (GX521D_PIXELS/8)
typedef struct {
  int32_t threshold;
  uint8_t blurred[GX521D_PIXELS],validity[GX521D_PIXELS];
  uint8_t high[GX521D_FEATURE_BITMAP_BYTES],low[GX521D_FEATURE_BITMAP_BYTES];
} Gx521dFeatureBase;
/* Type17 full-resolution first feature stage; no feature records or matching. */
int gx521d_feature_base(const uint8_t image[GX521D_PIXELS],Gx521dFeatureBase *out);

int gx521d_feature_pack_bitmap(const uint8_t image[GX521D_PIXELS],const uint8_t mask[GX521D_PIXELS],
    uint8_t bitmap[GX521D_FEATURE_BITMAP_BYTES],int32_t *threshold);
typedef struct {
  int32_t threshold;
  uint8_t orientation[GX521D_PIXELS],enhanced[GX521D_PIXELS],bitmap[GX521D_FEATURE_BITMAP_BYTES];
} Gx521dFeatureEnhanced;
int gx521d_feature_enhance(const uint8_t image[GX521D_PIXELS],Gx521dFeatureEnhanced *out);
