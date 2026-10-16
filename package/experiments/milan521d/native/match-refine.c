/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Type17 orientation consensus and affine refit (5eca0,57470,5f0f0,8e8c0).
 * Least-squares adaptation from Milan geometry.c,
 * (C)2026 goodix-fp-linux-dev contributors. */
#include "521d-match-refine.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define MILAN_AFFINE_Q8_ONE 256
#define MILAN_AFFINE_Q8_HALF 128
#define MILAN_AFFINE_Q8_SHIFT 8
#define MILAN_RECOGNITION_NO_MODEL_RESIDUAL 0x190000
#define MILAN_RECOGNITION_INLIER_SQUARED_LIMIT 0x64000
static int32_t milan_reinterpret_uint32_as_int32(uint32_t bits)
{int32_t result;memcpy(&result,&bits,4);return result;}
static int32_t
milan_divide_affine_coefficient (int64_t numerator,
                                 int64_t denominator,
                                 int64_t half_denominator)
{
  uint64_t dividend_bits;
  int64_t dividend;
  int64_t quotient;

  if (denominator == 0)
    return 0;
  /* Native rounds with wrapping qword arithmetic before signed division. The
   * rounded dividend can be negative even with a positive Gram determinant. */
  dividend_bits = numerator < 0 ?
                  (uint64_t) half_denominator - (uint64_t) numerator :
                  (uint64_t) numerator + (uint64_t) half_denominator;
  memcpy (&dividend, &dividend_bits, sizeof (dividend));
  quotient = dividend / denominator;
  return milan_reinterpret_uint32_as_int32 (
    numerator < 0 ? UINT32_C (0) - (uint32_t) quotient :
    (uint32_t) quotient);
}

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
milan_refine_affine_least_squares (
  const GoodixMilanFeatureRecord *enrolled_records,
  const GoodixMilanFeatureRecord *probe_records,
  const int32_t                  *pairs,
  size_t                          pair_count,
  const uint8_t                  *inliers,
  int32_t                         previous_residual,
  int32_t                         affine[6])
{
  int64_t matrix[3][3] = { { 0 } };
  int64_t right[3][2] = { { 0 } };
  size_t inlier_count = 0;
  int32_t refined[6];

  for (size_t i = 0; i < pair_count; i++)
    {
      if (!inliers[i])
        continue;
      int32_t enrolled_index = pairs[i * 2];
      int32_t probe_index = pairs[i * 2 + 1];
      int64_t source[3] = {
        (uint16_t) probe_records[probe_index].refined_x,
        (uint16_t) probe_records[probe_index].refined_y,
        MILAN_AFFINE_Q8_ONE,
      };
      int64_t target[2] = {
        (uint16_t) enrolled_records[enrolled_index].refined_x,
        (uint16_t) enrolled_records[enrolled_index].refined_y,
      };

      for (size_t row = 0; row < 3; row++)
        {
          for (size_t column = 0; column < 3; column++)
            matrix[row][column] += source[row] * source[column];
          for (size_t column = 0; column < 2; column++)
            right[row][column] += source[row] * target[column];
        }
      inlier_count++;
    }
  if (inlier_count < 3)
    return;
  for (size_t row = 0; row < 3; row++)
    {
      for (size_t column = 0; column < 3; column++)
        matrix[row][column] =
          (matrix[row][column] + MILAN_AFFINE_Q8_HALF) >>
          MILAN_AFFINE_Q8_SHIFT;
      for (size_t column = 0; column < 2; column++)
        right[row][column] =
          (right[row][column] + MILAN_AFFINE_Q8_HALF) >>
          MILAN_AFFINE_Q8_SHIFT;
    }

  int64_t cofactors[3][3] = {
    {
      milan_wrapped_subtract (
        milan_wrapped_multiply (matrix[1][1], matrix[2][2]),
        milan_wrapped_multiply (matrix[1][2], matrix[2][1])),
      milan_wrapped_subtract (
        milan_wrapped_multiply (matrix[0][2], matrix[2][1]),
        milan_wrapped_multiply (matrix[0][1], matrix[2][2])),
      milan_wrapped_subtract (
        milan_wrapped_multiply (matrix[0][1], matrix[1][2]),
        milan_wrapped_multiply (matrix[0][2], matrix[1][1])),
    },
    {
      milan_wrapped_subtract (
        milan_wrapped_multiply (matrix[2][0], matrix[1][2]),
        milan_wrapped_multiply (matrix[1][0], matrix[2][2])),
      milan_wrapped_subtract (
        milan_wrapped_multiply (matrix[0][0], matrix[2][2]),
        milan_wrapped_multiply (matrix[2][0], matrix[0][2])),
      milan_wrapped_subtract (
        milan_wrapped_multiply (matrix[1][0], matrix[0][2]),
        milan_wrapped_multiply (matrix[0][0], matrix[1][2])),
    },
    {
      milan_wrapped_subtract (
        milan_wrapped_multiply (matrix[1][0], matrix[2][1]),
        milan_wrapped_multiply (matrix[2][0], matrix[1][1])),
      milan_wrapped_subtract (
        milan_wrapped_multiply (matrix[2][0], matrix[0][1]),
        milan_wrapped_multiply (matrix[0][0], matrix[2][1])),
      milan_wrapped_subtract (
        milan_wrapped_multiply (matrix[0][0], matrix[1][1]),
        milan_wrapped_multiply (matrix[1][0], matrix[0][1])),
    },
  };
  int64_t determinant = milan_wrapped_add (
    milan_wrapped_add (
      milan_wrapped_multiply (matrix[0][0], cofactors[0][0]),
      milan_wrapped_multiply (matrix[0][1], cofactors[0][1])),
    milan_wrapped_multiply (matrix[0][2], cofactors[0][2]));
  int64_t rounded_determinant = milan_wrapped_add (
    determinant, MILAN_AFFINE_Q8_HALF);
  int64_t denominator = rounded_determinant >> MILAN_AFFINE_Q8_SHIFT;
  int64_t half_denominator = rounded_determinant >> 9;

  if (denominator == 0)
    return;
  for (size_t output = 0; output < 2; output++)
    {
      int64_t numerator[3];

      for (size_t row = 0; row < 3; row++)
        numerator[row] = milan_wrapped_add (
          milan_wrapped_add (
            milan_wrapped_multiply (right[0][output], cofactors[row][0]),
            milan_wrapped_multiply (right[1][output], cofactors[row][1])),
          milan_wrapped_multiply (right[2][output], cofactors[row][2]));

      refined[output * 3] = milan_divide_affine_coefficient (
        numerator[0], denominator, half_denominator);
      refined[output * 3 + 1] = milan_divide_affine_coefficient (
        numerator[1], denominator, half_denominator);
      refined[output * 3 + 2] = milan_divide_affine_coefficient (
        numerator[2], denominator, half_denominator);
    }

  size_t refined_inlier_count = 0;
  uint64_t refined_residual_sum = 0;

  for (size_t i = 0; i < pair_count; i++)
    {
      int32_t enrolled_index = pairs[i * 2];
      int32_t probe_index = pairs[i * 2 + 1];
      int32_t x = (uint16_t) probe_records[probe_index].refined_x;
      int32_t y = (uint16_t) probe_records[probe_index].refined_y;
      int64_t transformed_x =
        (((int64_t) refined[0] * x + (int64_t) refined[1] * y +
          MILAN_AFFINE_Q8_HALF) >> MILAN_AFFINE_Q8_SHIFT) +
        refined[2];
      int64_t transformed_y =
        (((int64_t) refined[3] * x + (int64_t) refined[4] * y +
          MILAN_AFFINE_Q8_HALF) >> MILAN_AFFINE_Q8_SHIFT) +
        refined[5];
      /* Refinement can publish large coefficients after orientation filtering.
       * Native narrows the residual to a signed dword before squaring it. */
      int64_t dx = milan_reinterpret_uint32_as_int32 (
        (uint32_t) transformed_x -
        (uint16_t) enrolled_records[enrolled_index].refined_x);
      int64_t dy = milan_reinterpret_uint32_as_int32 (
        (uint32_t) transformed_y -
        (uint16_t) enrolled_records[enrolled_index].refined_y);
      uint64_t squared = (uint64_t) (dx * dx) + (uint64_t) (dy * dy);

      if (squared < MILAN_RECOGNITION_INLIER_SQUARED_LIMIT)
        {
          refined_inlier_count++;
          refined_residual_sum += squared;
        }
    }
  int32_t refined_residual = refined_inlier_count == 0
                                ? MILAN_RECOGNITION_NO_MODEL_RESIDUAL
                               : (int32_t) ((refined_residual_sum +
                                             (refined_inlier_count >> 1)) /
                                            refined_inlier_count);

  if (refined_residual < previous_residual &&
      refined_inlier_count >= inlier_count)
    memcpy (affine, refined, sizeof(refined));
}


static int average(const int16_t *angles,const uint8_t *active,unsigned count)
{
 int sum=0;unsigned n=0;
 for(unsigned i=0;i<count;i++)if(active[i]){sum+=angles[i];n++;}
 return n?(sum+(int)(n/2))/(int)n:0;
}
static void consensus(const int16_t *angles,unsigned count,uint8_t *active)
{
 memset(active,1,count);
 for(unsigned retry=0;retry<42;retry++){
  int mean=average(angles,active,count),worst=-1;int16_t magnitude=0;
  for(unsigned i=0;i<count;i++)if(active[i]){
   int delta=angles[i]-(int16_t)mean,absolute=abs(delta);
   if(absolute>abs((int)magnitude)){worst=(int)i;magnitude=(int16_t)absolute;}
  }
  if(worst<0)return;
  active[worst]=0;mean=average(angles,active,count);
  int delta=abs((int)angles[worst]-(int16_t)mean);
  if(delta<=1024||delta>=0x6088){active[worst]=1;return;}
 }
}
int gx521d_match_refine(const Gx521dMatchGeometryInput *in,Gx521dMatchRefined *out)
{
 if(!in||!out)return 0;
 Gx521dMatchPairs pairs;Gx521dMatchGeometry model;
 if(in->group>2||!gx521d_match_pairs(&in->input,&pairs)||!gx521d_match_geometry(in,&model))return 0;
 Gx521dMatchRefined result={0};unsigned count=0;
 while(count<31&&pairs.pairs[in->group][count][0]>=0)count++;
 if(count<=2){*out=result;return 1;}
 memcpy(result.affine,model.affine,24);memcpy(result.mask,model.mask,42);
 int16_t angles[42]={0};uint8_t active[42]={0};unsigned map[42],angle_count=0;
 const GoodixMilanFeatureRecord *a=in->input.features.enrolled,*b=in->input.features.probe;
 for(unsigned i=0;i<count;i++)if(model.mask[i]){
  unsigned x=(unsigned)pairs.pairs[in->group][i][0],y=(unsigned)pairs.pairs[in->group][i][1];
  int delta=a[x].orientation-b[y].orientation;
  if(delta>0x3244)delta-=0x6488;else if(delta< -0x3244)delta+=0x6488;
  angles[angle_count]=(int16_t)delta;map[angle_count++]=i;
 }
 consensus(angles,angle_count,active);
 for(unsigned i=0;i<angle_count;i++)if(!active[i])result.mask[map[i]]=0;
 for(unsigned i=0;i<count;i++){
  if(result.mask[i]){result.retained++;result.enrolled_mask[pairs.pairs[in->group][i][0]]=1;}
 }
 if(result.retained>=4&&model.residual>0x4000)
  milan_refine_affine_least_squares(a,b,&pairs.pairs[in->group][0][0],count,result.mask,model.residual,result.affine);
 *out=result;return 1;
}

#include "521d-match-pair-fit.h"
int gx521d_match_refine_pairs(const Gx521dMatchPairFitInput *in,Gx521dMatchRefined *out)
{
 if(!in||!out)return 0;
 Gx521dMatchGeometry model;if(!gx521d_match_geometry_pairs(in,&model))return 0;
 Gx521dMatchRefined result={0};unsigned count=0;
 while(count<31&&in->pairs[count][0]>=0)count++;
 if(count<=2){*out=result;return 1;}
 memcpy(result.affine,model.affine,24);memcpy(result.mask,model.mask,42);
 int16_t angles[42]={0};uint8_t active[42]={0};unsigned map[42],angle_count=0;
 const GoodixMilanFeatureRecord *a=in->features.enrolled,*b=in->features.probe;
 for(unsigned i=0;i<count;i++)if(model.mask[i]){
  unsigned x=(unsigned)in->pairs[i][0],y=(unsigned)in->pairs[i][1];
  int delta=a[x].orientation-b[y].orientation;
  if(delta>0x3244)delta-=0x6488;else if(delta< -0x3244)delta+=0x6488;
  angles[angle_count]=(int16_t)delta;map[angle_count++]=i;
 }
 consensus(angles,angle_count,active);
 for(unsigned i=0;i<angle_count;i++)if(!active[i])result.mask[map[i]]=0;
 for(unsigned i=0;i<count;i++){
  if(result.mask[i]){result.retained++;result.enrolled_mask[in->pairs[i][0]]=1;}
 }
 if(result.retained>=4&&model.residual>0x4000)
  milan_refine_affine_least_squares(a,b,&in->pairs[0][0],count,result.mask,model.residual,result.affine);
 *out=result;return 1;
}
