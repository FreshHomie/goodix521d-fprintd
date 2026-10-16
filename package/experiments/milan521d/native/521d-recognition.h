/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-template.h"
typedef struct {uint32_t status;int32_t decision;uint8_t workspace[0x694];} Gx521dRecognition;
/* Type17 default-tag recognition. Current scope: headerFA0, no adaptive rescue.
 * Return0 on completed decision, -1 on invalid input, -2 unsupported mode.
 * A completed status0 does not mean a match: decision must be positive. */
int gx521d_recognize(Gx521dTemplate *,const Gx521dFeatureObject *,uint32_t selector,int flags,Gx521dRecognition *);
