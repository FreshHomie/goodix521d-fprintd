/* SPDX-License-Identifier: MIT
 * Type17 interior record topology owner91940, optional auxiliary records absent. */
#include "521d-match-topology.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>
static int32_t coordinate(int64_t n){return (int32_t)(n>0?(n+128)>>8:-((128-n)>>8));}
static uint32_t word(const GoodixMilanFeatureRecord *r,unsigned off){uint32_t v;memcpy(&v,(const uint8_t *)r+off,4);return v;}
int gx521d_match_topology(const Gx521dMatchTopologyInput *in,int32_t out[11])
{
 if(!in||!out||in->input.features.enrolled_count>120||in->input.features.probe_count>120||in->filtered>31)return -1;
 for(unsigned i=0;i<6;i++)if(in->input.affine[i]<-1048576||in->input.affine[i]>1048576)return -1;
 const Gx521dMatchDistanceInput *f=&in->input.features;const int32_t *a=in->input.affine;int32_t result[11]={0};
 for(unsigned i=0;i<f->probe_count;i++){
  uint16_t x=(uint16_t)f->probe[i].refined_x,y=(uint16_t)f->probe[i].refined_y;
  int px=coordinate((int64_t)a[0]*x+(int64_t)a[1]*y+(int64_t)a[2]*256),py=coordinate((int64_t)a[3]*x+(int64_t)a[4]*y+(int64_t)a[5]*256);
  if(px<1536||px>=73*256||py<1536||py>=57*256)continue;
  result[0]++;int best=-1,squared=INT32_MAX;
  for(unsigned j=0;j<f->enrolled_count;j++){
   if((f->enrolled[j].foreground&3)!=(f->probe[i].foreground&3))continue;
   int dx=px-(uint16_t)f->enrolled[j].refined_x,dy=py-(uint16_t)f->enrolled[j].refined_y;
   if(dx>512||dx<-512||dy>512||dy<-512)continue;
   int d=dx*dx+dy*dy;if(d<squared){squared=d;best=(int)j;}
  }
  if(best<0||squared>=262144)continue;
  int distance=__builtin_popcount(word(&f->enrolled[best],28)^word(&f->probe[i],28));
  if(distance<=5&&!in->enrolled_mask[best]&&((int32_t)word(&f->enrolled[best],12)>0||(int32_t)word(&f->probe[i],12)>0))result[5]++;
  result[1]++;result[4]+=distance;
 }
 if(result[1])result[4]/=result[1];
 if(result[0]){result[2]=result[1]*100/result[0];result[3]=(int32_t)in->filtered*100/result[0];}
 memcpy(out,result,sizeof(result));return 0;
}
