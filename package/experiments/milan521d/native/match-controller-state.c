/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Integer square root and CORDIC adapted from Milan feature/extraction.c,
 * Copyright(C)2026 goodix-fp-linux-dev contributors.
 * Type17 configuration and readiness transcribed from owners5d060/5ebd0. */
#include "521d-match-controller-state.h"
#include "milan/transform-private.h"
#include <stddef.h>
#include <string.h>
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

static int64_t floor_div(int64_t a,int64_t b){int64_t q=a/b,r=a%b;return q-((r!=0)&&((r<0)!=(b<0)));}
static int64_t ceil_div(int64_t a,int64_t b){return -floor_div(-a,b);}
static void row_interval(int32_t constant,int32_t coefficient,int32_t dimension,int64_t *lo,int64_t *hi)
{
 int64_t max=(dimension-1)*256;
 if(!coefficient){*lo=constant>=0&&constant<=max?0:0xfffffff;*hi=*lo?-(int64_t)0xfffffff:79;return;}
 if(coefficient>0){*lo=ceil_div(-(int64_t)constant,coefficient);*hi=floor_div(max-constant,coefficient);}
 else{*lo=ceil_div(max-constant,coefficient);*hi=floor_div(-(int64_t)constant,coefficient);}
}
int gx521d_match_controller_state(const Gx521dMatchControllerStateInput *in,Gx521dMatchControllerState *out)
{
 if(!in||!out)return -1;
 for(unsigned i=0;i<6;i++)if(in->affine[i]<-1048576||in->affine[i]>1048576)return -1;
 Gx521dMatchControllerState r={0};memcpy(r.config,in->config_seed,sizeof(r.config));
 unsigned low=in->selector&15,high=(in->selector>>8)&15;
 if(low>2||high>2||(low&&high)){r.status=0x80000003;r.config[0]=r.config[1]=0;}
 else{
  r.config[0]=low==2?2:0;r.config[1]=low?1:0;r.config[2]=5;
  if(high){r.config[0]=high==1?-2:-4;r.config[1]=0;}
  r.config[3]=218;r.config[4]=in->flags!=0;r.config[5]=in->flags!=0&&in->flags!=2;
  r.config[6]=22;r.config[7]=45;r.config[8]=40;r.config[9]=38;r.config[10]=100;r.config[11]=16;
  r.config[12]=137;r.config[13]=1;r.config[15]=17;r.config[16]=0;r.config[17]=1;
 }
 uint32_t a=(uint32_t)in->affine[0],c=(uint32_t)in->affine[3];
 int32_t length=(int32_t)milan_match_integer_sqrt_u64(a*a+c*c)+1;
 int32_t x=goodix_milan_transform_s32(a<<8)/length,y=goodix_milan_transform_s32(c<<8)/length;
 int16_t angle=(int16_t)feature_cordic(y,&x);if(angle<0)angle=(int16_t)(angle+0x6488);
 r.rotation=(int32_t)(((int64_t)angle*0x395)>>16);r.rows[0]=64;
 for(int32_t row=0;row<64;row++){
  int64_t lo,hi,lo2,hi2;
  row_interval(row*in->affine[1]+in->affine[2],in->affine[0],80,&lo,&hi);
  row_interval(row*in->affine[4]+in->affine[5],in->affine[3],64,&lo2,&hi2);
  if(lo<lo2)lo=lo2;
  if(lo<0)lo=0;
  if(hi>hi2)hi=hi2;
  if(hi>79)hi=79;
  if(hi>=lo){r.rows[2+row]=(int32_t)lo;r.rows[258+row]=(int32_t)hi;r.area+=(int32_t)(hi-lo+1);if(row<r.rows[0])r.rows[0]=row;if(row>r.rows[1])r.rows[1]=row;}
 }
 for(int stage=1;stage<=3;stage++)r.readiness[stage-1]=(r.rotation<stage*5||r.rotation+stage*5>360)&&r.area*100>(100-(stage+2)*5)*5120;
 *out=r;return 0;
}
