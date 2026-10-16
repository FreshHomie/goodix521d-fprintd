/* SPDX-License-Identifier: MIT
 * Root capture host only: one USB/TLS owner, no vendor algorithm execution.
 * Commands arrive on inherited private pipes from the privileged controller.
 */
#define _GNU_SOURCE
#include "fprint.h"
#include "fpi-image-device.h"
#include <glib-unix.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct { GCancellable *cancel;gboolean clear,complete,timed_out,stop; } Session;
static gboolean interrupt_session(gpointer data) {
  Session *session=data;session->stop=TRUE;
  if(session->cancel)g_cancellable_cancel(session->cancel);
  return G_SOURCE_CONTINUE;
}
static gboolean timeout_session(gpointer data) {
  Session *session=data;session->timed_out=TRUE;g_cancellable_cancel(session->cancel);
  return G_SOURCE_REMOVE;
}
static gboolean cancel_calibration(gpointer data) {
  g_cancellable_cancel(((Session *)data)->cancel);return G_SOURCE_REMOVE;
}
static void state_changed(FpImageDevice *device,FpiImageDeviceState state,gpointer data) {
  (void)device;Session *session=data;
  if(session->clear && state==FPI_IMAGE_DEVICE_STATE_AWAIT_FINGER_ON && !session->complete) {
    session->complete=TRUE;g_idle_add(cancel_calibration,session);
  }
}
static gboolean private_directory(const char *path) {
  struct stat st;
  return path && path[0]=='/' && !lstat(path,&st) && S_ISDIR(st.st_mode) &&
         st.st_uid==geteuid() && !(st.st_mode&0077);
}
int main(void) {
  /* A daemon/controller crash must release the USB interface and TLS state. */
  pid_t parent=getppid();
  if(prctl(PR_SET_PDEATHSIG,SIGTERM) || getppid()!=parent)return 4;
  if(g_strcmp0(g_getenv("GOODIX521D_RETAIN_CAPTURE"),"1") ||
     g_strcmp0(g_getenv("GOODIX521D_CAPTURE_PROFILE"),"pr81-full") ||
     g_strcmp0(g_getenv("GOODIX521D_SAVE_FRAMES"),"1"))return 2;
  Session session={0};
  guint term=g_unix_signal_add(SIGTERM,interrupt_session,&session);
  guint intr=g_unix_signal_add(SIGINT,interrupt_session,&session);
  g_autoptr(FpContext) context=fp_context_new();fp_context_enumerate(context);
  GPtrArray *devices=fp_context_get_devices(context);FpDevice *device=NULL;
  for(guint i=0;i<devices->len;i++) {
    FpDevice *candidate=g_ptr_array_index(devices,i);
    if(!g_strcmp0(fp_device_get_driver(candidate),"goodixtls52xd")) {
      if(device)return 3;
      device=candidate;
    }
  }
  if(!device)return 3;
  g_signal_connect(device,"fpi-image-device-state-changed",G_CALLBACK(state_changed),&session);
  g_autoptr(GError) error=NULL;
  if(!fp_device_open_sync(device,NULL,&error)) {
    fprintf(stderr,"Session open failed: %s\n",error->message);return 4;
  }
  puts("SESSION_OPENED");fflush(stdout);
  char *line=NULL;size_t capacity=0;
  while(!session.stop && getline(&line,&capacity,stdin)>0) {
    size_t length=strlen(line);
    if(length>4096 || !length || line[length-1]!='\n')break;
    line[length-1]=0;
    if(!strcmp(line,"QUIT"))break;
    char *folder=NULL;gboolean clear=FALSE;gboolean enroll=FALSE;
    if(g_str_has_prefix(line,"CALIBRATE ")) { clear=TRUE;folder=line+10; }
    else if(g_str_has_prefix(line,"ENROLL ")) { enroll=TRUE;folder=line+7; }
    else if(g_str_has_prefix(line,"VERIFY "))folder=line+7;
    else break;
    if(!private_directory(folder) || chdir(folder))break;
    g_setenv("GOODIX521D_CONTACT_CAPTURE",enroll || clear?"0":"1",TRUE);
    g_setenv("GOODIX521D_CALIBRATION_ONLY",clear?"1":"0",TRUE);
    g_setenv("GOODIX521D_RETAIN_FORCE_FRESH",clear?"1":"0",TRUE);
    session.cancel=g_cancellable_new();session.clear=clear;session.complete=FALSE;session.timed_out=FALSE;
    guint timer=g_timeout_add_seconds(40,timeout_session,&session);
    g_autoptr(FpImage) image=fp_device_capture_sync(device,TRUE,session.cancel,&error);
    int status=0;
    if(session.timed_out || session.stop)status=4;
    else if(clear && session.complete && g_error_matches(error,G_IO_ERROR,G_IO_ERROR_CANCELLED))
      puts("CLEAR_CALIBRATION_SAVED");
    else if(error && error->domain==FP_DEVICE_RETRY)status=10;
    else if(error || (!image && !clear) || clear)status=4;
    if(error && status==4)fprintf(stderr,"Session capture failed: %s\n",error->message);
    if(!session.timed_out)g_source_remove(timer);
    g_clear_error(&error);g_clear_object(&session.cancel);
    printf("SESSION_DONE %d\n",status);fflush(stdout);
  }
  free(line);g_clear_error(&error);
  if(!fp_device_close_sync(device,NULL,&error)) {
    fprintf(stderr,"Session close failed: %s\n",error->message);return 4;
  }
  g_source_remove(term);g_source_remove(intr);
  puts("SESSION_CLOSED");fflush(stdout);return 0;
}
