/* SPDX-License-Identifier: LGPL-2.1-or-later; type17 internal study append942d0. */
#include "521d-study-append.h"
#include "521d-retained-copy.h"
#include "milan/relations.h"
#include <stdlib.h>
#include <string.h>
static int32_t word(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
static void put(uint8_t *p,int32_t v){memcpy(p,&v,4);}
int gx521d_study_append(Gx521dTemplate *owned,const Gx521dFeatureObject *probe,const uint8_t workspace[0x694],int32_t *action)
{
 if(!owned||!probe||!workspace||!action)return -1;
 unsigned n=owned->stored.header[4],base=owned->stored.header[6];int32_t matched=word(workspace+0x648);
 if(!n||n>=50||n>=owned->stored.header[5]||matched<0||(unsigned)matched>=n||base!=1+n*(n-1)/2)return -1;
 Gx521dTemplate *t=malloc(sizeof(*t));GoodixMilanRelationMatrix *matrix=calloc(1,sizeof(*matrix));if(!t||!matrix){free(t);free(matrix);return -1;}*t=*owned;int code=-1;
 int32_t seed[50][7],row[50][7];
 for(unsigned i=0;i<50;i++){seed[i][0]=word(workspace+32*i+28);memcpy(seed[i]+1,workspace+32*i+4,24);}
 if(gx521d_study_relations(t,probe,seed,row))goto done;
 Gx521dFeatureObject *feature=&t->features[n];if(gx521d_retained_copy(probe,feature))goto done;
 const uint8_t *h=t->features[matched].scalar_header;
 put(feature->scalar_header+0x110,word(h+0x110));put(feature->scalar_header+0x114,(int32_t)base);put(feature->scalar_header+0x124,1);put(feature->scalar_header+0x12c,(int32_t)n);put(feature->scalar_header+0x130,0);put(feature->scalar_header+0x134,word(h+0x134));put(feature->scalar_header+0x138,word(h+0x138));
 for(unsigned i=0;i<n;i++)memcpy(t->relations[base+i],row[i],28);
 t->stored.header[4]++;t->stored.header[6]+=n;put(t->stored.tail+4*n,(int32_t)n);
 put(t->stored.tail+0x514,(int32_t)((uint32_t)word(t->stored.tail+0x514)+1));
 matrix->feature_count=n+1;matrix->reference_feature_index=(size_t)t->stored.graph[0];matrix->graph_established=t->stored.graph[3];memcpy(matrix->slots,t->relations+1,sizeof(matrix->slots));int32_t active[50]={0};
 for(unsigned i=0;i<=n;i++){matrix->row_bases[i]=word(t->features[i].scalar_header+0x114);active[i]=word(t->features[i].scalar_header+0x110);}
 if(goodix_milan_relation_matrix_close(matrix,active)<0)goto done;
 memcpy(t->relations+1,matrix->slots,sizeof(matrix->slots));t->stored.relation_count=0;
 for(unsigned i=1;i<t->stored.header[6];i++)if(t->relations[i][0]>=0){Gx521dStoredRelation *r=&t->stored.relations[t->stored.relation_count++];r->index=i;memcpy(r->values,t->relations[i],28);}
 for(unsigned i=0;i<=n;i++){
  put(t->features[i].scalar_header+0x110,active[i]);uint8_t bytes[GX521D_PACKED_FEATURE_MAX];size_t length=gx521d_template_feature_pack(&t->features[i],bytes,sizeof(bytes));
  if(!length||!gx521d_stored_feature_read(bytes,length,&t->stored.features[i]))goto done;
 }
 *owned=*t;*action=1;code=0;
 done:gx521d_template_delete(t);free(matrix);return code;
}
