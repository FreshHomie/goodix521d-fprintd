/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 primary correspondence owner8fe10 and selector91320.
 * Spatial reduction adapted from Milan match/correspondence.c,
 * (C)2026 goodix-fp-linux-dev contributors. */
#include "521d-match-pairs.h"
#include <string.h>
typedef struct {int32_t enrolled,probe,best,second;} Pair;
static void select_pairs(const GoodixMilanFeatureRecord *coordinates,unsigned count,
                         const int32_t distance[120][2],const int32_t index[120][2],
                         unsigned limit,const int32_t config[5],int32_t out[31][2])
{
 Pair pairs[31];unsigned used=0;
 for(unsigned row=0;row<count;row++){
  int best=index[row][0];if(best<0||distance[row][0]*config[2]>=distance[row][1]*config[3])continue;
  Pair candidate={(int32_t)row,best,distance[row][0],distance[row][1]};
  uint32_t x=(uint16_t)coordinates[best].refined_x,y=(uint16_t)coordinates[best].refined_y;
  int replaced=0;
  for(unsigned i=0;i<used;i++){
   const GoodixMilanFeatureRecord *selected=&coordinates[pairs[i].probe];
   uint32_t dx=x-(uint16_t)selected->refined_x,dy=y-(uint16_t)selected->refined_y;
   uint32_t bits=dx*dx+dy*dy;int32_t square;memcpy(&square,&bits,4);
   if(square>=65536)continue;
   if(candidate.best>=pairs[i].best){if(!replaced){replaced=-1;break;}continue;}
   if(!replaced){pairs[i]=candidate;replaced=1;}
   else {pairs[i]=pairs[used-1];used--;}
  }
  if(replaced)continue;
  if(used==limit){
   unsigned worst=0;for(unsigned i=1;i<used;i++)if(pairs[i].best>pairs[worst].best)worst=i;
   if(candidate.best<pairs[worst].best)pairs[worst]=candidate;
  }else pairs[used++]=candidate;
 }
 for(unsigned i=0;i<used;i++){out[i][0]=pairs[i].enrolled;out[i][1]=pairs[i].probe;}
}
int gx521d_match_pairs(const Gx521dMatchPairsInput *in,Gx521dMatchPairs *out)
{
 if(!in||!out||in->enrolled_coverage>100||in->probe_coverage>100||in->features.config[2]<0||in->features.config[2]>1000||in->features.config[3]<0||in->features.config[3]>1000)return 0;
 Gx521dMatchDistances d;if(!gx521d_match_distances(&in->features,&d))return 0;
 unsigned a=in->enrolled_coverage,b=in->probe_coverage,limit;
 if(a<80&&b<80)limit=(31*a*b+5000)/10000;
 else limit=31*(a<b?a:b)/100;
 if(limit<5)limit=5;
 Gx521dMatchPairs result;memset(&result,255,sizeof(result));
 select_pairs(in->features.probe,in->features.enrolled_count,d.distance[0],d.index[0],limit,in->features.config,result.pairs[0]);
 select_pairs(in->features.probe,in->features.enrolled_count,d.distance[1],d.index[1],limit,in->features.config,result.pairs[1]);
 int32_t reverse[31][2];memset(reverse,255,sizeof(reverse));
 select_pairs(in->features.enrolled,in->features.probe_count,d.distance[2],d.index[2],31,in->features.config,reverse);
 for(unsigned i=0;i<31;i++){result.pairs[2][i][0]=reverse[i][1];result.pairs[2][i][1]=reverse[i][0];}
 *out=result;return 1;
}
