/* SPDX-License-Identifier: MIT
 * Type17 record metrics: independently transcribed RVAs90730,8fd10.
 * Source point positions and duplicate pair flags are compared as unsigned words.
 */
#include "521d-match-record-metrics.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>
static uint32_t word(const GoodixMilanFeatureRecord *r,unsigned off)
{uint32_t v;memcpy(&v,(const uint8_t *)r+off,4);return v;}
static int same_position(const GoodixMilanFeatureRecord *a,const GoodixMilanFeatureRecord *b)
{return a->refined_x==b->refined_x&&a->refined_y==b->refined_y;}
static int32_t round_coordinate(int64_t n)
{return (int32_t)(n>0?(n+128)>>8:-((128-n)>>8));}
int gx521d_match_record_metrics(const Gx521dMatchRecordMetricsInput *in,Gx521dMatchRecordMetrics *out)
{
 if(!in||!out||in->features.enrolled_count>120||in->features.probe_count>120)return -1;
 for(unsigned i=0;i<6;i++)if(in->affine[i]<-1048576||in->affine[i]>1048576)return -1;
 const Gx521dMatchDistanceInput *f=&in->features;Gx521dMatchRecordMetrics result={0};
 GoodixMilanFeatureRecord probe[120];memcpy(probe,f->probe,sizeof(probe));
 for(unsigned i=0;i+1<f->probe_count;i++){
  uint32_t own=word(&probe[i],28);memcpy((uint8_t *)&probe[i]+48,&own,4);
  for(unsigned j=i+1;j<f->probe_count;j++)if(same_position(&probe[i],&probe[j])){
   uint32_t other=word(&probe[j],28);memcpy((uint8_t *)&probe[i]+48,&other,4);memcpy((uint8_t *)&probe[j]+48,&own,4);
  }
 }
 uint16_t flags[120]={0},coordinates[120][4]={{0}};
 for(unsigned i=0;i<f->probe_count;i++){
  result.probe_secondary[i]=word(&probe[i],48);
  uint16_t x=(uint16_t)probe[i].refined_x,y=(uint16_t)probe[i].refined_y;
  const int32_t *a=in->affine;
  int32_t px=round_coordinate((int64_t)a[0]*x+(int64_t)a[1]*y+(int64_t)a[2]*256);
  int32_t py=round_coordinate((int64_t)a[3]*x+(int64_t)a[4]*y+(int64_t)a[5]*256);
  if(px<0||px>=80*256||py<0||py>=64*256)continue;
  unsigned j=0;for(;j<i;j++)if(same_position(&probe[i],&probe[j]))break;
  if(j==i)result.values[0]++;
  int best=-1,squared=INT32_MAX;
  for(j=0;j<f->enrolled_count;j++){
   if((f->enrolled[j].foreground&3)!=(probe[i].foreground&3))continue;
   int dx=px-(uint16_t)f->enrolled[j].refined_x,dy=py-(uint16_t)f->enrolled[j].refined_y;
   if(dx>256||dx<-256||dy>256||dy<-256)continue;
   int d=dx*dx+dy*dy;if(d<squared){squared=d;best=(int)j;}
  }
  if(best<0||squared>=65536)continue;
  coordinates[i][0]=x;coordinates[i][1]=y;coordinates[i][2]=(uint16_t)f->enrolled[best].refined_x;coordinates[i][3]=(uint16_t)f->enrolled[best].refined_y;
  unsigned previous=0;for(j=0;j<i;j++)if(flags[j]&&!memcmp(coordinates[i],coordinates[j],8)){previous=flags[j];break;}
  unsigned d1=(unsigned)__builtin_popcount(word(&f->enrolled[best],28)^word(&probe[i],28));
  unsigned d2=(unsigned)__builtin_popcount(word(&f->enrolled[best],28)^word(&probe[i],48));
  int distance=(int)(d1<d2?d1:d2);unsigned current=0;
  if(!(previous&1)){result.values[1]++;result.values[4]+=distance;current=1;}
  if(squared<16384&&!(previous&2)){result.values[7]++;current+=2;}
  if(distance<=5&&!(previous&32)){result.enrolled_mask[best]=1;result.values[5]++;current+=32;}
  flags[i]=(uint16_t)current;
 }
 if(result.values[1])result.values[4]/=result.values[1];
 if(result.values[0]){result.values[8]=result.values[7]*100/result.values[0];result.values[2]=result.values[1]*100/result.values[0];}
 *out=result;return 0;
}
