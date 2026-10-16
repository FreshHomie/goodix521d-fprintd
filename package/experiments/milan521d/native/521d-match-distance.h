/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-feature-records.h"
typedef struct {
 uint32_t enrolled_count,probe_count,enrolled_partition,probe_partition;
 /* First64-bit and first128-bit Hamming gates; entries2/3 belong to later
  * ratio selection; entry4 gates auxiliary reductions by record identifier. */
 int32_t config[5];
 GoodixMilanFeatureRecord enrolled[120],probe[120];
} Gx521dMatchDistanceInput;
typedef struct {
 /* Groups: enrolled->probe, identifier-gated enrolled->probe, probe->enrolled.
  * Each row contains best and second-best candidates. Unused=192/-1. */
 int32_t distance[3][120][2],index[3][120][2];
} Gx521dMatchDistances;
/* Descriptor admission and nearest-neighbour reduction only; no match decision.
 * Invalid counts/partitions/config leave output unchanged. */
int gx521d_match_distances(const Gx521dMatchDistanceInput *in,Gx521dMatchDistances *out);
