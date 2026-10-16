/* SPDX-License-Identifier: MIT
 * Workspace-only background-acquisition experiment. Not installed or packaged. */
#pragma once
#include <glib.h>
#include <string.h>
#include <time.h>

#define GOODIX_CACHE_MAX_US ((guint64)120 * G_USEC_PER_SEC)

static inline guint64 goodix_cache_boottime_us(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_BOOTTIME, &now) != 0 || now.tv_sec < 0)
        return 0;
    return (guint64)now.tv_sec * G_USEC_PER_SEC + now.tv_nsec / 1000;
}

static inline gboolean goodix_cache_deadline_valid(guint64 deadline)
{
    guint64 now = goodix_cache_boottime_us();
    return now != 0 && deadline > now && deadline - now <= GOODIX_CACHE_MAX_US;
}

static inline gboolean goodix_cache_image_step(const gchar *name)
{
    return !g_strcmp0(name, "clear-0") || !g_strcmp0(name, "clear-1") ||
           !g_strcmp0(name, "clear-2") || !g_strcmp0(name, "clear-3");
}

/* The measured awake-run baseline drift was at most2. Refuse a change above6
 * in any region in this experiment. This is not a universal device setting. */
static inline gboolean goodix_cache_baseline_valid(const guint16 cached[8],
                                                   const guint16 current[8])
{
    for (guint i = 0; i < 8; i++)
        if (ABS((gint)cached[i] - (gint)current[i]) > 6)
            return FALSE;
    return TRUE;
}

static inline gboolean goodix_cache_options_valid(gboolean full, gboolean contact,
        gboolean clear, gboolean reuse, gboolean saved, const gchar *deadline,
        guint64 *parsed)
{
    return full && contact && clear && reuse && saved && deadline &&
        g_ascii_string_to_unsigned(deadline, 10, 1, G_MAXUINT64, parsed, NULL) &&
        goodix_cache_deadline_valid(*parsed);
}
