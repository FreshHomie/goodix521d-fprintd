/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include <stdint.h>
typedef struct {
 int32_t affine[6],mode,interpolation,context_count;
 uint32_t enhanced;
 uint8_t probe_high[640],probe_low[640],probe_mask[40];
 uint8_t enrolled_high[640],enrolled_low[640],enrolled_mask[40];
} Gx521dMatchOverlapInput;
typedef struct {int32_t score,primary,secondary,detail,coverage,reserved;} Gx521dMatchOverlap;
/* Full-resolution type17 image overlap; no final authentication decision. */
int gx521d_match_overlap(const Gx521dMatchOverlapInput *,Gx521dMatchOverlap *);
