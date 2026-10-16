/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 container, adapted tagged codec design from Milan template/codec.c
 * (C)2026 goodix-fp-linux-dev contributors. Type17 schema from RVA69510/68bc0. */
#include "521d-template.h"
#include <stdlib.h>
#include <string.h>
static uint32_t u32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static uint32_t crc(const uint8_t *p,size_t n)
{uint32_t c=UINT32_MAX;for(size_t i=0;i<n;i++){c^=p[i];for(unsigned b=0;b<8;b++)c=(c>>1)^(0xedb88320U&(0U-(c&1)));}return ~c;}
static const uint8_t header_tags[]={0x81,0x98,0x9a,0x9b,0x91,0x97,0x92,0x9e,0x9f,0x9c,0x9d,0xfa,0xfb};
static const uint8_t relation_tags[]={0xe3,0xe1,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9};
static int valid(const Gx521dStoredTemplate *t)
{
 if(!t)return 0;
 const uint32_t *h=t->header;uint32_t count=h[4];
 if(h[0]!=0x2df15f||h[1]!=17||h[2]!=64||h[3]!=80||!count||count>50||h[5]<count||h[5]>50||h[6]!=1+count*(count-1)/2||h[7]!=120||h[8]!=120||h[9]!=0||h[10]!=1||h[11]>1||t->relation_count>1225)return 0;
 if(t->graph[3]<0||t->graph[3]>1||t->graph[0]<-1||t->graph[0]>=(int32_t)count||(t->graph[3]&&t->graph[0]<0))return 0;
 uint8_t seen[1226]={0};
 for(uint32_t i=0;i<count;i++){
  const Gx521dStoredFeature *f=&t->features[i];
  uint32_t base=i?1+i*(i-1)/2:0;
  if(f->count>120||f->scalars[1]!=base||f->scalars[2]>f->count)return 0;
 }
 for(uint32_t i=0;i<t->relation_count;i++){
  uint32_t index=t->relations[i].index;
  if(!index||index>=h[6]||index>=1226||seen[index]||t->relations[i].values[0]<0)return 0;
  seen[index]=1;
 }
 return 1;
}
typedef struct {const uint8_t *p;size_t n;} R;
static int scalar(R *r,uint8_t tag,uint32_t *out)
{if(r->n<5||r->p[0]!=tag)return 0;*out=u32(r->p+1);r->p+=5;r->n-=5;return 1;}
static int block(R *r,uint8_t tag,size_t size,const uint8_t **out)
{uint32_t n;if(!scalar(r,tag,&n)||n!=size||r->n<size)return 0;*out=r->p;r->p+=size;r->n-=size;return 1;}
int gx521d_template_read(const uint8_t *bytes,size_t n,Gx521dStoredTemplate *out)
{
 if(!bytes||!out||n<1433||n>GX521D_TEMPLATE_MAX||bytes[0]!=0x87||bytes[5]!=0x86||u32(bytes+6)!=n-10||u32(bytes+1)!=crc(bytes+10,n-10))return 0;
 Gx521dStoredTemplate *t=calloc(1,sizeof(*t));if(!t)return 0;
 int ok=0;R r={bytes+10,n-10};const uint8_t *p;uint32_t v;
 for(unsigned i=0;i<13;i++)if(!scalar(&r,header_tags[i],&t->header[i]))goto done;
 if(!t->header[4]||t->header[4]>50)goto done;
 for(uint32_t i=0;i<t->header[4];i++){
  if(r.n<5||r.p[0]!=0x95)goto done;
  size_t size=5+(size_t)u32(r.p+1);
  if(size>r.n||!gx521d_stored_feature_read(r.p,size,&t->features[i]))goto done;
  r.p+=size;r.n-=size;
 }
 while(r.n&&r.p[0]==0x96){
  if(t->relation_count>=1225||!block(&r,0x96,40,&p))goto done;
  R inner={p,40};Gx521dStoredRelation *rel=&t->relations[t->relation_count++];
  if(!scalar(&inner,relation_tags[0],&rel->index))goto done;
  for(unsigned j=0;j<7;j++){if(!scalar(&inner,relation_tags[j+1],&v))goto done;rel->values[j]=(int32_t)v;}
 }
 if(!block(&r,0x93,20,&p))goto done;
 R graph={p,20};
 for(unsigned i=0;i<4;i++){if(!scalar(&graph,(uint8_t)(0xf2+i),&v))goto done;t->graph[i]=(int32_t)v;}
 if(!block(&r,0x94,0x530,&p)||r.n)goto done;
 R tail={p,0x530};
 if(!block(&tail,0xa1,200,&p))goto done;
 memcpy(t->tail,p,200);
 if(!scalar(&tail,0xa2,&v))goto done;
 put(t->tail+0xc8,v);
 if(!block(&tail,0xa3,64,&p))goto done;
 memcpy(t->tail+0xcc,p,64);
 if(!block(&tail,0xa4,1024,&p))goto done;
 memcpy(t->tail+0x10c,p,1024);
 for(unsigned i=0;i<4;i++){if(!scalar(&tail,(uint8_t)(0xa5+i),&v))goto done;put(t->tail+0x50c+4*i,v);}
 if(tail.n||!valid(t))goto done;
 *out=*t;ok=1;
 done:free(t);return ok;
}
static uint8_t *emit(uint8_t *p,uint8_t tag,uint32_t v){*p++=tag;put(p,v);return p+4;}
static uint8_t *emitblock(uint8_t *p,uint8_t tag,const uint8_t *src,size_t n)
{p=emit(p,tag,(uint32_t)n);memcpy(p,src,n);return p+n;}
size_t gx521d_template_pack(const Gx521dStoredTemplate *t,uint8_t *bytes,size_t capacity)
{
 if(!bytes||!valid(t))return 0;
 size_t n=1433+45*t->relation_count;
 for(uint32_t i=0;i<t->header[4];i++)n+=1455+32*t->features[i].count+(t->features[i].embedded_tag?5:0);
 if(n>GX521D_TEMPLATE_MAX||capacity<n)return 0;
 bytes[0]=0x87;bytes[5]=0x86;uint8_t *p=bytes+10;
 for(unsigned i=0;i<13;i++)p=emit(p,header_tags[i],t->header[i]);
 for(uint32_t i=0;i<t->header[4];i++)p+=gx521d_stored_feature_pack(&t->features[i],p,(size_t)(bytes+n-p));
 for(uint32_t i=0;i<t->relation_count;i++){
  const Gx521dStoredRelation *r=&t->relations[i];p=emit(p,0x96,40);p=emit(p,0xe3,r->index);
  for(unsigned j=0;j<7;j++)p=emit(p,relation_tags[j+1],(uint32_t)r->values[j]);
 }
 p=emit(p,0x93,20);for(unsigned i=0;i<4;i++)p=emit(p,(uint8_t)(0xf2+i),(uint32_t)t->graph[i]);
 p=emit(p,0x94,0x530);p=emitblock(p,0xa1,t->tail,200);p=emit(p,0xa2,u32(t->tail+0xc8));
 p=emitblock(p,0xa3,t->tail+0xcc,64);p=emitblock(p,0xa4,t->tail+0x10c,1024);
 for(unsigned i=0;i<4;i++)p=emit(p,(uint8_t)(0xa5+i),u32(t->tail+0x50c+4*i));
 put(bytes+6,(uint32_t)n-10);put(bytes+1,crc(bytes+10,n-10));return (size_t)(p-bytes);
}
void gx521d_template_delete(Gx521dTemplate *t)
{if(t){volatile uint8_t *p=(volatile uint8_t *)t;for(size_t i=0;i<sizeof(*t);i++)p[i]=0;free(t);}}
Gx521dTemplate *gx521d_template_load(const uint8_t *bytes,size_t n)
{
 Gx521dTemplate *t=malloc(sizeof(*t));Gx521dStoredTemplate *s=malloc(sizeof(*s));
 if(!t||!s||!gx521d_template_read(bytes,n,s)||!gx521d_template_restore(s,t)){free(s);gx521d_template_delete(t);return NULL;}
 free(s);return t;
}
