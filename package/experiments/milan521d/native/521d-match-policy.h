/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
typedef struct {int32_t metrics[24],mode,counter;} Gx521dMatchPolicyInput;
typedef struct {int32_t counter,accepted,possible;} Gx521dMatchPolicy;
/* Type17 candidate admission, default pinned tables; caller supplies measurements.
 * This is one candidate policy, not the complete multi-feature controller. */
int gx521d_match_policy(const Gx521dMatchPolicyInput *,Gx521dMatchPolicy *);
