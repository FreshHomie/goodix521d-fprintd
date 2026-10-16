/* SPDX-License-Identifier: LGPL-2.1-or-later; type17 append relations92c00. */
#include "521d-study-relations.h"
#include "521d-study-relation.h"
#include "milan/relations.h"
#include <stdlib.h>
#include <string.h>
static int32_t word(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
static int build_relations(const Gx521dTemplate *t,const Gx521dFeatureObject *probe,const int32_t seed[50][7],int32_t out[50][7],int replacement)
{
 if(!t||!probe||!seed||!out)return -1;
 unsigned n=t->stored.header[4];if(!n||n>50||t->stored.header[1]!=17)return -1;
 if(word(probe->scalar_header)!=80||word(probe->scalar_header+4)!=64||word(probe->scalar_header+0xf0)<0||word(probe->scalar_header+0xf0)>120||word(probe->scalar_header+0x150))return -1;
 int32_t row[50][7];static const int32_t unset[7]={-1,256,0,0,0,256,0},identity[6]={256,0,0,0,256,0};
 for(unsigned i=0;i<50;i++)memcpy(row[i],unset,28);
 for(unsigned i=0;i<n;i++)if(replacement||seed[i][0]>0)memcpy(row[i],seed[i],28);
 unsigned stack[51]={0},used=1;stack[0]=n;uint8_t seen[50]={0};
 while(used){
  unsigned current=stack[--used];int32_t current_to_new[6];memcpy(current_to_new,identity,24);
  if(current!=n&&goodix_milan_transform_invert(row[current]+1,current_to_new))return -1;
  for(unsigned prior=n;prior-->0;){
   if(seen[prior])continue;
   if(current==prior)continue;
   int32_t prior_to_current[6],rank;
   if(current==n){rank=row[prior][0];if(goodix_milan_transform_invert(row[prior]+1,prior_to_current))return -1;}
   else {
    unsigned high=current>prior?current:prior,low=current>prior?prior:current;int32_t base=word(t->features[high].scalar_header+0x114);
    if(base<0||(unsigned)base+low>=1226)return -1;
    const int32_t *edge=t->relations[(unsigned)base+low];rank=edge[0];
    if(current<prior)memcpy(prior_to_current,edge+1,24);
    else if(goodix_milan_transform_invert(edge+1,prior_to_current))return -1;
   }
   if(rank<1||rank>42)continue;
   seen[prior]=1;if(used>=51)return -1;stack[used++]=prior;
   if(current!=n&&row[prior][0]<2){
    int32_t prior_to_new[6];goodix_milan_transform_compose(current_to_new,prior_to_current,prior_to_new);
    if(goodix_milan_transform_invert(prior_to_new,row[prior]+1))return -1;
    row[prior][0]=2;
   }
  }
 }
 Gx521dMatchCandidateInput *pair=malloc(sizeof(*pair));if(!pair)return -1;pair->probe=*probe;pair->selector=0;pair->flags=0;
 for(unsigned i=0;i<n;i++)if(!seen[i]&&seed[i][0]!=-2){
  pair->enrolled=t->features[i];if(gx521d_study_relation(pair,row[i])){free(pair);return -1;}
 }
 memset(pair,0,sizeof(*pair));free(pair);memcpy(out,row,sizeof(row));return 0;
}

int gx521d_study_relations(const Gx521dTemplate *t,const Gx521dFeatureObject *p,const int32_t seed[50][7],int32_t out[50][7]){return build_relations(t,p,seed,out,0);}
int gx521d_study_replacement_relations(const Gx521dTemplate *t,const Gx521dFeatureObject *p,const int32_t seed[50][7],int32_t out[50][7]){return build_relations(t,p,seed,out,1);}
