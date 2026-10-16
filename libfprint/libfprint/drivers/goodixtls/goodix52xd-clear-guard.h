/* SPDX-License-Identifier: MIT */
#pragma once
#include <glib.h>

/* This reference is measured locally at the pre-image FDT report. It is not
 * inferred from a finger that happens to be present when activation starts. */
static inline gboolean goodix52xd_clear_sample(const guint16 reference[8],
                                              const guint16 values[8])
{
    for (guint i = 0; i < 8; i++)
        if (ABS((gint)reference[i] - (gint)values[i]) > 12)
            return FALSE;
    return TRUE;
}

static inline guint goodix52xd_clear_streak(const guint16 reference[8],
                                           const guint16 previous[8],
                                           const guint16 values[8], guint streak)
{
    if (!goodix52xd_clear_sample(reference, values)) return 0;
    if (streak)
        for (guint i = 0; i < 8; i++)
            if (ABS((gint)previous[i] - (gint)values[i]) > 3) return 1;
    return MIN(streak + 1, 4);
}
