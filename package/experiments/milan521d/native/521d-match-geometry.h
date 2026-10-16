/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-match-pairs.h"
typedef struct {Gx521dMatchPairsInput input;uint32_t group,minimum_inliers;} Gx521dMatchGeometryInput;
typedef struct {int32_t affine[6],residual;uint8_t mask[44];} Gx521dMatchGeometry;
/* Initial type17 triangle model only; orientation/refit/scoring follow later. */
int gx521d_match_geometry(const Gx521dMatchGeometryInput *in,Gx521dMatchGeometry *out);
