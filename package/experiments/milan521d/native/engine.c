/* SPDX-License-Identifier: LGPL-2.1-or-later; type17 owners66140 and5ca10. */
#include "521d-engine.h"
#include <stdlib.h>
#include <string.h>
static int32_t word(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
extern int gx521d_recognize_queued(Gx521dTemplate *,Gx521dProbeCache *,const Gx521dFeatureObject *,uint32_t,int,Gx521dRecognition *);
static int import_queued(Gx521dTemplate *t,Gx521dProbeCache *cache,int32_t trigger,uint32_t selector,int32_t *action)
{
 unsigned head=0,tail=1,visits=0;int32_t ring[20]={0};ring[0]=trigger;*action=0;
 while(head!=tail){
  if(++visits>4096)return -1;
  int32_t current=ring[head];head=(head+1)%20;
  for(unsigned i=0;i<20;i++)if(cache->ages[i]!=-1){
   Gx521dRecognition recognition;const Gx521dFeatureObject *p=&cache->features[i];
   if(gx521d_recognize_queued(t,cache,p,selector,current,&recognition))return -1;
   if(word(recognition.workspace+0x688)){
    Gx521dStudyUpdate update={0,-1};
    if(gx521d_study_update(t,cache,p,recognition.workspace,1,&update))return -1;
    /* Type17 consumes strongly confirmed pending samples even when the full
     * gallery does not select a replacement. The last update controls action. */
    *action=update.selected>-1?5:0;int32_t age=cache->ages[i];
    for(unsigned j=0;j<20;j++)if(cache->ages[j]>age)cache->ages[j]--;
    cache->ages[i]=-1;
    if(update.selected>-1){ring[tail]=update.selected;tail=(tail+1)%20;if(update.selected==current)break;}
   }
  }
 }
 return 0;
}
int gx521d_engine_identify(Gx521dTemplate *owned,Gx521dProbeCache *cache,const Gx521dFeatureObject *probe,uint32_t selector,int flags,Gx521dEngineResult *out)
{
 if(!owned||!cache||!probe||!out||flags<0||flags>2)return -1;
 Gx521dTemplate *t=malloc(sizeof(*t));Gx521dProbeCache *local=malloc(sizeof(*local));
 if(!t||!local){free(t);free(local);return -1;}*t=*owned;*local=*cache;int code=-1;
 Gx521dEngineResult result={.study={0,-1}};
 if(gx521d_recognize_cached(t,local,probe,selector,flags,&result.recognition))goto done;
 if(!result.recognition.status&&word(result.recognition.workspace+0x688)&&flags){
  if(gx521d_study_update(t,local,probe,result.recognition.workspace,flags!=2,&result.study))goto done;
 }
 if(!result.recognition.status&&t->stored.header[11]==0){
  if(result.study.selected>-1&&flags!=2){int32_t action=0;
   if(import_queued(t,local,result.study.selected,selector,&action))goto done;
   if(action==5)result.study.action=5;
  }
  if(result.study.action>3){
   t->stored.header[12]+=(result.study.action==5?2u:1u);
   if((int32_t)t->stored.header[12]>50){t->stored.header[11]=1;for(unsigned i=0;i<20;i++)local->ages[i]=-1;}
  }
 }
 *owned=*t;*cache=*local;*out=result;code=0;
 done:gx521d_template_delete(t);memset(local,0,sizeof(*local));free(local);return code;
}
