/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 alternate correspondence owner91080/91cd0.
 * Spatial scan/reduction adapted from Milan correspondence.c,
 * Copyright (C)2026 goodix-fp-linux-dev contributors. */
#include "521d-match-alternate.h"
#include "milan/transform-private.h"
#include <string.h>
static int32_t
milan_match_scan_raw_s32 (int32_t first,
                          uint32_t first_coordinate,
                          int32_t second,
                          uint32_t second_coordinate,
                          int32_t translation)
{
  return goodix_milan_transform_affine_s32 (
    first, first_coordinate, second, second_coordinate,
    (uint32_t) translation << 8);
}

static int32_t
milan_match_scan_round_pixel_s32 (int32_t value)
{
  int32_t rounded_q8 = goodix_milan_transform_sar32 (
    (uint32_t) value + UINT32_C (0x80), 8);

  return goodix_milan_transform_sar32 (
    (uint32_t) rounded_q8 + UINT32_C (0x80), 8);
}

static int
milan_match_scan_residual_within (int32_t mapped,
                                  uint32_t expected)
{
  int32_t difference = goodix_milan_transform_s32 (
    (uint32_t) mapped - expected);

  if (difference <= 0)
    difference = goodix_milan_transform_s32 (
      expected - (uint32_t) mapped);
  return difference <= 0x1500;
}

typedef struct {int32_t enrolled,probe,best,second;} Pair;
static void select_pairs(const GoodixMilanFeatureRecord *coordinates,unsigned count,
                         const int32_t distance[120][2],const int32_t index[120][2],
                         unsigned limit,const int32_t config[5],int32_t out[31][2])
{
 Pair pairs[31];unsigned used=0;
 for(unsigned row=0;row<count;row++){
  int best=index[row][0];if(best<0||distance[row][0]*config[2]>=distance[row][1]*config[3])continue;
  Pair candidate={(int32_t)row,best,distance[row][0],distance[row][1]};
  uint32_t x=(uint16_t)coordinates[best].refined_x,y=(uint16_t)coordinates[best].refined_y;
  int replaced=0;
  for(unsigned i=0;i<used;i++){
   const GoodixMilanFeatureRecord *selected=&coordinates[pairs[i].probe];
   uint32_t dx=x-(uint16_t)selected->refined_x,dy=y-(uint16_t)selected->refined_y;
   uint32_t bits=dx*dx+dy*dy;int32_t square;memcpy(&square,&bits,4);
   if(square>=65536)continue;
   if(candidate.best>=pairs[i].best){if(!replaced){replaced=-1;break;}continue;}
   if(!replaced){pairs[i]=candidate;replaced=1;}
   else {pairs[i]=pairs[used-1];used--;}
  }
  if(replaced)continue;
  if(used==limit){
   unsigned worst=0;for(unsigned i=1;i<used;i++)if(pairs[i].best>pairs[worst].best)worst=i;
   if(candidate.best<pairs[worst].best)pairs[worst]=candidate;
  }else pairs[used++]=candidate;
 }
 for(unsigned i=0;i<used;i++){out[i][0]=pairs[i].enrolled;out[i][1]=pairs[i].probe;}
}

static int hamming(const GoodixMilanFeatureRecord *a,const GoodixMilanFeatureRecord *b,unsigned begin,unsigned end)
{
 int n=0;for(unsigned i=begin;i<end;i++)n+=__builtin_popcount((unsigned)(((const uint8_t *)a)[i]^((const uint8_t *)b)[i]));return n;
}
int gx521d_match_alternate(const Gx521dMatchAlternateInput *in,Gx521dMatchAlternate *out)
{
 if(!in||!out)return -1;
 const Gx521dMatchDistanceInput *f=&in->features;
 if(f->enrolled_count>120||f->probe_count>120||f->enrolled_partition>f->enrolled_count||f->probe_partition>f->probe_count||f->config[0]<-1||f->config[0]>64||f->config[1]<-1||f->config[1]>128||f->config[2]<0||f->config[2]>1000||f->config[3]<0||f->config[3]>1000)return -1;
 for(unsigned i=0;i<6;i++)if(in->affine[i]<-1048576||in->affine[i]>1048576)return -1;
 int32_t inverse[6];if(goodix_milan_transform_invert_s32_checked(in->affine,inverse))return -1;
 int32_t distance[120][2],index[120][2];
 for(unsigned i=0;i<120;i++)for(unsigned j=0;j<2;j++){distance[i][j]=192;index[i][j]=-1;}
 for(unsigned i=0;i<f->enrolled_count;i++){
  uint32_t x=(uint16_t)f->enrolled[i].refined_x,y=(uint16_t)f->enrolled[i].refined_y;
  int32_t ix=milan_match_scan_round_pixel_s32(milan_match_scan_raw_s32(inverse[0],x,inverse[1],y,inverse[2]));
  int32_t iy=milan_match_scan_round_pixel_s32(milan_match_scan_raw_s32(inverse[3],x,inverse[4],y,inverse[5]));
  if(ix<=5||ix>=75||iy<=5||iy>=59)continue;
  unsigned begin=i<f->enrolled_partition?0:f->probe_partition,end=i<f->enrolled_partition?f->probe_partition:f->probe_count;
  for(unsigned j=begin;j<end;j++){
   uint32_t px=(uint16_t)f->probe[j].refined_x,py=(uint16_t)f->probe[j].refined_y;
   int32_t rx=milan_match_scan_raw_s32(in->affine[0],px,in->affine[1],py,in->affine[2]);
   int32_t ry=milan_match_scan_raw_s32(in->affine[3],px,in->affine[4],py,in->affine[5]);
   int32_t qx=goodix_milan_transform_sar32((uint32_t)rx,8),qy=goodix_milan_transform_sar32((uint32_t)ry,8);
   int32_t bx=milan_match_scan_round_pixel_s32(rx),by=milan_match_scan_round_pixel_s32(ry);
   if(!milan_match_scan_residual_within(qx,x)||!milan_match_scan_residual_within(qy,y)||bx<=5||bx>75||by<=5||by>59)continue;
   int d=hamming(&f->enrolled[i],&f->probe[j],16,24);if(d>f->config[0])continue;
   d+=hamming(&f->enrolled[i],&f->probe[j],24,32);if(d>f->config[1])continue;
   d+=hamming(&f->enrolled[i],&f->probe[j],32,40);
   if(d<distance[i][0]){distance[i][1]=distance[i][0];index[i][1]=index[i][0];distance[i][0]=d;index[i][0]=(int32_t)j;}
   else if(d<distance[i][1]){distance[i][1]=d;index[i][1]=(int32_t)j;}
  }
 }
 Gx521dMatchAlternate result;memset(&result,255,sizeof(result));select_pairs(f->probe,f->enrolled_count,distance,index,31,f->config,result.pairs);
 *out=result;return 0;
}
