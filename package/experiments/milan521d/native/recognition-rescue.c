/* SPDX-License-Identifier: LGPL-2.1-or-later; type17 cached rescue64590. */
#include "521d-recognition-cached.h"
#include "521d-match-candidate.h"
#include "521d-match-policy.h"
#include "521d-match-secondary-policy.h"
#include <stdlib.h>
#include <string.h>
extern int gx521d_match_candidate_rescue(const Gx521dMatchCandidateInput *,Gx521dMatchCandidate *,int *);
int gx521d_recognition_rescue(const Gx521dProbeCache *cache,const Gx521dFeatureObject *probe,uint32_t selector,int flags,int32_t *decision)
{
 if(!cache||!probe||!decision)return -1;
 Gx521dMatchCandidateInput *in=malloc(sizeof(*in));if(!in)return -1;
 in->probe=*probe;in->selector=selector;in->flags=flags;
 int32_t result=0;int counter=10,retry_counter=3,retry_rejected=0,rejections=0,accepted=0;int code=-1;
 for(unsigned i=0;i<20;i++){
  if(cache->ages[i]<-1||cache->ages[i]>19)goto done;
  if(cache->ages[i]==-1)continue;
  int32_t records;memcpy(&records,cache->features[i].scalar_header+0xf0,4);if(!records)continue;
  in->enrolled=cache->features[i];Gx521dMatchCandidate candidate;int progress=0;
  if(gx521d_match_candidate_rescue(in,&candidate,&progress))goto done;
  int32_t m[24]={0};if(progress)m[23]=progress-1;
  if(candidate.measured){
   memcpy(m,candidate.metrics,sizeof(m));Gx521dMatchPolicyInput pi={.mode=1,.counter=counter};memcpy(pi.metrics,m,sizeof(m));Gx521dMatchPolicy policy;
   if(gx521d_match_policy(&pi,&policy))goto done;
   counter=policy.counter;accepted=policy.possible;
   Gx521dMatchSecondaryPolicyInput secondary={.accepted=accepted,.possible=policy.accepted,.retry_counter=retry_counter,.retry_rejected=retry_rejected,.rejection_count=rejections};
   memcpy(&secondary.probe_records,probe->scalar_header+0xf0,4);memcpy(secondary.metrics,m,sizeof(m));Gx521dMatchSecondaryPolicy late;
   if(gx521d_match_secondary_policy(&secondary,&late))goto done;
   accepted=late.accepted;retry_counter=late.retry_counter;retry_rejected=late.retry_rejected;rejections=late.rejection_count;
   if(accepted==1){result=501;break;}
  }
  if(!accepted&&m[23]>0){
   /* For C7zero both classifier status pairs are zero. Mode3's residual
    * check is unconditional; mode1/2 require nonzero class counts. */
   if(m[23]==3&&(m[4]<205||m[7]>350))rejections++;
  }
 }
 if(result<1&&retry_counter<1)result=702;
 if(result>0&&rejections>3)result=-702;
 *decision=result;code=0;
 done:memset(in,0,sizeof(*in));free(in);return code;
}
