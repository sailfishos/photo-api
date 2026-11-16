// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <photo-api.h>

typedef struct _CaptureData {
    const char *filename;
    PhotoStreamUsage usage;
    pthread_mutex_t mutex;
    pthread_cond_t wait;
    bool capture_done : 1;
    bool capture_ok : 1;
} CaptureData;

static void shutter(void *userdata)
{
    fprintf(stderr, "Shutter!\n");
}

static void capture_done(void *userdata, bool successful)
{
    CaptureData *data = userdata;

    pthread_mutex_lock(&data->mutex);

    if (successful) {
        fprintf(stderr, "Capture done\n");
    } else {
        fprintf(stderr, "Capture failed\n");
    }

    data->capture_done = true;
    data->capture_ok = successful;
    pthread_cond_signal(&data->wait);
    pthread_mutex_unlock(&data->mutex);
}

static const PhotoCameraCallbacks camera_callbacks = {
    .shutter = shutter,
    .capture_done = capture_done,
};

static bool save_buffer_to_file(PhotoBuffer *buffer, const char *filename)
{
    const PhotoBufferInfo *info = photo_buffer_get_info(buffer);
    if (info->num_planes != 1) {
        fprintf(stderr, "Buffer has unsupported number of planes: %d\n", info->num_planes);
        photo_buffer_release(buffer);
        return false;
    }

    const PhotoBufferPlaneData *planes = photo_buffer_map(buffer);
    if (!planes) {
        fprintf(stderr, "Failed to map image buffer\n");
        photo_buffer_release(buffer);
        return false;
    }

    FILE *f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open '%s': %s\n", filename, strerror(errno));
        photo_buffer_release(buffer);
        return false;
    }

    fwrite(planes[0].data, 1, planes[0].length, f);
    fclose(f);

    photo_buffer_release(buffer);

    return true;
}

static bool do_capture(PhotoCamera *camera, CaptureData *data)
{
    PhotoStream *preview, *capture;

    preview = photo_camera_create_stream(camera, PHOTO_STREAM_USAGE_PREVIEW, NULL, NULL);
    if (!preview) {
        fprintf(stderr, "Failed to create preview stream\n");
        return false;
    }

    capture = photo_camera_create_stream(camera, data->usage, NULL, NULL);
    if (!capture) {
        fprintf(stderr, "Failed to create capture stream\n");
        photo_stream_destroy(preview);
        return false;
    }

    PhotoStreamConfig *stream_config = photo_stream_configure(capture, NULL);
    if (!stream_config) {
        fprintf(stderr, "Failed to configure capture stream\n");
        photo_stream_destroy(preview);
        photo_stream_destroy(capture);
        return false;
    }

    const PhotoFixedConfig *fixed = photo_stream_config_get_info(stream_config);

    if (fixed->format)
        printf("Format: 0x%08x (%c%c%c%c)\n", fixed->format,
               fixed->format & 0xff, (fixed->format >> 8) & 0xff,
               (fixed->format >> 16) & 0xff, fixed->format >> 24);
    else
        printf("Format: 0 (opaque)\n");

    printf("Size: %dx%d\n", fixed->width, fixed->height);

    photo_stream_config_apply(stream_config);

    PhotoStream *streams[2] = { preview, capture };

    if (!photo_camera_request_streams(camera, 2, streams)) {
        fprintf(stderr, "Failed to request still capture configuration\n");
        photo_stream_destroy(preview);
        photo_stream_destroy(capture);
        return false;
    }

    photo_stream_start(preview);

    photo_controls_trigger(photo_camera_get_controls(camera), PHOTO_CONTROL_LOCK_ALL);

    if (!photo_stream_take_picture(capture)) {
        fprintf(stderr, "Failed to take picture using capture stream\n");
        photo_camera_release_streams(camera);
        photo_stream_destroy(preview);
        photo_stream_destroy(capture);
        return false;
    }

    pthread_mutex_lock(&data->mutex);
    while (!data->capture_done) {
        pthread_cond_wait(&data->wait, &data->mutex);
    }
    bool ok = data->capture_ok;
    pthread_mutex_unlock(&data->mutex);

    if (ok) {
        float channel_gains[4];
        float color_transform[9];
        float lux_estimate;
        PhotoMetadataRequest meta_reqs[] = {
            { .id = PHOTO_CONTROL_CHANNEL_GAINS, .dest.f = channel_gains },
            { .id = PHOTO_CONTROL_COLOR_TRANSFORM, .dest.f = color_transform },
            { .id = PHOTO_CONTROL_LUX_ESTIMATE, .dest.f = &lux_estimate },
        };
        PhotoBuffer *buffer = photo_stream_get_next_buffer_with_metadata(capture,
                                sizeof(meta_reqs) / sizeof(meta_reqs[0]), meta_reqs);
        if (meta_reqs[0].out_valid) {
            printf("Channel gains (RGGB): [%f, %f, %f, %f]\n",
                   channel_gains[0], channel_gains[1],
                   channel_gains[2], channel_gains[3]);
        }
        if (meta_reqs[1].out_valid) {
            printf("Color transform (RGB): [%f, %f, %f,\n"
                   "                        %f, %f, %f,\n"
                   "                        %f, %f, %f]\n",
                   color_transform[0], color_transform[1], color_transform[2],
                   color_transform[3], color_transform[4], color_transform[5],
                   color_transform[6], color_transform[7], color_transform[8]);
        }
        if (meta_reqs[2].out_valid) {
            printf("Lux estimate: %f\n", lux_estimate);
        }
        if (buffer) {
            ok = save_buffer_to_file(buffer, data->filename);
        } else {
            fprintf(stderr, "No capture buffer returned\n");
            ok = false;
        }
    }

    photo_stream_stop(preview);

    photo_camera_release_streams(camera);
    photo_stream_destroy(preview);
    photo_stream_destroy(capture);

    return ok;
}

static bool parse_float(const char *str, float *val)
{
    char *end = NULL;

    *val = strtof(str, &end);

    return *end == '\0' && !isnan(*val);
}

static bool parse_flash_mode(const char *str, PhotoControlFlashMode *val)
{
    if (!strcmp(str, "auto")) {
        *val = PHOTO_AUTO_FLASH;
    } else if (!strcmp(str, "auto-redeye")) {
        *val = PHOTO_AUTO_FLASH_REDEYE;
    } else if (!strcmp(str, "on")) {
        *val = PHOTO_FLASH_ON;
    } else if (!strcmp(str, "off")) {
        *val = PHOTO_FLASH_OFF;
    } else if (!strcmp(str, "torch")) {
        *val = PHOTO_FLASH_TORCH;
    } else {
        return false;
    }

    return true;
}

static bool parse_focus_mode(const char *str, PhotoControlFocusMode *val)
{
    if (!strcmp(str, "auto")) {
        *val = PHOTO_AUTO_FOCUS;
    } else if (!strcmp(str, "continuous")) {
        *val = PHOTO_CONTINUOUS_AUTO_FOCUS;
    } else if (!strcmp(str, "manual")) {
        *val = PHOTO_MANUAL_FOCUS;
    } else {
        return false;
    }

    return true;
}

static bool parse_stream_usage(const char *str, PhotoStreamUsage *val)
{
    if (!strcmp(str, "preview")) {
        *val = PHOTO_STREAM_USAGE_PREVIEW;
    } else if (!strcmp(str, "still-capture")) {
        *val = PHOTO_STREAM_USAGE_STILL_CAPTURE;
    } else if (!strcmp(str, "recording")) {
        *val = PHOTO_STREAM_USAGE_RECORDING;
    } else if (!strcmp(str, "webcam")) {
        *val = PHOTO_STREAM_USAGE_WEBCAM;
    } else if (!strcmp(str, "raw")) {
        *val = PHOTO_STREAM_USAGE_RAW;
    } else {
        return false;
    }

    return true;
}

static void print_usage(void)
{
    fprintf(stderr,
            "Usage: camera-shoot [--ae-compensation <float>]\n"
            "                    [--flash-mode auto|auto-redeye|on|off|torch]\n"
            "                    [--focus-mode auto|continuous|manual]\n"
            "                    [--lens_focus <float>]\n"
            "                    [--zoom <float>]\n"
            "                    [--stream-usage preview|still-capture|recording|webcam|raw]\n"
            "                    <id> <output>\n");
}

int main(int argc, char *argv[])
{
    const char *camera_name = NULL;

    CaptureData data = {
        .usage = PHOTO_STREAM_USAGE_STILL_CAPTURE,
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .wait = PTHREAD_COND_INITIALIZER,
    };

    bool set_ae_compensation = false;
    float ae_compensation;

    bool set_flash_mode = false;
    PhotoControlFlashMode flash_mode;

    bool set_focus_mode = false;
    PhotoControlFocusMode focus_mode;

    bool set_lens_focus = false;
    float lens_focus;

    bool set_zoom = false;
    float zoom;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            bool valid = false;
            if (i + 1 < argc) {
                if (!strcmp(argv[i], "--ae-compensation")) {
                    set_ae_compensation = true;
                    valid = parse_float(argv[i + 1], &ae_compensation);
                } else if (!strcmp(argv[i], "--flash-mode")) {
                    set_flash_mode = true;
                    valid = parse_flash_mode(argv[i + 1], &flash_mode);
                } else if (!strcmp(argv[i], "--focus-mode")) {
                    set_focus_mode = true;
                    valid = parse_focus_mode(argv[i + 1], &focus_mode);
                } else if (!strcmp(argv[i], "--lens_focus")) {
                    set_lens_focus = true;
                    valid = parse_float(argv[i + 1], &lens_focus);
                } else if (!strcmp(argv[i], "--zoom")) {
                    set_zoom = true;
                    valid = parse_float(argv[i + 1], &zoom);
                } else if (!strcmp(argv[i], "--stream-usage")) {
                    valid = parse_stream_usage(argv[i + 1], &data.usage);
                }
            }
            if (valid) {
                i += 1;
            } else {
                print_usage();
                return EXIT_FAILURE;
            }
        } else if (!camera_name) {
            camera_name = argv[i];
        } else if (!data.filename) {
            data.filename = argv[i];
        } else {
            print_usage();
            return EXIT_FAILURE;
        }
    }

    if (!camera_name || !data.filename) {
        print_usage();
        return EXIT_FAILURE;
    }

    PhotoCamera *camera = photo_camera_open(camera_name, &camera_callbacks, &data);
    if (camera) {
        PhotoControls *ctrl = photo_camera_get_controls(camera);

        if (set_ae_compensation)
            photo_control_set_float(ctrl, PHOTO_CONTROL_AE_COMPENSATION, ae_compensation);
        if (set_flash_mode)
            photo_control_set_enum(ctrl, PHOTO_CONTROL_FLASH_MODE, flash_mode);
        if (set_focus_mode)
            photo_control_set_enum(ctrl, PHOTO_CONTROL_FOCUS_MODE, focus_mode);
        if (set_lens_focus)
            photo_control_set_float(ctrl, PHOTO_CONTROL_LENS_FOCUS, lens_focus);
        if (set_zoom)
            photo_control_set_float(ctrl, PHOTO_CONTROL_ZOOM, zoom);

        photo_controls_commit(ctrl);

        int ret = do_capture(camera, &data) ? EXIT_SUCCESS : EXIT_FAILURE;
        photo_camera_close(camera);
        return ret;
    } else {
        fprintf(stderr, "Failed to open camera\n");
    }

    return EXIT_FAILURE;
}
