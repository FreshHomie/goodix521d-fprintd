/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-match-distance.h"
typedef struct {Gx521dMatchDistanceInput features;int32_t affine[6];} Gx521dMatchAlternateInput;
typedef struct {int32_t pairs[31][2];} Gx521dMatchAlternate;
int gx521d_match_alternate(const Gx521dMatchAlternateInput *,Gx521dMatchAlternate *);
