/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 cross-feature recognition owner60eb0. Transform helpers adapted from
 * Milan transform-private.h, Copyright(C)2026 goodix-fp-linux-dev contributors. */
#include "521d-match-aggregate.h"
#include "521d-match-novelty.h"
#include "521d-match-policy.h"
#include "milan/relations.h"
#include <string.h>
#include <stddef.h>
static int possible(const int32_t metrics[24],int *out)
{
 Gx521dMatchPolicyInput p={.mode=1,.counter=10};memcpy(p.metrics,metrics,96);Gx521dMatchPolicy r;
 if(gx521d_match_policy(&p,&r))return -1;
 *out=r.possible;return 0;
}
int gx521d_match_aggregate(const Gx521dMatchAggregateInput *in,int32_t *out)
{
 if(!in||!out||in->count>50||(in->active_match!=0&&in->active_match!=1)||(in->possible_match!=0&&in->possible_match!=1))return -1;
 if(in->active_match&&in->reference_feature>=in->count)return -1;
 for(unsigned i=0;i<6;i++)if(in->root_affine[i]<-1048576||in->root_affine[i]>1048576)return -1;
 for(unsigned i=0;i<in->count;i++){
  if(in->flags[i]>2||in->metadata[i][0]<0||in->metadata[i][0]>1||in->metadata[i][1]<0||in->metadata[i][1]>100||in->metadata[i][2]<0||in->metadata[i][2]+(int32_t)i>1226)return -1;
  for(unsigned k=0;k<24;k++)if(in->metrics[i][k]<-2048||in->metrics[i][k]>60000)return -1;
 }
 if(!in->active_match){*out=1;return 0;}
 int result=0;
 for(unsigned i=0;i<in->count;i++){
  if(in->flags[i]!=2||(in->metrics[i][0]<15&&in->metrics[i][4]<200))continue;
  int32_t current[24];memcpy(current,in->metrics[i],96);unsigned examined=0,consistent=0;
  for(unsigned j=0;j<in->count;j++){
   if(j==i||!in->flags[j])continue;
   if(in->metadata[j][0]!=1)continue;
   int32_t affine[6];unsigned root=in->reference_feature;
   if(j==root)memcpy(affine,in->root_affine,24);
   else{
    int32_t relation[6];
    if(j<root)memcpy(relation,in->relations[in->metadata[root][2]+j]+1,24);
    else if(goodix_milan_transform_invert(in->relations[in->metadata[j][2]+root]+1,relation))return -1;
    goodix_milan_transform_compose(relation,in->root_affine,affine);
   }
   int32_t novel[3];if(gx521d_match_novelty(affine,novel))return -1;
   int coverage=100-novel[0]*100/64/80;
   int penalty=0,strong=0;
   if(coverage>50&&in->metadata[j][1]>80&&in->metrics[j][9]>75){penalty=in->possible_match?5:10;strong=1;}
   else if(coverage>45&&in->metadata[j][1]>70&&in->metrics[j][9]>70)penalty=in->possible_match?8:15;
   if(!penalty)continue;
   examined++;int32_t other[24];memcpy(other,in->metrics[j],96);other[4]+=penalty;other[2]+=penalty;other[7]=0;
   int allowed;if(possible(other,&allowed))return -1;
   if(allowed)consistent++;else if(strong){current[4]-=4;current[2]-=4;}
  }
  if(examined&&consistent*2<examined){current[4]-=4;current[2]-=4;}
  int allowed;if(possible(current,&allowed))return -1;
  if(allowed==1)result=1;
 }
 *out=result;return 0;
}
