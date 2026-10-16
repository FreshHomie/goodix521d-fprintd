/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "521d-default.h"
#include <string.h>
int gx521d_default_step(Gx521dImageContext *context,const uint16_t raw[GX521D_PIXELS],
    Gx521dDefaultResult *out,Gx521dImageTrace *trace)
{
  if (!out) return -1;
  memset(out,0,sizeof(*out));Gx521dImageResult image;Gx521dImageTrace local;
  if (!trace) trace=&local;
  int status=gx521d_image_step(context,raw,&image,trace);if (status) return status;
  /* RVA0x7a6b0: classifier rejection takes precedence over post-render status.
   * The quality helper's retry return only affects optional diagnostics. */
  out->status=image.classification_status==0xc351 ? 0xc351:image.post_status;
  out->coverage=image.coverage;out->quality=image.coverage<=5 ? 0:image.quality;
  memcpy(out->image,image.image,sizeof(out->image));
  /* Core clears the full caller span before type17 publishes its three bytes. */
  memcpy(out->auxiliary,trace->classified.auxiliary,3);
  return 0;
}
