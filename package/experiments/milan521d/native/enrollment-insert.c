/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 provisional enrollment56440. Graph routing/propagation adapted from
 * Milan enrollment/template.c, Copyright(C)2026 goodix-fp-linux-dev contributors.
 * Type17 full-resolution residual masks and progress from56440/56ac0/67730. */
#include "521d-enrollment-insert.h"
#include "521d-enrollment-relation.h"
#include "521d-match-controller-state.h"
#include "milan/enrollment/propagation.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
static int32_t word(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
static void put(uint8_t *p,int32_t v){memcpy(p,&v,4);}
static const int32_t identity[6]={256,0,0,0,256,0};
Gx521dTemplate *gx521d_enrollment_create(void)
{
 Gx521dTemplate *t=calloc(1,sizeof(*t));if(!t)return NULL;
 uint32_t head[13]={0x2df15f,17,64,80,0,50,0,120,120,0,1,0,0};memcpy(t->stored.header,head,52);
 t->stored.graph[0]=t->stored.graph[1]=t->stored.graph[2]=-1;
 memset(t->stored.tail,255,204);memcpy(t->stored.tail+0xcc,"Milan_v_3.01.09.11",18);
 for(unsigned i=0;i<1226;i++){t->relations[i][0]=-1;memcpy(t->relations[i]+1,identity,24);}
 return t;
}
static int overlap(const Gx521dTemplate *t,unsigned low,unsigned high,const int32_t affine[6],int32_t *score)
{
 const Gx521dFeatureObject *a=&t->features[low],*b=&t->features[high];Gx521dMatchOverlapInput input={.enhanced=1};Gx521dMatchOverlap output;
 memcpy(input.affine,affine,24);memcpy(input.enrolled_high,a->high,640);memcpy(input.enrolled_low,a->low,640);memcpy(input.enrolled_mask,a->scalar_header+40,40);
 memcpy(input.probe_high,b->high,640);memcpy(input.probe_low,b->low,640);memcpy(input.probe_mask,b->scalar_header+40,40);
 if(gx521d_match_overlap(&input,&output))return -1;
 *score=output.score;return 0;
}
typedef struct {Gx521dTemplate *t;GoodixMilanRelationMatrix *m;} Propagation;
static int propagation_overlap(void *data,size_t a,size_t b,int32_t *score)
{
 Propagation *p=data;size_t high=a>b?a:b,low=a>b?b:a;GoodixMilanRelationSlot *slot=goodix_milan_enrollment_relation_slot(p->m,high,low);
 if(!slot||slot->values[0]<0)return -1;
 return overlap(p->t,(unsigned)low,(unsigned)high,slot->values+1,score);
}
static int project(const int32_t affine[6],Gx521dMatchControllerState *out)
{
 Gx521dMatchControllerStateInput input={0};memcpy(input.affine,affine,24);return gx521d_match_controller_state(&input,out);
}
static int sync(Gx521dTemplate *t)
{
 t->stored.relation_count=0;
 for(unsigned i=1;i<t->stored.header[6];i++)if(t->relations[i][0]>=0){Gx521dStoredRelation *r=&t->stored.relations[t->stored.relation_count++];r->index=i;memcpy(r->values,t->relations[i],28);}
 for(unsigned i=0;i<t->stored.header[4];i++){
  uint8_t bytes[GX521D_PACKED_FEATURE_MAX];size_t n=gx521d_template_feature_pack(&t->features[i],bytes,sizeof(bytes));
  if(!n||!gx521d_stored_feature_read(bytes,n,&t->stored.features[i]))return -1;
 }
 return 0;
}
int gx521d_enrollment_insert(Gx521dTemplate *owned,const Gx521dFeatureObject *feature,uint32_t *status,uint32_t *progress)
{
 if(!owned||!feature||!status||!progress)return -1;
 unsigned current=owned->stored.header[4];const uint8_t *h=feature->scalar_header;
 if(current>50||owned->stored.header[5]!=50||owned->stored.header[1]!=17||owned->stored.header[6]!=(current?1+current*(current-1)/2:0))return -1;
 if(current==50){*status=0x80000005;*progress=0;return 0;}
 if(word(h)!=80||word(h+4)!=64||word(h+0xf0)<0||word(h+0xf0)>120||word(h+0x118)<0||word(h+0x118)>word(h+0xf0)||word(h+0x108))return -1;
 if(word(h+0x150)||word(h+0x14c))return -2;
 if(!word(h+0xf0)){*status=0x80000006;*progress=0;return 0;}
 Gx521dTemplate *t=malloc(sizeof(*t));GoodixMilanRelationMatrix *matrix=calloc(1,sizeof(*matrix));Gx521dMatchCandidateInput *pair=malloc(sizeof(*pair));
 if(!t||!matrix||!pair){free(t);free(matrix);free(pair);return -1;}*t=*owned;int result=-1;
 Gx521dFeatureObject *f=&t->features[current];*f=*feature;memset(f->validity,0,sizeof(f->validity));
 unsigned base=t->stored.header[6];put(f->scalar_header+0x114,(int32_t)base);put(f->scalar_header+0x124,0);put(f->scalar_header+0x12c,(int32_t)current);put(f->scalar_header+0x130,0);put(f->scalar_header+0x134,0);memset(f->scalar_header+0x160,0,8);
 t->stored.header[4]++;uint32_t packed=0x64000064;
 int active[50]={0},accepted[50]={0};int32_t scores[50]={0};int best=-1,best_connected=-1,accepted_count=0;
 unsigned best_prior=0,best_connected_prior=0;
 for(unsigned i=0;i<current;i++){
  pair->enrolled=t->features[i];pair->probe=*f;pair->selector=0;pair->flags=0;Gx521dEnrollmentRelation relation;
  if(gx521d_enrollment_relation(pair,&relation))goto done;
  memcpy(t->relations[base+i],relation.relation,28);
  if(!relation.admitted)continue;
  accepted[i]=1;accepted_count++;
  if(overlap(t,i,current,relation.relation+1,&scores[i]))goto done;
  if(relation.inliers>best){best=relation.inliers;best_prior=i;}
  if(word(t->features[i].scalar_header+0x110)==1&&relation.inliers>best_connected){best_connected=relation.inliers;best_connected_prior=i;}
 }
 t->stored.header[6]=current?base+current:1;
 matrix->feature_count=current+1;matrix->reference_feature_index=(size_t)t->stored.graph[0];matrix->graph_established=t->stored.graph[3];memcpy(matrix->slots,t->relations+1,sizeof(matrix->slots));
 for(unsigned i=0;i<=current;i++){matrix->row_bases[i]=word(t->features[i].scalar_header+0x114);active[i]=word(t->features[i].scalar_header+0x110);}
 if(current&&accepted_count){
  packed=0x64000000;int32_t current_to_reference[6];memcpy(current_to_reference,identity,24);
  if(best_connected>=0){
   unsigned prior=best_connected_prior;GoodixMilanRelationSlot *direct=goodix_milan_enrollment_relation_slot(matrix,current,prior);int32_t prior_to_reference[6];
   if(!direct||goodix_milan_enrollment_feature_to_reference(matrix,prior,prior_to_reference))goto done;
   if(prior==matrix->reference_feature_index)memcpy(current_to_reference,direct->values+1,24);
   else goodix_milan_transform_compose(prior_to_reference,direct->values+1,current_to_reference);
   GoodixMilanRelationSlot *root=goodix_milan_enrollment_relation_slot(matrix,current,matrix->reference_feature_index);if(!root)goto done;
   memcpy(root->values+1,current_to_reference,24);if(root->values[0]<0)root->values[0]=0;active[current]=1;packed+=(prior+1)<<8;
  }else if(!matrix->graph_established&&best>5&&scores[best_prior]>205){
   matrix->reference_feature_index=best_prior;matrix->graph_established=1;active[current]=active[best_prior]=1;
   memcpy(current_to_reference,matrix->slots[base-1+best_prior].values+1,24);packed+=(best_prior+1)<<8;
  }
  if(matrix->graph_established&&active[current]){
   Propagation context={t,matrix};int bridge[50]={0};
   for(unsigned prior=0;prior<current;prior++)bridge[prior]=accepted[prior]&&!active[prior];
   for(unsigned prior=0;prior<current;prior++)if(bridge[prior]&&scores[prior]>208){
    int32_t inverse[6],routed[6];if(goodix_milan_transform_invert(matrix->slots[base-1+prior].values+1,inverse))goto done;
    goodix_milan_transform_compose(current_to_reference,inverse,routed);
    if(goodix_milan_relation_matrix_store_reference(matrix,prior,routed))goto done;
    active[prior]=1;
    if(goodix_milan_enrollment_propagate_lower(matrix,active,prior,routed,205,propagation_overlap,&context)||goodix_milan_enrollment_propagate_higher(matrix,active,prior,current,routed,205,propagation_overlap,&context))goto done;
   }
  }
  int32_t residual=5120;
  if(matrix->graph_established&&active[current]){
   uint8_t bits[640];memcpy(bits,f->packed_mask,640);
   for(unsigned prior=0;prior<current;prior++)if(active[prior]&&word(t->features[prior].scalar_header+0x124)!=5){
    int32_t prior_to_root[6],root_to_prior[6],transform[6];
    if(goodix_milan_enrollment_feature_to_reference(matrix,prior,prior_to_root)||goodix_milan_transform_invert(prior_to_root,root_to_prior))goto done;
    goodix_milan_transform_compose(root_to_prior,current_to_reference,transform);Gx521dMatchControllerState view;if(project(transform,&view))goto done;
    for(int32_t y=view.rows[0];y<=view.rows[1];y++)for(int32_t x=view.rows[2+y];x<=view.rows[258+y];x++)bits[y*10+x/8]&=(uint8_t)~(1U<<(x%8));
   }
   residual=0;for(unsigned i=0;i<640;i++){unsigned b=bits[i];while(b){b&=b-1;residual++;}}if(residual<20)residual=0;
  }else{
   unsigned prior_selected=0;
   for(unsigned prior=0;prior<current;prior++)if(accepted[prior]&&scores[prior]>205){Gx521dMatchControllerState view;if(project(matrix->slots[base-1+prior].values+1,&view))goto done;int32_t missing=5120-view.area;if(missing<residual){residual=missing;prior_selected=prior;}}
   packed+=(prior_selected+1)<<8;
  }
  packed+=(uint32_t)(residual*100/5121);
  for(unsigned prior=current;prior-->0;){
   if(word(t->features[prior].scalar_header+0x124)==5)continue;
   if(matrix->slots[base-1+prior].values[0]>=6){Gx521dMatchControllerState view;if(project(matrix->slots[base-1+prior].values+1,&view))goto done;packed=(packed&0xffffff)|((uint32_t)((5120-view.area)*100/5121)<<24);}
   break;
  }
 }
 if(current==49&&goodix_milan_relation_matrix_close(matrix,active)<0)goto done;
 memcpy(t->relations+1,matrix->slots,sizeof(matrix->slots));t->stored.graph[0]=(int32_t)matrix->reference_feature_index;t->stored.graph[3]=matrix->graph_established;
 for(unsigned i=0;i<=current;i++)put(t->features[i].scalar_header+0x110,active[i]);
 for(unsigned i=0;i<50;i++)put(t->stored.tail+4*i,i<=current?(int32_t)i:-1);
 if(sync(t))goto done;
 *owned=*t;*status=0;*progress=packed;result=0;
 done:gx521d_template_delete(t);memset(pair,0,sizeof(*pair));free(pair);free(matrix);return result;
}
