/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-recognition.h"
#include "521d-probe-cache.h"
/* Initial recognition plus transient miss enqueue and FA1 cached rescue.
 * This API does not run adaptive study. Cache and template are atomic on error. */
int gx521d_recognize_cached(Gx521dTemplate *,Gx521dProbeCache *,const Gx521dFeatureObject *,uint32_t selector,int flags,Gx521dRecognition *);
