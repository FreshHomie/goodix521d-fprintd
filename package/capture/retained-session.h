/* SPDX-License-Identifier: MIT */
#pragma once
#include <glib.h>
#include <stdlib.h>
#include <time.h>
static inline gint64 goodix_retained_boottime(void) {
  struct timespec value;
  if (clock_gettime(CLOCK_BOOTTIME,&value)) return -1;
  return (gint64)value.tv_sec*G_USEC_PER_SEC+value.tv_nsec/1000;
}
static inline gboolean goodix_retained_valid(gint64 mono,gint64 boot,
                                             gint64 now,gint64 boot_now) {
  gint64 age=now-mono,boot_age=boot_now-boot;
  return mono>0 && boot>0 && boot_now>0 && age>=0 && age<=80*G_USEC_PER_SEC &&
         llabs(boot_age-age)<=100000;
}
