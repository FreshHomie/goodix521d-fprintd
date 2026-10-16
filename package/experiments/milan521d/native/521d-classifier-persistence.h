/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-classification.h"
#include "521d-persistence.h"
/* Type17 owner 0x735b0 does not update the older component/class-history
 * fields. Keep them opaque so import/export still preserves an imported state. */
typedef struct {
  Gx521dClassifierContext classifier;
  Gx521dClassifierStoredState retained;
} Gx521dPersistableClassifier;
void gx521d_persistable_classifier_init(Gx521dPersistableClassifier *context);
/* Import establishes a fresh classifier context. The vendor's adaptive mask
 * threshold/average are not in this section and restart at defaults 60/60. */
int gx521d_persistable_classifier_import(Gx521dPersistableClassifier *context,
    const uint8_t *blob,size_t bytes);
int gx521d_persistable_classifier_export(const Gx521dPersistableClassifier *context,
    uint8_t *blob,size_t bytes);
