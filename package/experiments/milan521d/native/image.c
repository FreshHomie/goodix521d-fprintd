/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "521d-image.h"
#include <stdlib.h>
#include <string.h>
struct Gx521dImageContext {
  uint16_t setup[GX521D_PIXELS];
  Gx521dGainContext *gain;
  Gx521dClassifierContext classifier;
  int previous_selected;
};
int gx521d_image_context_new(const uint16_t background[GX521D_PIXELS],Gx521dImageContext **out)
{
  if (!background || !out) return -1;
  *out=NULL;uint16_t input[GX521D_PIXELS];memcpy(input,background,sizeof(input));
  int status=gx521d_prepare_input(input,1);if (status) return status;
  Gx521dImageContext *context=calloc(1,sizeof(*context));if (!context) return -1;
  uint32_t mean=0;
  if (gx521d_make_setup(input,context->setup,&mean) || !(context->gain=gx521d_gain_context_new())) {
    free(context);return -1;
  }
  gx521d_classifier_init(&context->classifier);*out=context;return 0;
}
void gx521d_image_context_free(Gx521dImageContext *context)
{
  if (context) {gx521d_gain_context_free(context->gain);free(context);}
}
int gx521d_image_step(Gx521dImageContext *context,const uint16_t raw[GX521D_PIXELS],
    Gx521dImageResult *out,Gx521dImageTrace *trace)
{
  if (!context || !raw || !out) return -1;
  memset(out,0,sizeof(*out));uint16_t live[GX521D_PIXELS];memcpy(live,raw,sizeof(live));
  int status=gx521d_prepare_input(live,0);if (status) return status;
  Gx521dMask initial,mask;Gx521dColdGain before,after;Gx521dPostRender post;
  uint8_t primary[GX521D_PIXELS],refined[GX521D_PIXELS],selection[GX521D_PIXELS],selected[GX521D_PIXELS];
  if (gx521d_make_mask(live,context->setup,&initial) || gx521d_prepare_render_mask(live,&initial,&mask) ||
      gx521d_gain_step(context->gain,live,context->setup,&initial,&before) ||
      gx521d_render_primary(before.working,&mask,primary) || gx521d_render_refined(before.working,&mask,refined) ||
      gx521d_selection_mask(primary,selection) || gx521d_select_image(primary,refined,selection,selected,&out->selected) ||
      gx521d_post_render(context->gain,live,context->setup,&mask,&before,selected,selection,out->selected,&post,&after)) return -1;
  memcpy(out->image,post.image,sizeof(out->image));out->post_status=post.status;
  Gx521dColdClassification classified;Gx521dClassificationDecision decision;
  /* Core RVA0x7a099 preserves the selection from entry for classifier output;
   * RVA0x7a5ce passes it after this frame's selection has already changed. */
  if (gx521d_classification_step(&context->classifier,live,context->setup,&mask,&after,context->previous_selected,&classified) ||
      gx521d_classification_publish(classified.classes,mask.count,classified.states,context->previous_selected,&decision) ||
      gx521d_encode_metadata(out->image,classified.classes,decision.mode,decision.apply_mask) ||
      gx521d_quality_with_status(out->image,&out->quality,&out->coverage,&out->quality_status)) return -1;
  memcpy(classified.auxiliary,decision.auxiliary_summary,3);
  out->classification_status=decision.status;out->mode=decision.mode;out->apply_mask=decision.apply_mask;
  context->previous_selected=out->selected;
  if (trace) {
    memcpy(trace->live,live,sizeof(live));memcpy(trace->setup,context->setup,sizeof(context->setup));
    trace->initial=initial;trace->mask=mask;trace->before=before;trace->after=after;trace->post=post;
    trace->classified=classified;trace->history=context->classifier.history;
  }
  return 0;
}

#include "521d-checkpoint.h"
int gx521d_image_checkpoint(const Gx521dImageContext *c,Gx521dImageCheckpoint *out)
{
  if(!c||!out)return -1;
  memset(out,0,sizeof(*out));out->version=1;out->rows=GX521D_ROWS;out->columns=GX521D_COLUMNS;
  out->previous_selected=c->previous_selected;memcpy(out->setup,c->setup,sizeof(out->setup));out->history=c->classifier.history;
  return gx521d_gain_checkpoint(c->gain,&out->gain);
}
int gx521d_image_restore(const Gx521dImageCheckpoint *in,Gx521dImageContext **out)
{
  if(!out)return -1;
  *out=NULL;
  if(!in||in->version!=1||in->rows!=GX521D_ROWS||in->columns!=GX521D_COLUMNS||
     in->previous_selected<0||in->previous_selected>1||in->history.count<0||in->history.count>50||
     in->history.mask_threshold<0||in->history.mask_threshold>100||in->history.mask_average<0||in->history.mask_average>100)return -1;
  Gx521dImageContext *c=calloc(1,sizeof(*c));if(!c)return -1;
  c->gain=gx521d_gain_context_new();
  if(!c->gain||gx521d_gain_restore(c->gain,&in->gain)){gx521d_image_context_free(c);return -1;}
  memcpy(c->setup,in->setup,sizeof(c->setup));c->previous_selected=in->previous_selected;c->classifier.history=in->history;
  *out=c;return 0;
}
void gx521d_image_reset_entry_selection(Gx521dImageContext *context)
{if(context)context->previous_selected=0;}
