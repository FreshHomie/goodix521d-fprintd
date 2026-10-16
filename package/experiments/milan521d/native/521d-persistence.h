/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-preprocess.h"
#include <stddef.h>
#define GX521D_CLASSIFIER_STORE_BYTES (24 + GX521D_PIXELS * 2 + GX521D_PIXELS / 2)
/* Isolated classifier section of the vendor workspace. This is not a full
 * preprocessing workspace or a fingerprint template. Scalars follow descriptor
 * offsets +0x20, +0x24, +0x28: DLL globals RVA 0x170550, 0x170530,
 * 0x170558 respectively. The last scalar is history.count; the other two
 * belong to the older retained-component/class-history path. */
typedef struct {
  int32_t scalar20,scalar24,scalar28,version;
  uint8_t component_age[GX521D_PIXELS];
  uint8_t classes[3][GX521D_PIXELS];
  uint8_t reference_age[GX521D_PIXELS];
  uint16_t reference[GX521D_PIXELS];
} Gx521dClassifierStoredState;
_Static_assert(sizeof(Gx521dClassifierStoredState)==35856,"Classifier codec state layout");
/* Returns 0 on success. Rejects the wrong length/version/magic without changing
 * the destination. The vendor loader itself updates version before rejecting a
 * bad version; this bounded public codec deliberately rejects it atomically. */
int gx521d_classifier_store(const Gx521dClassifierStoredState *state,
    uint8_t *blob,size_t bytes);
int gx521d_classifier_load(Gx521dClassifierStoredState *state,
    const uint8_t *blob,size_t bytes);
