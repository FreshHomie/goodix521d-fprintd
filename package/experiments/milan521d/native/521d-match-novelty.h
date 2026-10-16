/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include <stdint.h>
/* Full80x64 blank mask union, owner881b0(mode0). Outputs novel pixels and
 * required x/y offsets; original mask is retained. Used by recognition60eb0. */
int gx521d_match_novelty(const int32_t affine[6],int32_t out[3]);
