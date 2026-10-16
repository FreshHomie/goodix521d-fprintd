/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Default type17 candidate measurement path of owner62a00. */
#include "521d-match-candidate.h"
#include "521d-match-correspondence.h"
#include "521d-match-post-refine.h"
#include "521d-match-controller-state.h"
#include "521d-match-affine-details.h"
#include <string.h>
#include <stddef.h>
static int32_t word(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
static void put(uint8_t *p,int32_t v){memcpy(p,&v,4);}
static int valid(const Gx521dFeatureObject *a)
{
 const uint8_t *h=a->scalar_header;
 return word(h)==80&&word(h+4)==64&&word(h+0xf0)>=0&&word(h+0xf0)<=120&&word(h+0x118)>=0&&word(h+0x118)<=word(h+0xf0)&&word(h+0x108)==0&&word(h+0x150)==0&&word(h+0x11c)>=0&&word(h+0x11c)<=100&&word(h+0x120)>=0&&word(h+0x120)<=100;
}
static int candidate(const Gx521dMatchCandidateInput *in,Gx521dMatchCandidate *out,int *progress,int rescue)
{
 if(!in||!out||!valid(&in->enrolled)||!valid(&in->probe))return -1;
 Gx521dMatchCandidate r={0};Gx521dMatchControllerStateInput ci={.affine={256,0,0,0,256,0},.selector=in->selector,.flags=in->flags};Gx521dMatchControllerState cs;
 if(gx521d_match_controller_state(&ci,&cs)||cs.status)return -1;
 const uint8_t *a=in->enrolled.scalar_header,*b=in->probe.scalar_header;
 if(!word(b+0xf0)){*out=r;return 0;}
 Gx521dMatchCorrespondenceInput corr={0};Gx521dMatchDistanceInput *f=&corr.input.features;
 f->enrolled_count=(uint32_t)word(a+0xf0);f->probe_count=(uint32_t)word(b+0xf0);
 f->enrolled_partition=(uint32_t)word(a+0x118);f->probe_partition=(uint32_t)word(b+0x118);
 memcpy(f->config,cs.config+6,20);memcpy(f->enrolled,in->enrolled.records,sizeof(f->enrolled));memcpy(f->probe,in->probe.records,sizeof(f->probe));
 corr.input.enrolled_coverage=(uint32_t)word(a+0x120);corr.input.probe_coverage=(uint32_t)word(b+0x120);
 memcpy(corr.enrolled_mask,a+40,40);memcpy(corr.probe_mask,b+40,40);
 Gx521dMatchCorrespondence co;if(gx521d_match_correspondence(&corr,&co))return -1;
 int32_t primary=word(co.candidate),auxiliary=word(co.candidate+12);
 if(primary<=(rescue?6:4)){*out=r;return 0;}
 if(progress)*progress=1;
 memcpy(ci.affine,co.affine,24);if(gx521d_match_controller_state(&ci,&cs))return -1;
 int readiness=3;for(int i=2;i>=0;i--)if(!cs.readiness[i]){readiness=2-i;break;}
 if(progress)*progress=readiness+1;
 Gx521dMatchPostRefineInput post={0};post.features=*f;memcpy(post.image.affine,co.affine,24);
 post.image.mode=1;post.image.context_count=primary;post.image.enhanced=1;
 memcpy(post.image.probe_high,in->probe.high,640);memcpy(post.image.probe_low,in->probe.low,640);memcpy(post.image.probe_mask,b+40,40);
 memcpy(post.image.enrolled_high,in->enrolled.high,640);memcpy(post.image.enrolled_low,in->enrolled.low,640);memcpy(post.image.enrolled_mask,a+40,40);
 Gx521dMatchOverlap overlap;if(gx521d_match_overlap_diagnostic(&post.image,&overlap))return -1;
 int base=cs.config[0]+(rescue?2:0),offset=cs.config[1]+(rescue?1:0);
 if(overlap.score<=207+base){*out=r;return 0;}
 memcpy(post.candidate,co.candidate,sizeof(post.candidate));put(post.candidate+16,overlap.score);put(post.candidate+20,overlap.detail);
 put(post.candidate+36,(int32_t)(((uint32_t)overlap.reserved<<16)+(uint32_t)overlap.coverage));
 Gx521dMatchPostRefine refined;if(gx521d_match_post_refine(&post,&refined))return -1;
 int32_t details[3];if(gx521d_match_affine_details(refined.affine,details))return -1;
 r.measured=1;memcpy(r.affine,refined.affine,24);memcpy(r.candidate,refined.candidate,sizeof(r.candidate));
 int32_t *m=r.metrics;int p=primary-offset,q=auxiliary-offset,score=word(r.candidate+16),detail=word(r.candidate+20);uint32_t packed=(uint32_t)word(r.candidate+36);
 m[0]=p>5?p:5;m[1]=q>5?q:5;m[12]=word(r.candidate+4)-2*offset;if(m[12]<m[0])m[12]=m[0];
 m[2]=score-base;m[4]=detail-base;m[3]=base>0?m[2]:score;m[5]=base>0?m[4]:detail;
 if(p<5){m[2]-=4;m[4]-=4;}if(q<5){m[3]-=4;m[5]-=4;}
 m[6]=(int32_t)(packed&65535)*5120/256;m[7]=(int32_t)(packed>>16);
 m[8]=word(a+0x11c);m[9]=word(b+0x11c);m[10]=word(a+0x120);m[11]=word(b+0x120);
 if(rescue&&m[0]<8&&m[6]<1500){m[2]-=4;m[4]-=4;}
 m[13]=word(r.candidate+40);m[14]=word(r.candidate+0x128);m[15]=(uint32_t)(details[0]-170)>214;m[17]=details[2]>15000;m[23]=readiness;
 *out=r;return 0;
}

int gx521d_match_candidate(const Gx521dMatchCandidateInput *in,Gx521dMatchCandidate *out)
{return candidate(in,out,NULL,0);}
int gx521d_match_candidate_progress(const Gx521dMatchCandidateInput *in,Gx521dMatchCandidate *out,int *progress)
{if(!progress)return -1;int stage=0;int status=candidate(in,out,&stage,0);if(!status)*progress=stage;return status;}

int gx521d_match_candidate_rescue(const Gx521dMatchCandidateInput *in,Gx521dMatchCandidate *out,int *progress){if(!progress)return -1;int stage=0;int status=candidate(in,out,&stage,1);if(!status)*progress=stage;return status;}
