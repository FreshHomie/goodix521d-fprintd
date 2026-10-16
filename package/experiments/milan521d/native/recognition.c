/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 recognition dispatcher62600 and enclosing controller62a00.
 * Transform helpers(C)2026 goodix-fp-linux-dev contributors. */
#include "521d-recognition-cached.h"
#include "521d-match-candidate.h"
#include "521d-match-controller-state.h"
#include "521d-match-policy.h"
#include "521d-match-secondary-policy.h"
#include "521d-match-aggregate.h"
#include "milan/relations.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
static int32_t word(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
static void put(uint8_t *p,int32_t v){memcpy(p,&v,4);}
static const int32_t identity[6]={256,0,0,0,256,0};
static int route(const Gx521dTemplate *t,unsigned feature,const int32_t affine[6],int32_t out[6])
{
 unsigned root=(unsigned)t->stored.graph[0];if(root>=t->stored.header[4])return -1;
 if(feature==root){memcpy(out,affine,24);return 0;}
 int32_t rel[6];
 if(feature>root)memcpy(rel,t->relations[t->stored.features[feature].scalars[1]+root]+1,24);
 else if(goodix_milan_transform_invert(t->relations[t->stored.features[root].scalars[1]+feature]+1,rel))return -1;
 goodix_milan_transform_compose(rel,affine,out);return 0;
}
static void reorder(Gx521dTemplate *t)
{
 unsigned n=t->stored.header[4];uint32_t order[50];memcpy(order,t->stored.tail,200);
 for(unsigned i=0;i+1<n;i++){
  unsigned best=i;
  for(unsigned j=i+1;j<n;j++){
   const uint8_t *a=t->features[order[best]].scalar_header,*b=t->features[order[j]].scalar_header;
   if(word(b+0x110)>word(a+0x110)||(word(b+0x110)==word(a+0x110)&&(word(b+0x134)<word(a+0x134)||(word(b+0x134)==word(a+0x134)&&word(b+0x12c)<word(a+0x12c)))))best=j;
  }
  unsigned v=order[i];order[i]=order[best];order[best]=v;
 }
 memcpy(t->stored.tail,order,200);
}
extern int gx521d_recognition_rescue(const Gx521dProbeCache *,const Gx521dFeatureObject *,uint32_t,int,int32_t *);
static int recognize(Gx521dTemplate *owned,Gx521dProbeCache *cache,const Gx521dFeatureObject *probe,uint32_t selector,int flags,Gx521dRecognition *out,int dispatched,int trigger)
{
 if(!owned||!probe||!out||flags<0||flags>2)return -1;
 unsigned n=owned->stored.header[4];if(!n||n>50)return -1;
 if(owned->stored.header[11]>1||(owned->stored.header[11]&&!cache)||word(probe->scalar_header+0x150))return -2;
 uint8_t seen[50]={0};for(unsigned i=0;i<n;i++){int k=word(owned->stored.tail+4*i);if(k<0||(unsigned)k>=n||seen[k])return -1;seen[k]=1;}
 Gx521dMatchControllerStateInput ci={.affine={256,0,0,0,256,0},.selector=selector,.flags=flags};Gx521dMatchControllerState cs;
 if(gx521d_match_controller_state(&ci,&cs)||cs.status)return -1;
 if(!dispatched){if(trigger<0||(unsigned)trigger>=n)return -1;cs.config[13]=0;cs.config[14]=trigger;}
 Gx521dTemplate *t=malloc(sizeof(*t));Gx521dMatchCandidateInput *input=malloc(sizeof(*input));Gx521dMatchAggregateInput *aggregate=calloc(1,sizeof(*aggregate));
 if(!t||!input||!aggregate){free(t);free(input);free(aggregate);return -1;}*t=*owned;
 int retry_gate=1;
 Gx521dRecognition r={0};put(r.workspace+0x648,-1);memcpy(r.workspace+0x650,identity,24);memcpy(r.workspace+0x66c,identity,24);
 input->probe=*probe;input->selector=selector;input->flags=flags;
 for(unsigned i=0;dispatched&&i<n;i++)if(word(t->features[i].scalar_header+0x124)==5){put(t->features[i].scalar_header+0x124,0);t->stored.features[i].scalars[5]=0;}
 int failure=-1;
 if(!word(probe->scalar_header+0xf0)){r.status=0x80000006;goto publish;}
 aggregate->count=n;aggregate->reference_feature=(uint32_t)t->stored.graph[0];memcpy(aggregate->relations,t->relations,sizeof(aggregate->relations));
 int counter=2,retry_rejected=0,rejections=0,last_progress=0,accepted_count=0,accepted=0,possible=0,max_auxiliary=0;
 for(unsigned position=0;position<n;position++){
  unsigned index=(unsigned)word(t->stored.tail+4*position);uint8_t *header=t->features[index].scalar_header;
  if(!word(header+0xf0))continue;
  if(!cs.config[13]&&(int32_t)index!=cs.config[14]){put(r.workspace+32*index+28,-2);continue;}
  if(word(r.workspace+0x688)==1&&word(r.workspace+0x644)==1&&word(header+0x110)==1)continue;
  aggregate->flags[index]=1;int32_t *metrics=aggregate->metrics[index];
  metrics[8]=word(header+0x11c);metrics[9]=word(probe->scalar_header+0x11c);metrics[10]=word(header+0x120);metrics[11]=word(probe->scalar_header+0x120);
  input->enrolled=t->features[index];Gx521dMatchCandidate candidate;int progress=0;
  if(gx521d_match_candidate_progress(input,&candidate,&progress))goto done;
  if(progress){last_progress=1;metrics[23]=progress-1;}
  if(candidate.measured){
   memcpy(metrics,candidate.metrics,96);if(word(candidate.candidate+12)>max_auxiliary)max_auxiliary=word(candidate.candidate+12);Gx521dMatchPolicyInput policy={.mode=cs.config[4],.counter=counter};memcpy(policy.metrics,metrics,96);Gx521dMatchPolicy decision;
   if(gx521d_match_policy(&policy,&decision))goto done;
   counter=decision.counter;/* Owner62a00 admits the eligibility output; the counter-gated output
    * is its stronger support flag (the isolated policy ABI names differ). */
   accepted=decision.possible;possible=decision.accepted;
   if(accepted){
    Gx521dMatchSecondaryPolicyInput late={.accepted=accepted,.possible=possible,.retry_counter=2,.retry_rejected=retry_rejected,.rejection_count=rejections,.probe_records=(uint32_t)word(probe->scalar_header+0xf0)};memcpy(late.metrics,metrics,96);Gx521dMatchSecondaryPolicy second;
    if(gx521d_match_secondary_policy(&late,&second))goto done;
    accepted=second.accepted;possible=second.possible;retry_rejected=second.retry_rejected;rejections=second.rejection_count;
   }
   put(r.workspace+32*index+28,-2);
   if(accepted){
    int32_t primary=word(candidate.candidate);put(r.workspace+32*index+28,primary);memcpy(r.workspace+32*index+4,candidate.affine,24);
    accepted_count++;r.decision=101;uint32_t hits=(uint32_t)word(t->stored.tail+0x50c)+1;put(t->stored.tail+0x50c,(int32_t)hits);
    uint32_t used=(uint32_t)word(header+0x134)+1;put(header+0x134,(int32_t)used);t->stored.features[index].scalars[9]=used;
    aggregate->flags[index]=2;put(r.workspace+0x684,1);if(possible==1)put(r.workspace+0x688,1);
    if(word(header+0x110)==1){
     put(r.workspace+0x644,1);
     if(primary>word(r.workspace+0x668)){put(r.workspace+0x668,primary);int32_t routed[6];if(route(t,index,candidate.affine,routed))goto done;memcpy(r.workspace+0x66c,routed,24);}
    }
    if(primary>word(r.workspace+0x64c)){put(r.workspace+0x64c,primary);put(r.workspace+0x648,(int32_t)index);memcpy(r.workspace+0x650,candidate.affine,24);}
    if(!cs.config[4])break;
   }
  }
  if(!accepted&&metrics[23]>0){
   if((metrics[23]==3&&(metrics[4]<205||metrics[7]>350||metrics[20]+metrics[22]>6))||(metrics[23]==2&&metrics[20]+metrics[22]>0&&(metrics[4]<200||metrics[7]>450))||(metrics[23]==1&&metrics[20]+metrics[22]>2&&(metrics[4]<195||metrics[7]>550)))rejections++;
  }
 }
 if(r.decision<1&&counter<1){r.decision=701;retry_rejected=0;}
 if((r.decision>0)&&rejections>3){r.decision=-701;put(r.workspace+0x684,0);put(r.workspace+0x688,0);retry_rejected=1;}
 if(r.decision>0&&((cs.config[4]==1&&word(r.workspace+0x688)!=1&&accepted_count<2)||(word(r.workspace+0x688)==1&&max_auxiliary<7))){
  aggregate->active_match=word(r.workspace+0x644);aggregate->possible_match=word(r.workspace+0x688);memcpy(aggregate->root_affine,r.workspace+0x66c,24);
  for(unsigned i=0;i<n;i++){aggregate->metadata[i][0]=word(t->features[i].scalar_header+0x110);aggregate->metadata[i][1]=word(t->features[i].scalar_header+0x11c);aggregate->metadata[i][2]=(int32_t)t->stored.features[i].scalars[1];}
  int32_t allowed;if(gx521d_match_aggregate(aggregate,&allowed))goto done;
  if(!allowed){retry_gate=0;put(r.workspace+0x684,0);put(r.workspace+0x688,0);r.decision=-104;}else r.decision=104;
 }
 if(r.decision>0){reorder(t);if(word(r.workspace+0x688)==1)r.decision=111;}
 else if(r.decision!=-104&&r.decision!=-701)r.decision=word(probe->scalar_header+0x120)<50?-101:last_progress&&word(probe->scalar_header+0x11c)<25?-102:-100;
 /* Last template classifier class is zero in this supported C7zero path;
  * the retry gate tests that class, not the residual rejection count. */
 retry_gate=retry_gate&&cs.config[13]==1&&!retry_rejected;
 if(cache&&r.decision<1&&retry_gate){
  if(t->stored.header[11]==0&&word(probe->scalar_header+0x11c)>40){if(gx521d_probe_cache_add(cache,probe,0))goto done;}
  if(t->stored.header[11]==1&&gx521d_recognition_rescue(cache,probe,selector,flags,&r.decision))goto done;
 }
 publish:put(r.workspace+0x690,(int32_t)selector);*owned=*t;*out=r;failure=0;
 done:gx521d_template_delete(t);memset(input,0,sizeof(*input));free(input);free(aggregate);return failure;
}

int gx521d_recognize(Gx521dTemplate *t,const Gx521dFeatureObject *p,uint32_t selector,int flags,Gx521dRecognition *r){return recognize(t,NULL,p,selector,flags,r,1,-1);}
int gx521d_recognize_cached(Gx521dTemplate *t,Gx521dProbeCache *cache,const Gx521dFeatureObject *p,uint32_t selector,int flags,Gx521dRecognition *r)
{
 if(!t||!cache||!r)return -1;
 Gx521dProbeCache *copy=malloc(sizeof(*copy));if(!copy)return -1;*copy=*cache;
 int status=recognize(t,copy,p,selector,flags,r,1,-1);if(!status)*cache=*copy;
 memset(copy,0,sizeof(*copy));free(copy);return status;
}

int gx521d_recognize_queued(Gx521dTemplate *t,Gx521dProbeCache *cache,const Gx521dFeatureObject *p,uint32_t selector,int trigger,Gx521dRecognition *r){return recognize(t,cache,p,selector,1,r,0,trigger);}
