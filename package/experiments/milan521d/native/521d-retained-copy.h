/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-feature-object.h"
/* Type17 retained-copy65f90, source count<=120 and auxiliary metadata absent.
 * Copies image/record data while retaining destination lifecycle fields. Atomic
 * on invalid arguments, and permits source==destination. */
int gx521d_retained_copy(const Gx521dFeatureObject *,Gx521dFeatureObject *);
