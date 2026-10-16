/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 scale branch adapted from pinned Milan feature mechanisms.
 * Numeric Gaussian constants:3.0.21.290 RVA0xf0c80, selectors200..208.
 */
#include "521d-feature-scale.h"
#include "milan/feature/feature.h"
#include "milan/private.h"
#include <string.h>
static const struct {int length;uint16_t taps[11];} kernels[9]={
 {9,{84,957,5433,15398,21790,15398,5433,957,84}},
 {5,{204,11222,42683,11222,204}},
 {7,{2,548,13185,38067,13185,548,2}},
 {7,{17,1171,14621,33919,14621,1171,17}},
 {7,{74,2089,15496,30219,15496,2089,74}},
 {7,{228,3230,15847,26926,15847,3230,228}},
 {9,{29,543,4456,15747,23985,15747,4456,543,29}},
 {9,{102,1058,5619,15304,21371,15304,5619,1058,102}},
 {11,{25,274,1752,6594,14605,19037,14605,6594,1752,274,25}}
};
static void filter(const uint16_t *source,uint16_t *output,int level)
{
 uint16_t horizontal[GX521D_PIXELS];int radius=kernels[level].length/2;
 for(int y=0;y<64;y++)for(int x=0;x<80;x++) {
  uint64_t sum=0;for(int k=0;k<kernels[level].length;k++)sum+=(uint64_t)source[y*80+goodix_milan_reflect101_index(x+k-radius,80)]*kernels[level].taps[k];
  horizontal[y*80+x]=(uint16_t)(sum>>16);
 }
 for(int y=0;y<64;y++)for(int x=0;x<80;x++) {
  uint64_t sum=0;for(int k=0;k<kernels[level].length;k++)sum+=(uint64_t)horizontal[goodix_milan_reflect101_index(y+k-radius,64)*80+x]*kernels[level].taps[k];
  output[y*80+x]=(uint16_t)(sum>>16);
 }
}
int gx521d_feature_scale_prepared(const uint8_t blurred[GX521D_PIXELS],const uint8_t enhanced[GX521D_PIXELS],Gx521dFeatureScale *out)
{
 if(!blurred||!enhanced||!out)return -1;
 uint16_t lifted[GX521D_PIXELS];memset(out,0,sizeof(*out));
 memcpy(out->blurred,blurred,GX521D_PIXELS);memcpy(out->enhanced,enhanced,GX521D_PIXELS);
 for(int i=0;i<GX521D_PIXELS;i++){lifted[i]=(uint16_t)blurred[i]<<8;out->gradient_input[i]=(int32_t)enhanced[i]<<8;}
 filter(lifted,out->scales[0],0);
 for(int level=1;level<GX521D_FEATURE_SCALES;level++)filter(out->scales[level-1],out->scales[level],level);
 for(int y=1;y<63;y++)for(int x=1;x<79;x++) {
  int i=y*80+x;int32_t horizontal=out->gradient_input[i+1]-out->gradient_input[i-1],vertical=out->gradient_input[i+80]-out->gradient_input[i-80];
  horizontal=(int32_t)((uint32_t)horizontal<<12);vertical=(int32_t)((uint32_t)vertical<<12);
  out->orientation[i]=(int16_t)feature_cordic(vertical,&horizontal);
  out->magnitude[i]=(uint32_t)(horizontal>>12);if(out->magnitude[i]>0x3ffff)out->magnitude[i]=0x3ffff;
 }
 return 0;
}
int gx521d_feature_scale(const uint8_t image[GX521D_PIXELS],Gx521dFeatureScale *out)
{
 if(!image||!out)return -1;
 Gx521dFeatureBase base;uint8_t dense[GX521D_PIXELS],enhanced[GX521D_PIXELS];
 if(gx521d_feature_base(image,&base)||goodix_milan_feature_enhance(image,64,80,dense,enhanced))return -1;
 return gx521d_feature_scale_prepared(base.blurred,enhanced,out);
}
int gx521d_feature_scale_type17(const uint8_t image[GX521D_PIXELS],Gx521dFeatureScale *out)
{
 if(!image||!out)return -1;
 static const uint16_t taps[5]={3571,16004,26386,16004,3571};
 uint16_t horizontal[GX521D_PIXELS];uint8_t blurred[GX521D_PIXELS],enhanced[GX521D_PIXELS],dense[GX521D_PIXELS];
 for(int y=0;y<64;y++)for(int x=0;x<80;x++) {
  uint64_t sum=0;for(int k=0;k<5;k++)sum+=((uint64_t)image[y*80+goodix_milan_reflect101_index(x+k-2,80)]<<8)*taps[k];
  horizontal[y*80+x]=(uint16_t)(sum>>16);
 }
 for(int y=0;y<64;y++)for(int x=0;x<80;x++) {
  uint64_t sum=0;for(int k=0;k<5;k++)sum+=(uint64_t)horizontal[goodix_milan_reflect101_index(y+k-2,64)*80+x]*taps[k];
  blurred[y*80+x]=(uint8_t)(sum>>24);
 }
 if(goodix_milan_feature_enhance(image,64,80,dense,enhanced))return -1;
 return gx521d_feature_scale_prepared(blurred,enhanced,out);
}
