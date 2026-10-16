/* Private shared implementation; see original LGPL notices below. */
#pragma once
/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Adapted from Milan match/overlap.c and score.c,
 * Copyright (C) 2026 goodix-fp-linux-dev contributors.
 * Type17 full-resolution owner: RVA88480. */
#include "521d-match-overlap.h"
#include "milan/transform-private.h"
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

static int
milan_packed_bitmap_bit (const uint8_t *bitmap,
                          size_t         columns,
                          size_t         x,
                          size_t         y)
{
  size_t index = y * columns + x;

  return (bitmap[index / 8] >> (index % 8)) & 1;
}

static int
milan_warp_sample (const uint8_t *bitmap,
                    size_t         rows,
                    size_t         columns,
                    int32_t        raw_x,
                    int32_t        raw_y)
{
  int32_t x = raw_x >> 10;
  int32_t y = raw_y >> 10;

  if (x + 1 < 0 || y + 1 < 0 || x >= (int32_t) columns ||
      y >= (int32_t) rows)
    return -1;
  int32_t fraction_x = raw_x - x * 0x400;
  int32_t fraction_y = raw_y - y * 0x400;
  if (x >= 0 && y >= 0 && x + 1 < (int32_t) columns &&
      y + 1 < (int32_t) rows)
    {
      int32_t top = milan_packed_bitmap_bit (bitmap, columns, x, y) *
                      (0x400 - fraction_x) +
                    milan_packed_bitmap_bit (bitmap, columns, x + 1, y) *
                      fraction_x;
      int32_t bottom = milan_packed_bitmap_bit (bitmap, columns, x, y + 1) *
                         (0x400 - fraction_x) +
                       milan_packed_bitmap_bit (bitmap, columns, x + 1, y + 1) *
                         fraction_x;

      return (top * (0x400 - fraction_y) + bottom * fraction_y +
              0x80000) >> 20;
    }

  int32_t sum = 0;
  int32_t count = 0;
  if (x >= 0 && y >= 0)
    {
      sum += milan_packed_bitmap_bit (bitmap, columns, (size_t) x,
                                      (size_t) y);
      count++;
    }
  if (x + 1 >= 0 && x + 1 < (int32_t) columns && y >= 0)
    {
      sum += milan_packed_bitmap_bit (bitmap, columns, (size_t) (x + 1),
                                      (size_t) y);
      count++;
    }
  if (y + 1 >= 0 && y + 1 < (int32_t) rows)
    {
      if (x >= 0)
        {
          sum += milan_packed_bitmap_bit (bitmap, columns, (size_t) x,
                                          (size_t) (y + 1));
          count++;
        }
      if (x + 1 >= 0 && x + 1 < (int32_t) columns)
        {
          sum += milan_packed_bitmap_bit (bitmap, columns,
                                          (size_t) (x + 1),
                                          (size_t) (y + 1));
          count++;
        }
    }
  return count > 0 ? sum / count : -1;
}

static int
milan_warp_sample_u8 (const uint8_t *image,
                       size_t         rows,
                       size_t         columns,
                       int32_t        raw_x,
                       int32_t        raw_y)
{
  int32_t x = raw_x >> 10;
  int32_t y = raw_y >> 10;

  if (x + 1 < 0 || y + 1 < 0 || x >= (int32_t) columns ||
      y >= (int32_t) rows)
    return -1;
  int32_t fraction_x = raw_x - x * 0x400;
  int32_t fraction_y = raw_y - y * 0x400;
  if (x >= 0 && y >= 0 && x + 1 < (int32_t) columns &&
      y + 1 < (int32_t) rows)
    {
      int32_t top = image[(size_t) y * columns + (size_t) x] *
                      (0x400 - fraction_x) +
                    image[(size_t) y * columns + (size_t) (x + 1)] *
                      fraction_x;
      int32_t bottom = image[(size_t) (y + 1) * columns + (size_t) x] *
                         (0x400 - fraction_x) +
                       image[(size_t) (y + 1) * columns +
                             (size_t) (x + 1)] * fraction_x;

      return (top * (0x400 - fraction_y) + bottom * fraction_y +
              0x80000) >> 20;
    }

  int32_t sum = 0;
  int32_t count = 0;
  for (int32_t dy = 0; dy <= 1; dy++)
    for (int32_t dx = 0; dx <= 1; dx++)
      {
        int32_t sample_x = x + dx;
        int32_t sample_y = y + dy;

        if (sample_x >= 0 && sample_x < (int32_t) columns && sample_y >= 0 &&
            sample_y < (int32_t) rows)
          {
            sum += image[(size_t) sample_y * columns + (size_t) sample_x];
            count++;
          }
      }
  return count > 0 ? sum / count : -1;
}

static int
milan_build_overlap_mask (const uint8_t source_mask[64 * 80],
                           const uint8_t target_mask[64 * 80],
                           const int32_t transform[6], int interpolation,
                           uint8_t       overlap_mask[64 * 80],
                           size_t       *origin_x,
                           size_t       *origin_y,
                           size_t       *overlap_rows,
                           size_t       *overlap_columns,
                           int32_t       adjusted_transform[6])
{
  const int32_t corners[4][2] = {
    { interpolation, interpolation }, { 79-interpolation, interpolation }, { 79-interpolation, 63-interpolation }, { interpolation, 63-interpolation },
  };
  int32_t minimum_x = INT32_MAX;
  int32_t minimum_y = INT32_MAX;
  int32_t maximum_x = INT32_MIN;
  int32_t maximum_y = INT32_MIN;
  int32_t inverse[6];

  for (size_t i = 0; i < 4; i++)
    {
      int32_t x = goodix_milan_transform_affine_s32 (
        transform[0], corners[i][0], transform[1], corners[i][1],
        (uint32_t) transform[2] + 0x80) >> 8;
      int32_t y = goodix_milan_transform_affine_s32 (
        transform[3], corners[i][0], transform[4], corners[i][1],
        (uint32_t) transform[5] + 0x80) >> 8;

      if (x < minimum_x)
        minimum_x = x;
      if (x > maximum_x)
        maximum_x = x;
      if (y < minimum_y)
        minimum_y = y;
      if (y > maximum_y)
        maximum_y = y;
    }
  if (minimum_x < 0)
    minimum_x = 0;
  if (minimum_y < 0)
    minimum_y = 0;
  if (maximum_x > 79)
    maximum_x = 79;
  if (maximum_y > 63)
    maximum_y = 63;
  if (maximum_x < minimum_x || maximum_y < minimum_y)
    return -1;
  *origin_x = (size_t) minimum_x;
  *origin_y = (size_t) minimum_y;
  *overlap_columns = (size_t) (maximum_x - minimum_x + 1);
  *overlap_rows = (size_t) (maximum_y - minimum_y + 1);
  memcpy (adjusted_transform, transform, 6 * sizeof(*transform));
  adjusted_transform[2] = goodix_milan_transform_s32 (
    (uint32_t) transform[2] - (uint32_t) minimum_x * 0x100);
  adjusted_transform[5] = goodix_milan_transform_s32 (
    (uint32_t) transform[5] - (uint32_t) minimum_y * 0x100);

  if (milan_warp_inverse (adjusted_transform, inverse) != 0)
    return -1;

  for (size_t y = 0; y < *overlap_rows; y++)
    for (size_t x = 0; x < *overlap_columns; x++)
      {
        int32_t raw_x = goodix_milan_transform_affine_s32 (
          inverse[0], x, inverse[3], y, inverse[2]);
        int32_t raw_y = goodix_milan_transform_affine_s32 (
          inverse[1], x, inverse[4], y, inverse[5]);
        size_t target_x = x + *origin_x;
        size_t target_y = y + *origin_y;
        int source = milan_warp_sample_u8 (
          source_mask, 64, 80, raw_x, raw_y);

        overlap_mask[y * *overlap_columns + x] =
          source > 0 && target_mask[target_y * 80 + target_x] ? 1 : 0;
      }
  return 0;
}

static int
bitmap_classes (
  const uint8_t *target_bitmap,
  const uint8_t *source_bitmap,
  const uint8_t *overlap_mask,
  size_t         bitmap_rows,
  size_t         bitmap_columns,
  size_t         overlap_x,
  size_t         overlap_y,
  size_t         overlap_rows,
  size_t         overlap_columns,
  const int32_t  transform[6],
  int32_t        classes[4],
  int32_t       *valid_count)
{
  int32_t inverse[6];

  if (!target_bitmap || !source_bitmap || !overlap_mask ||
      bitmap_rows == 0 || bitmap_columns == 0 || overlap_rows == 0 ||
      overlap_columns == 0 || overlap_rows > bitmap_rows ||
      overlap_columns > bitmap_columns || overlap_x > bitmap_columns ||
      overlap_y > bitmap_rows || overlap_columns > bitmap_columns - overlap_x ||
      overlap_rows > bitmap_rows - overlap_y || bitmap_rows > INT32_MAX ||
      bitmap_columns > INT32_MAX || !transform || !classes || !valid_count)
    return -1;
  if (milan_warp_inverse (transform, inverse) != 0)
    return -1;

  memset (classes, 0, 4 * sizeof(*classes));
  *valid_count = 0;
  for (size_t y = 0; y < overlap_rows; y++)
    for (size_t x = 0; x < overlap_columns; x++)
      {
        int32_t raw_x = goodix_milan_transform_affine_s32 (
          inverse[0], x, inverse[3], y, inverse[2]);
        int32_t raw_y = goodix_milan_transform_affine_s32 (
          inverse[1], x, inverse[4], y, inverse[5]);
        int source = milan_warp_sample (
          source_bitmap, bitmap_rows, bitmap_columns, raw_x, raw_y);

        if (source < 0 || !overlap_mask[y * overlap_columns + x])
          continue;
        int target = milan_packed_bitmap_bit (
          target_bitmap, bitmap_columns, x + overlap_x, y + overlap_y);

        if (source < 2 && target < 2)
          {
            classes[target + source * 2]++;
            (*valid_count)++;
          }
      }
  return 0;
}

enum {
  OVERLAP_CLASS_ZERO = 0,
  OVERLAP_CLASS_ONE = 1,
  OVERLAP_CLASS_TWO = 2,
  OVERLAP_CLASS_MATCH = 3,
};

enum {
  OVERLAP_WEIGHT_HIGH_VALID = 0,
  OVERLAP_WEIGHT_LOW_VALID = 1,
  OVERLAP_WEIGHT_QUALITY = 2,
};

enum {
  MATCH_METRIC_PRIMARY_COUNT = 0,
  MATCH_METRIC_RETAINED_COUNT = 1,
  MATCH_METRIC_OVERLAP_SCORE = 4,
  MATCH_METRIC_OVERLAP_DETAIL = 5,
  MATCH_METRIC_LOW_BITMAP_DETAIL = 8,
  MATCH_METRIC_OVERLAP_COVERAGE = 9,
  MATCH_METRIC_GEOMETRIC_PERCENT = 11,
  MATCH_METRIC_SCALE_PENALTY = 12,
  MATCH_METRIC_ORTHOGONALITY_PENALTY = 13,
  MATCH_METRIC_STRONG_ORTHOGONALITY_PENALTY = 14,
};

#define MATCH_Q8_ONE 0x100
#define OVERLAP_REJECTED_SCORE 128
#define EXTENDED_DENOMINATOR_TYPE_MASK 0x413000U
#define FINAL_SCORE_DEFAULT_DENOMINATOR 31
#define FINAL_SCORE_EXTENDED_DENOMINATOR 42

static int32_t
secondary_result (int32_t primary_score,
                                     int32_t secondary_score,
                                     int32_t secondary_detail)
{
  return secondary_score > primary_score && secondary_detail > 195 ?
         secondary_score :
         primary_score;
}

static int
overlap_result (
  const int32_t classes[4],
  int32_t       valid_count,
  int32_t       full_count,
  int32_t       mode,
  const int32_t weights[3],
  int32_t       context_count,
  int32_t      *context_confidence,
  int32_t      *score,
  int32_t      *coverage,
  int32_t      *detail)
{
  int32_t raw_score;
  int32_t quality;

  if (coverage)
    *coverage = (full_count / 2 + valid_count * MATCH_Q8_ONE) / full_count;
  if (detail)
    {
      int32_t non_match_total = classes[OVERLAP_CLASS_ZERO] +
                                classes[OVERLAP_CLASS_ONE] +
                                classes[OVERLAP_CLASS_TWO];
      int32_t result = non_match_total > 0 ?
                       (non_match_total / 2 +
                        classes[OVERLAP_CLASS_ZERO] * MATCH_Q8_ONE) /
                       (non_match_total + 1) :
                       0;
      int32_t match_fraction =
        (valid_count / 2 + classes[OVERLAP_CLASS_MATCH] * MATCH_Q8_ONE) /
        (valid_count + 1);

      if (match_fraction < 15)
        result -= ((15 - match_fraction) >> 1) + 3;
      *detail = result;
    }
  if (mode != 0)
    {
      if (*context_confidence < 0)
        {
          *context_confidence = classes[OVERLAP_CLASS_MATCH] * MATCH_Q8_ONE /
                                (classes[OVERLAP_CLASS_ONE] +
                                 classes[OVERLAP_CLASS_TWO] +
                                 classes[OVERLAP_CLASS_MATCH] + 1);
        }
    }
  if (valid_count == 0)
    {
      *score = OVERLAP_REJECTED_SCORE;
      return 0;
    }
  if (full_count / 2 < valid_count)
    {
      raw_score = weights[OVERLAP_WEIGHT_HIGH_VALID] + 38 +
                  (valid_count / 2 +
                   classes[OVERLAP_CLASS_ZERO] * MATCH_Q8_ONE) / valid_count;
    }
  else
    {
      raw_score = (valid_count / 2 +
                   classes[OVERLAP_CLASS_ZERO] * MATCH_Q8_ONE) /
                  (valid_count + 1) +
                  valid_count * 19 / (full_count / 2) +
                  weights[OVERLAP_WEIGHT_LOW_VALID] + 19;
    }
  quality = (valid_count / 2 +
             classes[OVERLAP_CLASS_MATCH] * MATCH_Q8_ONE) / valid_count +
            weights[OVERLAP_WEIGHT_QUALITY];
  if (mode == 0)
    {
      *score = quality > 23 || (raw_score > 230 && quality > 16) ?
               raw_score :
               OVERLAP_REJECTED_SCORE;
      return 0;
    }
  *score = quality > 23 ||
           (quality > 18 && *context_confidence > 62) ||
           (quality > 19 && *context_confidence > 50) ||
           (quality > 17 && ((*context_confidence > 40 &&
                              context_count > 7) ||
                             context_count > 8)) ||
           (raw_score > 230 && quality > 16) ?
           raw_score :
           OVERLAP_REJECTED_SCORE;
  return 0;
}


static void expand(const uint8_t bits[40],uint8_t mask[5120])
{
 for(unsigned y=0;y<64;y++)for(unsigned x=0;x<80;x++) {
 unsigned bit=(y/4)*20+x/4;mask[y*80+x]=(bits[bit/8]>>(bit%8))&1;
 }
}
static int overlap_base(const Gx521dMatchOverlapInput *in,Gx521dMatchOverlap *out)
{
 if(!in||!out||in->mode<0||in->mode>1||in->interpolation!=0||in->context_count<0||in->context_count>120||in->enhanced>1)return -1;
 for(unsigned i=0;i<6;i++)if(in->affine[i]<-1048576||in->affine[i]>1048576)return -1;
 Gx521dMatchOverlap result={0};uint8_t source[5120],target[5120],mask[5120];int32_t adjusted[6];size_t ox,oy,rows,cols;
 expand(in->probe_mask,source);expand(in->enrolled_mask,target);
 if(milan_build_overlap_mask(source,target,in->affine,in->interpolation,mask,&ox,&oy,&rows,&cols,adjusted)!=0){*out=result;return 0;}
 int32_t classes[4],valid,confidence=-1,weights[3]={0,0,0};
 if(bitmap_classes(in->enrolled_high,in->probe_high,mask,64,80,ox,oy,rows,cols,adjusted,classes,&valid))return -1;
 overlap_result(classes,valid,5120,in->mode,weights,in->context_count,&confidence,&result.primary,&result.coverage,&result.detail);
 result.score=result.primary;
 if(in->enhanced){
 int32_t detail,coverage;
 if(bitmap_classes(in->enrolled_low,in->probe_low,mask,64,80,ox,oy,rows,cols,adjusted,classes,&valid))return -1;
 overlap_result(classes,valid,5120,in->mode,weights,in->context_count,&confidence,&result.secondary,&coverage,&detail);
 if(in->mode&&detail>result.detail&&result.primary>220)result.detail=detail;
 result.score=secondary_result(result.primary,result.secondary,detail);
 }
 *out=result;return 0;
}
