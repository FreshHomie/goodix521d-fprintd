/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-feature-refinement.h"
#include "milan/feature/feature.h"
#define GX521D_MATERIALIZED_MAX 300
#define GX521D_RECORDS_MAX 120
typedef struct {
 int32_t count,materialized_count;
 GoodixMilanFeatureRecord records[GX521D_RECORDS_MAX],materialized[GX521D_MATERIALIZED_MAX];
 GoodixMilanFeatureRank ranks[GX521D_MATERIALIZED_MAX];
 GoodixMilanFeatureAux auxiliary[GX521D_MATERIALIZED_MAX];
} Gx521dFeatureRecords;
int gx521d_feature_records(const uint8_t image[GX521D_PIXELS],int capacity,Gx521dFeatureRecords *out);
