/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-match-refine.h"
typedef struct {Gx521dMatchDistanceInput features;int32_t pairs[31][2];uint32_t minimum_inliers;} Gx521dMatchPairFitInput;
int gx521d_match_geometry_pairs(const Gx521dMatchPairFitInput *,Gx521dMatchGeometry *);
int gx521d_match_refine_pairs(const Gx521dMatchPairFitInput *,Gx521dMatchRefined *);
