/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Local Goodix 521d adapter. Proprietary code only runs in the external helper.
 */
#include "drivers_api.h"
#include <signal.h>
#include <string.h>
#define ENGINE_HASH "5995d6565e602416cbe00f171ad4941203e345ccaddb490199274166adac4f5c"
#define MAX_TEMPLATE (128 * 1024)
#define GOODIX_BRIDGE "/usr/share/goodix521d/development/re/vendor-harness/integration/bridge.py"
#ifndef GOODIX_BRIDGE
#error Build with the absolute path to the local bridge
#endif

typedef struct { FpDevice parent; GSubprocess *child; guint completed; } FpiDeviceGoodixVendorReplay;
typedef FpDeviceClass FpiDeviceGoodixVendorReplayClass;
GType fpi_device_goodix_vendor_replay_get_type (void);
G_DEFINE_TYPE (FpiDeviceGoodixVendorReplay, fpi_device_goodix_vendor_replay, FP_TYPE_DEVICE)
static const FpIdEntry ids[] = { { .virtual_envvar="GOODIX_VENDOR_REPLAY" }, { .vid=0, .pid=0 } };

typedef struct { FpDevice *device; gchar *line; } Update;
static gboolean update_main (gpointer data)
{
  Update *update = data;
  FpDevice *dev = update->device;
  FpiDeviceGoodixVendorReplay *self = (FpiDeviceGoodixVendorReplay *) dev;
  if (fpi_device_get_current_action (dev) != FPI_DEVICE_ACTION_NONE)
    {
      if (g_str_equal (update->line, "STATE PRESS"))
        fpi_device_report_finger_status (dev, FP_FINGER_STATUS_NEEDED);
      else if (g_str_has_prefix (update->line, "STATE "))
        fpi_device_report_finger_status (dev, FP_FINGER_STATUS_NONE);
      else if (g_str_equal (update->line, "RETRY"))
        fpi_device_enroll_progress (dev, self->completed, NULL, fpi_device_retry_new (FP_DEVICE_RETRY_GENERAL));
      else if (g_str_has_prefix (update->line, "PROGRESS "))
        {
          guint n = (guint) g_ascii_strtoull (update->line+9, NULL, 10);
          if (n == self->completed+1 && n <= 18)
            {
              self->completed = n;
              fpi_device_enroll_progress (dev, n, NULL, NULL);
            }
        }
    }
  g_object_unref (dev); g_free (update->line); g_free (update);
  return G_SOURCE_REMOVE;
}
static void worker (GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
  FpiDeviceGoodixVendorReplay *self = source;
  g_autoptr(GError) error = NULL;
  g_autoptr(GDataInputStream) stream = g_data_input_stream_new (g_subprocess_get_stdout_pipe (self->child));
  gchar *terminal = NULL;
  const gchar *input = task_data;
  (void) cancellable;
  if (input && !g_output_stream_write_all (g_subprocess_get_stdin_pipe (self->child), input, strlen(input), NULL, NULL, &error))
    goto fail;
  if (!g_output_stream_close (g_subprocess_get_stdin_pipe (self->child), NULL, &error))
    goto fail;
  for (;;)
    {
      gsize length = 0;
      gchar *line = g_data_input_stream_read_line (stream, &length, NULL, &error);
      if (!line) break;
      if (length > MAX_TEMPLATE*2+16 || terminal)
        { g_free (line); g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Invalid helper protocol"); break; }
      if (g_str_has_prefix (line, "TEMPLATE ") || g_str_has_prefix (line, "MATCH "))
        terminal = line;
      else if (g_str_has_prefix (line, "STATE ") || g_str_has_prefix (line, "PROGRESS ") || g_str_equal (line, "RETRY"))
        {
          Update *update = g_new0 (Update,1);
          update->device = g_object_ref (source); update->line = line;
          g_main_context_invoke (NULL, update_main, update);
        }
      else
        { g_free (line); g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "Unknown helper response"); break; }
    }
  if (error) goto fail;
  if (!g_subprocess_wait_check (self->child, NULL, &error)) goto fail;
  if (!terminal) { g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_FAILED, "Helper returned no result"); goto fail; }
  g_task_return_pointer (task, terminal, g_free);
  return;
fail:
  g_subprocess_send_signal (self->child, SIGTERM);
  g_subprocess_wait (self->child, NULL, NULL);
  g_free (terminal);
  g_task_return_error (task, g_steal_pointer (&error));
}
static void completed (GObject *object, GAsyncResult *result, gpointer unused)
{
  FpDevice *dev = FP_DEVICE (object);
  FpiDeviceGoodixVendorReplay *self = (FpiDeviceGoodixVendorReplay *) dev;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *terminal = g_task_propagate_pointer (G_TASK (result), &error);
  (void) unused;
  g_clear_object (&self->child);
  fpi_device_report_finger_status (dev, FP_FINGER_STATUS_NONE);
  if (g_cancellable_is_cancelled (fpi_device_get_cancellable (dev)))
    { g_clear_error (&error); error = g_error_new_literal (G_IO_ERROR,G_IO_ERROR_CANCELLED,"Operation cancelled"); }
  if (fpi_device_get_current_action (dev) == FPI_DEVICE_ACTION_ENROLL)
    {
      FpPrint *print = NULL;
      g_autofree guint8 *bytes = NULL;
      gsize n = 0;
      if (!error && terminal && g_str_has_prefix (terminal,"TEMPLATE "))
        {
          const gchar *hex = terminal+9;
          n = strlen(hex)/2;
          if (strlen(hex)%2 || n < 15 || n > MAX_TEMPLATE || self->completed != 18)
            error = fpi_device_error_new (FP_DEVICE_ERROR_DATA_INVALID);
          else
            {
              bytes = g_malloc(n);
              for (gsize i=0;i<n;i++)
                {
                  gint a=g_ascii_xdigit_value(hex[2*i]), b=g_ascii_xdigit_value(hex[2*i+1]);
                  if (a<0 || b<0) { error=fpi_device_error_new(FP_DEVICE_ERROR_DATA_INVALID); break; }
                  bytes[i]=(guint8)((a<<4)|b);
                }
            }
        }
      else if (!error) error=fpi_device_error_new(FP_DEVICE_ERROR_PROTO);
      if (!error)
        {
          fpi_device_get_enroll_data(dev,&print);
          fpi_print_set_type(print,FPI_PRINT_RAW);
          fpi_print_set_device_stored(print,FALSE);
          GVariant *payload=g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,bytes,n,1);
          g_object_set(print,"fpi-data",g_variant_new("(su@ay)",ENGINE_HASH,1,payload),NULL);
        }
      fpi_device_enroll_complete(dev,print?g_object_ref(print):NULL,g_steal_pointer(&error));
    }
  else
    {
      if (!error)
        {
          if (g_str_equal(terminal,"MATCH 1") || g_str_equal(terminal,"MATCH 0"))
            fpi_device_verify_report(dev,g_str_equal(terminal,"MATCH 1")?FPI_MATCH_SUCCESS:FPI_MATCH_FAIL,NULL,NULL);
          else if (g_str_equal(terminal,"MATCH RETRY"))
            fpi_device_verify_report(dev,FPI_MATCH_ERROR,NULL,fpi_device_retry_new(FP_DEVICE_RETRY_GENERAL));
          else error=fpi_device_error_new(FP_DEVICE_ERROR_PROTO);
        }
      fpi_device_verify_complete(dev,g_steal_pointer(&error));
    }
}
static void start (FpDevice *dev, const gchar *operation, gchar *input)
{
  FpiDeviceGoodixVendorReplay *self = (FpiDeviceGoodixVendorReplay *) dev;
  g_autoptr(GError) error=NULL;
  g_autoptr(GSubprocessLauncher) launcher=g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDIN_PIPE|G_SUBPROCESS_FLAGS_STDOUT_PIPE);
  /* Capture helper is linked to the original build; do not recurse into this driver. */
  g_subprocess_launcher_unsetenv(launcher,"LD_LIBRARY_PATH");
  self->completed=0;
  self->child=g_subprocess_launcher_spawn(launcher,&error,"/usr/bin/python3",GOODIX_BRIDGE,operation,"--replay",g_getenv("GOODIX_VENDOR_REPLAY"),NULL);
  if (!self->child) { g_free(input); fpi_device_action_error(dev,g_steal_pointer(&error)); return; }
  GTask *task=g_task_new(dev,NULL,completed,NULL);
  g_task_set_task_data(task,input,g_free);
  g_task_run_in_thread(task,worker); g_object_unref(task);
}
static void enroll (FpDevice *dev) { start(dev,"enroll",NULL); }
static void verify (FpDevice *dev)
{
  FpPrint *print=NULL;
  g_autoptr(GVariant) data=NULL;
  fpi_device_get_verify_data(dev,&print); g_object_get(print,"fpi-data",&data,NULL);
  if (!data || !g_variant_is_of_type(data,G_VARIANT_TYPE("(suay)")))
    { fpi_device_verify_complete(dev,fpi_device_error_new(FP_DEVICE_ERROR_DATA_INVALID)); return; }
  const gchar *engine; guint version; g_autoptr(GVariant) payload=NULL;
  g_variant_get(data,"(&su@ay)",&engine,&version,&payload);
  gsize n=0; const guint8 *bytes=g_variant_get_fixed_array(payload,&n,1);
  if (version!=1 || strcmp(engine,ENGINE_HASH) || n<15 || n>MAX_TEMPLATE)
    { fpi_device_verify_complete(dev,fpi_device_error_new(FP_DEVICE_ERROR_DATA_INVALID)); return; }
  gchar *hex=g_malloc(n*2+1);
  for (gsize i=0;i<n;i++) { hex[2*i]="0123456789abcdef"[bytes[i]>>4]; hex[2*i+1]="0123456789abcdef"[bytes[i]&15]; }
  hex[n*2]=0; start(dev,"verify",hex);
}
static void cancel (FpDevice *dev)
{
  FpiDeviceGoodixVendorReplay *self=(FpiDeviceGoodixVendorReplay *)dev;
  if (self->child) g_subprocess_send_signal(self->child,SIGTERM);
}
static void open_device (FpDevice *dev) { fpi_device_open_complete(dev,NULL); }
static void close_device (FpDevice *dev) { fpi_device_close_complete(dev,NULL); }
static void fpi_device_goodix_vendor_replay_init (FpiDeviceGoodixVendorReplay *self) { (void)self; }
static void fpi_device_goodix_vendor_replay_class_init (FpiDeviceGoodixVendorReplayClass *klass)
{
  FpDeviceClass *dev=FP_DEVICE_CLASS(klass);
  dev->id="goodix_vendor_replay"; dev->full_name="Goodix 521d with isolated vendor engine (local test)";
  dev->type=FP_DEVICE_TYPE_VIRTUAL; dev->id_table=ids;
  dev->nr_enroll_stages=18; dev->scan_type=FP_SCAN_TYPE_PRESS; dev->temp_hot_seconds=-1;
  dev->open=open_device; dev->close=close_device; dev->enroll=enroll; dev->verify=verify; dev->cancel=cancel;
  fpi_device_class_auto_initialize_features(dev);
}
