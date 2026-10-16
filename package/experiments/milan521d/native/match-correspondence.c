/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 correspondence controller63f20, composed with validated native stages.
 * Forward mask overlap mechanism adapted from Milan overlap.c,
 * Copyright(C)2026 goodix-fp-linux-dev contributors. */
#include "521d-match-correspondence.h"
#include "521d-match-alternate.h"
#include "521d-match-pair-fit.h"
#include "521d-match-topology.h"
#include "milan/transform-private.h"
#include <stddef.h>
#include <string.h>
static int32_t signed32(uint32_t n){int32_t v;memcpy(&v,&n,4);return v;}
static int32_t get(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
static void set(uint8_t *p,int32_t v){memcpy(p,&v,4);}
static int position(const GoodixMilanFeatureRecord *a,const GoodixMilanFeatureRecord *b)
{return a->refined_x==b->refined_x&&a->refined_y==b->refined_y;}
static unsigned expanded_bit(const uint8_t *bits,unsigned x,unsigned y){unsigned i=(y/4)*20+x/4;return (bits[i/8]>>(i%8))&1;}
static unsigned forward_overlap(const uint8_t *probe,const uint8_t *enrolled,const int32_t affine[6])
{
 unsigned count=0;for(unsigned y=0;y<64;y++)for(unsigned x=0;x<80;x++)if(expanded_bit(probe,x,y)){
  int32_t px=goodix_milan_transform_affine_s32(affine[0],x,affine[1],y,(uint32_t)affine[2]+128)>>8;
  int32_t py=goodix_milan_transform_affine_s32(affine[3],x,affine[4],y,(uint32_t)affine[5]+128)>>8;
  if(px>=0&&px<80&&py>=0&&py<64&&expanded_bit(enrolled,(unsigned)px,(unsigned)py))count++;
 }
 return (count*256+2560)/5120;
}
static unsigned auxiliary_count(const Gx521dMatchDistanceInput *f,const Gx521dMatchPairs *pairs,const Gx521dMatchRefined *fit)
{
 unsigned n=0;uint8_t remaining[31];memcpy(remaining,fit->mask,31);
 for(unsigned i=0;i<31;i++){
  int a=pairs->pairs[1][i][0],b=pairs->pairs[1][i][1];if(a<0||b<0)continue;
  uint32_t x=(uint16_t)f->probe[b].refined_x,y=(uint16_t)f->probe[b].refined_y;
  int64_t px=((int64_t)fit->affine[0]*x+(int64_t)fit->affine[1]*y+128)>>8;
  int64_t py=((int64_t)fit->affine[3]*x+(int64_t)fit->affine[4]*y+128)>>8;
  uint32_t dx=(uint32_t)px+(uint32_t)fit->affine[2]-(uint16_t)f->enrolled[a].refined_x;
  uint32_t dy=(uint32_t)py+(uint32_t)fit->affine[5]-(uint16_t)f->enrolled[a].refined_y;
  if(signed32(dx*dx+dy*dy)>=0x64000)continue;
  n++;
  for(unsigned j=0;j<31;j++)if(remaining[j]==1&&pairs->pairs[0][j][1]>=0){
   const GoodixMilanFeatureRecord *p=&f->probe[pairs->pairs[0][j][1]];
   dx=x-(uint16_t)p->refined_x;dy=y-(uint16_t)p->refined_y;
   if(signed32(dx*dx+dy*dy)<0x400000)remaining[j]=0;
  }
 }
 for(unsigned i=0;i<31;i++)if(remaining[i]==1)n++;
 return n<31?n:31;
}
int gx521d_match_correspondence(const Gx521dMatchCorrespondenceInput *in,Gx521dMatchCorrespondence *out)
{
 if(!in||!out)return -1;
 Gx521dMatchPairs pairs;if(!gx521d_match_pairs(&in->input,&pairs))return -1;
 Gx521dMatchPairFitInput f;f.features=in->input.features;memcpy(f.pairs,pairs.pairs[0],248);f.minimum_inliers=3;
 Gx521dMatchRefined primary;if(!gx521d_match_refine_pairs(&f,&primary))return -1;
 Gx521dMatchCorrespondence result={0};memcpy(result.affine,primary.affine,24);memcpy(result.pairs,pairs.pairs[0],248);
 memcpy(result.candidate+0x54,primary.enrolled_mask,120);
 unsigned count=primary.retained,extra=0;
 if(count>6){extra=auxiliary_count(&f.features,&pairs,&primary);if(extra>count)count=extra;}
 set(result.candidate,(int32_t)count);set(result.candidate+4,(int32_t)count);set(result.candidate+12,(int32_t)extra);
 if(count>=1&&count<=15){
  Gx521dMatchAlternateInput scan;scan.features=f.features;int32_t config[5]={30,61,40,38,0};memcpy(scan.features.config,config,20);memcpy(scan.affine,result.affine,24);
  Gx521dMatchAlternate alternate;if(gx521d_match_alternate(&scan,&alternate))return -1;
  memcpy(f.pairs,alternate.pairs,248);f.minimum_inliers=2;Gx521dMatchRefined alt;
  if(!gx521d_match_refine_pairs(&f,&alt))return -1;
  if(alt.retained>count){
   Gx521dMatchTopologyInput topology;topology.input.features=f.features;memcpy(topology.input.affine,alt.affine,24);topology.filtered=alt.retained;memcpy(topology.enrolled_mask,alt.enrolled_mask,120);
   int32_t measurements[11];if(gx521d_match_topology(&topology,measurements))return -1;
   set(result.candidate+40,measurements[2]);set(result.candidate+44,measurements[3]);
   unsigned overlap=forward_overlap(in->probe_mask,in->enrolled_mask,result.affine);memcpy(result.affine,alt.affine,24);
   int t=measurements[2],g=measurements[3];
   if((overlap>=145&&t>=40)||(overlap>=105&&t>=45&&g>25)||(t>65&&g>50)||g>80||alt.retained>18){
    set(result.candidate+4,(int32_t)alt.retained);memcpy(result.candidate+0x54,alt.enrolled_mask,120);memcpy(result.candidate+0x108,measurements,44);
   }
  }
 }
 if(get(result.candidate)>3||get(result.candidate+4)>3){
  Gx521dMatchRecordMetricsInput records;records.features=f.features;memcpy(records.affine,result.affine,24);Gx521dMatchRecordMetrics metrics;
  if(gx521d_match_record_metrics(&records,&metrics))return -1;
  memcpy(result.candidate+0x108,metrics.values,44);set(result.candidate+40,metrics.values[2]);
  for(unsigned i=0;i<f.features.enrolled_count;i++)if(metrics.enrolled_mask[i]==1){
   int already_primary=primary.enrolled_mask[i]!=0,already_retained=result.candidate[0x54+i]!=0;
   for(unsigned j=0;j<f.features.enrolled_count;j++)if(position(&f.features.enrolled[i],&f.features.enrolled[j])){
    if(primary.enrolled_mask[j]==1)already_primary=1;
    if(result.candidate[0x54+j]==1)already_retained=1;
   }
   if(!already_primary)set(result.candidate,get(result.candidate)+1);
   if(!already_retained)set(result.candidate+4,get(result.candidate+4)+1);
  }
  if(metrics.values[1]>0&&metrics.values[4]<5){set(result.candidate,get(result.candidate)+1);set(result.candidate+4,get(result.candidate+4)+1);}
 }
 *out=result;return 0;
}
