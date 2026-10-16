/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include <stddef.h>
#include "521d-feature-object.h"
#define GX521D_PACKED_FEATURE_MAX 5300
/* One type17 tag95 element, not a complete template. Returns zero on invalid
 * input or insufficient capacity; failure leaves the destination unchanged. */
size_t gx521d_template_feature_pack(const Gx521dFeatureObject *feature,
                                   uint8_t *destination,size_t capacity);

/* Serialized representation: deliberately distinct from extraction workspace.
 * Packed records omit extraction strength and dense validity state. */
typedef struct {
 uint8_t high[640],low[640],coarse_mask[40];
 uint32_t count;
 uint8_t records[GX521D_RECORDS_MAX][32];
 uint32_t scalars[11],embedded_tag;
} Gx521dStoredFeature;
size_t gx521d_stored_feature_pack(const Gx521dStoredFeature *feature,
                                uint8_t *destination,size_t capacity);
/* Accepts exactly one canonical default type17 element. Unknown/duplicate tags,
 * incompatible geometry, missing/trailing data and bad lengths fail atomically.
 * Not a parser for complete Windows templates or optional sensor profiles. */
int gx521d_stored_feature_read(const uint8_t *source,size_t length,
                             Gx521dStoredFeature *destination);
