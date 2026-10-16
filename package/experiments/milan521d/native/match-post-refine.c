/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 post-admission owner89ac0, composed from separately validated stages. */
#include "521d-match-post-refine.h"
#include <stddef.h>
#include <string.h>
static int32_t get(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
static void set(uint8_t *p,int32_t v){memcpy(p,&v,4);}
int gx521d_match_post_refine(const Gx521dMatchPostRefineInput *in,Gx521dMatchPostRefine *out)
{
 if(!in||!out||in->image.mode!=1||in->image.interpolation||in->image.enhanced!=1)return -1;
 Gx521dMatchPostRefine result;memcpy(result.affine,in->image.affine,24);memcpy(result.candidate,in->candidate,sizeof(result.candidate));
 Gx521dMatchIndexRefitInput fit;fit.features=in->features;fit.radius=2;memcpy(fit.affine,in->image.affine,24);
 int32_t refined[6];if(gx521d_match_index_refit(&fit,refined))return -1;
 Gx521dMatchOverlapInput image=in->image;memcpy(image.affine,refined,24);Gx521dMatchOverlap overlap;
 if(gx521d_match_overlap_diagnostic(&image,&overlap))return -1;
 if(overlap.score!=128&&(get(result.candidate+16)==128||overlap.detail>get(result.candidate+20))){
  set(result.candidate+16,overlap.score);set(result.candidate+20,overlap.detail);
  uint32_t packed=((uint32_t)overlap.reserved<<16)+(uint32_t)overlap.coverage;memcpy(result.candidate+36,&packed,4);
  memcpy(result.affine,refined,24);
  Gx521dMatchRecordMetricsInput records;records.features=in->features;memcpy(records.affine,refined,24);Gx521dMatchRecordMetrics metrics;
  if(gx521d_match_record_metrics(&records,&metrics))return -1;
  memcpy(result.candidate+0x108,metrics.values,44);
  for(unsigned i=0;i<120;i++)if(metrics.enrolled_mask[i])result.candidate[0x54+i]=1;
  set(result.candidate+40,metrics.values[2]);set(result.candidate+44,metrics.values[3]);
 }
 *out=result;return 0;
}
