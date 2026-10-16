/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-match-distance.h"
typedef struct {Gx521dMatchDistanceInput features;int32_t affine[6],radius;} Gx521dMatchIndexRefitInput;
int gx521d_match_index_refit(const Gx521dMatchIndexRefitInput *,int32_t out[6]);
