// Goodix Tls driver for libfprint

// Copyright (C) 2021 Alexander Meiler <alex.meiler@protonmail.com>
// Copyright (C) 2021 Matthieu CHARETTE <matthieu.charette@gmail.com>
// Copyright (C) 2021 Michael Teuscher <michael.teuscher@pm.me>

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#include "fp-device.h"
#include "fp-image-device.h"
#include "fp-image.h"
#include "fpi-assembling.h"
#include "fpi-context.h"
#include "fpi-image-device.h"
#include "fpi-image.h"
#include "fpi-ssm.h"
#include "glibconfig.h"
#include "gusb/gusb-device.h"
#include <stdio.h>
#include <stdlib.h>
#define FP_COMPONENT "goodixtls52xd"

#include <glib.h>
#include <string.h>

#include "drivers_api.h"
#include "goodix.h"
#include "goodix_proto.h"
#include "goodix52xd.h"
#include "goodix52xd-pr81.h"
#include "goodix52xd-pr81-full.h"
#include "goodix52xd-clear-guard.h"
#include "cache-experiment.h"
#include "held-contact.h"
#include "retained-session.h"

#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define GOODIX52XD_WIDTH 64
#define GOODIX52XD_HEIGHT 80
#define GOODIX52XD_SCAN_WIDTH 64
#define GOODIX52XD_FRAME_SIZE (GOODIX52XD_WIDTH * GOODIX52XD_HEIGHT)
// For every 4 pixels there are 6 bytes and there are 8 extra start bytes and 5
// extra end
#define GOODIX52XD_RAW_FRAME_SIZE                                               \
    (GOODIX52XD_HEIGHT * GOODIX52XD_SCAN_WIDTH) / 4 * 6
#define GOODIX52XD_CAP_FRAMES 10 // Number of frames we capture per swipe

typedef unsigned short Goodix52xdPix;

struct _FpiDeviceGoodixTls52XD {
  FpiDeviceGoodixTls parent;

  guint8* otp;

  GSList* frames;

  gboolean timed_capture;
  FpiSsm* timed_wait_ssm;
  gboolean pr81_capture;
  gboolean pr81_full_capture;
  gboolean contact_capture;
  gboolean save_frames;
  guint diagnostic_burst;
  guint16 contact_baseline[8];
  gint64 contact_deadline;
  gboolean contact_probe;
  guint contact_probe_sample;
  gboolean identity_probe;
  gboolean clear_guard;
  gboolean clear_guard_invalid;
  guint16 clear_reference[8];
  guint16 clear_previous[8];
  guint clear_streak;
  gboolean clear_wait_announced;
  gint64 clear_deadline;
  gchar *clear_otp_hash;
  gboolean allow_held_contact;
  guint early_kind;
  guint early_streak;
  gboolean skip_background_images;
  guint64 cache_deadline_us;
  gboolean reuse_background;
  Goodix52xdPix reused_background[GOODIX52XD_FRAME_SIZE];
  guint16 reused_baseline[8];
  gboolean post_clear_guard;
  guint16 post_clear_reference[8];
  gboolean retained_session;
  gboolean retained_ready;
  gint64 retained_mono;
  gint64 retained_boot;

  Goodix52xdPix empty_img[GOODIX52XD_FRAME_SIZE];
};

G_DECLARE_FINAL_TYPE(FpiDeviceGoodixTls52XD, fpi_device_goodixtls52xd, FPI,
                     DEVICE_GOODIXTLS52XD, FpiDeviceGoodixTls);

G_DEFINE_TYPE(FpiDeviceGoodixTls52XD, fpi_device_goodixtls52xd,
              FPI_TYPE_DEVICE_GOODIXTLS);

// ---- ACTIVE SECTION START ----

enum activate_states {
    ACTIVATE_READ_AND_NOP,
    ACTIVATE_ENABLE_CHIP,
    ACTIVATE_NOP,
    ACTIVATE_CHECK_FW_VER,
    ACTIVATE_CHECK_PSK,
    ACTIVATE_RESET,
    ACTIVATE_OTP,
    ACTIVATE_SET_MCU_IDLE,
    ACTIVATE_SET_MCU_CONFIG,
    ACTIVATE_NUM_STATES,
};

static void check_none(FpDevice *dev, gpointer user_data, GError *error) {
  if (error) {
    fpi_ssm_mark_failed(user_data, error);
    return;
  }

  fpi_ssm_next_state(user_data);
}

static void check_firmware_version(FpDevice *dev, gchar *firmware,
                                   gpointer user_data, GError *error) {
  if (error) {
    fpi_ssm_mark_failed(user_data, error);
    return;
  }

  fp_dbg("Device firmware: \"%s\"", firmware);

  if (strncmp(firmware, GOODIX_52XD_FIRMWARE_PREFIX,
              strlen(GOODIX_52XD_FIRMWARE_PREFIX))) {
    g_set_error(&error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                "Invalid device firmware: \"%s\"", firmware);
    fpi_ssm_mark_failed(user_data, error);
    return;
  }

  fpi_ssm_next_state(user_data);
}

static void check_reset(FpDevice *dev, gboolean success, guint16 number,
                        gpointer user_data, GError *error) {
  if (error) {
    fpi_ssm_mark_failed(user_data, error);
    return;
  }

  if (!success) {
    g_set_error(&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                "Failed to reset device");
    fpi_ssm_mark_failed(user_data, error);
    return;
  }

  fp_dbg("Device reset number: %d", number);

  if (number != GOODIX_52XD_RESET_NUMBER) {
    g_set_error(&error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                "Invalid device reset number: %d", number);
    fpi_ssm_mark_failed(user_data, error);
    return;
  }

  fpi_ssm_next_state(user_data);
}

static void check_preset_psk_read(FpDevice *dev, gboolean success,
                                  guint32 flags, guint8 *psk, guint16 length,
                                  gpointer user_data, GError *error) {
  g_autofree gchar *psk_str = data_to_str(psk, length);

  if (error) {
    fpi_ssm_mark_failed(user_data, error);
    return;
  }

  if (!success) {
    g_set_error(&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                "Failed to read PSK from device");
    fpi_ssm_mark_failed(user_data, error);
    return;
  }

  fp_dbg("Device PSK: 0x%s", psk_str);
  fp_dbg("Device PSK flags: 0x%08x", flags);

  if (flags != GOODIX_52XD_PSK_FLAGS) {
    g_set_error(&error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                "Invalid device PSK flags: 0x%08x", flags);
    fpi_ssm_mark_failed(user_data, error);
    return;
  }

  if (length != sizeof(goodix_52xd_psk_0)) {
    g_set_error(&error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                "Invalid device PSK: 0x%s", psk_str);
    fpi_ssm_mark_failed(user_data, error);
    return;
  }

  if (memcmp(psk, goodix_52xd_psk_0, sizeof(goodix_52xd_psk_0))) {
    g_set_error(&error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                "Invalid device PSK: 0x%s", psk_str);
    fpi_ssm_mark_failed(user_data, error);
    return;
  }

  fpi_ssm_next_state(user_data);
}
static void check_idle(FpDevice* dev, gpointer user_data, GError* err)
{

    if (err) {
        fpi_ssm_mark_failed(user_data, err);
        return;
    }
    fpi_ssm_next_state(user_data);
}
static void check_config_upload(FpDevice* dev, gboolean success,
                                gpointer user_data, GError* error)
{
    if (error) {
        fpi_ssm_mark_failed(user_data, error);
    }
    else if (!success) {
        fpi_ssm_mark_failed(user_data,
                            g_error_new(FP_DEVICE_ERROR, FP_DEVICE_ERROR_PROTO,
                                        "failed to upload mcu config"));
    }
    else {
        fpi_ssm_next_state(user_data);
    }
}
static void check_powerdown_scan_freq(FpDevice* dev, gboolean success,
                                      gpointer user_data, GError* error)
{
    if (error) {
        fpi_ssm_mark_failed(user_data, error);
    }
    else if (!success) {
        fpi_ssm_mark_failed(user_data,
                            g_error_new(FP_DEVICE_ERROR, FP_DEVICE_ERROR_PROTO,
                                        "failed to set powerdown freq"));
    }
    fpi_ssm_next_state(user_data);
}

enum otp_write_states {
    OTP_WRITE_1,
    OTP_WRITE_2,

    OTP_WRITE_NUM,
};

static void otp_write_run(FpiSsm* ssm, FpDevice* dev)
{
    /*FpiDeviceGoodixTls52XD* self = FPI_DEVICE_GOODIXTLS52XD(dev);
    switch (fpi_ssm_get_cur_state(ssm)) {
    case OTP_WRITE_1:
        guint8 sensor1[] = {0x0a, 0x02};
        goodix_send_write_sensor_register(
        dev, 0x022c, sensor1, check_none,
        ssm);
        break;
    case OTP_WRITE_2:
        guint8 sensor2[] = {0x0a, 0x03};
        goodix_send_write_sensor_register(
            dev, 0x022c, sensor2, check_none,
            ssm);
        fpi_ssm_next_state(ssm);
        break;
    }*/
}

static void read_otp_callback(FpDevice* dev, guint8* data, guint16 len,
                              gpointer ssm, GError* err)
{
    if (err) {
        fpi_ssm_mark_failed(ssm, err);
        return;
    }
    if (len < 64) {
        fpi_ssm_mark_failed(ssm, g_error_new(FP_DEVICE_ERROR,
                                             FP_DEVICE_ERROR_DATA_INVALID,
                                             "OTP is invalid (len: %d)", 64));
        return;
    }
    FpiDeviceGoodixTls52XD* self = FPI_DEVICE_GOODIXTLS52XD(dev);
    self->otp = malloc(64);
    memcpy(self->otp, data, len);
    fpi_ssm_next_state(ssm);
}

static void activate_run_state(FpiSsm* ssm, FpDevice* dev)
{

    switch (fpi_ssm_get_cur_state(ssm)) {
    case ACTIVATE_READ_AND_NOP:
        // Nop seems to clear the previous command buffer. But we are
        // unable to do so.
        goodix_start_read_loop(dev);
        goodix_send_nop(dev, check_none, ssm);
        break;

    case ACTIVATE_ENABLE_CHIP:
      goodix_send_enable_chip(dev, TRUE, check_none, ssm);
      break;

    case ACTIVATE_NOP:
      goodix_send_nop(dev, check_none, ssm);
      break;

    case ACTIVATE_CHECK_FW_VER:
      goodix_send_firmware_version(dev, check_firmware_version, ssm);
      break;

    case ACTIVATE_CHECK_PSK:
      goodix_send_preset_psk_read(dev, GOODIX_52XD_PSK_FLAGS, 32,
                                  check_preset_psk_read, ssm);
      break;

    case ACTIVATE_RESET:
      goodix_send_reset(dev, TRUE, 20, check_reset, ssm);
      break;

    case ACTIVATE_OTP:
      goodix_send_read_otp(dev, read_otp_callback, ssm);
      break;

    case ACTIVATE_SET_MCU_IDLE:
        goodix_send_mcu_switch_to_idle_mode(dev, 20, check_idle, ssm);
        break;

    case ACTIVATE_SET_MCU_CONFIG:
        goodix_send_upload_config_mcu(dev, goodix_52xd_config,
                                      sizeof(goodix_52xd_config), NULL,
                                      check_config_upload, ssm);
        break;
    }
}

static void calibrate_background(FpDevice* dev);

static void tls_activation_complete(FpDevice* dev, gpointer user_data,
                                    GError* error)
{
    if (error) {
        fp_err("failed to complete tls activation: %s", error->message);
        fpi_image_device_activate_complete(FP_IMAGE_DEVICE(dev), error);
        return;
    }
    FpImageDevice* image_dev = FP_IMAGE_DEVICE(dev);

    if (FPI_DEVICE_GOODIXTLS52XD(dev)->pr81_capture)
        calibrate_background(dev);
    else
        fpi_image_device_activate_complete(image_dev, NULL);
}

static void activate_complete(FpiSsm* ssm, FpDevice* dev, GError* error)
{
    G_DEBUG_HERE();
    if (!error)
        goodix_tls(dev, tls_activation_complete, NULL);
    else {
        fp_err("failed during activation: %s (code: %d)", error->message,
               error->code);
        fpi_image_device_activate_complete(FP_IMAGE_DEVICE(dev), error);
    }
}

// ---- ACTIVE SECTION END ----

// -----------------------------------------------------------------------------

// ---- SCAN SECTION START ----

enum SCAN_STAGES {
    SCAN_STAGE_SWITCH_TO_FDT_DOWN,
    SCAN_STAGE_SWITCH_TO_FDT_MODE,
    SCAN_STAGE_FULL_MODE_CONFIG,
    SCAN_STAGE_FULL_MODE_REPORT,
    SCAN_STAGE_REGISTER_STOP,
    SCAN_STAGE_GET_IMG,

    SCAN_STAGE_NUM,
};

static void check_none_cmd(FpDevice* dev, guint8* data, guint16 len,
                           gpointer ssm, GError* err)
{
    if (err) {
        fpi_ssm_mark_failed(ssm, err);
        return;
    }
    fpi_ssm_next_state(ssm);
}

// Bitdepth is 12, but we have to fit it in a byte
static unsigned char squash(int v) { return v / 16; }

static void decode_frame(Goodix52xdPix frame[GOODIX52XD_FRAME_SIZE],
                         const guint8* raw_frame)
{
    Goodix52xdPix uncropped[GOODIX52XD_SCAN_WIDTH * GOODIX52XD_HEIGHT];
    Goodix52xdPix* pix = uncropped;
    for (int i = 0; i < GOODIX52XD_RAW_FRAME_SIZE; i += 6) {
        const guint8* chunk = raw_frame + i;
        *pix++ = ((chunk[0] & 0xf) << 8) + chunk[1];
        *pix++ = (chunk[3] << 4) + (chunk[0] >> 4);
        *pix++ = ((chunk[5] & 0xf) << 8) + chunk[2];
        *pix++ = (chunk[4] << 4) + (chunk[5] >> 4);
    }

    for (int y = 0; y != GOODIX52XD_HEIGHT; ++y) {
        for (int x = 0; x != GOODIX52XD_WIDTH; ++x) {
            const int idx = x + y * GOODIX52XD_SCAN_WIDTH;
            frame[x + y * GOODIX52XD_WIDTH] = uncropped[idx];
        }
    }
}
static int goodix_cmp_short(const void* a, const void* b)
{
    return (int) (*(short*) a - *(short*) b);
}

static void rotate_frame(Goodix52xdPix frame[GOODIX52XD_FRAME_SIZE])
{
    Goodix52xdPix buff[GOODIX52XD_FRAME_SIZE];

    for (int y = 0; y != GOODIX52XD_HEIGHT; ++y) {
        for (int x = 0; x != GOODIX52XD_WIDTH; ++x) {
            buff[x * GOODIX52XD_WIDTH + y] = frame[x + y * GOODIX52XD_WIDTH];
        }
    }
    memcpy(frame, buff, GOODIX52XD_FRAME_SIZE);
}
static void squash_frame(Goodix52xdPix* frame, guint8* squashed)
{
    for (int i = 0; i != GOODIX52XD_FRAME_SIZE; ++i) {
        squashed[i] = squash(frame[i]);
    }
}
/**
 * @brief Squashes the 12 bit pixels of a raw frame into the 4 bit pixels used
 * by libfprint.
 * @details Borrowed from the elan driver. We reduce frames to
 * within the max and min.
 *
 * @param frame
 * @param squashed
 */
static void squash_frame_linear(Goodix52xdPix* frame, guint8* squashed)
{
    Goodix52xdPix min = 0xffff;
    Goodix52xdPix max = 0;

    for (int i = 0; i != GOODIX52XD_FRAME_SIZE; ++i) {
        const Goodix52xdPix pix = frame[i];
        if (pix < min) {
            min = pix;
        }
        if (pix > max) {
            max = pix;
        }
    }

    for (int i = 0; i != GOODIX52XD_FRAME_SIZE; ++i) {
        const Goodix52xdPix pix = frame[i];
        if (pix - min == 0 || max - min == 0) {
            squashed[i] = 0;
        }
        else {
            squashed[i] = (pix - min) * 0xff / (max - min);
        }
    }
}

/**
 * @brief Subtracts the background from the frame
 *
 * @param frame
 * @param background
 */
static gboolean postprocess_frame(Goodix52xdPix frame[GOODIX52XD_FRAME_SIZE],
                                  Goodix52xdPix background[GOODIX52XD_FRAME_SIZE])
{
    int sum = 0;
    for (int i = 0; i != GOODIX52XD_FRAME_SIZE; ++i) {
        Goodix52xdPix* og_px = frame + i;
        Goodix52xdPix bg_px =  background[i];
            if (bg_px > *og_px) {
                *og_px = 0;
            }
            else {
                *og_px -= bg_px;
            }
            *og_px = MAX(bg_px - *og_px, 0);
            *og_px = MAX(*og_px - bg_px, 0);
            sum += *og_px;
            
    }
    if (sum == 0) {
        fp_warn("frame darker than background, finger on scanner during "
                "calibration?");
    }
    return sum != 0;
}

typedef struct _frame_processing_info {
    FpiDeviceGoodixTls52XD* dev;
    GSList** frames;
    guint frame_number;

} frame_processing_info;

static void save_raw_pgm(const gchar* path, const Goodix52xdPix* pixels)
{
    FILE* file = fopen(path, "w");
    if (!file) {
        fp_warn("Could not save diagnostic image: %s", path);
        return;
    }
    fprintf(file, "P2\n%d %d\n4095\n", GOODIX52XD_WIDTH, GOODIX52XD_HEIGHT);
    for (guint i = 0; i < GOODIX52XD_FRAME_SIZE; i++)
        fprintf(file, "%u%c", pixels[i],
                (i + 1) % GOODIX52XD_WIDTH ? ' ' : '\n');
    if (fclose(file) != 0)
        fp_warn("Could not finish diagnostic image: %s", path);
}

static gchar* diagnostic_image_path(FpiDeviceGoodixTls52XD* self,
                                    const gchar* kind, guint frame_number)
{
    const gchar* prefix = g_getenv("GOODIX521D_FRAME_DUMP_PREFIX");
    if (g_strcmp0(prefix, "enroll") != 0 && g_strcmp0(prefix, "verify") != 0)
        prefix = "capture";
    if (frame_number)
        return g_strdup_printf("%s-burst-%04u-%s-%02u.pgm", prefix,
                               self->diagnostic_burst, kind, frame_number);
    return g_strdup_printf("%s-burst-%04u-%s.pgm", prefix,
                           self->diagnostic_burst, kind);
}

static void save_processed_pgm(const gchar* path, const guint8* pixels)
{
    FILE* file = fopen(path, "wb");
    if (!file) {
        fp_warn("Could not save diagnostic image: %s", path);
        return;
    }
    fprintf(file, "P5\n%d %d\n255\n", GOODIX52XD_WIDTH, GOODIX52XD_HEIGHT);
    if (fwrite(pixels, 1, GOODIX52XD_FRAME_SIZE, file) != GOODIX52XD_FRAME_SIZE)
        fp_warn("Could not finish diagnostic image: %s", path);
    if (fclose(file) != 0)
        fp_warn("Could not close diagnostic image: %s", path);
}

static void process_frame(Goodix52xdPix* raw_frame, frame_processing_info* info)
{
    struct fpi_frame* frame =
        g_malloc(GOODIX52XD_FRAME_SIZE + sizeof(struct fpi_frame));
    //postprocess_frame(raw_frame, info->dev->empty_img);
    if (info->dev->pr81_capture)
        for (guint i = 0; i < GOODIX52XD_FRAME_SIZE; i++)
            raw_frame[i] = goodix_pr81_correct_pixel(raw_frame[i], info->dev->empty_img[i]);
    squash_frame_linear(raw_frame, frame->data);

    if (info->dev->save_frames) {
        g_autofree gchar* path = diagnostic_image_path(info->dev, "processed",
                                                      ++info->frame_number);
        save_processed_pgm(path, frame->data);
    }

    *(info->frames) = g_slist_append(*(info->frames), frame);
}

static void save_frame(FpiDeviceGoodixTls52XD* self, guint8* raw)
{
    Goodix52xdPix* frame = malloc(GOODIX52XD_FRAME_SIZE * sizeof(Goodix52xdPix));
    decode_frame(frame, raw);
    if (self->save_frames) {
        guint frame_number = g_slist_length(self->frames);
        if (frame_number == 1) {
            self->diagnostic_burst++;
            g_autofree gchar* background = diagnostic_image_path(self, "background", 0);
            save_raw_pgm(background, self->empty_img);
            fp_dbg("Frame diagnostic: burst %u, saving ten frames and their background",
                   self->diagnostic_burst);
        }
        g_autofree gchar* path = diagnostic_image_path(self, "raw", frame_number);
        save_raw_pgm(path, frame);
    }
    if (self->pr81_capture) {
        guint min = 4095, max = 0, clipped = 0;
        for (guint i = 0; i < GOODIX52XD_FRAME_SIZE; i++) {
            min = MIN(min, frame[i]);
            max = MAX(max, frame[i]);
            clipped += frame[i] == 0 || frame[i] == 4095;
        }
        fp_dbg("Raw frame statistics: min=%u max=%u clipped=%u/%u",
               min, max, clipped, GOODIX52XD_FRAME_SIZE);
        if (g_slist_length(self->frames) == 1)
            save_raw_pgm("capture-raw.pgm", frame);
    }
    self->frames = g_slist_append(self->frames, frame);
}

static void scan_on_read_img(FpDevice* dev, guint8* data, guint16 len,
                             gpointer ssm, GError* err)
{
    if (err) {
        fpi_ssm_mark_failed(ssm, err);
        return;
    }


    if (len < GOODIX52XD_RAW_FRAME_SIZE ||
        (FPI_DEVICE_GOODIXTLS52XD(dev)->pr81_full_capture &&
         len != GOODIX52XD_RAW_FRAME_SIZE + 4)) {
        fpi_ssm_mark_failed(ssm, g_error_new(FP_DEVICE_ERROR,
            FP_DEVICE_ERROR_DATA_INVALID, "Short image: %u bytes, expected at least %u",
            len, GOODIX52XD_RAW_FRAME_SIZE));
        return;
    }
    fp_dbg("Decrypted image length: %u", len);
    FpiDeviceGoodixTls52XD* self = FPI_DEVICE_GOODIXTLS52XD(dev);
    save_frame(self, data);
    if (g_slist_length(self->frames) <= GOODIX52XD_CAP_FRAMES) {
        /* The diagnostic burst does not re-arm the touch event per frame. */
        fpi_ssm_jump_to_state(ssm, self->pr81_capture
                                       ? SCAN_STAGE_REGISTER_STOP
                                       : self->timed_capture
                                       ? SCAN_STAGE_GET_IMG
                                       : SCAN_STAGE_SWITCH_TO_FDT_MODE);
    }
    else {
        GSList* raw_frames = g_slist_nth(self->frames, 1);

        FpImageDevice* img_dev = FP_IMAGE_DEVICE(dev);

        GSList* frames = NULL;
        frame_processing_info pinfo = {.dev = self, .frames = &frames};

        g_slist_foreach(raw_frames, (GFunc) process_frame, &pinfo);
        frames = g_slist_reverse(frames);

        /* The 521d is a static press sensor, not a swipe sensor: the captured
         * frames all show a stationary finger. Swipe-style movement estimation
         * hallucinates motion and smears them into an unmatchable strip, so
         * average the frames into a single clean press image instead. */
        FpImage* img = fp_image_new(GOODIX52XD_WIDTH, GOODIX52XD_HEIGHT);
        {
            guint nframes = g_slist_length(frames);
            gsize npix = (gsize) GOODIX52XD_WIDTH * GOODIX52XD_HEIGHT;
            for (gsize i = 0; i < npix; i++) {
                guint sum = 0;
                for (GSList *l = frames; l; l = l->next)
                    sum += ((struct fpi_frame *) l->data)->data[i];
                img->data[i] = (guint8) (sum / nframes);
            }
        }

        if (self->save_frames) {
            g_autofree gchar* path = diagnostic_image_path(self, "average", 0);
            save_processed_pgm(path, img->data);
        }

        g_slist_free_full(frames, g_free);
        g_slist_free_full(self->frames, g_free);
        self->frames = g_slist_alloc();

        fpi_image_device_image_captured(img_dev, img);


        fpi_image_device_report_finger_status(img_dev, FALSE);

        fpi_ssm_next_state(ssm);
    }
}

enum scan_empty_img_state {
    SCAN_EMPTY_FDT_CONFIG,
    SCAN_EMPTY_FDT_REPORT,
    SCAN_EMPTY_START_WARMUP,
    SCAN_EMPTY_WARMUP,
    SCAN_EMPTY_STOP_WARMUP,
    SCAN_EMPTY_START_BACKGROUND,
    SCAN_EMPTY_GET_IMG,
    SCAN_EMPTY_STOP_BACKGROUND,

    SCAN_EMPTY_NUM,
};

static void on_scan_empty_img(FpDevice* dev, guint8* data, guint16 length,
                              gpointer ssm, GError* error)
{
    if (error) {
        fpi_ssm_mark_failed(ssm, error);
        return;
    }
    if (length < GOODIX52XD_RAW_FRAME_SIZE) {
        fpi_ssm_mark_failed(ssm, g_error_new(FP_DEVICE_ERROR,
            FP_DEVICE_ERROR_DATA_INVALID, "Short calibration image: %u bytes", length));
        return;
    }
    FpiDeviceGoodixTls52XD* self = FPI_DEVICE_GOODIXTLS52XD(dev);
    decode_frame(self->empty_img, data);
    save_raw_pgm("background.pgm", self->empty_img);
    fp_dbg("PR81 background captured: %u decrypted bytes", length);
    fpi_ssm_next_state(ssm);
}
static void on_warmup_img(FpDevice* dev, guint8* data, guint16 length,
                           gpointer ssm, GError* error)
{
    if (error) {
        fpi_ssm_mark_failed(ssm, error);
        return;
    }
    if (length < GOODIX52XD_RAW_FRAME_SIZE) {
        fpi_ssm_mark_failed(ssm, g_error_new(FP_DEVICE_ERROR,
            FP_DEVICE_ERROR_DATA_INVALID, "Short warmup image: %u bytes", length));
        return;
    }
    fpi_ssm_next_state(ssm);
}

static void scan_empty_run(FpiSsm* ssm, FpDevice* dev)
{

    switch (fpi_ssm_get_cur_state(ssm)) {
    case SCAN_EMPTY_FDT_CONFIG: {
        guint8 mode[sizeof(goodix_pr81_report)];
        memcpy(mode, goodix_pr81_report, sizeof(mode));
        mode[sizeof(mode) - 1] = 0x00;
        goodix_send_mcu_switch_to_fdt_mode(dev, mode, sizeof(mode), FALSE,
                                           NULL, check_none_cmd, ssm);
        break;
    }
    case SCAN_EMPTY_FDT_REPORT:
        goodix_send_mcu_switch_to_fdt_mode(dev, (guint8*) goodix_pr81_report,
                                           sizeof(goodix_pr81_report), TRUE,
                                           NULL, check_none_cmd, ssm);
        break;
    case SCAN_EMPTY_START_WARMUP:
    case SCAN_EMPTY_START_BACKGROUND:
        goodix_send_write_sensor_register(dev, GOODIX_PR81_REGISTER,
                                           GOODIX_PR81_START, check_none, ssm);
        break;
    case SCAN_EMPTY_STOP_WARMUP:
    case SCAN_EMPTY_STOP_BACKGROUND:
        goodix_send_write_sensor_register(dev, GOODIX_PR81_REGISTER,
                                           GOODIX_PR81_STOP, check_none, ssm);
        break;
    case SCAN_EMPTY_WARMUP:
        goodix_tls_read_image(dev, (guint8*) goodix_pr81_warmup,
                               sizeof(goodix_pr81_warmup), on_warmup_img, ssm);
        break;

    case SCAN_EMPTY_GET_IMG: {
        goodix_tls_read_image(dev, (guint8*) goodix_pr81_image,
                               sizeof(goodix_pr81_image), on_scan_empty_img, ssm);
        break;
    }
    }
}

static void background_complete(FpiSsm* ssm, FpDevice* dev, GError* error)
{
    fpi_image_device_activate_complete(FP_IMAGE_DEVICE(dev), error);
}

static void calibrate_background(FpDevice* dev)
{
    fp_dbg("PR81 calibration: keep the sensor clear until the countdown");
    fpi_ssm_start(fpi_ssm_new(dev, scan_empty_run, SCAN_EMPTY_NUM), background_complete);
}

static guint16 pr81_read_u16(const guint8* data)
{
    return (guint16) data[0] | ((guint16) data[1] << 8);
}

static guint32 pr81_read_u32(const guint8* data)
{
    return (guint32) data[0] | ((guint32) data[1] << 8) |
           ((guint32) data[2] << 16) | ((guint32) data[3] << 24);
}

static void pr81_full_reply(FpDevice* dev, guint8* data, guint16 length,
                             gpointer user_data, GError* error)
{
    FpiSsm* ssm = user_data;
    if (error) {
        fpi_ssm_mark_failed(ssm, error);
        return;
    }
    FpiDeviceGoodixTls52XD* self = FPI_DEVICE_GOODIXTLS52XD(dev);
    const GoodixPr81Step* step = &goodix_pr81_full_startup[fpi_ssm_get_cur_state(ssm)];
    gboolean valid = TRUE;
    switch (step->check) {
    case PR81_FIRMWARE: {
        if (!data || length == 0) {
            valid = FALSE;
            break;
        }
        g_autofree gchar* firmware = g_strndup((gchar*) data, length);
        fp_dbg("Device firmware: \"%s\"", firmware);
        valid = g_strcmp0(firmware, GOODIX_52XD_FIRMWARE_VERSION) == 0;
        break;
    }
    case PR81_PSK:
        valid = length >= 41 && data[0] == 0 &&
                pr81_read_u32(data + 1) == GOODIX_52XD_PSK_FLAGS &&
                pr81_read_u32(data + 5) == sizeof(goodix_52xd_psk_0) &&
                memcmp(data + 9, goodix_52xd_psk_0, sizeof(goodix_52xd_psk_0)) == 0;
        break;
    case PR81_RESET:
        valid = length >= 3 && data[0] == 1 &&
                pr81_read_u16(data + 1) == GOODIX_52XD_RESET_NUMBER;
        break;
    case PR81_CHIP:
        valid = length >= 4;
        if (valid)
            fp_dbg("PR81 chip ID: 0x%04x", pr81_read_u16(data));
        break;
    case PR81_OTP:
        valid = length == 64;
        if (valid) {
            g_free(self->otp);
            self->otp = g_memdup2(data, length);
            fp_dbg("PR81 OTP captured: %u bytes (request 40 00)", length);
            g_autofree gchar *otp_hash = g_compute_checksum_for_data(G_CHECKSUM_SHA256, data, length);
            if (self->identity_probe) {
                g_print("CONTACT_IDENTITY %s\n", otp_hash);
                fflush(stdout);
                fpi_ssm_mark_failed(ssm, g_error_new_literal(G_IO_ERROR,
                    G_IO_ERROR_CANCELLED, "Identity probe complete before calibration"));
                return;
            }
            if (self->clear_guard && g_strcmp0(otp_hash, self->clear_otp_hash) != 0) {
                fpi_ssm_mark_failed(ssm, g_error_new_literal(FP_DEVICE_ERROR,
                    FP_DEVICE_ERROR_DATA_INVALID, "Clear reference belongs to a different sensor OTP"));
                return;
            }
        }
        break;
    case PR81_CONFIG:
        valid = length >= 1 && data[0] == 1;
        break;
    case PR81_POV:
        valid = length >= 1;
        break;
    case PR81_FDT:
    case PR81_BASELINE: {
        guint16 regions[8];
        valid = goodix_pr81_read_regions(data, length, regions);
        /* Opt-in measurement before any calibration image. Normal capture
         * remains unchanged. The relative down-event alone cannot tell us
         * whether the sensor was already touched at startup. */
        if (valid && self->contact_probe &&
            g_strcmp0(step->name, "clear-mode-report") == 0) {
            guint n = self->contact_probe_sample++;
            g_print("CONTACT_PROBE %u %u %u %u %u %u %u %u %u %u\n",
                    n, n / 25, regions[0], regions[1], regions[2], regions[3],
                    regions[4], regions[5], regions[6], regions[7]);
            fflush(stdout);
            if (self->contact_probe_sample == 100) {
                fpi_ssm_mark_failed(ssm, g_error_new_literal(G_IO_ERROR,
                    G_IO_ERROR_CANCELLED, "Contact probe complete before calibration"));
            } else {
                fpi_ssm_jump_to_state_delayed(ssm, fpi_ssm_get_cur_state(ssm), 200);
            }
            return;
        }
        if (valid && self->allow_held_contact &&
            g_strcmp0(step->name, "clear-mode-report") == 0) {
            if (!goodix_held_deadline_valid(self->cache_deadline_us)) {
                fpi_ssm_mark_failed(ssm,g_error_new_literal(FP_DEVICE_ERROR,
                    FP_DEVICE_ERROR_GENERAL,"Early-contact cache expired; lift and refresh"));
                return;
            }
            guint kind=goodix_early_classify(self->clear_reference,regions);
            if (kind==GOODIX_EARLY_UNKNOWN) {
                fpi_ssm_mark_failed(ssm,g_error_new_literal(FP_DEVICE_ERROR,
                    FP_DEVICE_ERROR_GENERAL,"Early contact ambiguous; lift and refresh"));
                return;
            }
            self->early_streak=goodix_early_streak(self->early_kind,kind,
                self->clear_previous,regions,self->early_streak);
            self->early_kind=kind;
            memcpy(self->clear_previous,regions,sizeof(regions));
            if(self->early_streak<2) {
                fpi_ssm_jump_to_state_delayed(ssm,fpi_ssm_get_cur_state(ssm),200);
                return;
            }
            g_print("EARLY_STARTUP %s\n",kind==GOODIX_EARLY_HELD ? "HELD" : "CLEAR");fflush(stdout);
        }
        if (valid && self->clear_guard && !self->allow_held_contact &&
            g_strcmp0(step->name, "clear-mode-report") == 0) {
            self->clear_streak = goodix52xd_clear_streak(self->clear_reference,
                self->clear_previous, regions, self->clear_streak);
            memcpy(self->clear_previous, regions, sizeof(regions));
            /* A cached background only needs a confirmed lift/new touch;
             * fresh background calibration retains the four-sample gate. */
            if (self->clear_streak < (self->reuse_background ? 2u : 4u)) {
                if (!self->clear_wait_announced && !goodix52xd_clear_sample(self->clear_reference, regions)) {
                    g_print("CLEAR_GUARD WAIT\n");fflush(stdout);
                    self->clear_wait_announced = TRUE;
                }
                if (g_get_monotonic_time() >= self->clear_deadline) {
                    fpi_ssm_mark_failed(ssm, g_error_new_literal(FP_DEVICE_ERROR,
                        FP_DEVICE_ERROR_GENERAL, "Clear sensor wait timed out; lift all fingers before calibration"));
                    return;
                }
                fpi_ssm_jump_to_state_delayed(ssm, fpi_ssm_get_cur_state(ssm), 200);
                return;
            }
            g_print("CLEAR_GUARD READY\n");fflush(stdout);
        }
        if (valid && step->check == PR81_BASELINE) {
            if (self->skip_background_images) {
                g_print("CACHED_STARTUP OBSERVED_BASELINE %u %u %u %u %u %u %u %u\n",
                    regions[0],regions[1],regions[2],regions[3],regions[4],regions[5],regions[6],regions[7]);fflush(stdout);
            }
            if(self->allow_held_contact) {
                guint kind=goodix_early_final_classify(self->reused_baseline,regions);
                if (!goodix_held_deadline_valid(self->cache_deadline_us) ||
                    kind==GOODIX_EARLY_UNKNOWN || kind!=self->early_kind) {
                    fpi_ssm_mark_failed(ssm,g_error_new_literal(FP_DEVICE_ERROR,
                        FP_DEVICE_ERROR_GENERAL,"Early contact changed or expired; lift and refresh"));
                    return;
                }
                g_print("EARLY_STARTUP BASELINE_%s\n",kind==GOODIX_EARLY_HELD ? "HELD" : "CLEAR");fflush(stdout);
            }
            if (self->skip_background_images && !self->allow_held_contact &&
                (!goodix_cache_deadline_valid(self->cache_deadline_us) ||
                 !goodix_cache_baseline_valid(self->reused_baseline, regions))) {
                fpi_ssm_mark_failed(ssm, g_error_new_literal(FP_DEVICE_ERROR,
                    FP_DEVICE_ERROR_GENERAL, "Cached contact baseline changed or expired; refresh with sensor clear"));
                return;
            }
            if (self->post_clear_guard && !self->reuse_background &&
                !goodix52xd_clear_sample(self->post_clear_reference,regions)) {
                fpi_ssm_mark_failed(ssm,g_error_new_literal(FP_DEVICE_ERROR,
                    FP_DEVICE_ERROR_GENERAL,"Finger appeared during calibration; lift and retry"));
                return;
            }
            memcpy(self->contact_baseline, self->reuse_background ? self->reused_baseline : regions, sizeof(regions));
            fp_dbg("PR81 clear contact baseline: %u %u %u %u %u %u %u %u",
                   regions[0], regions[1], regions[2], regions[3],
                   regions[4], regions[5], regions[6], regions[7]);
            g_print("CAPTURE_BASELINE %u %u %u %u %u %u %u %u\n",
                    self->contact_baseline[0],self->contact_baseline[1],self->contact_baseline[2],self->contact_baseline[3],
                    self->contact_baseline[4],self->contact_baseline[5],self->contact_baseline[6],self->contact_baseline[7]);
            fflush(stdout);
        }
        break;
    }
    case PR81_NONE:
        break;
    }
    if (!valid) {
        fpi_ssm_mark_failed(ssm, g_error_new(FP_DEVICE_ERROR,
            FP_DEVICE_ERROR_DATA_INVALID,
            "PR81 startup %s: invalid response (%u bytes)",
            step->name, length));
        return;
    }
    fpi_ssm_next_state(ssm);
}

static void pr81_full_image(FpDevice* dev, guint8* data, guint16 length,
                             gpointer user_data, GError* error)
{
    FpiSsm* ssm = user_data;
    if (error) {
        fpi_ssm_mark_failed(ssm, error);
        return;
    }
    const GoodixPr81Step* step = &goodix_pr81_full_startup[fpi_ssm_get_cur_state(ssm)];
    if (length != GOODIX52XD_RAW_FRAME_SIZE + 4) {
        fpi_ssm_mark_failed(ssm, g_error_new(FP_DEVICE_ERROR,
            FP_DEVICE_ERROR_DATA_INVALID, "PR81 %s: got %u bytes, expected %u",
            step->name, length, GOODIX52XD_RAW_FRAME_SIZE + 4));
        return;
    }
    Goodix52xdPix frame[GOODIX52XD_FRAME_SIZE];
    decode_frame(frame, data);
    save_raw_pgm(step->image_path, frame);
    if (step->background) {
        FpiDeviceGoodixTls52XD* self = FPI_DEVICE_GOODIXTLS52XD(dev);
        memcpy(self->empty_img, self->reuse_background ? self->reused_background : frame, sizeof(frame));
        save_raw_pgm("background.pgm", self->empty_img);
        if (self->retained_session) {
            self->retained_mono=g_get_monotonic_time();
            self->retained_boot=goodix_retained_boottime();
        }
    }
    fp_dbg("PR81 full calibration: %s captured (%u bytes)", step->name, length);
    fpi_ssm_next_state(ssm);
}

static void pr81_nop_complete(FpDevice* dev, gpointer user_data, GError* error)
{
    if (error)
        fpi_ssm_mark_failed(user_data, error);
    else
        /* Match upstream's 100 ms NOP drain before issuing the next command. */
        fpi_ssm_next_state_delayed(user_data, 100);
}

static void pr81_full_startup_run(FpiSsm* ssm, FpDevice* dev)
{
    FpiDeviceGoodixTls52XD *self = FPI_DEVICE_GOODIXTLS52XD(dev);
    if ((self->clear_guard || self->contact_probe || self->identity_probe) &&
        g_cancellable_is_cancelled(fpi_device_get_cancellable(dev))) {
        fpi_ssm_mark_failed(ssm, g_error_new_literal(G_IO_ERROR,
            G_IO_ERROR_CANCELLED, "Contact wait cancelled"));
        return;
    }
    const GoodixPr81Step* step = &goodix_pr81_full_startup[fpi_ssm_get_cur_state(ssm)];
    if (self->skip_background_images) {
        if (!goodix_cache_deadline_valid(self->cache_deadline_us)) {
            fpi_ssm_mark_failed(ssm, g_error_new_literal(FP_DEVICE_ERROR,
                FP_DEVICE_ERROR_GENERAL, "Cached background expired; refresh with sensor clear"));
            return;
        }
        if (step->kind == PR81_IMAGE) {
            if (!goodix_cache_image_step(step->name)) {
                fpi_ssm_mark_failed(ssm, g_error_new_literal(FP_DEVICE_ERROR,
                    FP_DEVICE_ERROR_DATA_INVALID, "Unreviewed calibration image step"));
                return;
            }
            if (step->background) {
                memcpy(self->empty_img, self->reused_background, sizeof(self->empty_img));
                save_raw_pgm("background.pgm", self->empty_img);
            }
            g_print("CACHED_STARTUP SKIPPED_IMAGE %s\n", step->name);fflush(stdout);
            fpi_ssm_next_state(ssm);
            return;
        }
    }
    fp_dbg("PR81 full startup: %s", step->name);
    switch (step->kind) {
    case PR81_NOP:
        goodix_send_nop(dev, pr81_nop_complete, ssm);
        break;
    case PR81_TLS:
        goodix_tls_full(dev, check_none, ssm);
        break;
    case PR81_IMAGE:
        goodix_tls_read_image(dev, (guint8*) step->payload, step->length,
                              pr81_full_image, ssm);
        break;
    case PR81_COMMAND:
        goodix_send_protocol(dev, step->command, (guint8*) step->payload,
            step->length, NULL, TRUE, GOODIX_TIMEOUT, step->reply,
            pr81_full_reply, ssm);
        break;
    }
}

static void pr81_full_startup_complete(FpiSsm* ssm, FpDevice* dev, GError* error)
{
    FpiDeviceGoodixTls52XD *self=FPI_DEVICE_GOODIXTLS52XD(dev);
    self->retained_ready=self->retained_session && !error;
    if (!error)
        fp_dbg("PR81 full startup completed; clear reference and contact baseline ready");
    fpi_image_device_activate_complete(FP_IMAGE_DEVICE(dev), error);
}

/* A retained USB/TLS session keeps the verified blank and clear FDT baseline.
 * Every new touch still checks stable contact, age and the system-sleep epoch. */
static void retained_reply(FpDevice *dev,guint8 *data,guint16 length,
                            gpointer user_data,GError *error) {
    FpiSsm *ssm=user_data;
    FpiDeviceGoodixTls52XD *self=FPI_DEVICE_GOODIXTLS52XD(dev);
    guint16 values[8];
    if (error) { self->retained_ready=FALSE;fpi_ssm_mark_failed(ssm,error);return; }
    if (!goodix_pr81_read_regions(data,length,values) ||
        !goodix_retained_valid(self->retained_mono,self->retained_boot,
                              g_get_monotonic_time(),goodix_retained_boottime())) {
        self->retained_ready=FALSE;
        fpi_ssm_mark_failed(ssm,g_error_new_literal(FP_DEVICE_ERROR,FP_DEVICE_ERROR_GENERAL,
            "Retained calibration expired or contact invalid; lift for fresh calibration"));return;
    }
    guint kind=goodix_early_final_classify(self->contact_baseline,values);
    if (!self->contact_capture && kind!=GOODIX_EARLY_CLEAR) {
        if (!self->clear_wait_announced) { g_print("CLEAR_GUARD WAIT\n");fflush(stdout);self->clear_wait_announced=TRUE; }
        self->early_streak=0;
    } else {
        self->early_streak=goodix_early_streak(self->early_kind,kind,
            self->clear_previous,values,self->early_streak);
    }
    self->early_kind=kind;memcpy(self->clear_previous,values,sizeof(values));
    if (self->early_streak<2) {
        if (g_get_monotonic_time()>=self->clear_deadline) {
            self->retained_ready=FALSE;
            fpi_ssm_mark_failed(ssm,g_error_new_literal(FP_DEVICE_ERROR,FP_DEVICE_ERROR_GENERAL,
                "Retained contact wait timed out; lift all fingers"));return;
        }
        fpi_ssm_jump_to_state_delayed(ssm,0,200);return;
    }
    save_raw_pgm("background.pgm",self->empty_img);
    g_print("SESSION_REUSED %" G_GINT64_FORMAT "\n",(g_get_monotonic_time()-self->retained_mono)/1000);
    g_print("SESSION_CONTACT %s\n",kind==GOODIX_EARLY_HELD?"HELD":"CLEAR");
    g_print("CONTACT_BASELINE %u %u %u %u %u %u %u %u\n",
        self->contact_baseline[0],self->contact_baseline[1],self->contact_baseline[2],self->contact_baseline[3],
        self->contact_baseline[4],self->contact_baseline[5],self->contact_baseline[6],self->contact_baseline[7]);
    fflush(stdout);fpi_ssm_next_state(ssm);
}
static void retained_run(FpiSsm *ssm,FpDevice *dev) {
    if (g_cancellable_is_cancelled(fpi_device_get_cancellable(dev))) {
        fpi_ssm_mark_failed(ssm,g_error_new_literal(G_IO_ERROR,G_IO_ERROR_CANCELLED,"Retained capture cancelled"));return;
    }
    goodix_send_protocol(dev,GOODIX_CMD_MCU_SWITCH_TO_FDT_MODE,
        (guint8 *)goodix_pr81_report,sizeof(goodix_pr81_report),NULL,TRUE,
        GOODIX_TIMEOUT,TRUE,retained_reply,ssm);
}
static void retained_complete(FpiSsm *ssm,FpDevice *dev,GError *error) {
    if (error) FPI_DEVICE_GOODIXTLS52XD(dev)->retained_ready=FALSE;
    fpi_image_device_activate_complete(FP_IMAGE_DEVICE(dev),error);
}

static void pr81_contact_reply(FpDevice* dev, guint8* data, guint16 length,
                               gpointer user_data, GError* error)
{
    FpiSsm* ssm = user_data;
    FpiDeviceGoodixTls52XD* self = FPI_DEVICE_GOODIXTLS52XD(dev);
    if (error) {
        fpi_ssm_mark_failed(ssm, error);
        return;
    }
    if (g_cancellable_is_cancelled(fpi_device_get_cancellable(dev))) {
        fpi_ssm_mark_failed(ssm, g_error_new_literal(G_IO_ERROR,
            G_IO_ERROR_CANCELLED, "Contact wait cancelled"));
        return;
    }
    if (self->skip_background_images &&
        !goodix_cache_deadline_valid(self->cache_deadline_us)) {
        fpi_ssm_mark_failed(ssm, g_error_new_literal(FP_DEVICE_ERROR,
            FP_DEVICE_ERROR_GENERAL, "Cached background expired; refresh with sensor clear"));
        return;
    }
    guint16 regions[8];
    if (!goodix_pr81_read_regions(data, length, regions)) {
        fpi_ssm_mark_failed(ssm, g_error_new(FP_DEVICE_ERROR,
            FP_DEVICE_ERROR_DATA_INVALID, "PR81 contact report is short: %u bytes", length));
        return;
    }
    guint changed = goodix_pr81_contact_regions(self->contact_baseline, regions);
    fp_dbg("PR81 contact: %u/8 regions dropped >20; values: %u %u %u %u %u %u %u %u",
           changed, regions[0], regions[1], regions[2], regions[3],
           regions[4], regions[5], regions[6], regions[7]);
    if (changed >= 4) {
        if (self->contact_capture) {
            g_print("CONTACT_CAPTURE_CONFIRMED\n");fflush(stdout);
        }
        fpi_ssm_next_state(ssm);
        return;
    }
    if (g_get_monotonic_time() >= self->contact_deadline) {
        fpi_ssm_mark_failed(ssm, g_error_new(FP_DEVICE_ERROR, FP_DEVICE_ERROR_GENERAL,
            "PR81 contact detection timed out: only %u/8 regions changed; keep the sensor clear during calibration",
            changed));
        return;
    }
    self->timed_wait_ssm = ssm;
    fpi_ssm_jump_to_state_delayed(ssm, SCAN_STAGE_SWITCH_TO_FDT_MODE, 200);
}

static void scan_get_img(FpDevice* dev, FpiSsm* ssm)
{
    FpImageDevice* img_dev = FP_IMAGE_DEVICE(dev);
    FpiDeviceGoodixTls52XD* self = FPI_DEVICE_GOODIXTLS52XD(img_dev);
    if (self->pr81_capture) {
        goodix_tls_read_image(dev, (guint8*) goodix_pr81_image,
                               sizeof(goodix_pr81_image), scan_on_read_img, ssm);
        return;
    }
    guint8 payload[] = {0x43, 0x03, self->otp[26] + 6, 0x00, self->otp[26], 0x00, self->otp[45] + 6, 0x00, self->otp[45], 0x00};
    goodix_tls_read_image(dev, payload, sizeof(payload), scan_on_read_img, ssm);
}

const guint8 fdt_switch_state_mode_52xd[] = {
    0x0d, 0x01, 0x27, 0x01, 0x21, 0x01, 0x27,
    0x01, 0x23, 0x01, 0x8d, 0x8d, 0x86, 0x86,
    0x97, 0x97, 0x8f, 0x8f, 0x9b, 0x9b, 0x92,
    0x92, 0x96, 0x96, 0x8c, 0x8c, 0x01
};

guint8 fdt_switch_state_down_52xd[] = {
    0x9c, 0x01, 0x27, 0x01, 0x21, 0x01, 0x21,
    0x01, 0x23, 0x01, 0x8d, 0x8d, 0x86, 0x86,
    0x97, 0x97, 0x8f, 0x8f, 0x9b, 0x9b, 0x92,
    0x92, 0x96, 0x96, 0x8c, 0x8c, 0x00, 0x00,
    0x05, 0x03, 0xa7, 0x00, 0xa1, 0x00, 0xa7,
    0x00, 0xa3, 0x00, 0x00
};

static void timed_mode_ready(FpDevice* dev, guint8* data, guint16 len,
                              gpointer ssm, GError* error)
{
    if (error) {
        fpi_ssm_mark_failed(ssm, error);
        return;
    }

    FpiDeviceGoodixTls52XD *self = FPI_DEVICE_GOODIXTLS52XD(dev);
    if (self->pr81_full_capture && self->contact_capture) {
        if (g_cancellable_is_cancelled(fpi_device_get_cancellable(dev))) {
            fpi_ssm_mark_failed(ssm, g_error_new_literal(G_IO_ERROR,
                G_IO_ERROR_CANCELLED, "Contact wait cancelled"));
            return;
        }
        /* Calibration is complete. Poll for confirmed contact immediately;
         * no image can be requested until pr81_contact_reply advances. */
        self->timed_wait_ssm = NULL;
        g_print("CONTACT_CAPTURE_READY\n");fflush(stdout);
        fpi_ssm_next_state(ssm);
        return;
    }
    fp_dbg("Timed capture: image burst starts in 3 seconds");
    self->timed_wait_ssm = ssm;
    fpi_ssm_next_state_delayed(ssm, 3000);
}

static void scan_run_state(FpiSsm* ssm, FpDevice* dev)
{
    FpImageDevice* img_dev = FP_IMAGE_DEVICE(dev);
    FpiDeviceGoodixTls52XD* self = FPI_DEVICE_GOODIXTLS52XD(img_dev);

    switch (fpi_ssm_get_cur_state(ssm)) {

    case SCAN_STAGE_SWITCH_TO_FDT_MODE:
        if (self->pr81_full_capture) {
            self->timed_wait_ssm = NULL;
            goodix_send_protocol(dev, GOODIX_CMD_MCU_SWITCH_TO_FDT_MODE,
                (guint8*) goodix_pr81_report, sizeof(goodix_pr81_report),
                NULL, TRUE, GOODIX_TIMEOUT, TRUE, pr81_contact_reply, ssm);
            break;
        }
        if (self->timed_capture) {
            /* Configure imaging without waiting for a finger interrupt. */
            guint8 mode[sizeof(fdt_switch_state_mode_52xd)];
            memcpy(mode, fdt_switch_state_mode_52xd, sizeof(mode));
            mode[sizeof(mode) - 1] = 0x00;
            goodix_send_mcu_switch_to_fdt_mode(dev, mode, sizeof(mode),
                                               FALSE, NULL, timed_mode_ready, ssm);
            break;
        }
        goodix_send_mcu_switch_to_fdt_mode(dev, (guint8*) fdt_switch_state_mode_52xd,
                                           sizeof(fdt_switch_state_mode_52xd), FALSE, NULL,
                                           check_none_cmd, ssm);
        break;

    case SCAN_STAGE_SWITCH_TO_FDT_DOWN:
        if (self->pr81_full_capture) {
            self->contact_deadline = g_get_monotonic_time() + 18 * G_USEC_PER_SEC;
            timed_mode_ready(dev, NULL, 0, ssm, NULL);
            break;
        }
        if (self->timed_capture) {
            fpi_ssm_next_state(ssm);
            break;
        }
        // FDT Down Cali
        fdt_switch_state_down_52xd[2] = self->otp[33];
        fdt_switch_state_down_52xd[4] = self->otp[41];
        fdt_switch_state_down_52xd[6] = self->otp[42];
        fdt_switch_state_down_52xd[8] = self->otp[43];

        // Image Cali
        fdt_switch_state_down_52xd[32] = self->otp[26];
        fdt_switch_state_down_52xd[36] = self->otp[45];
        fdt_switch_state_down_52xd[30] = fdt_switch_state_down_52xd[32] + 6;
        fdt_switch_state_down_52xd[34] = fdt_switch_state_down_52xd[36] + 6;



        fdt_switch_state_down_52xd[26] = 0x00;

        goodix_send_mcu_switch_to_fdt_down(dev, (guint8*) fdt_switch_state_down_52xd,
                                           sizeof(fdt_switch_state_down_52xd), FALSE, NULL,
                                           receive_fdt_down_ack, ssm);
        break;
    case SCAN_STAGE_FULL_MODE_CONFIG:
    case SCAN_STAGE_FULL_MODE_REPORT:
        if (self->pr81_full_capture) {
            guint8 mode[sizeof(goodix_pr81_full_mode)];
            memcpy(mode, goodix_pr81_full_mode, sizeof(mode));
            gboolean report = fpi_ssm_get_cur_state(ssm) == SCAN_STAGE_FULL_MODE_REPORT;
            mode[sizeof(mode) - 1] = report ? 1 : 0;
            goodix_send_protocol(dev, GOODIX_CMD_MCU_SWITCH_TO_FDT_MODE,
                mode, sizeof(mode), NULL, TRUE, GOODIX_TIMEOUT, report,
                check_none_cmd, ssm);
        }
        else
            fpi_ssm_next_state(ssm);
        break;
    case SCAN_STAGE_REGISTER_STOP:
        self->timed_wait_ssm = NULL;
        /* Upstream starts directly after mode setup; stop only between frames. */
        if (self->pr81_capture &&
            (!self->pr81_full_capture || g_slist_length(self->frames) > 1))
            goodix_send_write_sensor_register(dev, GOODIX_PR81_REGISTER,
                                               GOODIX_PR81_STOP, check_none, ssm);
        else
            fpi_ssm_next_state(ssm);
        break;
    case SCAN_STAGE_GET_IMG:
        self->timed_wait_ssm = NULL;
        fpi_image_device_report_finger_status(img_dev, TRUE);
        guint16 payload = self->pr81_capture ? GOODIX_PR81_START : 0x0005;
        goodix_send_write_sensor_register(dev, 556, payload, write_sensor_complete, ssm);
        break;
    }
}

static void receive_fdt_down_ack(FpDevice* dev, guint8* data, guint16 len,
                           gpointer ssm, GError* err)
{
    if (err) {
        fpi_ssm_mark_failed(ssm, err);
        return;
    }

    fdt_switch_state_down_52xd[26] = 0x01;
    goodix_send_mcu_switch_to_fdt_down(dev, (guint8*) fdt_switch_state_down_52xd,
                                        sizeof(fdt_switch_state_down_52xd), TRUE, NULL,
                                        check_none_cmd, ssm);
}

static void write_sensor_complete(FpDevice *dev, gpointer user_data, GError *error) 
{
    if (error) {
        fp_err("failed to scan: %s (code: %d)", error->message, error->code);
        fpi_ssm_mark_failed(user_data, error);
        return;
    }
    scan_get_img(dev, user_data);
}

static void scan_complete(FpiSsm* ssm, FpDevice* dev, GError* error)
{
    FPI_DEVICE_GOODIXTLS52XD(dev)->timed_wait_ssm = NULL;
    if (error) {
        fp_err("failed to scan: %s (code: %d)", error->message, error->code);
        FPI_DEVICE_GOODIXTLS52XD(dev)->retained_ready=FALSE;
        fpi_image_device_session_error(FP_IMAGE_DEVICE(dev), error);
        return;
    }
    fp_dbg("finished scan");
}

static void scan_start(FpiDeviceGoodixTls52XD* dev)
{
    fpi_ssm_start(fpi_ssm_new(FP_DEVICE(dev), scan_run_state, SCAN_STAGE_NUM),
                  scan_complete);
}

// ---- SCAN SECTION END ----

// ---- DEV SECTION START ----

static gboolean load_session_background(FpiDeviceGoodixTls52XD *self,
                                        const gchar *path, const gchar *baseline)
{
    if (!self->clear_guard || !baseline) return FALSE;
    g_auto(GStrv) fields = g_strsplit(baseline, ",", -1);
    if (g_strv_length(fields) != 8) return FALSE;
    for (guint i=0;i<8;i++) {
        guint64 value;
        if (!g_ascii_string_to_unsigned(fields[i],10,0,65535,&value,NULL)) return FALSE;
        self->reused_baseline[i]=(guint16)value;
    }
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if (fd<0) return FALSE;
    struct stat info;
    gboolean valid=fstat(fd,&info)==0 && S_ISREG(info.st_mode) &&
        info.st_uid==geteuid() && !(info.st_mode&0022) &&
        info.st_size==GOODIX52XD_FRAME_SIZE*2;
    guint8 data[GOODIX52XD_FRAME_SIZE*2];
    gsize position=0;
    while (valid && position<sizeof(data)) {
        ssize_t length=read(fd,data+position,sizeof(data)-position);
        if (length<=0) valid=FALSE;
        else position+=(gsize)length;
    }
    close(fd);
    if (!valid) return FALSE;
    for (guint i=0;i<GOODIX52XD_FRAME_SIZE;i++) {
        guint16 value=data[2*i]|((guint16)data[2*i+1]<<8);
        if (value>4095) return FALSE;
        self->reused_background[i]=value;
    }
    return TRUE;
}

static void dev_init(FpImageDevice *img_dev) {
  FpDevice *dev = FP_DEVICE(img_dev);
  GError *error = NULL;
  FPI_DEVICE_GOODIXTLS52XD(img_dev)->timed_capture =
      g_strcmp0(g_getenv("GOODIX521D_TIMED_CAPTURE"), "1") == 0;
  FPI_DEVICE_GOODIXTLS52XD(img_dev)->pr81_capture =
      g_strcmp0(g_getenv("GOODIX521D_CAPTURE_PROFILE"), "pr81") == 0;
  FPI_DEVICE_GOODIXTLS52XD(img_dev)->pr81_full_capture =
      g_strcmp0(g_getenv("GOODIX521D_CAPTURE_PROFILE"), "pr81-full") == 0;
  FPI_DEVICE_GOODIXTLS52XD(img_dev)->contact_probe =
      FPI_DEVICE_GOODIXTLS52XD(img_dev)->pr81_full_capture &&
      g_strcmp0(g_getenv("GOODIX521D_CONTACT_PROBE"), "1") == 0;
  FpiDeviceGoodixTls52XD *self = FPI_DEVICE_GOODIXTLS52XD(img_dev);
  self->retained_session=self->pr81_full_capture &&
      g_strcmp0(g_getenv("GOODIX521D_RETAIN_CAPTURE"),"1")==0;
  self->contact_capture = self->pr81_full_capture &&
      g_strcmp0(g_getenv("GOODIX521D_CONTACT_CAPTURE"), "1") == 0;
  self->clear_guard_invalid = FALSE;
  self->identity_probe = self->pr81_full_capture &&
      g_strcmp0(g_getenv("GOODIX521D_IDENTITY_PROBE"), "1") == 0;
  const gchar *reference = g_getenv("GOODIX521D_CLEAR_REFERENCE");
  self->clear_guard = self->pr81_full_capture && reference != NULL;
  if (self->clear_guard) {
      g_auto(GStrv) fields = g_strsplit(reference, ",", -1);
      self->clear_guard_invalid = g_strv_length(fields) != 8;
      for (guint i = 0; !self->clear_guard_invalid && i < 8; i++) {
          guint64 value;
          if (!g_ascii_string_to_unsigned(fields[i], 10, 0, 65535, &value, NULL))
              self->clear_guard_invalid = TRUE;
          else self->clear_reference[i] = (guint16)value;
      }
      g_free(self->clear_otp_hash);
      self->clear_otp_hash = g_strdup(g_getenv("GOODIX521D_CLEAR_OTP_SHA256"));
      if (!self->clear_otp_hash || strlen(self->clear_otp_hash) != 64)
          self->clear_guard_invalid = TRUE;
  }
  const gchar *session_background=g_getenv("GOODIX521D_SESSION_BACKGROUND");
  const gchar *post_reference=g_getenv("GOODIX521D_POST_CLEAR_REFERENCE");
  self->post_clear_guard=FALSE;
  if (post_reference) {
      g_auto(GStrv) fields=g_strsplit(post_reference,",",-1);
      if (!self->clear_guard || g_strv_length(fields)!=8) self->clear_guard_invalid=TRUE;
      else {
          self->post_clear_guard=TRUE;
          for (guint i=0;i<8;i++) {
              guint64 value;
              if (!g_ascii_string_to_unsigned(fields[i],10,0,65535,&value,NULL)) self->clear_guard_invalid=TRUE;
              else self->post_clear_reference[i]=(guint16)value;
          }
      }
  }
  self->reuse_background=FALSE;
  if (session_background) {
      self->reuse_background=load_session_background(self,session_background,g_getenv("GOODIX521D_SESSION_BASELINE"));
      if (!self->reuse_background) self->clear_guard_invalid=TRUE;
  }
  if (FPI_DEVICE_GOODIXTLS52XD(img_dev)->pr81_full_capture)
      FPI_DEVICE_GOODIXTLS52XD(img_dev)->pr81_capture = TRUE;
  if (FPI_DEVICE_GOODIXTLS52XD(img_dev)->pr81_capture)
      FPI_DEVICE_GOODIXTLS52XD(img_dev)->timed_capture = TRUE;
  FPI_DEVICE_GOODIXTLS52XD(img_dev)->save_frames =
      FPI_DEVICE_GOODIXTLS52XD(img_dev)->pr81_full_capture &&
      g_strcmp0(g_getenv("GOODIX521D_SAVE_FRAMES"), "1") == 0;

  self->skip_background_images =
      g_strcmp0(g_getenv("GOODIX521D_SKIP_BACKGROUND_IMAGES"), "1") == 0;
  if (self->skip_background_images &&
      !goodix_cache_options_valid(self->pr81_full_capture, self->contact_capture,
          self->clear_guard, self->reuse_background, self->save_frames,
          g_getenv("GOODIX521D_CACHE_DEADLINE_US"), &self->cache_deadline_us))
      self->clear_guard_invalid = TRUE;

  self->allow_held_contact =
      g_strcmp0(g_getenv("GOODIX521D_ALLOW_HELD_CONTACT"), "1") == 0;
  if (self->allow_held_contact &&
      (!self->skip_background_images || self->clear_guard_invalid ||
       !goodix_held_deadline_valid(self->cache_deadline_us)))
      self->clear_guard_invalid=TRUE;

  if (goodix_dev_init(dev, &error)) {
    fpi_image_device_open_complete(img_dev, error);
    return;
  }

  fpi_image_device_open_complete(img_dev, NULL);
}

static void dev_deinit(FpImageDevice *img_dev) {
  FpDevice *dev = FP_DEVICE(img_dev);
  GError *error = NULL;

  if (goodix_dev_deinit(dev, &error)) {
    fpi_image_device_close_complete(img_dev, error);
    return;
  }

  g_clear_pointer(&FPI_DEVICE_GOODIXTLS52XD(img_dev)->clear_otp_hash, g_free);

  fpi_image_device_close_complete(img_dev, NULL);
}

static void dev_activate(FpImageDevice *img_dev) {
    FpDevice* dev = FP_DEVICE(img_dev);

    if (FPI_DEVICE_GOODIXTLS52XD(img_dev)->pr81_full_capture) {
        FpiDeviceGoodixTls52XD *self = FPI_DEVICE_GOODIXTLS52XD(img_dev);
        if (self->clear_guard_invalid) {
            fpi_image_device_activate_complete(img_dev, g_error_new_literal(
                FP_DEVICE_ERROR, FP_DEVICE_ERROR_DATA_INVALID, "Invalid local clear reference"));
            return;
        }
        if (self->retained_session) {
            self->contact_capture=g_strcmp0(g_getenv("GOODIX521D_CONTACT_CAPTURE"),"1")==0;
            self->diagnostic_burst=0;
            if (self->retained_ready &&
                g_strcmp0(g_getenv("GOODIX521D_RETAIN_FORCE_FRESH"),"1")!=0 &&
                goodix_retained_valid(self->retained_mono,self->retained_boot,
                                      g_get_monotonic_time(),goodix_retained_boottime())) {
                self->early_kind=GOODIX_EARLY_UNKNOWN;self->early_streak=0;
                self->clear_wait_announced=FALSE;
                self->clear_deadline=g_get_monotonic_time()+12*G_USEC_PER_SEC;
                fpi_ssm_start(fpi_ssm_new(dev,retained_run,1),retained_complete);return;
            }
            GError *tls_error=NULL;
            self->retained_ready=FALSE;
            if (!goodix_shutdown_tls(dev,&tls_error)) {
                fpi_image_device_activate_complete(img_dev,tls_error);return;
            }
        }
        self->early_kind=GOODIX_EARLY_UNKNOWN;
        self->early_streak=0;
        self->contact_probe_sample = 0;
        self->clear_streak = 0;
        self->clear_wait_announced = FALSE;
        self->clear_deadline = g_get_monotonic_time() + 12 * G_USEC_PER_SEC;
        fp_dbg("PR81 calibration: keep the sensor clear until the finger request");
        goodix_start_read_loop(dev);
        fpi_ssm_start(fpi_ssm_new(dev, pr81_full_startup_run,
                                  G_N_ELEMENTS(goodix_pr81_full_startup)),
                      pr81_full_startup_complete);
        return;
    }

    fpi_ssm_start(fpi_ssm_new(dev, activate_run_state, ACTIVATE_NUM_STATES),
                  activate_complete);
}



static void dev_change_state(FpImageDevice* img_dev, FpiImageDeviceState state)
{
    FpiDeviceGoodixTls52XD* self = FPI_DEVICE_GOODIXTLS52XD(img_dev);
    G_DEBUG_HERE();

    if (state == FPI_IMAGE_DEVICE_STATE_AWAIT_FINGER_ON) {
        if (self->retained_session && g_strcmp0(g_getenv("GOODIX521D_CALIBRATION_ONLY"),"1")==0)
            return;
        scan_start(self);
    }
}

static void goodix52xd_reset_state(FpiDeviceGoodixTls52XD* self) {}

static void dev_deactivate(FpImageDevice *img_dev) {
    FpDevice* dev = FP_DEVICE(img_dev);
    FpiDeviceGoodixTls52XD* self = FPI_DEVICE_GOODIXTLS52XD(img_dev);
    if (self->timed_wait_ssm) {
        FpiSsm* waiting = self->timed_wait_ssm;
        self->timed_wait_ssm = NULL;
        fpi_ssm_cancel_delayed_state_change(waiting);
        fpi_ssm_mark_completed(waiting);
    }
    goodix_reset_state(dev);
    GError* error = NULL;
    if (!self->retained_session || !self->retained_ready)
        goodix_shutdown_tls(dev, &error);
    goodix52xd_reset_state(FPI_DEVICE_GOODIXTLS52XD(img_dev));
    fpi_image_device_deactivate_complete(img_dev, error);
}

// ---- DEV SECTION END ----

static void fpi_device_goodixtls52xd_init(FpiDeviceGoodixTls52XD* self)
{
    self->frames = g_slist_alloc();
}

static void fpi_device_goodixtls52xd_class_init(
    FpiDeviceGoodixTls52XDClass *class) {
  FpiDeviceGoodixTlsClass *gx_class = FPI_DEVICE_GOODIXTLS_CLASS(class);
  FpDeviceClass *dev_class = FP_DEVICE_CLASS(class);
  FpImageDeviceClass *img_dev_class = FP_IMAGE_DEVICE_CLASS(class);

  gx_class->interface = GOODIX_52XD_INTERFACE;
  gx_class->ep_in = GOODIX_52XD_EP_IN;
  gx_class->ep_out = GOODIX_52XD_EP_OUT;

  dev_class->id = "goodixtls52xd";
  dev_class->full_name = "Goodix TLS Fingerprint Sensor 52XD";
  dev_class->type = FP_DEVICE_TYPE_USB;
  dev_class->id_table = id_table;

  dev_class->scan_type = FP_SCAN_TYPE_PRESS;

  // TODO
  img_dev_class->bz3_threshold = 24;
  img_dev_class->img_width = GOODIX52XD_WIDTH;
  img_dev_class->img_height = GOODIX52XD_HEIGHT;

  img_dev_class->img_open = dev_init;
  img_dev_class->img_close = dev_deinit;
  img_dev_class->activate = dev_activate;
  img_dev_class->change_state = dev_change_state;
  img_dev_class->deactivate = dev_deactivate;

  fpi_device_class_auto_initialize_features(dev_class);
}
