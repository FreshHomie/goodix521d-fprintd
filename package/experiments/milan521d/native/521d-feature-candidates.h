/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef GX521D_FEATURE_CANDIDATES_H
#define GX521D_FEATURE_CANDIDATES_H
#include "521d-feature-scale.h"
#define GX521D_CANDIDATE_CAPACITY (6*68*52)
typedef struct {int32_t x,y,scale,response;} Gx521dCandidate;
typedef struct {uint32_t count;Gx521dCandidate points[GX521D_CANDIDATE_CAPACITY];} Gx521dCandidates;
/* Initial extrema only: no refinement, suppression, ranking or descriptors. */
int gx521d_feature_candidates_from_scales(const uint16_t scales[GX521D_FEATURE_SCALES][GX521D_PIXELS],Gx521dCandidates *out);
int gx521d_feature_candidates(const uint8_t image[GX521D_PIXELS],Gx521dCandidates *out);
#endif
