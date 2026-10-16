/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-template-feature.h"
#define GX521D_TEMPLATE_FEATURES 50
#define GX521D_TEMPLATE_RELATIONS 1226
#define GX521D_TEMPLATE_MAX (384*1024)
typedef struct {uint32_t index;int32_t values[7];} Gx521dStoredRelation;
typedef struct {
 /*81,98,9a,9b,91,97,92,9e,9f,9c,9d,fa,fb*/
 uint32_t header[13];
 Gx521dStoredFeature features[GX521D_TEMPLATE_FEATURES];
 uint32_t relation_count;
 Gx521dStoredRelation relations[GX521D_TEMPLATE_RELATIONS];
 int32_t graph[4];
 uint8_t tail[0x51c];
} Gx521dStoredTemplate;
typedef struct {
 Gx521dStoredTemplate stored;
 Gx521dFeatureObject features[GX521D_TEMPLATE_FEATURES];
 /* Slot0 included, as in the reference. Unset slots are {-1, identity}. */
 int32_t relations[GX521D_TEMPLATE_RELATIONS][7];
} Gx521dTemplate;
/* Canonical current type17 configuration; disjoint input/output buffers.
 * All failures preserve caller destination bytes. */
int gx521d_template_read(const uint8_t *bytes,size_t length,Gx521dStoredTemplate *out);
size_t gx521d_template_pack(const Gx521dStoredTemplate *in,uint8_t *bytes,size_t capacity);
int gx521d_template_restore(const Gx521dStoredTemplate *in,Gx521dTemplate *out);
/* Reference-style runtime export prunes redundant relation edges. */
size_t gx521d_template_export(const Gx521dTemplate *in,uint8_t *bytes,size_t capacity);
/* Load creates independent owned state. Delete clears biometric fields. */
Gx521dTemplate *gx521d_template_load(const uint8_t *bytes,size_t length);
void gx521d_template_delete(Gx521dTemplate *owned);
