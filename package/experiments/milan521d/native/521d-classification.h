/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#include "521d-render.h"
typedef struct {
  int32_t status,statistics[6],bins[400],quantiles[20];
} Gx521dClassificationHistogram;
typedef struct {
  int32_t parameters[4][18];
  uint16_t difference[GX521D_PIXELS];
  uint16_t blurred[GX521D_PIXELS];
  uint8_t valid[GX521D_PIXELS];
  uint16_t descending[GX521D_PIXELS],ascending[GX521D_PIXELS];
  int8_t descending_direction[GX521D_PIXELS],ascending_direction[GX521D_PIXELS];
  Gx521dClassificationHistogram primary_histogram;
  int32_t primary_summary[34];
  uint16_t gradient_average[GX521D_PIXELS];
  int32_t gradient_summary[34],gradient_summary_status;
} Gx521dClassificationPrepared;
int gx521d_classification_histogram(const uint16_t source[GX521D_PIXELS],
    const uint8_t valid[GX521D_PIXELS],int maximum,int minimum,
    Gx521dClassificationHistogram *out);
int gx521d_classification_primary_summary(const Gx521dClassificationHistogram *histogram,
    int upper_span,int lower_span,int32_t out[34]);
int gx521d_classification_score_summary(const uint16_t source[GX521D_PIXELS],
    const uint8_t valid[GX521D_PIXELS],int minimum,int extended,int32_t out[34]);
typedef struct {
  int32_t states[4];
  uint8_t classes[GX521D_PIXELS],auxiliary[GX521D_PIXELS];
} Gx521dColdClassification;
/* Fresh classifier context only; excludes temporal history and post-render. */
int gx521d_classification_cold(const uint16_t live[GX521D_PIXELS],
    const Gx521dMask *mask,const Gx521dClassificationPrepared *prepared,
    Gx521dColdClassification *out);
typedef struct {
  int32_t status,mode,apply_mask;
  uint32_t counts[3];
  uint8_t auxiliary_summary[3];
} Gx521dClassificationDecision;
int gx521d_classification_prepare(const uint16_t live[GX521D_PIXELS],
    const uint16_t setup[GX521D_PIXELS],const Gx521dMask *mask,
    Gx521dClassificationPrepared *out);
int gx521d_classification_direction(const uint16_t live[GX521D_PIXELS],
    const uint8_t valid[GX521D_PIXELS],int active_count);
/* Publishes a class plane and four states supplied by the inner classifier.
 * The type17 inner classifier itself is not implemented by this function. */
int gx521d_classification_publish(uint8_t classes[GX521D_PIXELS],int active_count,
    const int32_t states[4],int purpose,Gx521dClassificationDecision *out);

/* Intermediate snapshots used only by the differential validation harness. */
typedef struct {
  uint16_t scores[2][GX521D_PIXELS];
  int32_t summaries[2][34],statuses[2];
  uint8_t classes[2][GX521D_PIXELS];
} Gx521dClassificationTrace;
void gx521d_classification_walk(const Gx521dClassificationPrepared *prepared,
    int down,int intensity_cutoff,int raw_cutoff,uint16_t scores[GX521D_PIXELS]);

void gx521d_classification_admit(const Gx521dClassificationPrepared *prepared,
    int down,int kind,const uint16_t scores[GX521D_PIXELS],
    const int32_t score_summary[34],Gx521dColdClassification *out);

#include "521d-history.h"
typedef struct { Gx521dHistory history; } Gx521dClassifierContext;
void gx521d_classifier_init(Gx521dClassifierContext *context);
/* Isolated retained classifier: gain/calibration comes from its caller.
 * Does not import/export the vendor's persisted workspace or apply post-render updates. */
int gx521d_classification_step(Gx521dClassifierContext *context,
    const uint16_t live[GX521D_PIXELS],const uint16_t setup[GX521D_PIXELS],
    const Gx521dMask *mask,const Gx521dColdGain *gain,int purpose,
    Gx521dColdClassification *out);
