/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include <stdint.h>
typedef struct {int32_t affine[6];uint32_t selector;int32_t flags,config_seed[20];} Gx521dMatchControllerStateInput;
typedef struct {uint32_t status;int32_t config[20],rotation,area,readiness[3],rows[514];} Gx521dMatchControllerState;
/* Type17/80x64 default configuration and readiness owners5d060/5ebd0. */
int gx521d_match_controller_state(const Gx521dMatchControllerStateInput *,Gx521dMatchControllerState *);
