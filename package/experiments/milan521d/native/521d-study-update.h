/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-template.h"
#include "521d-probe-cache.h"
typedef struct {int32_t action,selected;} Gx521dStudyUpdate;
/* Internal confirmed-match owner93900. Does not authorize learning itself.
 * Cache is required for FA1 replacement. All failures preserve caller state. */
int gx521d_study_update(Gx521dTemplate *,Gx521dProbeCache *,const Gx521dFeatureObject *,const uint8_t workspace[0x694],int allow_replace,Gx521dStudyUpdate *);
