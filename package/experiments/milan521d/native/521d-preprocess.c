/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Goodix 521d profile 12 / algorithm type 17 input and setup stage.
 * Independently implemented from pinned 3.0.21.290 static behavior.
 * References: 0x772a0, 0x77060 (+ subtype jump table), 0x7e230.
 */
#include "521d-preprocess.h"
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

int gx521d_prepare_input(uint16_t frame[GX521D_PIXELS], int setup)
{
  if (!frame || (setup != 0 && setup != 1)) return -1;
  for (size_t i = 0; i < GX521D_PIXELS; i++)
    if (frame[i] > 4095) frame[i] = 4095;
  size_t admitted = 0;
  const size_t total = (GX521D_ROWS - 4) * (GX521D_COLUMNS - 4);
  for (size_t y = 2; y < GX521D_ROWS - 2; y++)
    for (size_t x = 2; x < GX521D_COLUMNS - 2; x++) {
      uint16_t v = frame[y * GX521D_COLUMNS + x];
      /* Type 17 selects strict (100, 3800), with 85% for initial setup
       * and 25% for live input. These differ from profile 9 / type 12. */
      admitted += v > 100 && v < 3800;
    }
  if (admitted * 100 <= total * (setup ? 85 : 25))
    return setup ? GX521D_RETRY_SETUP : GX521D_RETRY_RAW;
  memcpy(frame, frame + GX521D_COLUMNS, GX521D_COLUMNS * sizeof(*frame));
  memcpy(frame + (GX521D_ROWS - 1) * GX521D_COLUMNS,
         frame + (GX521D_ROWS - 2) * GX521D_COLUMNS,
         GX521D_COLUMNS * sizeof(*frame));
  for (size_t y = 0; y < GX521D_ROWS; y++) {
    size_t begin = y * GX521D_COLUMNS;
    frame[begin] = frame[begin + 1];
    frame[begin + GX521D_COLUMNS - 1] = frame[begin + GX521D_COLUMNS - 2];
  }
  return 0;
}

int gx521d_make_setup(const uint16_t normalized[GX521D_PIXELS],
                     uint16_t map[GX521D_PIXELS], uint32_t *mean)
{
  if (!normalized || !map || !mean) return -1;
  uint32_t sum = 0;
  for (size_t i = 0; i < GX521D_PIXELS; i++) {
    if (normalized[i] > 4095) return -1;
    sum += normalized[i];
    map[i] = normalized[i] + 7095;
  }
  *mean = (sum + GX521D_PIXELS / 2) / GX521D_PIXELS;
  return 0;
}

/* Based on upstream goodix_milan_profile9_build_contrast_mask,
 * Copyright (C) 2026 goodix-fp-linux-dev contributors, LGPL-2.1-or-later.
 * Type-17 differences verified in 3.0.21.290 owner 0x78870:
 * 16-row blocks; threshold span floor max(900,1730*70/100)=1211;
 * no type-12 edge-column adjustment; this returns the initial mask only.
 * Hole filling belongs to a separate later stage and must not change the
 * admission count here.
 */
int gx521d_make_mask(const uint16_t normalized_live[GX521D_PIXELS],
                    const uint16_t setup_map[GX521D_PIXELS], Gx521dMask *mask)
{
  if (!normalized_live || !setup_map || !mask) return -1;
  memset(mask,0,sizeof(*mask));
  mask->rows=GX521D_ROWS; mask->columns=GX521D_COLUMNS;
  int16_t difference[GX521D_PIXELS];
  int row_mean[GX521D_ROWS];
  int block_min=INT_MAX, block_max=INT_MIN;
  for (size_t i=0;i<GX521D_PIXELS;i++)
    difference[i]=(int16_t)(uint16_t)(setup_map[i]-normalized_live[i]);
  for (size_t y0=0;y0<GX521D_ROWS;y0+=16)
    for (size_t x0=0;x0<GX521D_COLUMNS;x0+=16) {
      int sum=0, samples=0;
      for (size_t y=y0;y<y0+16 && y<GX521D_ROWS;y++)
        for (size_t x=x0;x<x0+16 && x<GX521D_COLUMNS;x++) {
          size_t i=y*GX521D_COLUMNS+x;
          if (normalized_live[i]>=500) {sum+=difference[i];samples++;}
        }
      int mean=sum/(samples ? samples : 1);
      if (mean<block_min) block_min=mean;
      if (mean>block_max) block_max=mean;
    }
  for (size_t y=0;y<GX521D_ROWS;y++) {
    int sum=0;
    for (size_t x=0;x<GX521D_COLUMNS;x++) sum+=difference[y*GX521D_COLUMNS+x];
    row_mean[y]=sum/GX521D_COLUMNS;
  }
  int first=0,last=0;
  for (size_t group=6;group<GX521D_ROWS/4;group++) {
    first+=row_mean[group*4];last+=row_mean[group*4+3];
  }
  first/=GX521D_ROWS/4-6;last/=GX521D_ROWS/4-6;
  if (abs(first-last)>400) return 0;
  int span=block_max-block_min,split;
  if (span>1211) {
    int half=span*50/100;
    split=(block_max<=4095 || block_min>=4095) ? block_min+half : block_max-half;
  } else split=block_min-250;
  int high_count=0,low_count=0,high_sum=0,low_sum=0;
  for (size_t i=0;i<GX521D_PIXELS;i++) {
    if (difference[i]>split) {high_count++;high_sum+=difference[i];}
    else {low_count++;low_sum+=difference[i];}
  }
  if (!high_count) return 0;
  int high=high_sum/high_count,low=low_count ? low_sum/low_count : high+600;
  int separation=high-low;
  int threshold=high_count<low_count+150 ? split+separation/5 : split-separation/4;
  int below=0;
  for (size_t i=0;i<GX521D_PIXELS;i++) {
    if (difference[i]>threshold && normalized_live[i]<3800) {
      mask->pixels[i]=1;mask->count++;
    } else if (difference[i]<threshold && normalized_live[i]<3800) below++;
  }
  if (below*100>GX521D_PIXELS*30) {
    memset(mask->pixels,0,sizeof(mask->pixels));mask->count=0;
    threshold=block_min+150;
    for (size_t i=0;i<GX521D_PIXELS;i++)
      if (difference[i]>threshold && normalized_live[i]<3800) {
        mask->pixels[i]=1;mask->count++;
      }
  }
  mask->threshold=(int16_t)threshold;
  mask->percent=(uint16_t)(mask->count*100/GX521D_PIXELS);
  return 0;
}

/* Bounded gap fill follows upstream's postprocess owner, adapted to the
 * separately retained initial/render mask contract at 521d RVA 0x76530.
 * Mutation order matters: subsequent searches see earlier filled pixels. */
int gx521d_prepare_render_mask(const uint16_t live[GX521D_PIXELS],
                               const Gx521dMask *initial, Gx521dMask *render)
{
  if (!live || !initial || !render || initial->rows!=GX521D_ROWS ||
      initial->columns!=GX521D_COLUMNS) return -1;
  *render=*initial;
  render->count=0;
  for (size_t y=0;y<GX521D_ROWS;y++)
    for (size_t x=0;x<GX521D_COLUMNS;x++) {
      size_t index=y*GX521D_COLUMNS+x;
      if (!render->pixels[index]) {
        size_t up=y,down=y,left=x,right=x;
        while (up>0 && !render->pixels[up*GX521D_COLUMNS+x]) up--;
        while (down<GX521D_ROWS && !render->pixels[down*GX521D_COLUMNS+x]) down++;
        while (left>0 && !render->pixels[y*GX521D_COLUMNS+left]) left--;
        while (right<GX521D_COLUMNS && !render->pixels[y*GX521D_COLUMNS+right]) right++;
        int vertical=render->pixels[up*GX521D_COLUMNS+x] && up>=2 && down<=GX521D_ROWS-3;
        int horizontal=render->pixels[y*GX521D_COLUMNS+left] && left>=2 && right<=GX521D_COLUMNS-3;
        if (vertical || horizontal) render->pixels[index]=1;
      }
      render->count+=render->pixels[index]!=0;
    }
  for (size_t i=0;i<GX521D_PIXELS;i++)
    if (live[i]<=100 || live[i]>=3800) {
      render->count-=render->pixels[i]!=0;
      render->pixels[i]=0;
    }
  render->percent=(uint16_t)(render->count*100/GX521D_PIXELS);
  return 0;
}
