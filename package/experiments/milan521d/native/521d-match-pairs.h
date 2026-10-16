/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-match-distance.h"
typedef struct {Gx521dMatchDistanceInput features;uint32_t enrolled_coverage,probe_coverage;} Gx521dMatchPairsInput;
typedef struct {
 /* Primary, identifier-gated, reverse. Each pair is enrolled/probe. */
 int32_t pairs[3][31][2];
} Gx521dMatchPairs;
int gx521d_match_pairs(const Gx521dMatchPairsInput *in,Gx521dMatchPairs *out);
