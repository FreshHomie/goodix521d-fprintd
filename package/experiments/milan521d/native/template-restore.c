/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 load restoration. Graph closure adapted from Milan relations.c
 * (C)2026 goodix-fp-linux-dev contributors; type17 branches checked separately. */
#include "521d-template.h"
#include "milan/relations.h"
#include <stdlib.h>
#include <string.h>
static uint32_t u32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static int slot(const Gx521dTemplate *t,unsigned a,unsigned b)
{unsigned high=a>b?a:b,low=a>b?b:a;return (int)t->stored.features[high].scalars[1]+(int)low;}
static int edge(Gx521dTemplate *t,unsigned first,unsigned second,int32_t transform[6])
{
 int k=slot(t,first,second);const int32_t *r=t->relations[k];
 if(second>first)memcpy(transform,r+1,24);
 else if(goodix_milan_transform_invert(r+1,transform))return -2;
 return r[0];
}
static int close_graph(Gx521dTemplate *t)
{
 static const int32_t identity[6]={256,0,0,0,256,0};
 int32_t quality[50][50]={{0}};unsigned n=t->stored.header[4];
 for(unsigned root=0;root<n;root++){
  int seen[50];unsigned work[50],size=1;
  for(unsigned i=0;i<n;i++)seen[i]=-1;
  seen[root]=0;work[0]=root;
  while(size){
   unsigned current=work[--size];int32_t current_to_root[6];memcpy(current_to_root,identity,24);
   int rank=0;
   if(current!=root){rank=edge(t,root,current,current_to_root);if(rank==-2)return 0;quality[root][current]=quality[current][root]=rank;}
   for(unsigned candidate=n;candidate-->0;){
    if(seen[candidate]>-1||candidate==current)continue;
    int32_t transform[6];int candidate_rank=edge(t,current,candidate,transform);if(candidate_rank==-2)return 0;
    if(candidate_rank<=0)continue;
    quality[current][candidate]=quality[candidate][current]=candidate_rank;
    seen[candidate]=(int)root;work[size++]=candidate;
    int bottleneck=rank<candidate_rank?rank:candidate_rank;
    if(current==root)continue;
    int32_t direct[6];int direct_rank=edge(t,root,candidate,direct);if(direct_rank==-2)return 0;
    if(direct_rank>2||(direct_rank==2&&quality[root][candidate]>=bottleneck))continue;
    int32_t composed[6];goodix_milan_transform_compose(current_to_root,transform,composed);
    int32_t *r=t->relations[slot(t,root,candidate)];
    if(candidate>root)memcpy(r+1,composed,24);
    else if(goodix_milan_transform_invert(composed,r+1))return 0;
    r[0]=2;quality[root][candidate]=quality[candidate][root]=bottleneck;
    if(t->stored.features[root].scalars[0]==1||t->stored.features[candidate].scalars[0]==1)
     t->stored.features[root].scalars[0]=t->stored.features[candidate].scalars[0]=1;
   }
  }
 }
 return 1;
}
static void restore_feature(const Gx521dStoredFeature *s,Gx521dFeatureObject *f)
{
 memset(f,0,sizeof(*f));put(f->scalar_header,80);put(f->scalar_header+4,64);
 memcpy(f->high,s->high,640);memcpy(f->low,s->low,640);
 memset(f->scalar_header+0x28,255,200);memcpy(f->scalar_header+0x28,s->coarse_mask,40);
 put(f->scalar_header+0xf0,s->count);
 static const unsigned offsets[]={0x110,0x114,0x118,0x11c,0x120,0x124,0x128,0x12c,0x130,0x134,0x14c};
 for(unsigned i=0;i<11;i++)put(f->scalar_header+offsets[i],s->scalars[i]);
 put(f->scalar_header+0x150,s->embedded_tag);
 /* The full-resolution CE mask expands each coarse cell to4x4 pixels;
  * the packed expanded image is10 bytes per row. */
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<80;x++){
  unsigned bit=(y/4)*20+x/4;
  if((s->coarse_mask[bit/8]>>(bit%8))&1)f->packed_mask[y*10+x/8]|=(uint8_t)(1U<<(x%8));
 }
 for(unsigned i=0;i<s->count;i++){
  const uint8_t *p=s->records[i];GoodixMilanFeatureRecord *r=&f->records[i];uint32_t position=u32(p),tail=u32(p+28);
  r->foreground=(uint16_t)tail;r->refined_x=(int16_t)((position>>16)&0xfff0);r->refined_y=(int16_t)((position>>4)&0xfff0);
  unsigned angle=position&255;r->orientation=(int16_t)(angle<128?(int)angle*256:(128-(int)angle)*256);
  int32_t boundary=(int16_t)(tail>>16);put(r->payload,(uint32_t)boundary);memcpy(r->payload+4,p+4,24);
 }
}
int gx521d_template_restore(const Gx521dStoredTemplate *in,Gx521dTemplate *out)
{
 if(!in||!out)return 0;
 /* Reuse the complete pack admission contract, without modifying caller data. */
 uint8_t *check=malloc(GX521D_TEMPLATE_MAX);if(!check)return 0;
 size_t valid=gx521d_template_pack(in,check,GX521D_TEMPLATE_MAX);free(check);if(!valid)return 0;
 Gx521dTemplate *t=calloc(1,sizeof(*t));if(!t)return 0;t->stored=*in;
 static const int32_t unset[7]={-1,256,0,0,0,256,0};
 for(unsigned i=0;i<1226;i++)memcpy(t->relations[i],unset,28);
 for(unsigned i=0;i<in->relation_count;i++)memcpy(t->relations[in->relations[i].index],in->relations[i].values,28);
 if(!close_graph(t)){gx521d_template_delete(t);return 0;}
 for(unsigned i=0;i<in->header[4];i++)restore_feature(&t->stored.features[i],&t->features[i]);
 *out=*t;gx521d_template_delete(t);return 1;
}

size_t gx521d_template_export(const Gx521dTemplate *t,uint8_t *bytes,size_t capacity)
{
 if(!t||!bytes||!t->stored.header[4]||t->stored.header[4]>50)return 0;
 uint8_t *admission=malloc(GX521D_TEMPLATE_MAX);if(!admission)return 0;
 size_t admitted=gx521d_template_pack(&t->stored,admission,GX521D_TEMPLATE_MAX);free(admission);if(!admitted)return 0;
 unsigned n=t->stored.header[4];int component[50],parent[50];
 for(unsigned i=0;i<50;i++)component[i]=parent[i]=-1;
 /* Reference934d0/93a70: descending LIFO strong-edge forest, then weak
  * direct neighbours of each root; only forest edges survive serialization. */
 for(unsigned root=0;root<n;root++){
  if(component[root]>-1)continue;
  component[root]=(int)root;unsigned work[50],size=1;work[0]=root;
  while(size){
   unsigned current=work[--size];
   for(unsigned candidate=n;candidate-->0;){
    if(component[candidate]>-1||candidate==current)continue;
    if(t->relations[slot(t,current,candidate)][0]>2){
     component[candidate]=(int)root;parent[candidate]=(int)current;work[size++]=candidate;
    }
   }
  }
  for(unsigned candidate=0;candidate<n;candidate++)
   if(component[candidate]<0&&candidate!=root&&t->relations[slot(t,root,candidate)][0]>-1)
    component[candidate]=parent[candidate]=(int)root;
 }
 Gx521dStoredTemplate *s=malloc(sizeof(*s));if(!s)return 0;*s=t->stored;s->relation_count=0;
 for(unsigned high=1;high<n;high++)for(unsigned low=0;low<high;low++){
  unsigned index=(unsigned)slot(t,high,low);const int32_t *r=t->relations[index];
  if(r[0]<0||(parent[high]!=(int)low&&parent[low]!=(int)high))continue;
  Gx521dStoredRelation *target=&s->relations[s->relation_count++];target->index=index;memcpy(target->values,r,28);
 }
 uint8_t packed[GX521D_PACKED_FEATURE_MAX];
 for(unsigned i=0;i<n;i++){
  size_t size=gx521d_template_feature_pack(&t->features[i],packed,sizeof(packed));
  if(!size||!gx521d_stored_feature_read(packed,size,&s->features[i])){free(s);return 0;}
 }
 size_t size=gx521d_template_pack(s,bytes,capacity);free(s);return size;
}
