/* SPDX-License-Identifier: MIT
 * Measure FDT reports before calibration. No image, template or vendor engine. */
#include "fprint.h"
#include <glib-unix.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

static gboolean interrupt_probe(gpointer data) {
  g_cancellable_cancel(data);
  return G_SOURCE_CONTINUE;
}
static gboolean timeout_probe(gpointer data) {
  g_cancellable_cancel(data);
  return G_SOURCE_REMOVE;
}
int main(int argc, char **argv) {
  gboolean identity=argc==2 && !strcmp(argv[1],"--identity");
  if (argc>1 && !identity) return 2;
  if (g_strcmp0(g_getenv(identity?"GOODIX521D_IDENTITY_PROBE":"GOODIX521D_CONTACT_PROBE"),"1") ||
      g_strcmp0(g_getenv("GOODIX521D_CAPTURE_PROFILE"),"pr81-full") ||
      g_getenv("GOODIX521D_SAVE_FRAMES")) {
    fprintf(stderr,"Requires contact-only profile with frame saving disabled\n");
    return 2;
  }
  g_autoptr(GCancellable) cancellable=g_cancellable_new();
  guint interrupt_id=g_unix_signal_add(SIGINT,interrupt_probe,cancellable);
  guint term_id=g_unix_signal_add(SIGTERM,interrupt_probe,cancellable);
  guint timer=g_timeout_add_seconds(40,timeout_probe,cancellable);
  g_autoptr(FpContext) context=fp_context_new();
  fp_context_enumerate(context);
  FpDevice *selected=NULL;
  GPtrArray *devices=fp_context_get_devices(context);
  for (guint i=0;i<devices->len;i++) {
    FpDevice *candidate=g_ptr_array_index(devices,i);
    if (!g_strcmp0(fp_device_get_driver(candidate),"goodixtls52xd")) {
      if (selected) { fprintf(stderr,"Multiple matching readers\n");return 3; }
      selected=candidate;
    }
  }
  if (!selected) { fprintf(stderr,"No matching reader\n");return 3; }
  g_autoptr(GError) error=NULL;
  if (!fp_device_open_sync(selected,cancellable,&error)) {
    fprintf(stderr,"Open failed: %s\n",error->message);return 4;
  }
  g_autoptr(FpImage) image=fp_device_capture_sync(selected,TRUE,cancellable,&error);
  gboolean complete=g_error_matches(error,G_IO_ERROR,G_IO_ERROR_CANCELLED) &&
      !strcmp(error->message,identity?"Identity probe complete before calibration":"Contact probe complete before calibration");
  if (!complete || image) {
    fprintf(stderr,"Probe failed: %s\n",error?error->message:"Unexpected image or missing probe end");
  }
  g_clear_error(&error);
  if (!fp_device_close_sync(selected,NULL,&error)) {
    fprintf(stderr,"Close failed: %s\n",error->message);complete=FALSE;
  }
  /* Timer can have fired on failure; avoid removing a stale source ID. */
  GSource *source=g_main_context_find_source_by_id(NULL,timer);
  if (source) g_source_destroy(source);
  g_source_remove(interrupt_id);g_source_remove(term_id);
  if (!complete) return 4;
  puts(identity?"IDENTITY_PROBE_COMPLETE":"CONTACT_PROBE_COMPLETE");return 0;
}
