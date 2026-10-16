/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-calidata-image.h"
#include <stddef.h>
typedef struct {
  void *pixels;
  uint16_t width,height,ppi;
  uint8_t bits,channels;
  uint32_t reserved,bytes;
  uint16_t frames;
  uint8_t padding[14],quality,coverage,tail[6];
} Gx521dDescriptor;
_Static_assert(sizeof(Gx521dDescriptor)==48 && offsetof(Gx521dDescriptor,bytes)==20 && offsetof(Gx521dDescriptor,quality)==40,"Descriptor layout");
typedef struct {int parameters_ready,calibrated;Gx521dImageContext *image;Gx521dCalidataImage *loaded;} Gx521dPreprocessor;
int gx521d_preprocessor_init(Gx521dPreprocessor *context,const uint16_t background[GX521D_PIXELS]);
void gx521d_preprocessor_clear(Gx521dPreprocessor *context);
/* Default row12, control0, liveness disabled, additional_mode0 and auxiliary
 * enabled. Bounds checks intentionally reject unsafe caller buffer sizes. */
int gx521d_preprocessor_default(Gx521dPreprocessor *context,const Gx521dDescriptor *source,
    uint8_t auxiliary[GX521D_AUXILIARY_BYTES],Gx521dDescriptor *result,int32_t coverage_quality[2]);

/* Load after successful initialization; preserves the established calibration
 * readiness state and atomically replaces only a successfully decoded context. */
int gx521d_preprocessor_load(Gx521dPreprocessor *context,const uint8_t *blob,size_t bytes);
int gx521d_preprocessor_save(const Gx521dPreprocessor *context,uint8_t *blob,size_t bytes);
