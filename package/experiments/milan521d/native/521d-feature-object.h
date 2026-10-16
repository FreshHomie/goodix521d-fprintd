/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-feature-records.h"
typedef struct {
 uint8_t scalar_header[0x168];
 uint8_t high[640],low[640],validity[GX521D_PIXELS],packed_mask[640];
 GoodixMilanFeatureRecord records[GX521D_RECORDS_MAX];
} Gx521dFeatureObject;
/* Default enhanced, full-resolution type17 path, optional metadata absent. */
int gx521d_feature_extract(const uint8_t image[GX521D_PIXELS],int quality,int coverage,int mode,Gx521dFeatureObject *out);
