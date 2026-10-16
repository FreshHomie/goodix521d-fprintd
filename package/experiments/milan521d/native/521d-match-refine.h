/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-match-geometry.h"
typedef struct {int32_t affine[6];uint32_t retained;uint8_t mask[44],enrolled_mask[120];} Gx521dMatchRefined;
int gx521d_match_refine(const Gx521dMatchGeometryInput *in,Gx521dMatchRefined *out);
