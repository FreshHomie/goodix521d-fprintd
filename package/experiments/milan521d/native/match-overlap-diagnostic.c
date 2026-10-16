/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Full-resolution image mismatch diagnostic, owners8b980,8bc20,8c010. */
#include "521d-match-overlap-diagnostic.h"
#include "match-overlap-internal.h"
static unsigned filtered_mismatches(const uint8_t input[5120],size_t rows,size_t cols)
{
 uint8_t eroded[5120]={0},dilated[5120]={0};
 for(size_t y=1;y<rows;y++)for(size_t x=1;x<cols;x++)
  eroded[y*cols+x]=input[y*cols+x]&&input[y*cols+x-1]&&input[(y-1)*cols+x]&&input[(y-1)*cols+x-1];
 unsigned count=0;
 for(size_t y=0;y<rows;y++)for(size_t x=0;x<cols;x++){
  dilated[y*cols+x]=eroded[y*cols+x]||(x&&eroded[y*cols+x-1])||(y&&eroded[(y-1)*cols+x])||(x&&y&&eroded[(y-1)*cols+x-1]);
  count+=dilated[y*cols+x]!=0;
 }
 return count;
}
int gx521d_match_overlap_diagnostic(const Gx521dMatchOverlapInput *in,Gx521dMatchOverlap *out)
{
 Gx521dMatchOverlap result;if(!out||overlap_base(in,&result))return -1;
 uint8_t source[5120],target[5120],mask[5120];int32_t adjusted[6],inverse[6];size_t ox,oy,rows,cols;
 expand(in->probe_mask,source);expand(in->enrolled_mask,target);
 if(milan_build_overlap_mask(source,target,in->affine,0,mask,&ox,&oy,&rows,&cols,adjusted)) {*out=result;return 0;}
 if(milan_warp_inverse(adjusted,inverse))return -1;
 unsigned maximum=0;
 for(unsigned pass=0;pass<=in->enhanced;pass++){
  uint8_t mismatch[5120]={0};int32_t classes[4]={0},valid=0;const uint8_t *src=pass?in->probe_low:in->probe_high,*dst=pass?in->enrolled_low:in->enrolled_high;
  for(size_t y=0;y<rows;y++)for(size_t x=0;x<cols;x++){
   int32_t rx=goodix_milan_transform_affine_s32(inverse[0],x,inverse[3],y,inverse[2]);
   int32_t ry=goodix_milan_transform_affine_s32(inverse[1],x,inverse[4],y,inverse[5]);
   int v=milan_warp_sample(src,64,80,rx,ry);
   if(v>=0&&v<2&&mask[y*cols+x]){
    int bit=milan_packed_bitmap_bit(dst,80,x+ox,y+oy);classes[bit+v*2]++;valid++;
    if(v!=bit)mismatch[y*cols+x]=1;
   }
  }
  if(pass&&in->mode){int32_t confidence=0,weights[3]={0},score,detail;
   overlap_result(classes,valid,5120,0,weights,0,&confidence,&score,NULL,&detail);
   if(detail>result.detail)result.detail=detail;
  }
  unsigned count=filtered_mismatches(mismatch,rows,cols);if(count>maximum)maximum=count;
 }
 result.reserved=(int32_t)maximum;*out=result;return 0;
}
