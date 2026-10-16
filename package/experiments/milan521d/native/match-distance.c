/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 partitioned bidirectional descriptor reductions, reference RVA90340.
 * Descriptor Hamming mechanism also appears in upstream Milan correspondence.c. */
#include "521d-match-distance.h"
#include <string.h>
static int hamming(const uint8_t *a,const uint8_t *b,unsigned bytes)
{
 int count=0;
 for(unsigned i=0;i<bytes;i++){
  unsigned bits=a[i]^b[i];while(bits){bits&=bits-1;count++;}
 }
 return count;
}
static void update(int32_t distance[2],int32_t index[2],int value,unsigned candidate)
{
 if(value<distance[0]){distance[1]=distance[0];index[1]=index[0];distance[0]=value;index[0]=(int32_t)candidate;}
 else if(value<distance[1]){distance[1]=value;index[1]=(int32_t)candidate;}
}
int gx521d_match_distances(const Gx521dMatchDistanceInput *in,Gx521dMatchDistances *out)
{
 if(!in||!out||in->enrolled_count>120||in->probe_count>120||in->enrolled_partition>in->enrolled_count||in->probe_partition>in->probe_count||in->config[0]<-1||in->config[0]>64||in->config[1]<-1||in->config[1]>128||in->config[4]<0||in->config[4]>16384)return 0;
 Gx521dMatchDistances result;
 for(unsigned group=0;group<3;group++)for(unsigned i=0;i<120;i++)for(unsigned j=0;j<2;j++){result.distance[group][i][j]=192;result.index[group][i][j]=-1;}
 for(unsigned i=0;i<in->enrolled_count;i++){
  unsigned begin=i<in->enrolled_partition?0:in->probe_partition;
  unsigned end=i<in->enrolled_partition?in->probe_partition:in->probe_count;
  const GoodixMilanFeatureRecord *a=&in->enrolled[i];
  for(unsigned j=begin;j<end;j++){
   const GoodixMilanFeatureRecord *b=&in->probe[j];
   int distance=hamming(a->payload+4,b->payload+4,8);
   if(distance>in->config[0])continue;
   distance+=hamming(a->payload+12,b->payload+12,8);
   if(distance>in->config[1])continue;
   distance+=hamming(a->payload+20,b->payload+20,8);
   update(result.distance[0][i],result.index[0][i],distance,j);
   if(in->config[4]>0&&(a->foreground>>2)<in->config[4]&&(b->foreground>>2)<in->config[4])
    update(result.distance[1][i],result.index[1][i],distance,j);
   update(result.distance[2][j],result.index[2][j],distance,i);
  }
 }
 *out=result;return 1;
}
