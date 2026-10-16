/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Inverse arithmetic adapted from Milan match/overlap.c,
 * Copyright(C)2026 goodix-fp-linux-dev contributors.
 * Type17 full-rectangle novelty owner881b0/87b70. */
#include "521d-match-novelty.h"
#include "milan/transform-private.h"
#include <stddef.h>
#include <limits.h>
#include <string.h>
static int
milan_warp_inverse (const int32_t transform[6], int32_t inverse[6])
{
  uint64_t numerator[6];
  int32_t determinant = goodix_milan_transform_s32 (
    (uint32_t) transform[0] * (uint32_t) transform[4] -
    (uint32_t) transform[1] * (uint32_t) transform[3]);

  if (determinant == 0)
    {
      static const int32_t singular[6] = { 1, 0, 0, 0, 1, 0 };

      memcpy (inverse, singular, sizeof (singular));
      return 0;
    }
  /* Refinement can publish large affines. Native wraps before signed division,
   * then narrows the Q10 coefficients and every sampling accumulation. */
  numerator[0] = (uint64_t) (int64_t) transform[4] << 18;
  numerator[1] = (UINT64_C (0) - (uint64_t) (int64_t) transform[3]) << 18;
  numerator[2] =
    ((uint64_t) (int64_t) transform[5] * (uint64_t) (int64_t) transform[1] -
     (uint64_t) (int64_t) transform[4] * (uint64_t) (int64_t) transform[2]) << 10;
  numerator[3] = (UINT64_C (0) - (uint64_t) (int64_t) transform[1]) << 18;
  numerator[4] = (uint64_t) (int64_t) transform[0] << 18;
  numerator[5] =
    ((uint64_t) (int64_t) transform[3] * (uint64_t) (int64_t) transform[2] -
     (uint64_t) (int64_t) transform[5] * (uint64_t) (int64_t) transform[0]) << 10;
  for (size_t i = 0; i < 6; i++)
    {
      if (determinant == -1 && numerator[i] == (UINT64_C (1) << 63))
        return -1;
      inverse[i] = goodix_milan_transform_divide (numerator[i], determinant);
    }
  return 0;
}


int gx521d_match_novelty(const int32_t af[6],int32_t out[3])
{
 if(!af||!out)return -1;
 for(unsigned i=0;i<6;i++)if(af[i]<-1048576||af[i]>1048576)return -1;
 int32_t minx=INT32_MAX,maxx=INT32_MIN,miny=INT32_MAX,maxy=INT32_MIN,r[3]={0};
 for(unsigned i=0;i<4;i++){
  unsigned x=i&1?79:0,y=i&2?63:0;
  int32_t tx=goodix_milan_transform_affine_s32(af[0],x,af[1],y,(uint32_t)af[2]+128)>>8;
  int32_t ty=goodix_milan_transform_affine_s32(af[3],x,af[4],y,(uint32_t)af[5]+128)>>8;
  if(tx<minx)minx=tx;
  if(tx>maxx)maxx=tx;
  if(ty<miny)miny=ty;
  if(ty>maxy)maxy=ty;
 }
 int32_t width=maxx-minx+1,height=maxy-miny+1,oldx=minx<0?-minx:0,oldy=miny<0?-miny:0;
 int32_t posx=minx<0?0:minx,posy=miny<0?0:miny;
 int32_t unionw=80+oldx>posx+width?80+oldx:posx+width,unionh=64+oldy>posy+height?64+oldy:posy+height;
 if(width>512||height>512||unionw>800||unionh>1200){memcpy(out,r,sizeof(r));return 0;}
 r[1]=oldx;r[2]=oldy;
 int32_t adjusted[6],inv[6];memcpy(adjusted,af,24);adjusted[2]=goodix_milan_transform_s32((uint32_t)af[2]-(uint32_t)minx*256);adjusted[5]=goodix_milan_transform_s32((uint32_t)af[5]-(uint32_t)miny*256);
 if(milan_warp_inverse(adjusted,inv))return -1;
 for(int32_t y=0;y<height;y++)for(int32_t x=0;x<width;x++){
  if(x+minx>=0&&x+minx<80&&y+miny>=0&&y+miny<64)continue;
  int32_t sx=goodix_milan_transform_affine_s32(inv[0],(unsigned)x,inv[3],(unsigned)y,inv[2])>>10;
  int32_t sy=goodix_milan_transform_affine_s32(inv[1],(unsigned)x,inv[4],(unsigned)y,inv[5])>>10;
  if(sx>=-1&&sx<80&&sy>=-1&&sy<64)r[0]++;
 }
 memcpy(out,r,sizeof(r));return 0;
}
