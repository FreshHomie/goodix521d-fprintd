/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-feature-candidates.h"
#include <stddef.h>
typedef struct {
 int32_t x,y,scale,strength;
 int16_t refined_x,refined_y;
 int32_t scale_value;
} Gx521dRefinedCandidate;
typedef struct {
 Gx521dCandidate initial;
 Gx521dRefinedCandidate result;
 uint32_t curvature;
 int32_t accepted;
} Gx521dRefinementRecord;
/* rows64,columns80,9planes,valid initial x/y interior and scale1..6 only.
 * Rejections may update integer coordinates; curvature changes only on success. */
int gx521d_feature_refine_extremum(const uint16_t *scales,size_t rows,size_t columns,
 Gx521dRefinedCandidate *candidate,uint32_t *curvature);
