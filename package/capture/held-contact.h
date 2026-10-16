/* SPDX-License-Identifier: MIT
 * Diagnostic policy for a finger already present after recent clear calibration. */
#pragma once
#include "cache-experiment.h"
#include "goodix52xd-clear-guard.h"

#define GOODIX_HELD_MAX_US GOODIX_CACHE_MAX_US
enum { GOODIX_EARLY_UNKNOWN, GOODIX_EARLY_CLEAR, GOODIX_EARLY_HELD };

static inline gboolean goodix_held_deadline_valid(guint64 deadline)
{
    guint64 now=goodix_cache_boottime_us();
    return now && deadline>now && deadline-now<=GOODIX_HELD_MAX_US;
}

static inline guint goodix_early_classify(const guint16 reference[8],
                                         const guint16 values[8])
{
    /* Clear has precedence. A partially touched/ambiguous report never permits
     * bypassing the existing clear gate. Both thresholds are unchanged. */
    if (goodix52xd_clear_sample(reference,values)) return GOODIX_EARLY_CLEAR;
    guint changed=0;
    for (guint i=0;i<8;i++)
        if ((gint)reference[i]-(gint)values[i]>20)changed++;
    return changed>=4 ? GOODIX_EARLY_HELD : GOODIX_EARLY_UNKNOWN;
}

static inline guint goodix_early_streak(guint previous_kind,guint kind,
        const guint16 previous[8],const guint16 current[8],guint streak)
{
    if (kind==GOODIX_EARLY_UNKNOWN)return 0;
    if (kind!=previous_kind)return 1;
    if (streak)
        for(guint i=0;i<8;i++)
            if(ABS((gint)previous[i]-(gint)current[i])>3)return 1;
    return MIN(streak+1,2);
}

static inline guint goodix_early_final_classify(const guint16 baseline[8],
                                               const guint16 values[8])
{
    if(goodix_cache_baseline_valid(baseline,values))return GOODIX_EARLY_CLEAR;
    return goodix_early_classify(baseline,values)==GOODIX_EARLY_HELD
        ? GOODIX_EARLY_HELD : GOODIX_EARLY_UNKNOWN;
}
