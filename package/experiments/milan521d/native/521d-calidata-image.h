/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-calidata.h"
#include "521d-default.h"
typedef struct Gx521dCalidataImage Gx521dCalidataImage;
/* Import into a fresh default context, as reset/init/load does in the pinned
 * reference. Saved calibration fields are not a lossless runtime checkpoint. */
int gx521d_calidata_image_new(const uint8_t *blob,size_t bytes,Gx521dCalidataImage **out);
void gx521d_calidata_image_free(Gx521dCalidataImage *context);
int gx521d_calidata_image_step(Gx521dCalidataImage *context,const uint16_t raw[GX521D_PIXELS],
    Gx521dDefaultResult *out,Gx521dImageTrace *trace);
int gx521d_calidata_image_store(const Gx521dCalidataImage *context,uint8_t *blob,size_t bytes);

void gx521d_calidata_image_reset_entry_selection(Gx521dCalidataImage *context);
