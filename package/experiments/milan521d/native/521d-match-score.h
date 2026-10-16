/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include <stdint.h>
typedef struct {int32_t metrics[15];uint32_t alternate;} Gx521dMatchScoreInput;
/* Candidate score only; later type17 policy and template controller still apply. */
int gx521d_match_score(const Gx521dMatchScoreInput *,int32_t *);
