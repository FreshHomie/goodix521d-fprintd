/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-match-candidate.h"
#include "521d-match-geometry.h"
#include "521d-match-overlap.h"
typedef struct {int32_t pairs[42][2];Gx521dMatchGeometry geometry;Gx521dMatchOverlap overlap;int32_t inliers,admitted,relation[7];} Gx521dEnrollmentRelation;
/* Type17 enrollment correspondence8f780, fitting90ce0 and relation admission
 * as used by55c50. Input selector/flags are unused; optional metadata absent. */
int gx521d_enrollment_relation(const Gx521dMatchCandidateInput *,Gx521dEnrollmentRelation *);
