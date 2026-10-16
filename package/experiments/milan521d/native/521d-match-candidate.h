/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-feature-object.h"
typedef struct {Gx521dFeatureObject enrolled,probe;uint32_t selector;int32_t flags;} Gx521dMatchCandidateInput;
typedef struct {uint32_t measured;int32_t metrics[24],affine[6];uint8_t candidate[0x134];} Gx521dMatchCandidate;
/* Complete type17/default-tag single-feature measurement path within62a00.
 * Admission and enclosing multi-template state are separate APIs. */
int gx521d_match_candidate(const Gx521dMatchCandidateInput *,Gx521dMatchCandidate *);
/* Internal controller evidence: 0 if primary count did not exceed4, otherwise1+readiness. */
int gx521d_match_candidate_progress(const Gx521dMatchCandidateInput *,Gx521dMatchCandidate *,int *primary_passed);
