/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include "521d-classifier-persistence.h"
#include <string.h>
void gx521d_persistable_classifier_init(Gx521dPersistableClassifier *context)
{
  if (!context) return;
  memset(context,0,sizeof(*context));gx521d_classifier_init(&context->classifier);
  context->retained.version=10001;
}
int gx521d_persistable_classifier_import(Gx521dPersistableClassifier *context,
    const uint8_t *blob,size_t bytes)
{
  if (!context) return -1;
  Gx521dClassifierStoredState decoded;
  if (gx521d_classifier_load(&decoded,blob,bytes) || decoded.scalar28<0 || decoded.scalar28>50) return -1;
  gx521d_persistable_classifier_init(context);context->retained=decoded;
  Gx521dHistory *history=&context->classifier.history;
  history->count=decoded.scalar28;
  memcpy(history->reference,decoded.reference,sizeof(history->reference));
  memcpy(history->age,decoded.reference_age,sizeof(history->age));
  return 0;
}
int gx521d_persistable_classifier_export(const Gx521dPersistableClassifier *context,
    uint8_t *blob,size_t bytes)
{
  if (!context) return -1;
  Gx521dClassifierStoredState state=context->retained;
  const Gx521dHistory *history=&context->classifier.history;
  state.scalar28=history->count;
  memcpy(state.reference,history->reference,sizeof(state.reference));
  memcpy(state.reference_age,history->age,sizeof(state.reference_age));
  return gx521d_classifier_store(&state,blob,bytes);
}
