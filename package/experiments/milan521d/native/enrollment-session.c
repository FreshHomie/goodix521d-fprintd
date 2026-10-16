/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 public mode0 enrollment4b010/4b3f0. */
#include "521d-enrollment-session.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
_Static_assert(offsetof(Gx521dEnrollmentSession,retry)-offsetof(Gx521dEnrollmentSession,required)==20,"session scalar layout");
static int32_t word(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
static void put(uint8_t *p,int32_t v){memcpy(p,&v,4);}
Gx521dEnrollmentSession *gx521d_enrollment_start(unsigned required)
{
 if(!required||required>50)return NULL;
 Gx521dEnrollmentSession *s=calloc(1,sizeof(*s));if(!s)return NULL;
 s->template=gx521d_enrollment_create();if(!s->template){free(s);return NULL;}
 s->required=(uint16_t)required;return s;
}
void gx521d_enrollment_finish(Gx521dEnrollmentSession *s)
{if(s){gx521d_template_delete(s->template);memset(s,0,sizeof(*s));free(s);}}
int gx521d_enrollment_add(Gx521dEnrollmentSession *s,const uint8_t pixels[5120],int quality,int coverage,uint32_t *status,int32_t cq[2])
{
 if(!s||!s->template||!pixels||!status||!cq||!s->required||s->required>50||quality<0||quality>100||coverage<0||coverage>100)return -1;
 uint32_t code=0,raw_progress=0;
 if(s->accepted<s->required){
  Gx521dFeatureObject feature;
  if(gx521d_feature_extract(pixels,quality,coverage,0,&feature))return -1;
  if(gx521d_enrollment_insert(s->template,&feature,&code,&raw_progress))return -1;
  if(!code){
   s->previous_overlap=100-(int32_t)(raw_progress>>24);
   s->overlap=100-(int32_t)(raw_progress&255);
   s->accepted++;
   s->progress=(int32_t)s->accepted*100/s->required;if(s->progress>100)s->progress=100;
   if(s->template->stored.header[4]==50)s->progress=100;
  }
  memset(&feature,0,sizeof(feature));
 }
 *status=code;cq[0]=coverage;cq[1]=quality;return 0;
}
int gx521d_enrollment_delete_image(Gx521dEnrollmentSession *s)
{
 if(!s||!s->template||!s->required||s->required>50)return -1;
 Gx521dTemplate *t=s->template;unsigned count=t->stored.header[4];
 if(count){
  unsigned current=count-1;int32_t base=word(t->features[current].scalar_header+0x114);
  if(base<0||(unsigned)base>t->stored.header[6])return -1;
  for(unsigned i=(unsigned)base;i<t->stored.header[6];i++){
   static const int32_t unset[7]={-1,256,0,0,0,256,0};memcpy(t->relations[i],unset,28);
  }
  t->stored.header[6]=(unsigned)base;t->stored.header[4]--;
  if((int32_t)current==t->stored.graph[0]){
   t->stored.graph[3]=0;
   for(unsigned i=0;i<current;i++){put(t->features[i].scalar_header+0x110,0);t->stored.features[i].scalars[0]=0;}
  }
  t->stored.relation_count=0;
  for(unsigned i=1;i<t->stored.header[6];i++)if(t->relations[i][0]>=0){Gx521dStoredRelation *r=&t->stored.relations[t->stored.relation_count++];r->index=i;memcpy(r->values,t->relations[i],28);}
 }
 if(s->accepted){s->accepted--;s->progress=(int32_t)s->accepted*100/s->required;if(s->progress>100)s->progress=100;}
 return 0;
}
