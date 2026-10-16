/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-feature.h"
#define GX521D_FEATURE_SCALES 9
typedef struct {
 uint8_t blurred[GX521D_PIXELS],enhanced[GX521D_PIXELS];
 uint16_t scales[GX521D_FEATURE_SCALES][GX521D_PIXELS];
 int32_t gradient_input[GX521D_PIXELS];
 uint32_t magnitude[GX521D_PIXELS];
 int16_t orientation[GX521D_PIXELS];
} Gx521dFeatureScale;
/* Type17 branch: selectors200..208, enhanced-byte Q8 gradient input.
 * No extrema, descriptors or complete feature records yet. */
int gx521d_feature_scale(const uint8_t image[GX521D_PIXELS],Gx521dFeatureScale *out);

int gx521d_feature_scale_prepared(const uint8_t blurred[GX521D_PIXELS],const uint8_t enhanced[GX521D_PIXELS],Gx521dFeatureScale *out);
int gx521d_feature_scale_type17(const uint8_t image[GX521D_PIXELS],Gx521dFeatureScale *out);
