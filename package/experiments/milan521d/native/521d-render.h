/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-preprocess.h"
typedef struct {
  uint32_t sample_count, auxiliary_count, ready, initialized;
  uint16_t working[GX521D_PIXELS];
  uint16_t calibration[GX521D_PIXELS];
  uint16_t application_gain[GX521D_PIXELS];
  uint16_t auxiliary_gain[GX521D_PIXELS];
  uint16_t secondary_gain[GX521D_PIXELS];
  uint16_t secondary_working[GX521D_PIXELS];
  uint16_t reciprocal[GX521D_PIXELS];
} Gx521dColdGain;
typedef struct Gx521dGainContext Gx521dGainContext;
Gx521dGainContext *gx521d_gain_context_new(void);
void gx521d_gain_context_free(Gx521dGainContext *context);
/* Isolated gain evolution: excludes post-render/classifier state changes. */
int gx521d_gain_step(Gx521dGainContext *context,
                    const uint16_t live[GX521D_PIXELS],
                    const uint16_t setup[GX521D_PIXELS],
                    const Gx521dMask *initial,Gx521dColdGain *out);
/* Currently scoped to a freshly initialized preprocessing context. */
int gx521d_cold_gain(const uint16_t live[GX521D_PIXELS],
                    const uint16_t setup[GX521D_PIXELS],
                    const Gx521dMask *initial, Gx521dColdGain *out);
int gx521d_render_primary(const uint16_t working[GX521D_PIXELS],
                         const Gx521dMask *mask, uint8_t pixels[GX521D_PIXELS]);
int gx521d_render_refined(const uint16_t working[GX521D_PIXELS],
                         const Gx521dMask *mask, uint8_t pixels[GX521D_PIXELS]);
int gx521d_selection_mask(const uint8_t primary[GX521D_PIXELS],uint8_t mask[GX521D_PIXELS]);
int gx521d_select_image(const uint8_t primary[GX521D_PIXELS],
                       const uint8_t refined[GX521D_PIXELS],
                       const uint8_t selection_mask[GX521D_PIXELS],
                       uint8_t pixels[GX521D_PIXELS],int *selected);
int gx521d_quality(const uint8_t image[GX521D_PIXELS],int *quality,int *coverage);
int gx521d_quality_with_status(const uint8_t image[GX521D_PIXELS],int *quality,int *coverage,
                              int *quality_status);
/* Encodes caller-selected flags only; does not classify an image. */
int gx521d_encode_metadata(uint8_t image[GX521D_PIXELS],
                          const uint8_t mask[GX521D_PIXELS],int mode,int apply_mask);

typedef struct {
  int32_t status,mode,component_flag,disagreement,component_score;
  int32_t primary_metric,fallback_metric,quality_gate,nested_update;
  uint8_t diagnostic[GX521D_PIXELS],component_mask[GX521D_PIXELS];
  uint8_t primary_check[GX521D_PIXELS],fallback_check[GX521D_PIXELS];
  uint8_t image[GX521D_PIXELS];
} Gx521dPostRender;
/* Type17 default profile, orientation1. Updates retained gain context when
 * post-render requests a nested gain evaluation; no hardware access. */
int gx521d_post_render(Gx521dGainContext *context,
    const uint16_t live[GX521D_PIXELS],const uint16_t setup[GX521D_PIXELS],
    const Gx521dMask *mask,const Gx521dColdGain *before,
    const uint8_t selected_pixels[GX521D_PIXELS],const uint8_t selection_mask[GX521D_PIXELS],
    int selected,Gx521dPostRender *out,Gx521dColdGain *after);
