/* SPDX-License-Identifier: MIT */
#pragma once
#include "521d-match-distance.h"
typedef struct {Gx521dMatchDistanceInput features;int32_t affine[6];} Gx521dMatchRecordMetricsInput;
typedef struct {
 int32_t values[11];uint8_t enrolled_mask[120];uint32_t probe_secondary[120];
} Gx521dMatchRecordMetrics;
/* Default absent auxiliary records; full-resolution type17 topology owner. */
int gx521d_match_record_metrics(const Gx521dMatchRecordMetricsInput *,Gx521dMatchRecordMetrics *);
