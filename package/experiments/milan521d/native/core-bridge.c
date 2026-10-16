/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Reuse pinned upstream kernels for isolated type-17 experiments. The included
 * private core variant changes only documented quality-patch geometry.
 * Copyright (C) 2026 goodix-fp-linux-dev contributors (included core source).
 * Local adapter connects our validated geometry and mask owners.
 */
#ifndef MILAN_UPSTREAM_CORE
#error Define the pinned upstream core source path
#endif
#include MILAN_UPSTREAM_CORE
#include "521d-render.h"

struct Gx521dGainContext {
  GoodixMilanPreprocessState state;
  uint32_t ready;
};

Gx521dGainContext *gx521d_gain_context_new(void)
{
  Gx521dGainContext *context=calloc(1,sizeof(*context));
  if (context) goodix_milan_preprocess_reset(&context->state);
  return context;
}
void gx521d_gain_context_free(Gx521dGainContext *context) {free(context);}

static void gx521d_initialize_gain(GoodixMilanPreprocessState *state)
{
  /* Initializer 0x7ee80 uses the actual profile pixel count. */
  if (state->sample_count<=3) {
    profile9_initialize_gain_state(state);
    return;
  }
  if (state->application_gain_initialized) return;
  uint32_t sum=0;
  for (size_t i=0;i<GX521D_PIXELS;i++) sum+=state->calibration_map[i];
  uint32_t mean=(sum+GX521D_PIXELS/2)/GX521D_PIXELS;
  if (!mean) {state->sample_count=0;state->application_gain_initialized=1;return;}
  for (size_t i=0;i<GX521D_PIXELS;i++)
    state->application_gain_map[i]=(uint16_t)(((uint32_t)state->calibration_map[i]*8192+mean/2)/mean);
  state->application_gain_initialized=1;
}

int gx521d_gain_step(Gx521dGainContext *context,
                    const uint16_t live[GX521D_PIXELS],
                    const uint16_t setup[GX521D_PIXELS],
                    const Gx521dMask *initial, Gx521dColdGain *out)
{
  if (!context || !live || !setup || !initial || !out || initial->rows!=GX521D_ROWS ||
      initial->columns!=GX521D_COLUMNS) return -1;
  GoodixMilanPreprocessState *state=&context->state;
  gx521d_initialize_gain(state);
  goodix_milan_profile9_update_gain_ready(state->auxiliary_sample_count,
                                         state->sample_count,&context->ready);
  uint16_t gain[GX521D_PIXELS];
  uint16_t application_before[GX521D_PIXELS];
  memcpy(application_before,state->application_gain_map,sizeof(application_before));
  /* Type17 0x7d879..0x7d8a1: omit auxiliary maps while the primary
   * counter is >3 and the auxiliary counter is still <5. This differs
   * from the type12 rule based on calibration_ready. */
  int use_auxiliary=state->sample_count<=3 || state->auxiliary_sample_count>=5;
  memset(out,0,sizeof(*out));
  /* 0x7f2f3 builds the reciprocal plane before the gain-tail state update.
   * A commit can change the application map later in the same frame. */
  for (size_t i=0;i<GX521D_PIXELS;i++)
    out->reciprocal[i]=state->application_gain_map[i]==0 ? (uint16_t)(8000U<<13):
      (uint16_t)((8000U*8192+state->application_gain_map[i]/2)/state->application_gain_map[i]);
  int status=0;
  /* Native type 17 uses strict >80% admission here. The >90% special case
   * applies to type 12; see pinned owner 0x7d3a0 at 0x7d7ac..0x7d837. */
  if (initial->count*10>GX521D_PIXELS*8) {
    status=first_update_frame_core(live,setup,state->calibration_map,
               state->application_gain_map,state->auxiliary_gain_map,
               state->auxiliary_sample_count,state,context->ready,
               GX521D_ROWS,GX521D_COLUMNS,gain,out->working,out->secondary_working);
    /* Rebuild the temporary divisor to apply the type17 auxiliary-map gate.
     * Its first cold call already includes the secondary auxiliary map.
     * The upstream type12 low-count shortcut intentionally omits it. */
    uint16_t difference[GX521D_PIXELS],median[GX521D_PIXELS],adjusted[GX521D_PIXELS];
    uint32_t ratio[GX521D_PIXELS];
    profile9_update_source(live,setup,GX521D_PIXELS,difference);
    median3x3_core(difference,GX521D_ROWS,GX521D_COLUMNS,median);
    profile9_build_temporary_gain(difference,median,GX521D_PIXELS,gain,ratio,adjusted);
    for (size_t i=0;i<GX521D_PIXELS;i++) {
      uint16_t divisor=!use_auxiliary ? gain[i]:
        (uint16_t)(((uint32_t)gain[i]*state->secondary_auxiliary_gain_map[i]+4096)>>13);
      uint16_t primary=goodix_milan_profile9_combine_gain(gain[i],application_before[i],
                                        state->auxiliary_gain_map[i],!use_auxiliary);
      out->working[i]=state->calibration_map[i]==0 ? difference[i]:primary==0 ?
        (uint16_t)((uint32_t)difference[i]<<14):
        (uint16_t)(((uint32_t)difference[i]*16384+primary/2)/primary);
      out->secondary_working[i]=state->calibration_map[i]==0 ? difference[i] :
        divisor==0 ? (uint16_t)((uint32_t)difference[i]<<14) :
        (uint16_t)(((uint32_t)difference[i]*16384+divisor/2)/divisor);
    }
  } else {
    profile9_update_source(live,setup,GX521D_PIXELS,out->working);
    for (size_t i=0;i<GX521D_PIXELS;i++) {
      uint16_t d=out->working[i];
      uint16_t primary=goodix_milan_profile9_combine_gain(8192,state->application_gain_map[i],
                                             state->auxiliary_gain_map[i],!use_auxiliary);
      uint16_t secondary=!use_auxiliary ? 8192:state->secondary_auxiliary_gain_map[i];
      out->secondary_working[i]=state->calibration_map[i]==0 ? d:secondary==0 ?
        (uint16_t)((uint32_t)d<<14):(uint16_t)(((uint32_t)d*16384+secondary/2)/secondary);
      out->working[i]=state->calibration_map[i]==0 ? d:primary==0 ?
        (uint16_t)((uint32_t)d<<14):(uint16_t)(((uint32_t)d*16384+primary/2)/primary);
    }
  }
  out->sample_count=state->sample_count;
  out->auxiliary_count=state->auxiliary_sample_count;
  out->ready=context->ready;
  out->initialized=state->application_gain_initialized;
  memcpy(out->calibration,state->calibration_map,sizeof(out->calibration));
  memcpy(out->application_gain,state->application_gain_map,sizeof(out->application_gain));
  memcpy(out->auxiliary_gain,state->auxiliary_gain_map,sizeof(out->auxiliary_gain));
  memcpy(out->secondary_gain,state->secondary_auxiliary_gain_map,sizeof(out->secondary_gain));
  return status;
}

int gx521d_cold_gain(const uint16_t live[GX521D_PIXELS],
                    const uint16_t setup[GX521D_PIXELS],
                    const Gx521dMask *initial,Gx521dColdGain *out)
{
  Gx521dGainContext *context=gx521d_gain_context_new();
  if (!context) return -1;
  int status=gx521d_gain_step(context,live,setup,initial,out);
  gx521d_gain_context_free(context);
  return status;
}

int gx521d_render_primary(const uint16_t working[GX521D_PIXELS],
                         const Gx521dMask *mask,uint8_t pixels[GX521D_PIXELS])
{
  if (!working || !mask || !pixels || mask->rows!=GX521D_ROWS ||
      mask->columns!=GX521D_COLUMNS) return -1;
  return goodix_milan_preprocess_contrast(working,mask->pixels,
                                         GX521D_ROWS,GX521D_COLUMNS,pixels);
}

int gx521d_render_refined(const uint16_t working[GX521D_PIXELS],
                         const Gx521dMask *mask,uint8_t pixels[GX521D_PIXELS])
{
  if (!working || !mask || !pixels) return -1;
  uint16_t centered[GX521D_PIXELS];
  /* Profile 12/type17 selects axis mode one: center columns, not rows.
   * Preserve unsigned sums, signed-word narrowing and the observed clamp. */
  for (size_t x=0;x<GX521D_COLUMNS;x++) {
    uint32_t sum=0,count=0;
    for (size_t y=0;y<GX521D_ROWS;y++) {
      size_t i=y*GX521D_COLUMNS+x;
      if (mask->percent>=96 || mask->pixels[i]) {sum+=working[i];count++;}
    }
    uint16_t mean=count ? (uint16_t)(sum/count):0;
    for (size_t y=0;y<GX521D_ROWS;y++) {
      size_t i=y*GX521D_COLUMNS+x;
      int value=mask->percent<96 && !mask->pixels[i] ? 5000:
                  (int16_t)((int)working[i]-mean+5000);
      centered[i]=(uint16_t)(value>0 ? value:0);
    }
  }
  return gx521d_render_primary(centered,mask,pixels);
}

int gx521d_selection_mask(const uint8_t primary[GX521D_PIXELS],uint8_t mask[GX521D_PIXELS])
{
  return goodix_milan_preprocess_selection_mask(primary,GX521D_ROWS,GX521D_COLUMNS,mask);
}

int gx521d_select_image(const uint8_t primary[GX521D_PIXELS],
                       const uint8_t refined[GX521D_PIXELS],
                       const uint8_t mask[GX521D_PIXELS],
                       uint8_t pixels[GX521D_PIXELS],int *selected)
{
  if (!primary || !refined || !mask || !pixels || !selected) return -1;
  uint8_t a[GX521D_PIXELS],b[GX521D_PIXELS],m[GX521D_PIXELS],out[GX521D_PIXELS];
  /* Native column-difference metric is the transposed row metric. */
  for (size_t y=0;y<GX521D_ROWS;y++)
    for (size_t x=0;x<GX521D_COLUMNS;x++) {
      size_t i=y*GX521D_COLUMNS+x,t=x*GX521D_ROWS+y;
      a[t]=primary[i];b[t]=refined[i];m[t]=mask[i];
    }
  int status=goodix_milan_preprocess_select_output(a,b,m,GX521D_COLUMNS,GX521D_ROWS,
                                                   800,out,selected);
  if (!status)
    for (size_t y=0;y<GX521D_ROWS;y++)
      for (size_t x=0;x<GX521D_COLUMNS;x++) pixels[y*GX521D_COLUMNS+x]=out[x*GX521D_ROWS+y];
  return status;
}

int gx521d_quality_with_status(const uint8_t image[GX521D_PIXELS],int *quality,int *coverage,
                              int *quality_status)
{
  if (!image || !quality || !coverage || !quality_status) return -1;
  uint8_t cover[GX521D_PIXELS],valid[GX521D_PIXELS],refined[GX521D_PIXELS];
  int raw_coverage,original_valid,patch_score,fraction;
  if (goodix_milan_preprocess_quality_coverage_mask(image,GX521D_ROWS,GX521D_COLUMNS,
                                                  cover,&raw_coverage) ||
      goodix_milan_preprocess_quality_valid_mask(image,GX521D_ROWS,GX521D_COLUMNS,
                                                valid,&original_valid)) return -1;
  int propagated=goodix_milan_preprocess_quality_mask_coverage(valid,GX521D_PIXELS);
  if (quality_patch_score_core(image,valid,GX521D_ROWS,GX521D_COLUMNS,1,refined,&patch_score) ||
      quality_mask_fraction_core(image,refined,GX521D_ROWS,GX521D_COLUMNS,&fraction) ||
      quality_combine_core(raw_coverage,propagated,patch_score,fraction,35,0,1,quality,coverage)) return -1;
  /* RVA 0x4e640 returns the original patch score separately from the combined
   * output. RVA 0x4f31f tests that return, before the type17 bonus/penalty. */
  *quality_status=patch_score<45 ? 0x7532:0;
  /* Type17 owns the extra low-coverage penalty in 0x4f34e..0x4f37f.
   * Apply it before final clipping, preserving native integer ordering. */
  if (*quality>0) *quality+=7;
  if (*coverage<50) *quality=*quality * *coverage * *coverage /2500;
  if (*quality>100) *quality=100;
  return 0;
}

int gx521d_quality(const uint8_t image[GX521D_PIXELS],int *quality,int *coverage)
{
  int quality_status;
  return gx521d_quality_with_status(image,quality,coverage,&quality_status);
}

int gx521d_encode_metadata(uint8_t image[GX521D_PIXELS],
                          const uint8_t mask[GX521D_PIXELS],int mode,int apply_mask)
{
  if (!image || !mask) return -1;
  milan_profile9_encode_metadata(image,mask,GX521D_ROWS,GX521D_COLUMNS,mode,apply_mask);
  /* Reviewed default branch at 0x877f8 clears both low and high fields.
   * Upstream's default clears only the high field. Mode zero is a no-op. */
  if (mode!=0 && (mode<1 || mode>9)) {
    image[GX521D_COLUMNS-8]&=0xfe;
    image[GX521D_COLUMNS-7]&=0xfe;
  }
  return 0;
}

static void gx521d_center_columns(const uint16_t *source,const Gx521dMask *mask,uint16_t *out)
{
  for (int x=0;x<GX521D_COLUMNS;x++) {
    uint32_t sum=0,count=0;
    for (int y=0;y<GX521D_ROWS;y++) {
      int i=y*GX521D_COLUMNS+x;
      if (mask->percent>=96 || mask->pixels[i]) {sum+=source[i];count++;}
    }
    uint16_t mean=count ? (uint16_t)(sum/count):0;
    for (int y=0;y<GX521D_ROWS;y++) {
      int i=y*GX521D_COLUMNS+x;
      int value=mask->percent<96 && !mask->pixels[i] ? 5000:(int16_t)(source[i]-mean+5000);
      out[i]=(uint16_t)(value>0 ? value:0);
    }
  }
}
static int gx521d_post_disagreement(const uint16_t *source,const Gx521dMask *mask,
    int selected,Gx521dPostRender *out)
{
  uint16_t centered[GX521D_PIXELS];const uint16_t *diagnostic_source=source;
  if (selected) {gx521d_center_columns(source,mask,centered);diagnostic_source=centered;}
  uint8_t gradient[GX521D_PIXELS],residual[GX521D_PIXELS];int ignored=0;
  if (milan_profile9_diagnostic_contrast(diagnostic_source,mask->pixels,GX521D_ROWS,GX521D_COLUMNS,out->diagnostic) ||
      quality_coverage_mask_core(out->diagnostic,GX521D_ROWS,GX521D_COLUMNS,100,255,gradient,&ignored)) return -1;
  int source_count=0,missing=0;
  for (int i=0;i<GX521D_PIXELS;i++) {
    residual[i]=mask->pixels[i];
    if (mask->pixels[i]) {source_count++;if (!gradient[i]) missing++;else residual[i]=0;}
  }
  if (missing*100>source_count*20) {
    int patch=0,fraction=0;
    if (quality_patch_score_core(out->diagnostic,residual,GX521D_ROWS,GX521D_COLUMNS,1,NULL,&patch) ||
        quality_mask_fraction_core(out->diagnostic,residual,GX521D_ROWS,GX521D_COLUMNS,&fraction)) return -1;
    int score=patch;
    if (score<70) {
      if (fraction<20) {int factor=fraction*256/20;score=score*factor>>8;score=score*factor>>8;}
      if (fraction>=20) score+=15;
    }
    if (score<0) score=0;
    if (score>100) score=100;
    out->component_score=score;
    if (score<25) {out->component_flag=1;memcpy(out->component_mask,residual,sizeof(residual));}
  }
  out->disagreement=(out->component_flag ? missing:source_count)*100/GX521D_PIXELS;
  return 0;
}
int gx521d_post_render(Gx521dGainContext *context,const uint16_t live[GX521D_PIXELS],
    const uint16_t setup[GX521D_PIXELS],const Gx521dMask *mask,const Gx521dColdGain *before,
    const uint8_t selected_pixels[GX521D_PIXELS],const uint8_t selection_mask[GX521D_PIXELS],
    int selected,Gx521dPostRender *out,Gx521dColdGain *after)
{
  if (!context || !live || !setup || !mask || !before || !selected_pixels ||
      !selection_mask || !out || !after || mask->rows!=64 || mask->columns!=80 ||
      (selected!=0 && selected!=1)) return -1;
  memset(out,0,sizeof(*out));*after=*before;memcpy(out->image,selected_pixels,GX521D_PIXELS);
  out->primary_metric=200;out->fallback_metric=200;
  if (gx521d_post_disagreement(before->secondary_working,mask,selected,out)) return -1;
  if (selected ? gx521d_render_refined(before->secondary_working,mask,out->primary_check):
                 gx521d_render_primary(before->secondary_working,mask,out->primary_check)) return -1;
  const uint8_t *metric_mask=out->component_flag ? out->component_mask:selection_mask;
  out->primary_metric=milan_profile9_mean_absolute_difference(selected_pixels,out->primary_check,metric_mask,GX521D_PIXELS);
  out->quality_gate=out->primary_metric>55;
  if (!out->quality_gate && out->primary_metric>50) {
    if (milan_contrast_core(before->reciprocal,mask->pixels,64,80,out->fallback_check,0)) return -1;
    out->fallback_metric=milan_profile9_mean_absolute_difference(selected_pixels,out->fallback_check,metric_mask,GX521D_PIXELS);
    out->quality_gate=out->fallback_metric<75;
  }
  if (out->quality_gate) {
    out->mode=4;
    if (out->disagreement>40) {
      out->status=0x7531;out->mode=before->sample_count<100 && out->primary_metric>85 ? 3:2;
    }
    if (before->sample_count<100 && out->disagreement>90) {
      uint32_t saved=context->state.sample_count;context->state.sample_count=5;
      Gx521dColdGain nested;
      int status=gx521d_gain_step(context,live,setup,mask,&nested);
      context->state.sample_count=saved;if (status) return -1;
      *after=nested;after->sample_count=saved;
      /* The nested call has no caller-owned pair; those original scratch
       * outputs remain unchanged while the workspace working plane is updated. */
      memcpy(after->secondary_working,before->secondary_working,sizeof(after->secondary_working));
      memcpy(after->reciprocal,before->reciprocal,sizeof(after->reciprocal));
      out->nested_update=1;
    }
  }
  return gx521d_encode_metadata(out->image,metric_mask,out->mode,out->component_flag);
}

#include "521d-checkpoint.h"
int gx521d_gain_checkpoint(const Gx521dGainContext *c,Gx521dGainCheckpoint *out)
{
  if(!c||!out)return -1;
  memset(out,0,sizeof(*out));const GoodixMilanPreprocessState *s=&c->state;
  out->sample_count=s->sample_count;out->stable_count=s->stable_count;out->update_state=s->update_state;
  out->auxiliary_count=s->auxiliary_sample_count;out->ready=c->ready;out->initialized=s->application_gain_initialized;
  memcpy(out->calibration,s->calibration_map,sizeof(out->calibration));memcpy(out->application_gain,s->application_gain_map,sizeof(out->application_gain));
  memcpy(out->auxiliary_gain,s->auxiliary_gain_map,sizeof(out->auxiliary_gain));memcpy(out->secondary_gain,s->secondary_auxiliary_gain_map,sizeof(out->secondary_gain));
  memcpy(out->coarse_reference,s->coarse_reference,sizeof(out->coarse_reference));return 0;
}
int gx521d_gain_restore(Gx521dGainContext *c,const Gx521dGainCheckpoint *in)
{
  if(!c||!in||in->sample_count>400||in->initialized>1||in->ready>1)return -1;
  goodix_milan_preprocess_reset(&c->state);GoodixMilanPreprocessState *s=&c->state;
  s->sample_count=in->sample_count;s->stable_count=in->stable_count;s->update_state=in->update_state;
  s->auxiliary_sample_count=in->auxiliary_count;c->ready=in->ready;s->application_gain_initialized=(uint8_t)in->initialized;
  memcpy(s->calibration_map,in->calibration,sizeof(in->calibration));memcpy(s->application_gain_map,in->application_gain,sizeof(in->application_gain));
  memcpy(s->auxiliary_gain_map,in->auxiliary_gain,sizeof(in->auxiliary_gain));memcpy(s->secondary_auxiliary_gain_map,in->secondary_gain,sizeof(in->secondary_gain));
  memcpy(s->coarse_reference,in->coarse_reference,sizeof(in->coarse_reference));return 0;
}

#include "521d-feature.h"
int gx521d_feature_base(const uint8_t image[GX521D_PIXELS],Gx521dFeatureBase *out)
{
  if(!image||!out)return -1;
  memset(out,0,sizeof(*out));int ignored;
  if(quality_coverage_mask_core(image,64,80,110,1,out->validity,&ignored))return -1;
  uint16_t horizontal[GX521D_PIXELS];
  quality_gaussian_reflect101(image,64,80,milan_quality_gaussian6_kernel,3,horizontal,out->blurred);
  if(gx521d_feature_pack_bitmap(out->blurred,out->validity,out->high,&out->threshold))return -1;
  for(int i=0;i<GX521D_PIXELS;i++)if(out->blurred[i]>55)out->low[i/8]|=(uint8_t)(1U<<(i&7));
  return 0;
}
int gx521d_feature_pack_bitmap(const uint8_t image[GX521D_PIXELS],const uint8_t mask[GX521D_PIXELS],
    uint8_t bitmap[GX521D_FEATURE_BITMAP_BYTES],int32_t *selected)
{
  if(!image||!mask||!bitmap||!selected)return -1;
  memset(bitmap,0,GX521D_FEATURE_BITMAP_BYTES);
  uint32_t histogram[256]={0},active=0;
  for(int i=0;i<GX521D_PIXELS;i++)if(mask[i]){histogram[image[i]]++;active++;}
  uint32_t target=(205*active+128)>>8;int threshold=0;
  for(int v=1;v<256;v++) {
    histogram[v]+=histogram[v-1];
    if(histogram[v]>=target) {
      threshold=(int32_t)(histogram[v]-target)<=(int32_t)(target-histogram[v-1]) ? v:v-1;break;
    }
  }
  *selected=threshold;
  for(int i=0;i<GX521D_PIXELS;i++)if(image[i]>threshold)bitmap[i/8]|=(uint8_t)(1U<<(i&7));
  return 0;
}
