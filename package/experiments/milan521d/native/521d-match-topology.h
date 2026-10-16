/* SPDX-License-Identifier: MIT */
#pragma once
#include "521d-match-record-metrics.h"
typedef struct {Gx521dMatchRecordMetricsInput input;uint32_t filtered;uint8_t enrolled_mask[120];} Gx521dMatchTopologyInput;
int gx521d_match_topology(const Gx521dMatchTopologyInput *,int32_t out[11]);
