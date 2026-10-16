/* SPDX-License-Identifier: MIT
 * Capture one diagnostic burst using the existing local libfprint driver.
 * No enrollment, template storage, firmware update, or vendor DLL execution. */
#include "fprint.h"
#include "fpi-image-device.h"
#include <glib-unix.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  GCancellable *cancellable;
  gboolean clear_only, calibration_complete, cancellation_queued, timed_out;
} Capture;

static gboolean cancel(gpointer data) {
  Capture *capture=data;
  g_cancellable_cancel(capture->cancellable);
  return G_SOURCE_REMOVE;
}
static gboolean timeout(gpointer data) {
  Capture *capture=data;
  capture->timed_out=TRUE;
  g_cancellable_cancel(capture->cancellable);
  return G_SOURCE_CONTINUE;
}
static gboolean interrupted(gpointer data) {
  g_cancellable_cancel(((Capture *)data)->cancellable);
  return G_SOURCE_CONTINUE;
}
static void state_changed(FpImageDevice *device,FpiImageDeviceState state,gpointer data) {
  (void)device;
  Capture *capture=data;
  if (capture->clear_only && state==FPI_IMAGE_DEVICE_STATE_AWAIT_FINGER_ON && !capture->cancellation_queued) {
    capture->calibration_complete=TRUE;
    capture->cancellation_queued=TRUE;
    /* Let the state transition finish before cancelling the pending scan. */
    g_idle_add(cancel,capture);
  }
}
int main(int argc,char **argv) {
  if (argc!=2 || (strcmp(argv[1],"--capture") && strcmp(argv[1],"--clear-calibration"))) {
    fprintf(stderr,"Usage: %s --capture|--clear-calibration\n",argv[0]); return 2;
  }
  if (g_strcmp0(g_getenv("GOODIX521D_CAPTURE_PROFILE"),"pr81-full") ||
      g_strcmp0(g_getenv("GOODIX521D_SAVE_FRAMES"),"1")) {
    fprintf(stderr,"Requires the local pr81-full frame-dump profile\n"); return 2;
  }
  Capture capture={.cancellable=g_cancellable_new(),.clear_only=!strcmp(argv[1],"--clear-calibration")};
  guint interrupt_id=g_unix_signal_add(SIGINT,interrupted,&capture);
  guint term_id=g_unix_signal_add(SIGTERM,interrupted,&capture);
  guint timer_id=g_timeout_add_seconds(40,timeout,&capture);
  g_autoptr(FpContext) context=fp_context_new();
  fp_context_enumerate(context);
  GPtrArray *devices=fp_context_get_devices(context);
  FpDevice *selected=NULL;
  for (guint i=0;i<devices->len;i++) {
    FpDevice *candidate=g_ptr_array_index(devices,i);
    if (!g_strcmp0(fp_device_get_driver(candidate),"goodixtls52xd")) {
      if (selected) { fprintf(stderr,"Multiple matching readers: disconnect the other reader\n"); return 3; }
      selected=candidate;
    }
  }
  if (!selected) { fprintf(stderr,"No reader claimed by the local goodixtls52xd driver\n"); return 3; }
  if (!(fp_device_get_features(selected)&FP_DEVICE_FEATURE_CAPTURE)) { fprintf(stderr,"Driver does not expose capture\n"); return 3; }
  g_signal_connect(selected,"fpi-image-device-state-changed",G_CALLBACK(state_changed),&capture);
  g_autoptr(GError) error=NULL;
  if (!fp_device_open_sync(selected,capture.cancellable,&error)) {
    fprintf(stderr,"Open failed: %s\n",error->message); return 4;
  }
  g_autoptr(FpImage) image=fp_device_capture_sync(selected,TRUE,capture.cancellable,&error);
  int status=0;
  if (capture.timed_out) { fprintf(stderr,"Capture exceeded40 seconds\n"); status=4; }
  else if (capture.clear_only && capture.calibration_complete &&
           g_error_matches(error,G_IO_ERROR,G_IO_ERROR_CANCELLED)) {
    puts("CLEAR_CALIBRATION_SAVED");
  } else if (error && error->domain==FP_DEVICE_RETRY) {
    /* Capture mode still normalizes via NBIS in this libfprint version. Raw
     * file completeness is independently checked by the caller before use. */
    fprintf(stderr,"Raw capture completed; image-normalization retry: %s\n",error->message);
    status=10;
  } else if (error) { fprintf(stderr,"Capture failed: %s\n",error->message); status=4; }
  else if (!image && !capture.clear_only) { fprintf(stderr,"Capture returned no image\n"); status=4; }
  else if (capture.clear_only) { fprintf(stderr,"Clear-only capture did not reach calibration cancellation\n"); status=4; }
  else puts("RAW_BURST_SAVED");
  g_clear_error(&error);
  if (!fp_device_close_sync(selected,NULL,&error)) {
    fprintf(stderr,"Close failed: %s\n",error->message); status=4;
  }
  g_source_remove(timer_id); g_source_remove(interrupt_id); g_source_remove(term_id);
  g_object_unref(capture.cancellable);
  return status;
}
