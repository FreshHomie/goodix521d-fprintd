/* SPDX-License-Identifier: LGPL-2.1-or-later; transient probe owner66c70. */
#include "521d-probe-cache.h"
#include "521d-retained-copy.h"
#include "521d-match-overlap.h"
#include <stdlib.h>
#include <string.h>
void gx521d_probe_cache_init(Gx521dProbeCache *cache)
{if(cache){memset(cache,0,sizeof(*cache));for(unsigned i=0;i<20;i++)cache->ages[i]=-1;}}
int gx521d_probe_cache_add(Gx521dProbeCache *owned,const Gx521dFeatureObject *feature,int retained_flag)
{
 if(!owned||!feature||(retained_flag!=0&&retained_flag!=1))return -1;
 int32_t maximum=-100,minimum=21;int newest=-1,destination=-1;
 for(unsigned i=0;i<20;i++){
  if(owned->ages[i]<-1||owned->ages[i]>=20)return -1;
  if(owned->ages[i]>maximum){maximum=owned->ages[i];newest=(int)i;}
  if(owned->ages[i]<minimum){minimum=owned->ages[i];destination=(int)i;}
 }
 if(destination<0||newest<0)return -1;
 /* Validate source even on the no-copy duplicate path. */
 Gx521dFeatureObject copy=owned->features[destination];if(gx521d_retained_copy(feature,&copy))return -1;
 if(!retained_flag&&maximum!=-1){
  const Gx521dFeatureObject *previous=&owned->features[newest];Gx521dMatchOverlapInput input={.affine={256,0,0,0,256,0},.enhanced=1,.context_count=10};Gx521dMatchOverlap overlap;
  memcpy(input.probe_high,feature->high,640);memcpy(input.probe_low,feature->low,640);memcpy(input.probe_mask,feature->scalar_header+40,40);
  memcpy(input.enrolled_high,previous->high,640);memcpy(input.enrolled_low,previous->low,640);memcpy(input.enrolled_mask,previous->scalar_header+40,40);
  if(gx521d_match_overlap(&input,&overlap))return -1;
  if(overlap.score>190)return 0;
 }
 owned->features[destination]=copy;
 if(maximum==19){for(unsigned i=0;i<20;i++)if(owned->ages[i]!=-1)owned->ages[i]--;maximum--;}
 owned->ages[destination]=maximum+1;return 0;
}
