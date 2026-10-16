/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Adapted from Milan match/geometry.c,
 * Copyright (C) 2026 goodix-fp-linux-dev contributors.
 * Full-resolution type17 record-index fit: RVA8a500. */
#include "521d-match-index-refit.h"
#include "milan/transform-private.h"
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
static int32_t milan_reinterpret_uint32_as_int32(uint32_t bits)
{int32_t result;memcpy(&result,&bits,4);return result;}
static int64_t
milan_wrapped_multiply (int64_t first, int64_t second)
{
  uint64_t result = (uint64_t) first * (uint64_t) second;
  int64_t signed_result;

  memcpy (&signed_result, &result, sizeof(signed_result));
  return signed_result;
}

static int64_t
milan_wrapped_add (int64_t first, int64_t second)
{
  uint64_t result = (uint64_t) first + (uint64_t) second;
  int64_t signed_result;

  memcpy (&signed_result, &result, sizeof(signed_result));
  return signed_result;
}

static int64_t
milan_wrapped_subtract (int64_t first, int64_t second)
{
  uint64_t result = (uint64_t) first - (uint64_t) second;
  int64_t signed_result;

  memcpy (&signed_result, &result, sizeof(signed_result));
  return signed_result;
}

static void
milan_fill_record_index_map (const GoodixMilanFeatureRecord *records,
                             size_t                          begin,
                             size_t                          end,
                             int32_t                         radius,
                             int16_t                         map[5120])
{
  memset (map, 0xff,
          5120 * sizeof (*map));
  for (size_t index = begin; index < end; index++)
    {
      int32_t x = ((uint16_t) records[index].refined_x + 0x80) >> 8;
      int32_t y = ((uint16_t) records[index].refined_y + 0x80) >> 8;
      int32_t left = x > radius ? x - radius : 0;
      int32_t right = x + radius < 80 ?
                      x + radius :
                      80 - 1;
      int32_t top = y > radius ? y - radius : 0;
      int32_t bottom = y + radius < 64 ?
                       y + radius :
                       64 - 1;

      for (int32_t row = top; row <= bottom; row++)
        for (int32_t column = left; column <= right; column++)
          map[row * 80 + column] =
            (int16_t) index;
    }
}

static int
goodix_milan_refine_record_similarity (
  const GoodixMilanFeatureRecord *enrolled_records,
  size_t                          enrolled_record_count,
  size_t                          enrolled_partition,
  const GoodixMilanFeatureRecord *probe_records,
  size_t                          probe_record_count,
  size_t                          probe_partition,
  const int32_t                   transform[6],
  int32_t                         search_radius,
  int32_t                         refined[6])
{
  int32_t inverse[6];
  int32_t pairs[300];
  int16_t index_map[5120];
  size_t pair_count = 0;
  uint64_t source_x_sum = 0;
  uint64_t source_y_sum = 0;
  uint64_t target_x_sum = 0;
  uint64_t target_y_sum = 0;
  uint64_t source_square_sum = 0;
  int64_t dot_sum = 0;
  int64_t cross_sum = 0;

  if (enrolled_partition > enrolled_record_count ||
      probe_partition > probe_record_count || search_radius < 0 ||
      goodix_milan_transform_invert_s32_checked (transform, inverse) != 0)
    return -1;
  for (size_t partition = 0; partition < 2; partition++)
    {
      size_t enrolled_begin = partition == 0 ? 0 : enrolled_partition;
      size_t enrolled_end = partition == 0 ? enrolled_partition
                                            : enrolled_record_count;
      size_t probe_begin = partition == 0 ? 0 : probe_partition;
      size_t probe_end = partition == 0 ? probe_partition : probe_record_count;

      milan_fill_record_index_map (
        probe_records, probe_begin, probe_end, search_radius, index_map);
      for (size_t index = enrolled_begin; index < enrolled_end; index++)
        {
          int32_t x = (uint16_t) enrolled_records[index].refined_x;
          int32_t y = (uint16_t) enrolled_records[index].refined_y;
          int64_t mapped_x =
            (((int64_t) inverse[0] * x + (int64_t) inverse[1] * y + 0x80) >> 8) +
            inverse[2];
          int64_t mapped_y =
            (((int64_t) inverse[3] * x + (int64_t) inverse[4] * y + 0x80) >> 8) +
            inverse[5];
          /* Native rounds the translated coordinate as a wrapping dword. */
          int32_t pixel_x = milan_reinterpret_uint32_as_int32 (
            (uint32_t) mapped_x + UINT32_C (0x80)) >> 8;
          int32_t pixel_y = milan_reinterpret_uint32_as_int32 (
            (uint32_t) mapped_y + UINT32_C (0x80)) >> 8;

          if (pixel_x < 0 ||
              pixel_x >= 80 ||
              pixel_y < 0 ||
              pixel_y >= 64)
            continue;
          int32_t probe_index =
            index_map[pixel_y * 80 +
                      pixel_x];
          if (probe_index < 0 || pair_count == 150)
            continue;
          pairs[pair_count * 2] = (int32_t) index;
          pairs[pair_count * 2 + 1] = probe_index;
          pair_count++;
        }
    }
  if (pair_count < 3)
    return -1;
  for (size_t pair = 0; pair < pair_count; pair++)
    {
      int32_t enrolled_index = pairs[pair * 2];
      int32_t probe_index = pairs[pair * 2 + 1];
      uint32_t source_x = (uint16_t) probe_records[probe_index].refined_x;
      uint32_t source_y = (uint16_t) probe_records[probe_index].refined_y;
      uint32_t target_x =
        (uint16_t) enrolled_records[enrolled_index].refined_x;
      uint32_t target_y =
        (uint16_t) enrolled_records[enrolled_index].refined_y;

      source_x_sum += source_x;
      source_y_sum += source_y;
      target_x_sum += target_x;
      target_y_sum += target_y;
      source_square_sum +=
        (uint64_t) source_x * source_x + (uint64_t) source_y * source_y;
      dot_sum += (int64_t) target_x * source_x +
                 (int64_t) target_y * source_y;
      cross_sum += (int64_t) target_x * -(int64_t) source_y +
                   (int64_t) target_y * source_x;
    }

  int64_t count = (int64_t) pair_count;
  int64_t source_x = (int64_t) (source_x_sum + 0x80) >> 8;
  int64_t source_y = (int64_t) (source_y_sum + 0x80) >> 8;
  int64_t target_x = (int64_t) (target_x_sum + 0x80) >> 8;
  int64_t target_y = (int64_t) (target_y_sum + 0x80) >> 8;
  int64_t squares = (int64_t) (source_square_sum + 0x8000) >> 16;
  int64_t dot = (dot_sum + 0x8000) >> 16;
  int64_t cross = (cross_sum + 0x8000) >> 16;
  int64_t denominator =
    squares * count - source_x * source_x - source_y * source_y;

  if (denominator == 0)
    return -1;
  int64_t reciprocal = ((denominator >> 1) + INT64_C (0x800000000)) /
                       denominator;
  /* At most 150 unsigned-word pairs bound the sums and denominator, and
   * reciprocal * source_x/y fit signed64. Later fit products wrap natively. */
  int64_t center = milan_wrapped_add (
    milan_wrapped_multiply (reciprocal,
                           source_x * source_x + source_y * source_y),
    INT64_C (0x800000000)) / count;
  int64_t negative_source_x = -(reciprocal * source_x);
  int64_t negative_source_y = -(reciprocal * source_y);

  refined[0] = milan_reinterpret_uint32_as_int32 ((uint32_t) (
    milan_wrapped_multiply (count * dot - target_y * source_y -
                           target_x * source_x, reciprocal) >> 27));
  refined[4] = refined[0];
  refined[3] = milan_reinterpret_uint32_as_int32 ((uint32_t) (
    milan_wrapped_multiply (count * cross - target_y * source_x +
                           target_x * source_y, reciprocal) >> 27));
  refined[1] = milan_reinterpret_uint32_as_int32 (
    UINT32_C (0) - (uint32_t) refined[3]);
  refined[5] = milan_reinterpret_uint32_as_int32 ((uint32_t) (
    milan_wrapped_add (
      milan_wrapped_add (milan_wrapped_multiply (negative_source_x, cross),
                        milan_wrapped_multiply (negative_source_y, dot)),
      milan_wrapped_multiply (center, target_y)) >> 27));
  refined[2] = milan_reinterpret_uint32_as_int32 ((uint32_t) (
    milan_wrapped_add (
      milan_wrapped_subtract (milan_wrapped_multiply (negative_source_x, dot),
                             milan_wrapped_multiply (negative_source_y, cross)),
      milan_wrapped_multiply (center, target_x)) >> 27));
  return 0;
}


int gx521d_match_index_refit(const Gx521dMatchIndexRefitInput *in,int32_t out[6])
{
 if(!in||!out||in->features.enrolled_count>120||in->features.probe_count>120||in->features.enrolled_partition>in->features.enrolled_count||in->features.probe_partition>in->features.probe_count||in->radius<0||in->radius>8)return -1;
 for(unsigned i=0;i<6;i++)if(in->affine[i]<-1048576||in->affine[i]>1048576)return -1;
 int32_t refined[6];memcpy(refined,in->affine,sizeof(refined));
 (void)goodix_milan_refine_record_similarity(in->features.enrolled,in->features.enrolled_count,in->features.enrolled_partition,in->features.probe,in->features.probe_count,in->features.probe_partition,in->affine,in->radius,refined);
 memcpy(out,refined,sizeof(refined));return 0;
}
