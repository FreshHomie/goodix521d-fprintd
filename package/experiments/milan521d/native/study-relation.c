/* SPDX-License-Identifier: LGPL-2.1-or-later; type17 study92c00 fallback. */
#include "521d-study-relation.h"
#include "521d-match-pair-fit.h"
#include "521d-match-overlap.h"
#include <string.h>
static int32_t word(const uint8_t *p){int32_t v;memcpy(&v,p,4);return v;}
int gx521d_study_relation(const Gx521dMatchCandidateInput *in,int32_t relation[7])
{
 if(!in||!relation)return -1;
 const Gx521dFeatureObject *a=&in->enrolled,*b=&in->probe;
 const uint8_t *ah=a->scalar_header,*bh=b->scalar_header;
 if(word(ah)!=80||word(ah+4)!=64||word(bh)!=80||word(bh+4)!=64||word(ah+0xf0)<0||word(ah+0xf0)>120||word(bh+0xf0)<0||word(bh+0xf0)>120||word(ah+0x150)||word(bh+0x150))return -1;
 Gx521dMatchPairsInput pairs={0};pairs.features.enrolled_count=(uint32_t)word(ah+0xf0);pairs.features.probe_count=(uint32_t)word(bh+0xf0);
 pairs.features.enrolled_partition=(uint32_t)word(ah+0x118);pairs.features.probe_partition=(uint32_t)word(bh+0x118);
 pairs.enrolled_coverage=(uint32_t)word(ah+0x120);pairs.probe_coverage=(uint32_t)word(bh+0x120);
 int32_t config[5]={22,45,40,38,0};memcpy(pairs.features.config,config,20);
 memcpy(pairs.features.enrolled,a->records,sizeof(a->records));memcpy(pairs.features.probe,b->records,sizeof(b->records));
 Gx521dMatchPairs selected;if(!gx521d_match_pairs(&pairs,&selected))return -1;
 int32_t out[7]={-1,256,0,0,0,256,0};unsigned count=0;while(count<31&&selected.pairs[0][count][0]>=0)count++;
 if(count>4){
  /* Preserve owner92c00's record-array order rather than substituting the
   * initial-recognition correspondence controller. Its primary-pair indices
   * are applied to the opposite arrays here, including zeroed unused slots. */
  Gx521dMatchPairFitInput fit={.minimum_inliers=2};fit.features.enrolled_count=fit.features.probe_count=120;
  memcpy(fit.features.enrolled,b->records,sizeof(b->records));memcpy(fit.features.probe,a->records,sizeof(a->records));memcpy(fit.pairs,selected.pairs[0],sizeof(fit.pairs));
  Gx521dMatchGeometry geometry;if(!gx521d_match_geometry_pairs(&fit,&geometry))return -1;
  int32_t inliers=0;for(unsigned i=0;i<42;i++)inliers+=geometry.mask[i]!=0;
  Gx521dMatchOverlap overlap={0};
  if(inliers>4){
   Gx521dMatchOverlapInput image={.enhanced=1};memcpy(image.affine,geometry.affine,24);
   memcpy(image.enrolled_high,a->high,640);memcpy(image.enrolled_low,a->low,640);memcpy(image.enrolled_mask,ah+40,40);
   memcpy(image.probe_high,b->high,640);memcpy(image.probe_low,b->low,640);memcpy(image.probe_mask,bh+40,40);
   if(gx521d_match_overlap(&image,&overlap))return -1;
  }
  if(inliers>=15||(inliers>=10&&(overlap.score>210||overlap.detail>=205))||(inliers>=7&&(overlap.score>220||overlap.detail>=210))||(inliers>=5&&(overlap.score>228||overlap.detail>=215))){out[0]=inliers;memcpy(out+1,geometry.affine,24);}
 }
 memcpy(relation,out,sizeof(out));return 0;
}
