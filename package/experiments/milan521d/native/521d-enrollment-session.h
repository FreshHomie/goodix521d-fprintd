/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-enrollment-insert.h"
/* Profile12/type17 mode0, absent optional metadata; caller owns session.
 * A successful add can report a vendor rejection in *status. Invalid arguments
 * preserve session and outputs. State mirrors public session bytes8..31. */
typedef struct {
 Gx521dTemplate *template;
 uint16_t required,accepted;
 int32_t progress,previous_overlap,overlap,detail,retry;
} Gx521dEnrollmentSession;
Gx521dEnrollmentSession *gx521d_enrollment_start(unsigned required);
int gx521d_enrollment_add(Gx521dEnrollmentSession *,const uint8_t pixels[5120],int quality,int coverage,uint32_t *status,int32_t cq[2]);
int gx521d_enrollment_delete_image(Gx521dEnrollmentSession *);
void gx521d_enrollment_finish(Gx521dEnrollmentSession *);
