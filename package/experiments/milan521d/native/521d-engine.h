/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-recognition-cached.h"
#include "521d-study-update.h"
/* Full-resolution type17, default tag0 native owner. Template is caller-owned;
 * transient cache starts empty and is deliberately not serialized. */
typedef struct {Gx521dRecognition recognition;Gx521dStudyUpdate study;} Gx521dEngineResult;
/* Composed recognition + study owner66140. flags0:recognition only,
 * flags1:learning, flags2:append-only learning. Atomic on invalid input. */
int gx521d_engine_identify(Gx521dTemplate *,Gx521dProbeCache *,const Gx521dFeatureObject *,uint32_t selector,int flags,Gx521dEngineResult *);
