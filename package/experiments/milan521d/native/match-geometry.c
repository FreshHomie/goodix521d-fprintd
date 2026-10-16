/*
 * Goodix 521d initial triangle geometry, adapted from Milan match geometry
 * Type17 angle domain and tolerance from pinned owner8eeb0.
 * Copyright (C) 2026 goodix-fp-linux-dev contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "521d-match-geometry.h"
#define MILAN_MATCH_MAX_PAIRS 42

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum
{
  MILAN_AFFINE_Q8_ONE = 0x100,
  MILAN_AFFINE_Q8_HALF = 0x80,
  MILAN_AFFINE_Q8_SHIFT = 8,
  MILAN_AFFINE_SOLVE_SHIFT = 10,
  MILAN_AFFINE_SOLVE_SCALE = 1024,
  MILAN_ORIENTATION_HALF_PERIOD = 0x3244,
  MILAN_ORIENTATION_PERIOD = 0x6488,
  MILAN_ORIENTATION_FULL_PERIOD = 0x6488,
  MILAN_TRIANGLE_ORIENTATION_TOLERANCE = 0x800,
  MILAN_TRIANGLE_AFFINE_SYMMETRY_TOLERANCE = 0x32,
  MILAN_TRIANGLE_AFFINE_COEFFICIENT_LIMIT = 300,
  MILAN_TRIANGLE_MINIMUM_EDGE_SQUARED = 0x2ffff,
  MILAN_RECOGNITION_TRIANGLE_LIMIT = 0x3b2,
  MILAN_RECOGNITION_NO_MODEL_RESIDUAL = 0x190000,
  MILAN_RECOGNITION_INLIER_AXIS_LIMIT = 0x281,
  MILAN_RECOGNITION_INLIER_SQUARED_LIMIT = 0x64000,
  MILAN_RECOGNITION_ORIENTATION_LIMIT = 0x506,
  MILAN_RECOGNITION_EARLY_INLIER_COUNT = 20,
  MILAN_RECOGNITION_REFINE_INLIER_COUNT = 3,
  MILAN_RECOGNITION_REFINE_RESIDUAL = 0x4000,
};

static void
milan_affine_from_three_points (const int32_t source[6],
                                const int32_t target[6],
                                int32_t       affine[6])
{
  int64_t determinant;
  int64_t raw_a;
  int64_t raw_b;
  int64_t raw_c;
  int64_t raw_d;

  determinant =
    (int64_t) (source[0] - source[2]) * (source[3] - source[5]) -
    (int64_t) (source[2] - source[4]) * (source[1] - source[3]);
  if (determinant == 0)
    {
      raw_a = INT32_MAX;
      raw_b = INT32_MAX;
    }
  else
    {
      raw_a =
        ((int64_t) (target[0] - target[2]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[3] - source[5]) -
         (int64_t) (target[2] - target[4]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[1] - source[3])) /
        determinant;
      raw_b =
        ((int64_t) (target[0] - target[2]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[2] - source[4]) -
         (int64_t) (target[2] - target[4]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[0] - source[2])) /
        -determinant;
    }
  raw_a = (int32_t) raw_a;
  raw_b = (int32_t) raw_b;
  affine[0] = (int32_t) raw_a >> 2;
  affine[1] = (int32_t) raw_b >> 2;
  affine[2] = (int32_t) ((((int64_t) target[0] << MILAN_AFFINE_SOLVE_SHIFT) -
                           raw_b * source[1] - raw_a * source[0]) >>
                          MILAN_AFFINE_SOLVE_SHIFT);

  determinant =
    (int64_t) (source[2] - source[0]) * (source[5] - source[1]) -
    (int64_t) (source[4] - source[0]) * (source[3] - source[1]);
  if (determinant == 0)
    {
      raw_c = INT32_MAX;
      raw_d = INT32_MAX;
    }
  else
    {
      raw_c =
        ((int64_t) (target[3] - target[1]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[5] - source[1]) -
         (int64_t) (target[5] - target[1]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[3] - source[1])) /
        determinant;
      raw_d =
        ((int64_t) (target[3] - target[1]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[4] - source[0]) -
         (int64_t) (target[5] - target[1]) * MILAN_AFFINE_SOLVE_SCALE *
           (source[2] - source[0])) /
        -determinant;
    }
  raw_c = (int32_t) raw_c;
  raw_d = (int32_t) raw_d;
  affine[3] = (int32_t) raw_c >> 2;
  affine[4] = (int32_t) raw_d >> 2;
  affine[5] = (int32_t) ((((int64_t) target[5] << MILAN_AFFINE_SOLVE_SHIFT) -
                           raw_d * source[5] - raw_c * source[4]) >>
                          MILAN_AFFINE_SOLVE_SHIFT);
}

static int
milan_affine_is_valid (const int32_t affine[6])
{
  int64_t a = affine[0];
  int64_t b = affine[1];
  int64_t c = affine[3];
  int64_t d = affine[4];
  /* Native sign-extends each wrapped dword product before the qword sum. */
  int64_t first = (int64_t) (int32_t) (a * a) + (int32_t) (b * b);
  int64_t cross = (int64_t) (int32_t) (a * c) + (int32_t) (b * d);
  int64_t second = (int64_t) (int32_t) (c * c) + (int32_t) (d * d);
  int64_t trace = first + second;
  int64_t discriminant = (int64_t) (
    (uint64_t) trace * (uint64_t) trace +
    UINT64_C (4) * ((uint64_t) cross * (uint64_t) cross -
                    (uint64_t) first * (uint64_t) second));
  int64_t lower = ((int64_t) 163 << 9) - trace;
  int64_t upper = ((int64_t) 401 << 9) - trace;

  return discriminant >= 0 && trace >= 0 && lower < 1 && upper >= 0 &&
         lower * lower != discriminant && discriminant <= lower * lower &&
         upper * upper != discriminant && discriminant <= upper * upper;
}

static int32_t
milan_reinterpret_uint32_as_int32 (uint32_t value)
{
  int32_t result;

  memcpy (&result, &value, sizeof(result));
  return result;
}

static int32_t
milan_triangle_shift_right_two (uint32_t value)
{
  uint32_t shifted = value >> 2;

  if (value & UINT32_C (0x80000000))
    shifted |= UINT32_C (0xc0000000);
  return milan_reinterpret_uint32_as_int32 (shifted);
}

static int32_t
milan_triangle_distance_shift_then_add (int32_t first_x,
                                         int32_t first_y,
                                         int32_t second_x,
                                         int32_t second_y)
{
  uint32_t dx = (uint32_t) first_x - (uint32_t) second_x;
  uint32_t dy = (uint32_t) first_y - (uint32_t) second_y;
  uint32_t distance =
    (uint32_t) milan_triangle_shift_right_two (dx * dx) +
    (uint32_t) milan_triangle_shift_right_two (dy * dy);

  return milan_reinterpret_uint32_as_int32 (distance);
}

static int32_t
milan_triangle_distance_add_then_shift (int32_t first_x,
                                         int32_t first_y,
                                         int32_t second_x,
                                         int32_t second_y)
{
  uint32_t dx = (uint32_t) first_x - (uint32_t) second_x;
  uint32_t dy = (uint32_t) first_y - (uint32_t) second_y;

  return milan_triangle_shift_right_two (dx * dx + dy * dy);
}

static int
milan_triangle_edge_is_consistent (int32_t source_distance,
                                   int32_t target_distance)
{
  return milan_reinterpret_uint32_as_int32 (
           (uint32_t) source_distance * UINT32_C (5)) <=
           milan_reinterpret_uint32_as_int32 (
             (uint32_t) target_distance * UINT32_C (6)) &&
         milan_reinterpret_uint32_as_int32 (
           (uint32_t) target_distance * UINT32_C (5)) <=
           milan_reinterpret_uint32_as_int32 (
             (uint32_t) source_distance * UINT32_C (6)) &&
          source_distance > MILAN_TRIANGLE_MINIMUM_EDGE_SQUARED &&
          target_distance > MILAN_TRIANGLE_MINIMUM_EDGE_SQUARED;
}

static int32_t
milan_normalize_orientation_difference (int32_t difference,
                                        int32_t half_period,
                                        int32_t period)
{
  if (difference > half_period)
    difference -= period;
  if (difference < -half_period)
    difference += period;
  return difference;
}

static int
milan_triangle_orientations_are_consistent (int32_t differences[3])
{
  const int32_t tolerance = MILAN_TRIANGLE_ORIENTATION_TOLERANCE;

  /* Type17 flag0 skips the type12 opposite-angle retry (8f260). */
  int32_t average=(differences[0]+differences[1]+differences[2])/3;
  for(size_t i=0;i<3;i++)
    if(differences[i]-average>tolerance||differences[i]-average< -tolerance)return 0;
  return 1;
}

static int
milan_triangle_affine_is_consistent (const int32_t affine[6])
{
  return abs (affine[0] - affine[4]) <
           MILAN_TRIANGLE_AFFINE_SYMMETRY_TOLERANCE &&
         abs (affine[3] + affine[1]) <
           MILAN_TRIANGLE_AFFINE_SYMMETRY_TOLERANCE &&
         abs (affine[0]) < MILAN_TRIANGLE_AFFINE_COEFFICIENT_LIMIT &&
         abs (affine[3]) < MILAN_TRIANGLE_AFFINE_COEFFICIENT_LIMIT &&
         abs (affine[4]) < MILAN_TRIANGLE_AFFINE_COEFFICIENT_LIMIT &&
         abs (affine[1]) < MILAN_TRIANGLE_AFFINE_COEFFICIENT_LIMIT;
}


static int
filter_geometry (
  const GoodixMilanFeatureRecord *enrolled_records,
  const GoodixMilanFeatureRecord *probe_records,
  const int32_t                  *pairs,
  size_t                          match_count,
  int32_t                         best_affine[6],
  int                            *best_residual,
  uint8_t                        *output_mask,
  unsigned                        minimum_inliers)
{
  int best_inliers = 0;
  uint8_t best_mask[MILAN_MATCH_MAX_PAIRS] = { 0 };
  int triangle_count = 0;

  *best_residual = MILAN_RECOGNITION_NO_MODEL_RESIDUAL;
  memset (best_affine, 0, 6 * sizeof(*best_affine));
  for (size_t first = 0; first + 2 < match_count; first++)
    {
      if (triangle_count >= MILAN_RECOGNITION_TRIANGLE_LIMIT)
        break;
      for (size_t second = first + 1; second + 1 < match_count; second++)
        {
          const size_t edge[2] = { first, second };
          int32_t source[6];
          int32_t target[6];
          int32_t edge_orientations[2];

          for (size_t i = 0; i < 2; i++)
            {
              int32_t enrolled_index = pairs[edge[i] * 2];
              int32_t probe_index = pairs[edge[i] * 2 + 1];

              source[i * 2] =
                (uint16_t) probe_records[probe_index].refined_x;
              source[i * 2 + 1] =
                (uint16_t) probe_records[probe_index].refined_y;
              target[i * 2] =
                (uint16_t) enrolled_records[enrolled_index].refined_x;
              target[i * 2 + 1] =
                (uint16_t) enrolled_records[enrolled_index].refined_y;
              edge_orientations[i] = milan_normalize_orientation_difference (
                enrolled_records[enrolled_index].orientation -
                probe_records[probe_index].orientation,
                MILAN_ORIENTATION_HALF_PERIOD, MILAN_ORIENTATION_PERIOD);
            }
          if (!milan_triangle_edge_is_consistent (
                milan_triangle_distance_shift_then_add (
                  source[0], source[1], source[2], source[3]),
                milan_triangle_distance_shift_then_add (
                  target[0], target[1], target[2], target[3])))
            continue;
          for (size_t third = second + 1; third < match_count; third++)
            {
              /* The orientation retry mutates its per-triangle scratch. */
              int32_t orientation_differences[3] = {
                edge_orientations[0], edge_orientations[1], 0
              };
              int32_t affine[6];
              int inliers = 0;
              int64_t residual_sum = 0;
              uint8_t mask[MILAN_MATCH_MAX_PAIRS] = { 0 };

              {
                int32_t enrolled_index = pairs[third * 2];
                int32_t probe_index = pairs[third * 2 + 1];

                source[4] = (uint16_t) probe_records[probe_index].refined_x;
                source[5] = (uint16_t) probe_records[probe_index].refined_y;
                target[4] = (uint16_t) enrolled_records[enrolled_index].refined_x;
                target[5] = (uint16_t) enrolled_records[enrolled_index].refined_y;
                orientation_differences[2] =
                  milan_normalize_orientation_difference (
                    enrolled_records[enrolled_index].orientation -
                      probe_records[probe_index].orientation,
                    MILAN_ORIENTATION_HALF_PERIOD, MILAN_ORIENTATION_PERIOD);
              }
              if (!milan_triangle_edge_is_consistent (
                    milan_triangle_distance_shift_then_add (
                      source[0], source[1], source[4], source[5]),
                    milan_triangle_distance_shift_then_add (
                      target[0], target[1], target[4], target[5])) ||
                  !milan_triangle_edge_is_consistent (
                    milan_triangle_distance_add_then_shift (
                      source[2], source[3], source[4], source[5]),
                    milan_triangle_distance_add_then_shift (
                      target[2], target[3], target[4], target[5])) ||
                  !milan_triangle_orientations_are_consistent (
                    orientation_differences))
                continue;
              triangle_count++;
              milan_affine_from_three_points (source, target, affine);
              if (!milan_triangle_affine_is_consistent (affine))
                continue;
              for (size_t i = 0; i < match_count; i++)
                {
                  int32_t enrolled_index = pairs[i * 2];
                  int32_t probe_index = pairs[i * 2 + 1];
                  int32_t x = (uint16_t) probe_records[probe_index].refined_x;
                  int32_t y = (uint16_t) probe_records[probe_index].refined_y;
                  int64_t transformed_x =
                    (((int64_t) affine[0] * x +
                      (int64_t) affine[1] * y + 0x80) >> 8) + affine[2];
                  int64_t transformed_y =
                    (((int64_t) affine[3] * x +
                      (int64_t) affine[4] * y + 0x80) >> 8) + affine[5];
                  int64_t dx = transformed_x -
                    (uint16_t) enrolled_records[enrolled_index].refined_x;
                  int64_t dy = transformed_y -
                    (uint16_t) enrolled_records[enrolled_index].refined_y;
                  int64_t squared = dx * dx + dy * dy;

                  if (llabs (dx) < MILAN_RECOGNITION_INLIER_AXIS_LIMIT &&
                      llabs (dy) < MILAN_RECOGNITION_INLIER_AXIS_LIMIT &&
                      squared < MILAN_RECOGNITION_INLIER_SQUARED_LIMIT)
                    {
                      inliers++;
                      residual_sum += squared;
                      mask[i] = 1;
                    }
                }
              int residual = inliers == 0
                               ? MILAN_RECOGNITION_NO_MODEL_RESIDUAL
                               : (int) ((residual_sum + (inliers >> 1)) /
                                        inliers);
              if (inliers >= (int) minimum_inliers && (inliers > best_inliers ||
                   (inliers == best_inliers && residual < *best_residual)) &&
                  milan_affine_is_valid (affine))
                {
                  best_inliers = inliers;
                  *best_residual = residual;
                  memcpy (best_affine, affine, 6 * sizeof(*best_affine));
                  memcpy (best_mask, mask, sizeof(best_mask));
                }
              if (best_inliers > MILAN_RECOGNITION_EARLY_INLIER_COUNT)
                goto done;
            }
        }
    }
done:
  if (output_mask)memcpy(output_mask,best_mask,sizeof(best_mask));
  return best_inliers;
}

int gx521d_match_geometry(const Gx521dMatchGeometryInput *in,Gx521dMatchGeometry *out)
{
 if(!in||!out||in->group>2||in->minimum_inliers>42)return 0;
 Gx521dMatchPairs selected;if(!gx521d_match_pairs(&in->input,&selected))return 0;
 unsigned count=0;while(count<31&&selected.pairs[in->group][count][0]>=0)count++;
 Gx521dMatchGeometry result={0};
 filter_geometry(in->input.features.enrolled,in->input.features.probe,
                 &selected.pairs[in->group][0][0],count,result.affine,
                 &result.residual,result.mask,in->minimum_inliers);
 *out=result;return 1;
}

#include "521d-match-pair-fit.h"
int gx521d_match_geometry_pairs(const Gx521dMatchPairFitInput *in,Gx521dMatchGeometry *out)
{
 if(!in||!out||in->features.enrolled_count>120||in->features.probe_count>120||in->minimum_inliers>42)return 0;
 unsigned count=0;int end=0;
 for(unsigned i=0;i<31;i++){
  int a=in->pairs[i][0],b=in->pairs[i][1];
  if(a==-1&&b==-1){end=1;continue;}
  if(end||a<0||b<0||(unsigned)a>=in->features.enrolled_count||(unsigned)b>=in->features.probe_count)return 0;
  count++;
 }
 Gx521dMatchGeometry result={0};filter_geometry(in->features.enrolled,in->features.probe,&in->pairs[0][0],count,result.affine,&result.residual,result.mask,in->minimum_inliers);
 *out=result;return 1;
}
