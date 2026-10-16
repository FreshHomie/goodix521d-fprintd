/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-study-relations.h"
/* Internal append942d0, not an authentication or learning-admission API.
 * Caller must supply a confirmed-match workspace. Does not apply replacement,
 * queue import, or owner93900's graph-root repair. Atomic on failure. */
int gx521d_study_append(Gx521dTemplate *,const Gx521dFeatureObject *,const uint8_t workspace[0x694],int32_t *action);
