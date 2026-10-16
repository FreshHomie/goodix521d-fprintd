/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include <stdint.h>
typedef struct {
 uint32_t count,reference_feature;int32_t active_match,possible_match,root_affine[6];
 int32_t metadata[50][3]; /* active, quality, triangular relation row base */
 int32_t metrics[50][24];uint8_t flags[50],padding[2];int32_t relations[1226][7];
} Gx521dMatchAggregateInput;
/* Type17 cross-feature recognition safeguard60eb0. */
int gx521d_match_aggregate(const Gx521dMatchAggregateInput *,int32_t *possible);
