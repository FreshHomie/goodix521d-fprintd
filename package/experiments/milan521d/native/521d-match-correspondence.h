/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-match-pairs.h"
typedef struct {Gx521dMatchPairsInput input;uint8_t probe_mask[40],enrolled_mask[40];} Gx521dMatchCorrespondenceInput;
typedef struct {int32_t affine[6];uint8_t candidate[0x134];int32_t pairs[31][2];} Gx521dMatchCorrespondence;
/* Full-resolution type17 correspondence controller63f20, C7 tags0. */
int gx521d_match_correspondence(const Gx521dMatchCorrespondenceInput *,Gx521dMatchCorrespondence *);
