/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-match-index-refit.h"
#include "521d-match-overlap-diagnostic.h"
#include "521d-match-record-metrics.h"
typedef struct {
 Gx521dMatchDistanceInput features;Gx521dMatchOverlapInput image;
 uint8_t candidate[0x134];
} Gx521dMatchPostRefineInput;
typedef struct {int32_t affine[6];uint8_t candidate[0x134];} Gx521dMatchPostRefine;
/* Type17 post-admission refinement; optional auxiliary records absent. */
int gx521d_match_post_refine(const Gx521dMatchPostRefineInput *,Gx521dMatchPostRefine *);
