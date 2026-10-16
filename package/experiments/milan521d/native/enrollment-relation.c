/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Affine solve/validation adapted from Milan match/geometry.c,
 * Copyright(C)2026 goodix-fp-linux-dev contributors.
 * Type17 enrollment8f780/90ce0 and55c50 relation gate. */
#include "521d-enrollment-relation.h"
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
enum {MILAN_AFFINE_SOLVE_SCALE=1024,MILAN_AFFINE_SOLVE_SHIFT=10};
static void
milan_affine_from_three_points (const int32_t source[6],
                                const int32_t target[6],
                                int32_t       affine[6])
{
  int64_t determinant;
  int64_t raw_a;
  int64_t raw_b;
  int64_t raw_c;
  int64_t raw_d;

  determinant =
    (int64_t) (source[0] - source[2]) * (source[3] - source[5]) -
    (int64_t) (source[2] - source[4]) * (source[1] - source[3]);
  if (determinant == 0)
    {
      raw_a = INT32_MAX;
      raw_b = INT32_MAX;
    }
  else
    {
      raw_a =
        ((int64_t) (target[0] - target[2]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[3] - source[5]) -
         (int64_t) (target[2] - target[4]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[1] - source[3])) /
        determinant;
      raw_b =
        ((int64_t) (target[0] - target[2]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[2] - source[4]) -
         (int64_t) (target[2] - target[4]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[0] - source[2])) /
        -determinant;
    }
  raw_a = (int32_t) raw_a;
  raw_b = (int32_t) raw_b;
  affine[0] = (int32_t) raw_a >> 2;
  affine[1] = (int32_t) raw_b >> 2;
  affine[2] = (int32_t) ((((int64_t) target[0] << MILAN_AFFINE_SOLVE_SHIFT) -
                           raw_b * source[1] - raw_a * source[0]) >>
                          MILAN_AFFINE_SOLVE_SHIFT);

  determinant =
    (int64_t) (source[2] - source[0]) * (source[5] - source[1]) -
    (int64_t) (source[4] - source[0]) * (source[3] - source[1]);
  if (determinant == 0)
    {
      raw_c = INT32_MAX;
      raw_d = INT32_MAX;
    }
  else
    {
      raw_c =
        ((int64_t) (target[3] - target[1]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[5] - source[1]) -
         (int64_t) (target[5] - target[1]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[3] - source[1])) /
        determinant;
      raw_d =
        ((int64_t) (target[3] - target[1]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[4] - source[0]) -
         (int64_t) (target[5] - target[1]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[2] - source[0])) /
        -determinant;
    }
  raw_c = (int32_t) raw_c;
  raw_d = (int32_t) raw_d;
  affine[3] = (int32_t) raw_c >> 2;
  affine[4] = (int32_t) raw_d >> 2;
  affine[5] = (int32_t) ((((int64_t) target[5] << MILAN_AFFINE_SOLVE_SHIFT) -
                           raw_d * source[5] - raw_c * source[4]) >>
                          MILAN_AFFINE_SOLVE_SHIFT);
}

static int
milan_affine_is_valid (const int32_t affine[6])
{
  int64_t a = affine[0];
  int64_t b = affine[1];
  int64_t c = affine[3];
  int64_t d = affine[4];
  /* Native sign-extends each wrapped dword product before the qword sum. */
  int64_t first = (int64_t) (int32_t) (a * a) + (int32_t) (b * b);
  int64_t cross = (int64_t) (int32_t) (a * c) + (int32_t) (b * d);
  int64_t second = (int64_t) (int32_t) (c * c) + (int32_t) (d * d);
  int64_t trace = first + second;
  int64_t discriminant = (int64_t) (
    (uint64_t) trace * (uint64_t) trace +
    UINT64_C (4) * ((uint64_t) cross * (uint64_t) cross -
                    (uint64_t) first * (uint64_t) second));
  int64_t lower = ((int64_t) 163 << 9) - trace;
  int64_t upper = ((int64_t) 401 << 9) - trace;

  return discriminant >= 0 && trace >= 0 && lower < 1 && upper >= 0 &&
         lower * lower != discriminant && discriminant <= lower * lower &&
         upper * upper != discriminant && discriminant <= upper * upper;
}


static int32_t word(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
int gx521d_enrollment_relation(const Gx521dMatchCandidateInput *in,Gx521dEnrollmentRelation *out)
{
 if(!in||!out)return -1;
 const Gx521dFeatureObject *a=&in->enrolled,*b=&in->probe;
 const uint8_t *ah=a->scalar_header,*bh=b->scalar_header;
 if(word(ah)!=80||word(ah+4)!=64||word(bh)!=80||word(bh+4)!=64||word(ah+0xf0)<0||word(ah+0xf0)>120||word(bh+0xf0)<0||word(bh+0xf0)>120||word(ah+0x150)||word(bh+0x150))return -1;
 Gx521dMatchPairsInput pairs={0};pairs.enrolled_coverage=pairs.probe_coverage=100;
 pairs.features.enrolled_count=(uint32_t)word(ah+0xf0);pairs.features.probe_count=(uint32_t)word(bh+0xf0);
 int32_t config[5]={64,128,40,38,0};memcpy(pairs.features.config,config,20);
 memcpy(pairs.features.enrolled,a->records,sizeof(a->records));memcpy(pairs.features.probe,b->records,sizeof(b->records));
 Gx521dMatchPairs selected;if(!gx521d_match_pairs(&pairs,&selected))return -1;
 Gx521dEnrollmentRelation r={0};memset(r.pairs,255,sizeof(r.pairs));memcpy(r.pairs,selected.pairs[0],sizeof(selected.pairs[0]));
 static const int32_t identity[6]={256,0,0,0,256,0};r.relation[0]=-1;memcpy(r.relation+1,identity,24);
 unsigned count=0;while(count<31&&r.pairs[count][0]>=0)count++;
 if(count<=4){*out=r;return 0;}
 r.geometry.residual=0x190000;
 for(unsigned first=0;first+2<count;first++)for(unsigned second=first+1;second+1<count;second++)for(unsigned third=second+1;third<count;third++){
  unsigned which[3]={first,second,third};int32_t source[6],target[6],affine[6];
  for(unsigned k=0;k<3;k++){
   const GoodixMilanFeatureRecord *ar=&a->records[r.pairs[which[k]][0]],*br=&b->records[r.pairs[which[k]][1]];
   source[2*k]=(uint16_t)br->refined_x;source[2*k+1]=(uint16_t)br->refined_y;target[2*k]=(uint16_t)ar->refined_x;target[2*k+1]=(uint16_t)ar->refined_y;
  }
  milan_affine_from_three_points(source,target,affine);int32_t inliers=0,sum=0;uint8_t mask[44]={0};
  for(unsigned k=0;k<count;k++){
   const GoodixMilanFeatureRecord *ar=&a->records[r.pairs[k][0]],*br=&b->records[r.pairs[k][1]];
   int64_t x=(((int64_t)affine[0]*(uint16_t)br->refined_x+(int64_t)affine[1]*(uint16_t)br->refined_y)>>8)+affine[2];
   int64_t y=(((int64_t)affine[3]*(uint16_t)br->refined_x+(int64_t)affine[4]*(uint16_t)br->refined_y)>>8)+affine[5];
   if(x<=-INT64_C(0x100000000)||x>=INT64_C(0x100000000)||y<=-INT64_C(0x100000000)||y>=INT64_C(0x100000000))continue;
   int64_t dx=x-(uint16_t)ar->refined_x,dy=y-(uint16_t)ar->refined_y;if(dx<-640||dx>640||dy<-640||dy>640)continue;
   int64_t squared=dx*dx+dy*dy;if(squared<0x64000){mask[k]=1;inliers++;sum+=(int32_t)squared;}
  }
  int32_t residual=inliers?(sum+inliers/2)/inliers:0x190000;
  if((inliers>r.inliers||(inliers==r.inliers&&residual<r.geometry.residual))&&milan_affine_is_valid(affine)){
   r.inliers=inliers;r.geometry.residual=residual;memcpy(r.geometry.affine,affine,24);memcpy(r.geometry.mask,mask,44);
  }
  if(r.inliers>20)goto fitted;
 }
 fitted:
 if(r.inliers>4){
  Gx521dMatchOverlapInput image={0};image.enhanced=1;memcpy(image.affine,r.geometry.affine,24);
  memcpy(image.enrolled_high,a->high,640);memcpy(image.enrolled_low,a->low,640);memcpy(image.enrolled_mask,ah+40,40);
  memcpy(image.probe_high,b->high,640);memcpy(image.probe_low,b->low,640);memcpy(image.probe_mask,bh+40,40);
  if(gx521d_match_overlap(&image,&r.overlap))return -1;
 }
 r.admitted=r.inliers>10||(r.inliers>5&&r.overlap.detail>215)||(r.inliers>6&&r.overlap.detail>208&&r.overlap.coverage>64);
 if(r.admitted){r.relation[0]=r.inliers;memcpy(r.relation+1,r.geometry.affine,24);}
 *out=r;return 0;
}
