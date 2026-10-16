/*
 * Goodix 53x5 driver for libfprint - Milan feature materialization
 * Copyright (C) 2026 goodix-fp-linux-dev contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "521d-feature-records.h"
#include "milan/private.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void
feature_build_orientation_weights (uint32_t *weights,
                                   size_t    size,
                                   uint32_t  coefficient)
{
  for (size_t y = 0; y < size; y++)
    for (size_t x = y; x < size; x++)
      {
        uint32_t radius_squared = (uint32_t) (x * x + y * y);
        uint32_t power = radius_squared * coefficient;
        uint32_t weight = 0;

        if (power <= 0x6ee75)
          {
            uint32_t reduced = power >> 8;
            uint32_t reduced_squared = reduced * reduced;
            uint32_t approximation = reduced_squared >> 8;

            approximation =
              ((approximation * approximation >> 8) *
               ((power / 5 + 0x10000) >> 8)) /
                0xc00 +
              0x200 +
              (((power / 6 + 0x8000) >> 8) * reduced_squared >> 15) +
              (power >> 7);
            weight = ((approximation >> 1) + 0x40000) / approximation;
          }
        weights[y * size + x] = weight;
        weights[x * size + y] = weight;
      }
}

static uint32_t
feature_build_orientation_histogram (int32_t         center_x,
                                     int32_t         center_y,
                                     int32_t         scale_value,
                                     const uint32_t *magnitude,
                                     const int16_t  *orientation,
                                     size_t          rows,
                                     size_t          columns,
                                     uint32_t        histogram[36],
                                     int32_t        *selected_bin)
{
  int32_t radius_q16 = (int32_t) ((uint32_t) scale_value * 18U) >> 2;
  int32_t sigma_q16 = (int32_t) ((uint32_t) scale_value * 6U) >> 2;
  uint32_t raw[36] = { 0 };
  uint32_t extended[40];
  uint32_t weights[33 * 33] = { 0 };
  int32_t radius = radius_q16 < 0
                     ? -(((-radius_q16) >> 16) +
                         ((((uint32_t) -radius_q16) & 0x8000) != 0))
                     : (radius_q16 >> 16) +
                         (((uint32_t) radius_q16 & 0x8000) != 0);
  radius = radius > 32 ? 32 : radius;
  size_t size = (size_t) radius + 1;
  uint32_t coefficient =
    (uint32_t) (UINT64_C(0x800000000000) /
                ((int64_t) sigma_q16 * sigma_q16));

  feature_build_orientation_weights (weights, size, coefficient);
  int32_t min_y = 1 - center_y > -radius ? 1 - center_y : -radius;
  int32_t max_y = (int32_t) rows - center_y - 2 < radius
                    ? (int32_t) rows - center_y - 2
                    : radius;
  int32_t min_x = 1 - center_x > -radius ? 1 - center_x : -radius;
  int32_t max_x = (int32_t) columns - center_x - 2 < radius
                    ? (int32_t) columns - center_x - 2
                    : radius;

  for (int32_t delta_y = min_y; delta_y <= max_y; delta_y++)
    for (int32_t delta_x = min_x; delta_x <= max_x; delta_x++)
      {
        size_t pixel = (size_t) (center_y + delta_y) * columns +
                       (size_t) (center_x + delta_x);
        int32_t bin = ((int32_t) orientation[pixel] * -36 + 0x743d4) /
                      0x6488;
        uint32_t weighted =
          magnitude[pixel] *
          weights[(size_t) abs (delta_y) * size + (size_t) abs (delta_x)] >> 8;

        if (bin >= 36)
          bin = 0;
        raw[bin] += weighted;
        /* Type17 uses directed orientations, without antipodal duplication. */
      }
  extended[0] = raw[34];
  extended[1] = raw[35];
  memcpy (extended + 2, raw, sizeof(raw));
  extended[38] = raw[0];
  extended[39] = raw[1];
  for (size_t bin = 0; bin < 36; bin++)
    histogram[bin] = (extended[bin] + 4 * extended[bin + 1] +
                      6 * extended[bin + 2] + 4 * extended[bin + 3] +
                      extended[bin + 4]) >>
                     4;
  uint32_t peak = histogram[0];
  int32_t peak_bin = 0;
  for (int32_t bin = 1; bin < 36; bin++)
    if (histogram[bin] > peak)
      {
        peak = histogram[bin];
        peak_bin = bin;
      }
  *selected_bin = peak_bin + 18;
  return peak;
}

static void
feature_descriptor_sine_cosine (int16_t  angle,
                                int32_t *sine,
                                int32_t *cosine)
{
  static const int16_t angles[13] = {
    0x0c91, 0x076b, 0x03eb, 0x01fd, 0x0100, 0x0080, 0x0040,
    0x0020, 0x0010, 0x0008, 0x0004, 0x0002, 0x0001,
  };
  int32_t reduced = abs(angle);
  int16_t sin_value = 0;
  int16_t cos_value = 0x4000;
  int16_t accumulated = 0;

  if (reduced > 0x1922)
    reduced = 0x3244 - reduced;
  if (reduced == 0)
    {
      *sine = 0;
      *cosine = abs(angle) > 0x1922 ? -0x4000 : 0x4000;
      return;
    }
  if (reduced == 0x1922)
    {
      *sine = angle < 0 ? -0x4000 : 0x4000;
      *cosine = 0;
      return;
    }
  for (size_t iteration = 0; iteration < 13; iteration++)
    {
      int16_t cos_shift = cos_value >> iteration;
      int16_t sin_shift = sin_value >> iteration;
      int16_t delta;

      if (reduced - accumulated < 0)
        {
          cos_shift = (int16_t) -cos_shift;
          delta = (int16_t) -angles[iteration];
        }
      else
        {
          sin_shift = (int16_t) -sin_shift;
          delta = angles[iteration];
        }
      accumulated = (int16_t) (accumulated + delta);
      sin_value = (int16_t) (sin_value + cos_shift);
      cos_value = (int16_t) (cos_value + sin_shift);
    }
  *sine = ((int32_t) sin_value * 0x9b75 + 0x8000) >> 16;
  *cosine = ((int32_t) cos_value * 0x9b75 + 0x8000) >> 16;
  if (abs(angle) > 0x1922)
    *cosine = -*cosine;
  if (angle < 0)
    *sine = -*sine;
}

static void
feature_descriptor_accumulate (uint32_t accumulator[6][6][8],
                               int32_t  x,
                               int32_t  y,
                               int32_t  angle,
                               int32_t  weight)
{
  int32_t x_bin = x >> 9;
  int32_t y_bin = y >> 9;
  int32_t x_fraction = x - x_bin * 0x200;
  int32_t y_fraction = y - y_bin * 0x200;
  int32_t orientation_bin = (angle >> 12) & 7;
  int32_t next_orientation = (orientation_bin + 1) & 7;
  int32_t orientation_fraction = angle - (angle >> 12) * 0x1000;
  uint32_t scaled_weight = (uint32_t) (weight >> 9);
  uint32_t upper_y = (uint32_t) (y_fraction * (int32_t) scaled_weight) >> 9;
  uint32_t lower_y = scaled_weight - upper_y;
  uint32_t upper_right =
    (uint32_t) (x_fraction * (int32_t) upper_y) >> 9;
  uint32_t lower_right =
    (uint32_t) (x_fraction * (int32_t) lower_y) >> 9;
  uint32_t cell_weight[2][2] = {
    { lower_y - lower_right, lower_right },
    { upper_y - upper_right, upper_right },
  };

  for (size_t cell_y = 0; cell_y < 2; cell_y++)
    for (size_t cell_x = 0; cell_x < 2; cell_x++)
      {
        uint32_t value = cell_weight[cell_y][cell_x];
        uint32_t orientation_part = value * orientation_fraction;
        uint32_t *cell = accumulator[y_bin + 1 + (int32_t) cell_y]
                                    [x_bin + 1 + (int32_t) cell_x];

        cell[orientation_bin] +=
          (value - (orientation_part >> 12)) >> 5;
        cell[next_orientation] += orientation_part >> 17;
      }
}

void
gx521d_feature_descriptor_samples (
  int32_t         center_x,
  int32_t         center_y,
  int32_t         descriptor_scale,
  int16_t         feature_orientation,
  const uint32_t *magnitude,
  const int16_t  *orientation,
  size_t          rows,
  size_t          columns,
  int32_t         samples[128])
{
  int32_t scale3 = descriptor_scale * 3;
  int64_t radius_q16 = ((int64_t) scale3 * 0x38916) >> 16;
  int32_t inverse_scale = (int32_t) (UINT64_C(0x1000000000) / scale3);
  uint32_t weights[33 * 33] = { 0 };
  uint32_t accumulator[6][6][8] = { 0 };
  int32_t sine, cosine;
  int32_t radius = (int32_t) ((radius_q16 >> 16) +
                              (((uint64_t) radius_q16 & 0x8000) != 0));

  radius = radius > 32 ? 32 : radius;
  size_t size = (size_t) radius + 1;
  uint32_t coefficient =
    (uint32_t) (UINT64_C(0x200000000000) /
                ((int64_t) scale3 * scale3));
  feature_build_orientation_weights (weights, size, coefficient);
  feature_descriptor_sine_cosine (feature_orientation, &sine, &cosine);
  cosine = (int32_t) (((int64_t) cosine * inverse_scale) >> 25);
  sine = (int32_t) (((int64_t) sine * inverse_scale) >> 25);
  int32_t min_y = 1 - center_y > -radius ? 1 - center_y : -radius;
  int32_t max_y = (int32_t) rows - center_y - 2 < radius
                    ? (int32_t) rows - center_y - 2
                    : radius;
  int32_t min_x = 1 - center_x > -radius ? 1 - center_x : -radius;
  int32_t max_x = (int32_t) columns - center_x - 2 < radius
                    ? (int32_t) columns - center_x - 2
                    : radius;

  for (int32_t delta_y = min_y; delta_y <= max_y; delta_y++)
    for (int32_t delta_x = min_x; delta_x <= max_x; delta_x++)
      {
        int32_t rotated_x = delta_x * cosine - delta_y * sine;
        int32_t rotated_y = delta_x * sine + delta_y * cosine;
        size_t pixel = (size_t) (center_y + delta_y) * columns +
                       (size_t) (center_x + delta_x);

        if (abs (rotated_x) >= 0x500 || abs (rotated_y) >= 0x500)
          continue;
        int32_t relative = 0x3244 - orientation[pixel] -
                           feature_orientation;
        relative %= 0x6488;
        if (relative < 0)
          relative += 0x6488;
        int32_t angle = (int32_t) (((uint32_t) relative * 0x145f3U) >> 16);
        int32_t weighted = (int32_t) (
          magnitude[pixel] *
          weights[(size_t) abs (delta_y) * size + (size_t) abs (delta_x)]);
        feature_descriptor_accumulate (accumulator, rotated_x + 0x300,
                                       rotated_y + 0x300, angle, weighted);
      }
  for (size_t y = 0; y < 4; y++)
    for (size_t x = 0; x < 4; x++)
      for (size_t bin = 0; bin < 8; bin++)
        samples[(y * 4 + x) * 8 + bin] = accumulator[y + 1][x + 1][bin];
}

static uint64_t
feature_integer_square_root (uint64_t value)
{
  uint64_t result = 0;
  uint64_t bit = UINT64_C(1) << 62;

  while (bit > value)
    bit >>= 2;
  while (bit != 0)
    {
      if (value >= result + bit)
        {
          value -= result + bit;
          result = (result >> 1) + bit;
        }
      else
        result >>= 1;
      bit >>= 2;
    }
  return result;
}

static void
feature_normalize_descriptor (const int32_t *samples,
                              size_t         count,
                              int16_t       *normalized)
{
  uint64_t energy = 0;

  for (size_t i = 0; i < count; i++)
    energy += (uint64_t) (uint32_t) samples[i] * (uint32_t) samples[i];
  uint32_t threshold =
    (uint32_t) ((feature_integer_square_root (energy) * 0x3333) >> 16);
  int16_t clipped = (int16_t) feature_integer_square_root (threshold);
  for (size_t i = 0; i < count; i++)
    normalized[i] = (int16_t) (
      samples[i] < (int32_t) threshold
        ? (int16_t) feature_integer_square_root ((uint32_t) samples[i])
        : (int16_t) clipped);
}

static int16_t
feature_descriptor_median (const int16_t *values,
                           size_t         count)
{
  int16_t sorted[128];

  memcpy (sorted, values, count * sizeof(*values));
  for (size_t i = 1; i < count; i++)
    {
      int16_t value = sorted[i];
      size_t position = i;

      while (position > 0 && value < sorted[position - 1])
        {
          sorted[position] = sorted[position - 1];
          position--;
        }
      sorted[position] = value;
    }
  return sorted[(count - 1) / 2];
}

static void gx521d_hadamard(int16_t *values,int count)
{
 for(int stride=count/2;stride;stride/=2)for(int base=0;base<count;base+=stride*2)for(int i=0;i<stride;i++){
  int a=values[base+i],b=values[base+i+stride];values[base+i]=(int16_t)(a+b);values[base+i+stride]=(int16_t)(a-b);
 }
}
static void gx521d_descriptor(const Gx521dFeatureScale *space,const GoodixMilanFeatureAux *aux,GoodixMilanFeatureRecord *record)
{
 int32_t samples[128];int16_t normalized[128],transposed[128]={0},transformed[128]={0},central[32];
 gx521d_feature_descriptor_samples(aux->x,aux->y,aux->scale_value,record->orientation,space->magnitude,space->orientation,64,80,samples);
 feature_normalize_descriptor(samples,128,normalized);
 for(int cell=0;cell<16;cell++){int16_t block[8];memcpy(block,normalized+cell*8,sizeof(block));gx521d_hadamard(block,8);for(int f=0;f<8;f++)transposed[f*16+cell]=block[f];}
 for(int f=1;f<=6;f++){gx521d_hadamard(transposed+f*16,16);memcpy(transformed+(f-1)*16,transposed+f*16,32);}
 static const int cells[4]={5,6,9,10};for(int c=0;c<4;c++)memcpy(central+c*8,normalized+cells[c]*8,16);
 gx521d_hadamard(central,32);memcpy(transformed+96,central,64);
 uint8_t *bytes=(uint8_t *)record;memset(bytes+16,0,24);
 for(int i=0;i<128;i++)if(transformed[i]>0)bytes[16+i/8]|=1U<<(i&7);
 int16_t median=feature_descriptor_median(normalized,128);
 for(int i=0;i<64;i++)if(normalized[i*2+1]>median)bytes[32+i/8]|=1U<<(i&7);
}
static int gx521d_materialize(const Gx521dFeatureScale *space,const Gx521dRefinedCandidate *c,Gx521dFeatureRecords *out)
{
 uint32_t histogram[36];int ignored;
 uint32_t peak=feature_build_orientation_histogram(c->x,c->y,c->scale_value,space->magnitude,space->orientation,64,80,histogram,&ignored);
 uint32_t cutoff=(uint32_t)(((uint64_t)peak*0xcccd+0x8000)>>16);
 int pixel=c->y*80+c->x;int neighborhood=space->blurred[pixel]+space->blurred[pixel-1]+space->blurred[pixel+1]+space->blurred[pixel-80]+space->blurred[pixel+80];
 for(int bin=0;bin<36&&out->materialized_count<GX521D_MATERIALIZED_MAX;bin++) {
  int64_t prev=histogram[(bin+35)%36],cur=histogram[bin],next=histogram[(bin+1)%36];
  if(cur<=prev||cur<=next||cur<cutoff)continue;
  int64_t denominator=2*prev+2*next-4*cur;
  int16_t interpolated=(int16_t)(((prev-next)*0x200-(denominator>>1))/denominator+bin*0x200);
  if(interpolated<0)interpolated+=0x4800;else if(interpolated>=0x4800)interpolated-=0x4800;
  int index=out->materialized_count++;GoodixMilanFeatureRecord *r=&out->materialized[index];
  r->foreground=neighborhood>=640;r->refined_x=c->refined_x;r->refined_y=c->refined_y;
  r->orientation=(int16_t)((((int32_t)interpolated*0x6488)/36)>>9)-0x3244;r->strength=-abs(c->strength);
  out->ranks[index]=(GoodixMilanFeatureRank){r->strength,index};
  out->auxiliary[index]=(GoodixMilanFeatureAux){c->x,c->y,c->scale_value,peak,(uint32_t)cur,cur!=peak,0};
 }
 return 0;
}
static int compare_rank(const void *a,const void *b)
{
 const GoodixMilanFeatureRank *x=a,*y=b;
 if(x->strength!=y->strength)return x->strength<y->strength?-1:1;
 return (x->index>y->index)-(x->index<y->index);
}
int gx521d_feature_records(const uint8_t image[GX521D_PIXELS],int capacity,Gx521dFeatureRecords *out)
{
 if(!image||!out||capacity<0||capacity>GX521D_RECORDS_MAX)return -1;
 Gx521dFeatureScale *space=malloc(sizeof(*space));Gx521dCandidates *candidates=malloc(sizeof(*candidates));if(!space||!candidates){free(space);free(candidates);return -1;}
 memset(out,0,sizeof(*out));if(gx521d_feature_scale_type17(image,space)||gx521d_feature_candidates_from_scales(space->scales,candidates)){free(space);free(candidates);return -1;}
 uint8_t visited[GX521D_PIXELS]={0};
 for(unsigned i=0;i<candidates->count&&out->materialized_count<GX521D_MATERIALIZED_MAX;i++) {
  Gx521dCandidate c=candidates->points[i];Gx521dRefinedCandidate r={c.x,c.y,c.scale,c.response,0,0,0};uint32_t curvature=0;
  if(!gx521d_feature_refine_extremum(&space->scales[0][0],64,80,&r,&curvature))continue;
  int pixel=r.y*80+r.x;if(visited[pixel])continue;
  gx521d_materialize(space,&r,out);visited[pixel]=1;
 }
 GoodixMilanFeatureRank order[GX521D_MATERIALIZED_MAX];memcpy(order,out->ranks,sizeof(order));
 if(out->materialized_count>capacity)qsort(order,out->materialized_count,sizeof(*order),compare_rank);
 int selected[GX521D_RECORDS_MAX],count=0;
 if(out->materialized_count<=capacity){for(int i=0;i<out->materialized_count;i++)selected[count++]=i;}
 else {
  for(int i=0;i<out->materialized_count&&count<capacity;i++)if(!out->auxiliary[order[i].index].secondary_orientation)selected[count++]=order[i].index;
  for(int pass=0;pass<2&&count<capacity;pass++)for(int i=0;i<out->materialized_count&&count<capacity;i++){
   int k=order[i].index;if(!out->auxiliary[k].secondary_orientation)continue;
   int exists=0;for(int j=0;j<count;j++)exists|=selected[j]==k;if(exists)continue;
   uint32_t cutoff=(uint32_t)(((uint64_t)out->auxiliary[k].peak*(pass?0:0xe666)+0x8000)>>16);
   if(out->auxiliary[k].selected_peak>=cutoff)selected[count++]=k;
  }
 }
 out->count=count;
 for(int i=0;i<count;i++) {
  int k=selected[i];out->records[i]=out->materialized[k];out->records[i].foreground+=(uint16_t)(i*4);
  gx521d_descriptor(space,&out->auxiliary[k],&out->records[i]);
  out->records[i].refined_x=(int16_t)((out->records[i].refined_x+8)&0xfff0);out->records[i].refined_y=(int16_t)((out->records[i].refined_y+8)&0xfff0);
  int angle=out->records[i].orientation;out->records[i].orientation=(int16_t)(angle<0?-((0x80-angle)&0xff00):((angle+0x80)&0xff00));
 }
 free(space);free(candidates);return 0;
}
