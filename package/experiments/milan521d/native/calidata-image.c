/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "521d-calidata-image.h"
#include "521d-checkpoint.h"
#include "521d-classifier-persistence.h"
#include <stdlib.h>
#include <string.h>
struct Gx521dCalidataImage {
  Gx521dCalidata fields;
  Gx521dPersistableClassifier retained;
  Gx521dImageContext *image;
  int processed;
};
void gx521d_calidata_image_free(Gx521dCalidataImage *c)
{if(c){gx521d_image_context_free(c->image);free(c);}}
int gx521d_calidata_image_new(const uint8_t *blob,size_t bytes,Gx521dCalidataImage **out)
{
  if(!out)return -1;
  *out=NULL;Gx521dCalidataImage *c=calloc(1,sizeof(*c));if(!c)return -1;
  int status=gx521d_calidata_load(&c->fields,blob,bytes);
  if(status){free(c);return status;}
  Gx521dImageCheckpoint snapshot={.version=1,.rows=GX521D_ROWS,.columns=GX521D_COLUMNS};
  if(c->fields.sample_count<0||c->fields.sample_count>400){free(c);return -1;}
  snapshot.gain.sample_count=(uint32_t)c->fields.sample_count;
  memcpy(snapshot.setup,c->fields.setup,sizeof(snapshot.setup));
  memcpy(snapshot.gain.calibration,c->fields.calibration,sizeof(snapshot.gain.calibration));
  for(int i=0;i<GX521D_PIXELS;i++)snapshot.gain.application_gain[i]=snapshot.gain.auxiliary_gain[i]=snapshot.gain.secondary_gain[i]=8192;
  gx521d_persistable_classifier_init(&c->retained);
  uint8_t zero[GX521D_CLASSIFIER_STORE_BYTES]={0};
  if(memcmp(c->fields.classifier,zero,sizeof(zero)) &&
     gx521d_persistable_classifier_import(&c->retained,c->fields.classifier,sizeof(zero))) {
    free(c);return -1;
  }
  snapshot.history=c->retained.classifier.history;
  status=gx521d_image_restore(&snapshot,&c->image);
  if(status){free(c);return status;}
  *out=c;return 0;
}
int gx521d_calidata_image_step(Gx521dCalidataImage *c,const uint16_t raw[GX521D_PIXELS],
    Gx521dDefaultResult *out,Gx521dImageTrace *trace)
{
  if(!c)return -1;
  int status=gx521d_default_step(c->image,raw,out,trace);
  if(!status)c->processed=1;
  return status;
}
int gx521d_calidata_image_store(const Gx521dCalidataImage *c,uint8_t *blob,size_t bytes)
{
  if(!c)return -1;
  Gx521dImageCheckpoint snapshot;if(gx521d_image_checkpoint(c->image,&snapshot))return -1;
  Gx521dCalidata fields=c->fields;Gx521dPersistableClassifier retained=c->retained;
  fields.sample_count=(int32_t)snapshot.gain.sample_count;
  memcpy(fields.calibration,snapshot.gain.calibration,sizeof(fields.calibration));
  memcpy(fields.setup,snapshot.setup,sizeof(fields.setup));retained.classifier.history=snapshot.history;
  if(c->processed&&gx521d_persistable_classifier_export(&retained,fields.classifier,GX521D_CLASSIFIER_STORE_BYTES))return -1;
  return gx521d_calidata_store(&fields,blob,bytes);
}

void gx521d_calidata_image_reset_entry_selection(Gx521dCalidataImage *c)
{if(c)gx521d_image_reset_entry_selection(c->image);}
