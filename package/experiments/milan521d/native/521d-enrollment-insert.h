/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-template.h"
/* Provisional raw transaction56440. Public enrollment admission/rollback follows
 * separately. Type17/C7zero, classification0, capacity50, up to50 raw samples. */
Gx521dTemplate *gx521d_enrollment_create(void);
int gx521d_enrollment_insert(Gx521dTemplate *,const Gx521dFeatureObject *,uint32_t *status,uint32_t *progress);
