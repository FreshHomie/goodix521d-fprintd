/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Adapted from Milan geometry.c and feature/extraction.c,
 * Copyright(C)2026 goodix-fp-linux-dev contributors.
 * Type17 affine scale, folded rotation and orthogonality, owner8ade0. */
#include "521d-match-affine-details.h"
#include "milan/transform-private.h"
#include <stddef.h>
static uint64_t
milan_match_integer_sqrt_u64 (uint64_t value)
{
  uint64_t root = 0;
  uint64_t bit = UINT64_C (1) << 62;

  while (bit > value)
    bit >>= 2;
  while (bit != 0)
    {
      if (value >= root + bit)
        {
          value -= root + bit;
          root = (root >> 1) + bit;
        }
      else
        root >>= 1;
      bit >>= 2;
    }
  return root;
}

static uint32_t
milan_match_affine_product_root (uint64_t value)
{
  uint32_t root = 0;
  uint64_t bit = UINT64_C (0x80000000);

  /* Native doubles the root in a dword before widening the trial. */
  for (int shift = 31; shift >= 0; shift--, bit >>= 1)
    {
      uint64_t trial = ((uint32_t) (root * UINT32_C (2)) + bit) << shift;

      if (value >= trial)
        {
          root += (uint32_t) bit;
          value -= trial;
        }
    }
  return root;
}

static uint16_t
feature_cordic (int32_t vertical,
                int32_t *horizontal)
{
  static const uint16_t angles[13] = {
    0x0c91, 0x076b, 0x03eb, 0x01fd, 0x0100, 0x0080, 0x0040,
    0x0020, 0x0010, 0x0008, 0x0004, 0x0002, 0x0001,
  };
  static const int32_t gains[13] = {
    0xb505, 0xa1e9, 0x9d13, 0x9bdd, 0x9b8f, 0x9b7b, 0x9b77,
    0x9b75, 0x9b75, 0x9b75, 0x9b75, 0x9b75, 0x9b75,
  };
  int32_t original_horizontal = *horizontal;
  int32_t original_vertical = vertical;
  int32_t x = original_horizontal > 0 ? original_horizontal
                                       : -original_horizontal;
  int32_t y = original_vertical > 0 ? original_vertical : -original_vertical;
  uint16_t angle = 0;
  size_t stop = 12;

  if (y == 0)
    {
      *horizontal = x;
      return original_horizontal > 0 ? 0 : 0x3244;
    }
  if (x == 0)
    {
      *horizontal = y;
      return original_vertical > 0 ? 0x1922 : 0xe6de;
    }
  for (size_t iteration = 0; iteration < 13; iteration++)
    {
      int32_t shifted_y = y >> iteration;
      int32_t shifted_x = x >> iteration;

      if (y > 0)
        {
          x += shifted_y;
          y -= shifted_x;
          angle = (uint16_t) (angle + angles[iteration]);
        }
      else
        {
          x -= shifted_y;
          y += shifted_x;
          angle = (uint16_t) (angle - angles[iteration]);
        }
      if (y == 0)
        {
          stop = iteration;
          break;
        }
    }
  if (original_horizontal > 0)
    {
      if (original_vertical < 0)
        angle = (uint16_t) -angle;
    }
  else if (original_vertical > 0)
    angle = (uint16_t) (0x3244 - angle);
  else
    angle = (uint16_t) (angle + 0xcdbc);
  *horizontal = (int32_t) (((int64_t) gains[stop] * x + 0x8000) >> 16);
  return angle;
}


int gx521d_match_affine_details(const int32_t affine[6],int32_t out[3])
{
 if(!affine||!out)return -1;
 for(unsigned i=0;i<6;i++)if(affine[i]<-1048576||affine[i]>1048576)return -1;
 uint32_t a=(uint32_t)affine[0],b=(uint32_t)affine[1],c=(uint32_t)affine[3],d=(uint32_t)affine[4];
 uint32_t sq1=a*a+c*c,sq2=b*b+d*d,len1=(uint32_t)milan_match_integer_sqrt_u64(sq1),len2=(uint32_t)milan_match_integer_sqrt_u64(sq2);
 int32_t result[3]={goodix_milan_transform_s32(len1+len2)>>1,0,0};
 if(len1){
  int32_t x=goodix_milan_transform_s32(a<<8)/(int32_t)len1,y=goodix_milan_transform_s32(c<<8)/(int32_t)len1;
  int16_t angle=(int16_t)feature_cordic(y,&x);if(angle<0)angle=(int16_t)(angle+0x6488);
  int degrees=(int)(((int64_t)angle*0x395)>>16);if(degrees>=180)degrees-=180;result[1]=degrees<180-degrees?degrees:180-degrees;
  int64_t product=(int64_t)goodix_milan_transform_s32(sq1)*goodix_milan_transform_s32(sq2);
  uint32_t root=milan_match_affine_product_root((uint64_t)product);
  if(root){int64_t dot=goodix_milan_transform_s32(a*b+c*d);int32_t value=goodix_milan_transform_s32((uint32_t)((dot*65536)/(int64_t)root));result[2]=value<0?goodix_milan_transform_s32(0u-(uint32_t)value):value;}
 }
 for(unsigned i=0;i<3;i++)out[i]=result[i];
 return 0;
}
