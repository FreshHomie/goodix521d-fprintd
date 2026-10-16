/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-feature-object.h"
/* Transient process-owned probe cache. Never serialized into fingerprint files. */
typedef struct {int32_t ages[20];Gx521dFeatureObject features[20];} Gx521dProbeCache;
void gx521d_probe_cache_init(Gx521dProbeCache *);
/* Owner66c70. retained_flag controls duplicate suppression exactly as FA does;
 * it does not change authentication thresholds. Atomic on invalid input. */
int gx521d_probe_cache_add(Gx521dProbeCache *,const Gx521dFeatureObject *,int retained_flag);
