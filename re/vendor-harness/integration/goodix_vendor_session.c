/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Goodix521d: resident privileged capture controller, isolated vendor worker,
 * and a terminal learned-template result for the paired fprintd overlay.
 */
#include "drivers_api.h"
#include <signal.h>
#include <string.h>
#define ENGINE_HASH "5995d6565e602416cbe00f171ad4941203e345ccaddb490199274166adac4f5c"
#define MAX_TEMPLATE (384 * 1024)
#define MAX_GALLERY 10
#ifndef GOODIX_BRIDGE
#error Build with the absolute path to the local bridge
#endif
typedef enum { JOB_OPEN,JOB_ENROLL,JOB_VERIFY,JOB_IDENTIFY,JOB_CLOSE,JOB_SUSPEND,JOB_RESUME } JobKind;
typedef struct {
  FpDevice parent;GSubprocess *child;guint completed,serial;
  gboolean active,clear_retry_pending,suspend_pending;
  JobKind active_kind;
} FpiDeviceGoodixVendor;
typedef FpDeviceClass FpiDeviceGoodixVendorClass;
GType fpi_device_goodix_vendor_get_type(void);
G_DEFINE_TYPE(FpiDeviceGoodixVendor,fpi_device_goodix_vendor,FP_TYPE_DEVICE)
static const FpIdEntry ids[]={ {.vid=0x27c6,.pid=0x521d},{.vid=0,.pid=0} };
typedef struct { FpDevice *device;gchar *line;guint serial; } Update;
typedef struct { JobKind kind;gchar *command;GSubprocess *child;guint serial; } Job;

static void job_free(gpointer data) {
  Job *job=data;g_free(job->command);g_object_unref(job->child);g_free(job);
}
static gboolean update_main(gpointer data) {
  Update *update=data;FpDevice *dev=update->device;
  FpiDeviceGoodixVendor *self=(FpiDeviceGoodixVendor *)dev;
  FpiDeviceAction action=fpi_device_get_current_action(dev);
  if(update->serial==self->serial && action!=FPI_DEVICE_ACTION_NONE) {
    if(g_str_equal(update->line,"STATE PRESS"))fpi_device_report_finger_status(dev,FP_FINGER_STATUS_NEEDED);
    else if(g_str_equal(update->line,"STATE REMOVE")) {
      fpi_device_report_finger_status(dev,FP_FINGER_STATUS_PRESENT);
      if(action==FPI_DEVICE_ACTION_ENROLL)
        fpi_device_enroll_progress(dev,self->completed,NULL,fpi_device_retry_new(FP_DEVICE_RETRY_REMOVE_FINGER));
    } else if(g_str_has_prefix(update->line,"STATE "))fpi_device_report_finger_status(dev,FP_FINGER_STATUS_NONE);
    else if(action==FPI_DEVICE_ACTION_ENROLL) {
      if(g_str_equal(update->line,"RETRY"))fpi_device_enroll_progress(dev,self->completed,NULL,fpi_device_retry_new(FP_DEVICE_RETRY_GENERAL));
      else if(g_str_equal(update->line,"RETRY REMOVE"))fpi_device_enroll_progress(dev,self->completed,NULL,fpi_device_retry_new(FP_DEVICE_RETRY_REMOVE_FINGER));
      else if(g_str_equal(update->line,"RETRY CENTER"))fpi_device_enroll_progress(dev,self->completed,NULL,fpi_device_retry_new(FP_DEVICE_RETRY_CENTER_FINGER));
      else if(g_str_has_prefix(update->line,"PROGRESS ")) {
        guint number=(guint)g_ascii_strtoull(update->line+9,NULL,10);
        if(number==self->completed+1 && number<=18) {
          self->completed=number;fpi_device_enroll_progress(dev,number,NULL,NULL);
        }
      }
    }
  }
  g_object_unref(dev);g_free(update->line);g_free(update);return G_SOURCE_REMOVE;
}
static gboolean terminal_for(JobKind kind,const gchar *line) {
  if(kind==JOB_ENROLL)return g_str_has_prefix(line,"TEMPLATE ");
  if(kind==JOB_VERIFY)return g_str_has_prefix(line,"MATCH ");
  if(kind==JOB_IDENTIFY)return g_str_has_prefix(line,"IDENTIFY ");
  if(kind==JOB_CLOSE)return g_str_equal(line,"CLOSED");
  if(kind==JOB_SUSPEND)return g_str_equal(line,"SUSPENDED");
  return g_str_equal(line,"READY");
}
static void worker(GTask *task,gpointer source,gpointer data,GCancellable *cancel) {
  Job *job=data;(void)cancel;
  g_autoptr(GError) error=NULL;
  g_autoptr(GDataInputStream) stream=g_data_input_stream_new(g_subprocess_get_stdout_pipe(job->child));
  g_filter_input_stream_set_close_base_stream(G_FILTER_INPUT_STREAM(stream),FALSE);
  gchar *terminal=NULL;
  if(!g_output_stream_write_all(g_subprocess_get_stdin_pipe(job->child),job->command,
      strlen(job->command),NULL,NULL,&error))goto failed;
  for(;;) {
    gsize length=0;gchar *line=g_data_input_stream_read_line(stream,&length,NULL,&error);
    if(!line)break;
    if(length>MAX_TEMPLATE*2+16) {g_free(line);g_set_error_literal(&error,G_IO_ERROR,G_IO_ERROR_INVALID_DATA,"Oversized controller reply");break;}
    if(terminal_for(job->kind,line)) {terminal=line;break;}
    if(g_str_has_prefix(line,"STATE ") || g_str_has_prefix(line,"PROGRESS ") ||
       g_str_equal(line,"RETRY") || g_str_equal(line,"RETRY REMOVE") || g_str_equal(line,"RETRY CENTER")) {
      Update *update=g_new0(Update,1);update->device=g_object_ref(source);
      update->line=line;update->serial=job->serial;g_main_context_invoke(NULL,update_main,update);
    } else {g_free(line);g_set_error_literal(&error,G_IO_ERROR,G_IO_ERROR_INVALID_DATA,"Invalid controller reply");break;}
  }
  if(error)goto failed;
  if(!terminal) {g_set_error_literal(&error,G_IO_ERROR,G_IO_ERROR_FAILED,"Controller disconnected");goto failed;}
  if(job->kind==JOB_CLOSE && !g_subprocess_wait_check(job->child,NULL,&error))goto failed;
  g_task_return_pointer(task,terminal,g_free);return;
failed:
  if(g_subprocess_get_identifier(job->child))g_subprocess_send_signal(job->child,SIGTERM);
  g_subprocess_wait(job->child,NULL,NULL);
  g_debug("Goodix controller job %u failed: %s (exit status %d)",job->kind,error->message,
      g_subprocess_get_status(job->child));
  g_free(terminal);g_task_return_error(task,g_steal_pointer(&error));
}
static GVariant *payload_from_hex(const gchar *hex,GError **error) {
  gsize length=strlen(hex),size=length/2;
  g_autofree guint8 *bytes=NULL;
  if(length%2 || size<15 || size>MAX_TEMPLATE)goto invalid;
  bytes=g_malloc(size);
  for(gsize i=0;i<size;i++) {
    gint first=g_ascii_xdigit_value(hex[2*i]),second=g_ascii_xdigit_value(hex[2*i+1]);
    if(first<0 || second<0)goto invalid;
    bytes[i]=(guint8)((first<<4)|second);
  }
  if(bytes[0]!=0x87 || bytes[5]!=0x86 || bytes[10]!=0x81)goto invalid;
  guint32 declared;memcpy(&declared,bytes+6,4);
  if(GUINT32_FROM_LE(declared)!=size-10)goto invalid;
  return g_variant_new("(su@ay)",ENGINE_HASH,1,
      g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,bytes,size,1));
invalid:
  *error=fpi_device_error_new(FP_DEVICE_ERROR_DATA_INVALID);return NULL;
}
static gboolean suspended_after_action(gpointer data) {
  fpi_device_suspend_complete(FP_DEVICE(data),NULL);g_object_unref(data);return G_SOURCE_REMOVE;
}
static void completed(GObject *object,GAsyncResult *result,gpointer unused) {
  (void)unused;FpDevice *dev=FP_DEVICE(object);FpiDeviceGoodixVendor *self=(FpiDeviceGoodixVendor *)dev;
  Job *job=g_task_get_task_data(G_TASK(result));g_autoptr(GError) error=NULL;
  g_autofree gchar *terminal=g_task_propagate_pointer(G_TASK(result),&error);
  self->active=FALSE;
  GCancellable *cancel=fpi_device_get_current_action(dev)!=FPI_DEVICE_ACTION_NONE?
      fpi_device_get_cancellable(dev):NULL;
  if(self->suspend_pending || (cancel && g_cancellable_is_cancelled(cancel))) {
    g_clear_error(&error);error=g_error_new_literal(G_IO_ERROR,G_IO_ERROR_CANCELLED,"Operation cancelled");
  }
  if(error) {
    if(g_subprocess_get_identifier(job->child))g_subprocess_send_signal(job->child,SIGTERM);
    g_subprocess_wait(job->child,NULL,NULL);
  }
  if(error || job->kind==JOB_CLOSE)g_clear_object(&self->child);
  fpi_device_report_finger_status(dev,FP_FINGER_STATUS_NONE);
  if(job->kind==JOB_OPEN)fpi_device_open_complete(dev,g_steal_pointer(&error));
  else if(job->kind==JOB_CLOSE)fpi_device_close_complete(dev,g_steal_pointer(&error));
  else if(job->kind==JOB_SUSPEND)fpi_device_suspend_complete(dev,g_steal_pointer(&error));
  else if(job->kind==JOB_RESUME)fpi_device_resume_complete(dev,g_steal_pointer(&error));
  else if(job->kind==JOB_ENROLL) {
    FpPrint *print=NULL;GVariant *payload=NULL;
    if(!error && terminal && g_str_has_prefix(terminal,"TEMPLATE ") && self->completed==18)
      payload=payload_from_hex(terminal+9,&error);
    else if(!error)error=fpi_device_error_new(FP_DEVICE_ERROR_PROTO);
    if(!error) {
      fpi_device_get_enroll_data(dev,&print);fpi_print_set_type(print,FPI_PRINT_RAW);
      fpi_print_set_device_stored(print,FALSE);fpi_print_set_raw_data(print,payload);
    }
    fpi_device_enroll_complete(dev,print?g_object_ref(print):NULL,g_steal_pointer(&error));
  } else if(job->kind==JOB_IDENTIFY) {
    gboolean updated=FALSE;self->clear_retry_pending=FALSE;
    if(!error) {
      const gchar *reply=terminal+9;
      if(g_str_equal(reply,"NONE"))fpi_device_identify_report(dev,NULL,NULL,NULL);
      else if(g_str_equal(reply,"RETRY"))
        fpi_device_identify_report(dev,NULL,NULL,fpi_device_retry_new(FP_DEVICE_RETRY_GENERAL));
      else if(g_str_equal(reply,"REMOVE")) {
        self->clear_retry_pending=TRUE;
        fpi_device_identify_report(dev,NULL,NULL,fpi_device_retry_new(FP_DEVICE_RETRY_REMOVE_FINGER));
      } else {
        GPtrArray *gallery=NULL;gchar *end=NULL;
        guint64 index=g_ascii_strtoull(reply,&end,10);
        fpi_device_get_identify_data(dev,&gallery);
        if(!g_ascii_isdigit(*reply) || (*end && *end!=' ') || index>=gallery->len)
          error=fpi_device_error_new(FP_DEVICE_ERROR_PROTO);
        else {
          FpPrint *print=g_ptr_array_index(gallery,index);
          if(*end) {
            GVariant *payload=payload_from_hex(end+1,&error);
            if(!error) {fpi_print_set_raw_data(print,payload);updated=TRUE;}
          }
          if(!error) {
            g_message("Goodix identification: matched gallery template %" G_GUINT64_FORMAT "/%u, finger %u",index+1,gallery->len,fp_print_get_finger(print));
            fpi_device_identify_report(dev,print,NULL,NULL);
          }
        }
      }
    }
    fpi_device_identify_complete_with_update(dev,updated,g_steal_pointer(&error));
  } else {
    gboolean updated=FALSE;self->clear_retry_pending=FALSE;
    if(!error) {
      if(g_str_equal(terminal,"MATCH 1") || g_str_has_prefix(terminal,"MATCH 1 ")) {
        if(g_str_has_prefix(terminal,"MATCH 1 ")) {
          GVariant *payload=payload_from_hex(terminal+8,&error);
          if(!error) {
            FpPrint *print=NULL;fpi_device_get_verify_data(dev,&print);
            fpi_print_set_raw_data(print,payload);updated=TRUE;
          }
        }
        if(!error)fpi_device_verify_report(dev,FPI_MATCH_SUCCESS,NULL,NULL);
      } else if(g_str_equal(terminal,"MATCH 0"))fpi_device_verify_report(dev,FPI_MATCH_FAIL,NULL,NULL);
      else if(g_str_equal(terminal,"MATCH RETRY"))fpi_device_verify_report(dev,FPI_MATCH_ERROR,NULL,fpi_device_retry_new(FP_DEVICE_RETRY_GENERAL));
      else if(g_str_equal(terminal,"MATCH REMOVE")) {
        self->clear_retry_pending=TRUE;
        fpi_device_verify_report(dev,FPI_MATCH_ERROR,NULL,fpi_device_retry_new(FP_DEVICE_RETRY_REMOVE_FINGER));
      } else error=fpi_device_error_new(FP_DEVICE_ERROR_PROTO);
    }
    fpi_device_verify_complete_with_update(dev,updated,g_steal_pointer(&error));
  }
  if(self->suspend_pending) {
    self->suspend_pending=FALSE;
    g_idle_add(suspended_after_action,g_object_ref(dev));
  }
}
static void start(FpDevice *dev,JobKind kind,gchar *input) {
  FpiDeviceGoodixVendor *self=(FpiDeviceGoodixVendor *)dev;g_autoptr(GError) error=NULL;
  if(!self->child) {
    g_autoptr(GSubprocessLauncher) launcher=g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDIN_PIPE|G_SUBPROCESS_FLAGS_STDOUT_PIPE);
    g_subprocess_launcher_unsetenv(launcher,"LD_LIBRARY_PATH");
    self->child=g_subprocess_launcher_spawn(launcher,&error,"/usr/bin/python3",GOODIX_BRIDGE,"--session",NULL);
  }
  if(!self->child) {
    g_free(input);
    if(kind==JOB_SUSPEND)fpi_device_suspend_complete(dev,g_steal_pointer(&error));
    else if(kind==JOB_RESUME)fpi_device_resume_complete(dev,g_steal_pointer(&error));
    else fpi_device_action_error(dev,g_steal_pointer(&error));
    return;
  }
  const gchar *name=kind==JOB_OPEN?"OPEN":kind==JOB_CLOSE?"CLOSE":kind==JOB_SUSPEND?"SUSPEND":
      kind==JOB_RESUME?"RESUME":kind==JOB_ENROLL?"ENROLL":kind==JOB_IDENTIFY?
      (self->clear_retry_pending?"IDENTIFY-RETRY":"IDENTIFY"):(self->clear_retry_pending?"VERIFY-RETRY":"VERIFY");
  Job *job=g_new0(Job,1);job->kind=kind;job->child=g_object_ref(self->child);job->serial=++self->serial;
  job->command=input?g_strdup_printf("%s %s\n",name,input):g_strdup_printf("%s\n",name);
  g_free(input);self->active=TRUE;self->active_kind=kind;self->completed=0;
  g_debug("Goodix session command %s, controller %s",name,g_subprocess_get_identifier(self->child));
  GTask *task=g_task_new(dev,NULL,completed,NULL);g_task_set_task_data(task,job,job_free);
  g_task_run_in_thread(task,worker);g_object_unref(task);
}
static void enroll(FpDevice *dev) {start(dev,JOB_ENROLL,NULL);}
static const gchar *finger_name(FpPrint *print) {
  static const gchar *finger_names[]={"unknown","left-thumb","left-index-finger","left-middle-finger","left-ring-finger","left-little-finger","right-thumb","right-index-finger","right-middle-finger","right-ring-finger","right-little-finger"};
  FpFinger finger=fp_print_get_finger(print);
  return FP_FINGER_IS_VALID(finger)?finger_names[finger]:"unknown";
}
static gchar *print_hex(FpPrint *print,GError **error) {
  g_autoptr(GVariant) data=NULL;g_object_get(print,"fpi-data",&data,NULL);
  if(!data || !g_variant_is_of_type(data,G_VARIANT_TYPE("(suay)"))) {
    *error=fpi_device_error_new(FP_DEVICE_ERROR_DATA_INVALID);return NULL;
  }
  const gchar *engine;guint version;g_autoptr(GVariant) payload=NULL;
  g_variant_get(data,"(&su@ay)",&engine,&version,&payload);
  gsize size=0;const guint8 *bytes=g_variant_get_fixed_array(payload,&size,1);
  if(version!=1 || strcmp(engine,ENGINE_HASH) || size<15 || size>MAX_TEMPLATE) {
    *error=fpi_device_error_new(FP_DEVICE_ERROR_DATA_INVALID);return NULL;
  }
  gchar *hex=g_malloc(size*2+1);
  for(gsize i=0;i<size;i++) {hex[2*i]="0123456789abcdef"[bytes[i]>>4];hex[2*i+1]="0123456789abcdef"[bytes[i]&15];}
  hex[size*2]=0;return hex;
}
static void verify(FpDevice *dev) {
  FpPrint *print=NULL;g_autoptr(GError) error=NULL;
  fpi_device_get_verify_data(dev,&print);
  g_message("Goodix verification: selected %s",finger_name(print));
  gchar *hex=print_hex(print,&error);
  if(error)fpi_device_verify_complete(dev,g_steal_pointer(&error));
  else start(dev,JOB_VERIFY,hex);
}
static void identify(FpDevice *dev) {
  GPtrArray *gallery=NULL;g_autoptr(GError) error=NULL;
  fpi_device_get_identify_data(dev,&gallery);
  if(!gallery->len) {
    fpi_device_identify_report(dev,NULL,NULL,NULL);fpi_device_identify_complete(dev,NULL);return;
  }
  if(gallery->len>MAX_GALLERY) {
    fpi_device_identify_complete(dev,fpi_device_error_new(FP_DEVICE_ERROR_DATA_INVALID));return;
  }
  GString *input=g_string_new(NULL);g_string_append_printf(input,"%u",gallery->len);
  g_message("Goodix identification: checking %u enrolled templates",gallery->len);
  for(guint i=0;i<gallery->len;i++) {
    FpPrint *print=g_ptr_array_index(gallery,i);g_autofree gchar *hex=print_hex(print,&error);
    if(error) {
      g_string_free(input,TRUE);fpi_device_identify_complete(dev,g_steal_pointer(&error));return;
    }
    g_message("Goodix identification: gallery template %u/%u is %s",i+1,gallery->len,finger_name(print));
    g_string_append_printf(input,"\nTEMPLATE %s",hex);
  }
  start(dev,JOB_IDENTIFY,g_string_free(input,FALSE));
}
static void cancel(FpDevice *dev) {
  FpiDeviceGoodixVendor *self=(FpiDeviceGoodixVendor *)dev;
  /* A late cancellation of the previous action must not kill a new idle
   * resume/open command or the resident session after that action finished. */
  if(self->active && fpi_device_get_current_action(dev)!=FPI_DEVICE_ACTION_NONE &&
     self->child && self->active_kind!=JOB_RESUME && self->active_kind!=JOB_SUSPEND) {
    g_debug("Goodix session cancellation, controller %s",g_subprocess_get_identifier(self->child));
    g_subprocess_send_signal(self->child,SIGTERM);
  }
}
static void open_device(FpDevice *dev) {start(dev,JOB_OPEN,NULL);}
static void close_device(FpDevice *dev) {
  if(!((FpiDeviceGoodixVendor *)dev)->child)fpi_device_close_complete(dev,NULL);
  else start(dev,JOB_CLOSE,NULL);
}
static void suspend_device(FpDevice *dev) {
  FpiDeviceGoodixVendor *self=(FpiDeviceGoodixVendor *)dev;
  if(self->active) {self->suspend_pending=TRUE;cancel(dev);}
  else if(self->child)start(dev,JOB_SUSPEND,NULL);
  else fpi_device_suspend_complete(dev,NULL);
}
static void resume_device(FpDevice *dev) {start(dev,JOB_RESUME,NULL);}
static void fpi_device_goodix_vendor_init(FpiDeviceGoodixVendor *self) {(void)self;}
static void fpi_device_goodix_vendor_class_init(FpiDeviceGoodixVendorClass *klass) {
  FpDeviceClass *dev=FP_DEVICE_CLASS(klass);dev->id="goodix_vendor";
  dev->full_name="Goodix 521d with isolated vendor engine (local test)";
  dev->type=FP_DEVICE_TYPE_USB;dev->id_table=ids;dev->nr_enroll_stages=18;
  dev->scan_type=FP_SCAN_TYPE_PRESS;dev->temp_hot_seconds=-1;
  dev->open=open_device;dev->close=close_device;dev->enroll=enroll;dev->verify=verify;dev->identify=identify;dev->cancel=cancel;
  dev->suspend=suspend_device;dev->resume=resume_device;
  fpi_device_class_auto_initialize_features(dev);
}
