/* SPDX-License-Identifier: LGPL-2.1-or-later; bounded experimental worker ABI. */
#include "521d-engine.h"
#include "521d-enrollment-session.h"
#include "521d-wrapper.h"
#include <stdlib.h>
#include <string.h>
typedef struct {Gx521dTemplate *template;Gx521dProbeCache cache;} WorkerOwner;
void *gx_worker_load(const uint8_t *bytes,size_t size)
{
 WorkerOwner *p=calloc(1,sizeof(*p));if(!p)return NULL;
 p->template=gx521d_template_load(bytes,size);if(!p->template){free(p);return NULL;}
 gx521d_probe_cache_init(&p->cache);return p;
}
void gx_worker_delete(void *pointer)
{
 WorkerOwner *p=pointer;if(!p)return;gx521d_template_delete(p->template);
 volatile uint8_t *v=(volatile uint8_t *)p;for(size_t i=0;i<sizeof(*p);i++)v[i]=0;free(p);
}
int gx_worker_identify(void *pointer,const uint8_t *pixels,int quality,int coverage,int flags,int32_t result[4])
{
 if(!pointer||!pixels||!result||quality<0||quality>100||coverage<0||coverage>100||flags<0||flags>2)return -1;
 WorkerOwner *p=pointer;Gx521dFeatureObject feature;Gx521dEngineResult out;
 int rc=gx521d_feature_extract(pixels,quality,coverage,0,&feature);
 if(!rc){int32_t count;memcpy(&count,feature.scalar_header+0xf0,4);
  if(!count){memset(&feature,0,sizeof(feature));result[0]=131;result[1]=0;result[2]=0;result[3]=-1;return 0;}
 }
 if(!rc)rc=gx521d_engine_identify(p->template,&p->cache,&feature,0,flags,&out);
 memset(&feature,0,sizeof(feature));if(rc)return -1;
 result[0]=(int32_t)out.recognition.status;result[1]=out.recognition.decision;result[2]=out.study.action;result[3]=out.study.selected;return 0;
}
size_t gx_worker_export(void *pointer,uint8_t *bytes,size_t capacity)
{WorkerOwner *p=pointer;return p?gx521d_template_export(p->template,bytes,capacity):0;}
/* Input records are q,cov followed by5120 image bytes, packed as little-endian
 * dwords. Replay the adapter's rollback rule; no raw pointers cross this ABI. */
int gx_worker_enroll(const uint8_t *records,unsigned count,int rollback,uint8_t *packed,size_t capacity,int32_t result[4])
{
 if(!records||!result||!packed||count<1||count>18||(rollback!=0&&rollback!=1))return -1;
 Gx521dEnrollmentSession *s=gx521d_enrollment_start(18);if(!s)return -1;
 uint32_t status=0;int32_t cq[2];int code=-1;int rolled=0;
 for(unsigned i=0;i<count;i++){
  int32_t q,cov;const uint8_t *r=records+i*5128;memcpy(&q,r,4);memcpy(&cov,r+4,4);
  if(q<0||q>100||cov<0||cov>100||gx521d_enrollment_add(s,r+8,q,cov,&status,cq))goto done;
  rolled=0;
  if(rollback&&!status&&s->overlap>85&&(s->accepted<6||s->previous_overlap>75)){
   if(gx521d_enrollment_delete_image(s))goto done;
   rolled=1;
  }
  if(status||s->accepted!=i+1)break;
 }
 result[0]=(int32_t)status;result[1]=s->accepted;result[2]=s->progress;result[3]=rolled;
 if(count==18&&s->accepted==18){size_t n=gx521d_template_export(s->template,packed,capacity);if(!n||n>GX521D_TEMPLATE_MAX)goto done;code=(int)n;}else code=0;
 done:gx521d_enrollment_finish(s);return code;
}
int gx_worker_preprocess(const uint16_t *background,const uint16_t *raw,uint8_t *pixels,int32_t result[3])
{
 if(!background||!raw||!pixels||!result)return -1;
 for(unsigned i=0;i<5120;i++)if(background[i]>4095||raw[i]>4095)return -1;
 Gx521dPreprocessor p;int rc=gx521d_preprocessor_init(&p,background);if(rc)return -1;
 uint8_t auxiliary[GX521D_AUXILIARY_BYTES];int32_t cq[2];
 Gx521dDescriptor source={.pixels=(void *)raw,.width=80,.height=64,.ppi=500,.bits=16,.channels=1,.bytes=10240,.frames=1};
 Gx521dDescriptor dest={.pixels=pixels,.width=80,.height=64,.ppi=500,.bits=8,.channels=1,.bytes=5120,.frames=1};
 rc=gx521d_preprocessor_default(&p,&source,auxiliary,&dest,cq);gx521d_preprocessor_clear(&p);
 result[0]=rc;result[1]=dest.quality;result[2]=dest.coverage;return 0;
}
