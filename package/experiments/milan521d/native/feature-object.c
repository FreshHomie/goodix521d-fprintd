/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "521d-feature-object.h"
#include <stdlib.h>
#include <string.h>
static int quantile(const uint8_t *image,const uint8_t *mask,int fraction)
{
 uint32_t histogram[256]={0},active=0;for(int i=0;i<GX521D_PIXELS;i++)if(mask[i]){histogram[image[i]]++;active++;}
 uint32_t target=(fraction*active+128)>>8;
 for(int v=1;v<256;v++){histogram[v]+=histogram[v-1];if(histogram[v]>=target)return (int32_t)(histogram[v]-target)<=(int32_t)(target-histogram[v-1])?v:v-1;}
 return 0;
}
static void set32(uint8_t *out,int offset,int value){memcpy(out+offset,&value,4);}
int gx521d_feature_extract(const uint8_t image[GX521D_PIXELS],int quality,int coverage,int mode,Gx521dFeatureObject *out)
{
 if(!image||!out||quality<0||quality>100||coverage<0||coverage>100||(mode!=0&&mode!=1))return -1;
 Gx521dFeatureBase base;Gx521dFeatureScale *space=malloc(sizeof(*space));Gx521dFeatureRecords *records=malloc(sizeof(*records));
 if(!space||!records){free(space);free(records);return -1;}
 int capacity=(coverage*120+50)/100;
 if(gx521d_feature_base(image,&base)||gx521d_feature_scale_type17(image,space)||gx521d_feature_records(image,capacity,records)){free(space);free(records);return -1;}
 int tagged=1;for(int i=0;i<72;i++)if((image[i]&1)!=(i&1)){tagged=0;break;}
 int tag=0;if(tagged){static const int modes[4]={0,1,3,2};tag=modes[(image[72]&1)|((image[73]&1)<<1)]+(((image[75]&1)|((image[76]&1)<<1)|((image[77]&1)<<2))<<8);}
 memset(out,0,sizeof(*out));set32(out->scalar_header,0x150,tag);set32(out->scalar_header,0,80);set32(out->scalar_header,4,64);set32(out->scalar_header,0x11c,quality);set32(out->scalar_header,0x120,coverage);
 memcpy(out->validity,base.validity,sizeof(out->validity));int high=quantile(space->enhanced,out->validity,205),low=quantile(space->enhanced,out->validity,50);
 for(int i=0;i<GX521D_PIXELS;i++){if(space->enhanced[i]>high)out->high[i/8]|=1U<<(i&7);if(space->enhanced[i]<low)out->low[i/8]|=1U<<(i&7);}
 int count=records->count;memcpy(out->records,records->records,sizeof(out->records));
 for(int i=0;i<count;i++){
  int x=((uint16_t)out->records[i].refined_x+128)>>8,y=((uint16_t)out->records[i].refined_y+128)>>8;
  if(x<80&&y<64&&!out->validity[y*80+x]){count--;if(i!=count){out->records[i]=out->records[count];memset(&out->records[count],0,sizeof(out->records[count]));}i--;}
 }
 uint8_t final_validity[GX521D_PIXELS];memcpy(final_validity,out->validity,sizeof(final_validity));
 if(tagged&&(image[74]&1)) {
  static const int mapped[8]={0,1,0,4,5,5,0,0};int clear=mapped[(tag>>8)&7]>=4,radius=clear?5:2;
  uint16_t prefix[65][81]={{0}},density[GX521D_PIXELS];int total=0;
  for(int y=0;y<64;y++)for(int x=0;x<80;x++){
   int bad=y>0&&!(image[y*80+x]&1);total+=bad;
   prefix[y+1][x+1]=(uint16_t)(bad+prefix[y][x+1]+prefix[y+1][x]-prefix[y][x]);
  }
  if(total>=50) {
   for(int y=0;y<64;y++)for(int x=0;x<80;x++){
    int left=x-radius;if(left<0)left=0;int right=x+radius+1;if(right>80)right=80;
    int top=y-radius;if(top<0)top=0;int bottom=y+radius+1;if(bottom>64)bottom=64;
    density[y*80+x]=(uint16_t)(prefix[bottom][right]-prefix[top][right]-prefix[bottom][left]+prefix[top][left]);
    if(clear&&density[y*80+x])final_validity[y*80+x]=0;
   }
   for(int i=0;i<count;i++){
    int x=((uint16_t)out->records[i].refined_x+128)>>8,y=((uint16_t)out->records[i].refined_y+128)>>8;
    if(x<80&&y<64&&density[y*80+x]>1){count--;if(i!=count)out->records[i]=out->records[count];i--;}
   }
  }
 }
 memset(out->records+count,0,(GX521D_RECORDS_MAX-count)*sizeof(out->records[0]));
 set32(out->scalar_header,0xf0,count);
 set32(out->scalar_header,0x118,(int)goodix_milan_feature_partition_records((uint8_t *)out->records,count));
 memset(out->scalar_header+0x28,255,200);memset(out->scalar_header+0x28,0,40);
 for(int y=0;y<16;y++)for(int x=0;x<20;x++){
  int sum=0;for(int dy=0;dy<4;dy++)for(int dx=0;dx<4;dx++)sum+=final_validity[(y*4+dy)*80+x*4+dx];
  int value=sum*2>=16,bit=y*20+x;if(value)out->scalar_header[0x28+bit/8]|=1U<<(bit&7);
  for(int dy=0;dy<4;dy++)for(int dx=0;dx<4;dx++){int pixel=(y*4+dy)*80+x*4+dx;if(value)out->packed_mask[pixel/8]|=1U<<(pixel&7);}
 }
 free(space);free(records);return 0;
}
