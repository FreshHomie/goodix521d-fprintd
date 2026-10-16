/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-template.h"
/* Append-mode relation production92c00: copy supplied positive match edges,
 * route missing edges through existing neighbours, then fit untested samples.
 * Existing gallery is immutable. Output includes50 slots, unused slots unset. */
int gx521d_study_relations(const Gx521dTemplate *,const Gx521dFeatureObject *,const int32_t seed[50][7],int32_t out[50][7]);
