/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "521d-wrapper.h"
#include <string.h>
int gx521d_preprocessor_init(Gx521dPreprocessor *c,const uint16_t background[GX521D_PIXELS])
{
  if(!c)return 0x81;
  memset(c,0,sizeof(*c));c->parameters_ready=1;
  int status=gx521d_image_context_new(background,&c->image);if(!status)c->calibrated=1;return status;
}
void gx521d_preprocessor_clear(Gx521dPreprocessor *c)
{if(c){gx521d_image_context_free(c->image);gx521d_calidata_image_free(c->loaded);memset(c,0,sizeof(*c));}}
int gx521d_preprocessor_default(Gx521dPreprocessor *c,const Gx521dDescriptor *source,
    uint8_t auxiliary[GX521D_AUXILIARY_BYTES],Gx521dDescriptor *result,int32_t coverage_quality[2])
{
  if(!source||!result)return 0x81;
  if(!c||c->parameters_ready!=1||c->calibrated!=1)return 0x80;
  if((!c->image&&!c->loaded)||!coverage_quality||!auxiliary||!source->pixels||!result->pixels||source->bytes!=GX521D_PIXELS*2||result->bytes>GX521D_PIXELS)return 0x81;
  /* Export RVA0x4cd5c resets the entry selection on EACH mode0 invocation. */
  if(c->loaded)gx521d_calidata_image_reset_entry_selection(c->loaded);
  else gx521d_image_reset_entry_selection(c->image);
  Gx521dDefaultResult out;
  int status=c->loaded ? gx521d_calidata_image_step(c->loaded,source->pixels,&out,NULL):gx521d_default_step(c->image,source->pixels,&out,NULL);
  coverage_quality[0]=out.coverage;coverage_quality[1]=out.quality;
  result->coverage=(uint8_t)out.coverage;result->quality=(uint8_t)out.quality;
  if(status)return status;
  memcpy(result->pixels,out.image,result->bytes);memcpy(auxiliary,out.auxiliary,sizeof(out.auxiliary));return out.status;
}

int gx521d_preprocessor_load(Gx521dPreprocessor *c,const uint8_t *blob,size_t bytes)
{
  if(!c||c->parameters_ready!=1||c->calibrated!=1)return 0x80;
  Gx521dCalidataImage *loaded=NULL;int status=gx521d_calidata_image_new(blob,bytes,&loaded);
  if(status)return status;
  gx521d_image_context_free(c->image);gx521d_calidata_image_free(c->loaded);c->image=NULL;c->loaded=loaded;return 0;
}
int gx521d_preprocessor_save(const Gx521dPreprocessor *c,uint8_t *blob,size_t bytes)
{return c&&c->loaded ? gx521d_calidata_image_store(c->loaded,blob,bytes):0x80;}
