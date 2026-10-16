/* SPDX-License-Identifier: MIT */
#pragma once
#include "521d-match-policy.h"
typedef struct {int32_t metrics[24],accepted,possible,retry_counter,retry_rejected,rejection_count;uint32_t probe_records;} Gx521dMatchSecondaryPolicyInput;
typedef struct {int32_t accepted,possible,retry_counter,retry_rejected,rejection_count;} Gx521dMatchSecondaryPolicy;
int gx521d_match_secondary_policy(const Gx521dMatchSecondaryPolicyInput *,Gx521dMatchSecondaryPolicy *);
