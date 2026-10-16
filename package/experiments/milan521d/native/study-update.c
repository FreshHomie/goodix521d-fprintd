/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Goodix521d type17 owner93900, replacement93f60 and footprint selection94620.
 * Relation closure follows Milan relations.c (C)2026 contributors, extended
 * with the reference's nonpersistent 51st vertex. */
#include "521d-study-update.h"
#include "521d-study-append.h"
#include "521d-study-root.h"
#include "521d-retained-copy.h"
#include "milan/relations.h"
#include <stdlib.h>
#include <string.h>
static int32_t word(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
static void put(uint8_t *p,int32_t v){memcpy(p,&v,4);}
static unsigned slot(unsigned a,unsigned b){unsigned h=a>b?a:b,l=a>b?b:a;return 1+h*(h-1)/2+l;}
extern int gx521d_study_replacement_relations(const Gx521dTemplate *,const Gx521dFeatureObject *,const int32_t [50][7],int32_t [50][7]);
static const int32_t identity[6]={256,0,0,0,256,0};
/* Return b->a; row stores new->prior. */
static int read_edge(Gx521dTemplate *t,int32_t row[50][7],unsigned a,unsigned b,int32_t *rank,int32_t affine[6])
{
 unsigned n=t->stored.header[4];const int32_t *r;
 if(a==b){*rank=0;memcpy(affine,identity,24);return 0;}
 if(a==n||b==n)r=row[a==n?b:a];else r=t->relations[slot(a,b)];
 *rank=r[0];
 if(b>a)memcpy(affine,r+1,24);else if(goodix_milan_transform_invert(r+1,affine))return -1;
 return 0;
}
static int store_edge(Gx521dTemplate *t,int32_t row[50][7],unsigned a,unsigned b,const int32_t affine[6])
{
 unsigned n=t->stored.header[4];int32_t *r=(a==n||b==n)?row[a==n?b:a]:t->relations[slot(a,b)];
 if(b>a)memcpy(r+1,affine,24);else if(goodix_milan_transform_invert(affine,r+1))return -1;
 r[0]=2;return 0;
}
static int close_with_probe(Gx521dTemplate *t,int32_t row[50][7])
{
 unsigned n=t->stored.header[4];int32_t quality[51][51]={{0}};
 for(unsigned root=0;root<n;root++){
  unsigned work[51],used=1;int32_t seen[51];for(unsigned i=0;i<=n;i++)seen[i]=-1;
  work[0]=root;seen[root]=(int32_t)root;
  while(used){
   unsigned current=work[--used];int32_t current_to_root[6],root_rank=0;
   memcpy(current_to_root,identity,24);
   if(current!=root){if(read_edge(t,row,root,current,&root_rank,current_to_root))return -1;quality[root][current]=quality[current][root]=root_rank;}
   for(unsigned candidate=n+1;candidate-->0;){
    if(seen[candidate]>-1||candidate==current)continue;
    int32_t edge_rank,candidate_to_current[6];if(read_edge(t,row,current,candidate,&edge_rank,candidate_to_current))return -1;
    if(edge_rank<1)continue;
    quality[current][candidate]=quality[candidate][current]=edge_rank;seen[candidate]=(int32_t)root;
    if(used>=51)return -1;
    work[used++]=candidate;int32_t bottleneck=root_rank<edge_rank?root_rank:edge_rank;
    int32_t direct_rank,direct_affine[6];if(read_edge(t,row,root,candidate,&direct_rank,direct_affine))return -1;
    if(current==root||direct_rank>2||(direct_rank==2&&quality[root][candidate]>=bottleneck))continue;
    goodix_milan_transform_compose(current_to_root,candidate_to_current,direct_affine);
    if(store_edge(t,row,root,candidate,direct_affine))return -1;
    quality[root][candidate]=quality[candidate][root]=bottleneck;
    if(candidate<n&&(word(t->features[root].scalar_header+0x110)==1||word(t->features[candidate].scalar_header+0x110)==1)){
     put(t->features[root].scalar_header+0x110,1);put(t->features[candidate].scalar_header+0x110,1);
    }
   }
  }
 }
 return 0;
}
/* All transform arithmetic is wrapping 32-bit, followed by arithmetic shift. */
static int32_t wrap(uint32_t v){int32_t r;memcpy(&r,&v,4);return r;}
static void transform(const int32_t a[6],int32_t x,int32_t y,int32_t out[2])
{
 out[0]=wrap((uint32_t)a[0]*(uint32_t)x+(uint32_t)a[1]*(uint32_t)y+(uint32_t)a[2]+128)>>8;
 out[1]=wrap((uint32_t)a[3]*(uint32_t)x+(uint32_t)a[4]*(uint32_t)y+(uint32_t)a[5]+128)>>8;
}
static int inside_sensor(const int32_t p[2]){return p[0]>=0&&p[0]<80&&p[1]>=0&&p[1]<64;}
static int near_sensor(const int32_t p[2]){return p[0]>-5&&p[0]<85&&p[1]>-5&&p[1]<69;}
static int footprint(const int32_t affine[6],int32_t quad[10])
{
 static const int32_t corners[8]={0,0,80,0,80,64,0,64};int strict=0,near=0;
 for(unsigned i=0;i<4;i++){
  transform(affine,corners[2*i],corners[2*i+1],quad+2*i);
  strict|=quad[2*i]>5&&quad[2*i]<75&&quad[2*i+1]>5&&quad[2*i+1]<59;near|=near_sensor(quad+2*i);
 }
 memcpy(quad+8,quad,8);
 if(!strict){int32_t point[2];transform(affine,40,32,point);strict=inside_sensor(point);near|=near_sensor(point);
  if(!strict&&near){
   for(int x=0;x<80&&!strict;x+=16){transform(affine,x,32,point);strict=inside_sensor(point);}
   for(int y=0;y<64&&!strict;y+=16){transform(affine,40,y,point);strict=inside_sensor(point);}
  }
 }
 return strict;
}
static uint32_t cross(const int32_t *p,const int32_t *next,int x,int y)
{
 return ((uint32_t)x-(uint32_t)p[0])*((uint32_t)next[1]-(uint32_t)p[1])-((uint32_t)y-(uint32_t)p[1])*((uint32_t)next[0]-(uint32_t)p[0]);
}
static int point_inside(const int32_t q[10],int x,int y)
{
 return ((cross(q,q+2,x,y)^cross(q+4,q+6,x,y))&0x80000000u)==0&&((cross(q+2,q+4,x,y)^cross(q+6,q+8,x,y))&0x80000000u)==0;
}
static int redundancy(const Gx521dFeatureObject *f,int32_t quads[50][10],unsigned used)
{
 unsigned total=0,covered=0;
 for(unsigned y=1;y<64;y+=3)for(unsigned x=1;x<80;x+=3){
  total++;unsigned bit=(y/4)*20+x/4;
  if(!(f->scalar_header[40+bit/8]&(1u<<(bit%8)))){covered++;continue;}
  for(unsigned i=0;i<used;i++)if(point_inside(quads[i],(int)x,(int)y)){covered++;break;}
 }
 return total?(int)(covered*100/total):100;
}
static int selection_scores(Gx521dTemplate *t,const Gx521dFeatureObject *p,int32_t row[50][7],int32_t scores[51])
{
 unsigned n=t->stored.header[4];
 for(unsigned target=0;target<=n;target++){
  unsigned used=0;int32_t quads[50][10];
  for(unsigned source=0;source<n;source++){
   /* Gallery target: its own source index represents incoming probe. */
   unsigned destination=target==0?n:target-1,origin=(target&&source==destination)?n:source;
   int32_t rank,a[6],quad[10];if(read_edge(t,row,destination,origin,&rank,a))return -1;
   if(rank<0)continue;
   if(footprint(a,quad)){memcpy(quads[used++],quad,sizeof(quad));}
  }
  scores[target]=redundancy(target?&t->features[target-1]:p,quads,used);
 }
 return 0;
}
static int sync_stored(Gx521dTemplate *t)
{
 t->stored.relation_count=0;
 for(unsigned i=1;i<t->stored.header[6];i++)if(t->relations[i][0]>=0){Gx521dStoredRelation *r=&t->stored.relations[t->stored.relation_count++];r->index=i;memcpy(r->values,t->relations[i],28);}
 for(unsigned i=0;i<t->stored.header[4];i++){
  uint8_t bytes[GX521D_PACKED_FEATURE_MAX];size_t size=gx521d_template_feature_pack(&t->features[i],bytes,sizeof(bytes));
  if(!size||!gx521d_stored_feature_read(bytes,size,&t->stored.features[i]))return -1;
 }
 return 0;
}
static int replace(Gx521dTemplate *t,Gx521dProbeCache *cache,const Gx521dFeatureObject *probe,const uint8_t workspace[0x694],Gx521dStudyUpdate *result)
{
 unsigned n=t->stored.header[4];int32_t matched=word(workspace+0x648),seed[50][7],row[50][7],scores[51];
 for(unsigned i=0;i<50;i++){seed[i][0]=word(workspace+32*i+28);memcpy(seed[i]+1,workspace+32*i+4,24);}
 if(gx521d_study_replacement_relations(t,probe,seed,row)||close_with_probe(t,row)||selection_scores(t,probe,row,scores))return -1;
 int32_t selected=-1,highest=scores[0];
 for(unsigned i=0;i<n;i++)if(scores[i+1]>highest){selected=(int32_t)i;highest=scores[i+1];}
 int32_t q=word(probe->scalar_header+0x11c),cov=word(probe->scalar_header+0x120),oldq=word(t->features[matched].scalar_header+0x11c);
 if(selected>=0)result->action=4;
 else if((scores[matched+1]>95&&q>50)||(cov>80&&scores[0]*9<scores[matched+1]*10&&q*100>oldq*(oldq<60?90:80))){selected=matched;result->action=3;}
 if(t->stored.header[11]==1&&gx521d_probe_cache_add(cache,selected<0?probe:&t->features[selected],1))return -1;
 result->selected=selected;if(selected<0)return 0;
 Gx521dFeatureObject *f=&t->features[selected];int32_t active=word(t->features[matched].scalar_header+0x110),hits=word(t->features[matched].scalar_header+0x134);
 put(f->scalar_header+0x130,wrap((uint32_t)word(f->scalar_header+0x130)+1));put(f->scalar_header+0x110,active);put(f->scalar_header+0x134,hits);
 if(gx521d_retained_copy(probe,f))return -1;
 put(f->scalar_header+0x124,2);int32_t ordinal=word(f->scalar_header+0x12c);
 for(unsigned i=0;i<t->stored.header[5];i++){int32_t old=word(t->features[i].scalar_header+0x12c);if(old>ordinal)put(t->features[i].scalar_header+0x12c,old-1);}
 put(f->scalar_header+0x12c,(int32_t)t->stored.header[5]-1);
 for(unsigned i=0;i<n;i++)if((int32_t)i!=selected){
  int32_t *r=t->relations[slot(i,(unsigned)selected)];r[0]=row[i][0];
  if(i<(unsigned)selected)memcpy(r+1,row[i]+1,24);else if(goodix_milan_transform_invert(row[i]+1,r+1))return -1;
 }
 put(t->stored.tail+0x510,wrap((uint32_t)word(t->stored.tail+0x510)+1));return 0;
}
int gx521d_study_update(Gx521dTemplate *owned,Gx521dProbeCache *cache,const Gx521dFeatureObject *probe,const uint8_t workspace[0x694],int allow_replace,Gx521dStudyUpdate *out)
{
 if(!owned||!probe||!workspace||!out||(allow_replace!=0&&allow_replace!=1))return -1;
 unsigned n=owned->stored.header[4],cap=owned->stored.header[5];int32_t matched=word(workspace+0x648);
 if(!n||n>50||cap!=50||n>cap||matched<0||(unsigned)matched>=n||owned->stored.header[1]!=17||owned->stored.header[11]>1||owned->stored.header[9]||owned->stored.header[10]!=1||owned->stored.header[6]!=1+n*(n-1)/2)return -1;
 const uint8_t *h=probe->scalar_header;int32_t records=word(h+0xf0),q=word(h+0x11c),cov=word(h+0x120);
 if(word(h)!=80||word(h+4)!=64||records<0||records>120||word(h+0x118)<0||word(h+0x118)>records||q<0||q>100||cov<0||cov>100||word(h+0x150))return -1;
 if(owned->stored.header[11]&&allow_replace&&n==cap&&q>15&&!cache)return -1;
 for(unsigned i=0;i<n;i++)if(word(owned->features[i].scalar_header+0x114)!=(int32_t)(i?1+i*(i-1)/2:0))return -1;
 Gx521dTemplate *t=malloc(sizeof(*t));Gx521dProbeCache *local=cache?malloc(sizeof(*local)):NULL;
 if(!t||(cache&&!local)){free(t);free(local);return -1;}*t=*owned;if(cache)*local=*cache;
 Gx521dStudyUpdate result={0,-1};int code=-1;
 if(n<cap){if(gx521d_study_append(t,probe,workspace,&result.action))goto done;result.selected=(int32_t)n;}
 else if(allow_replace&&q>15){if(replace(t,local,probe,workspace,&result))goto done;}
 if(!t->stored.graph[3]||(t->stored.graph[3]==1&&result.selected==t->stored.graph[0]))if(gx521d_study_root(t))goto done;
 if(sync_stored(t))goto done;
 *owned=*t;if(cache)*cache=*local;*out=result;code=0;
 done:gx521d_template_delete(t);if(local){memset(local,0,sizeof(*local));free(local);}return code;
}
