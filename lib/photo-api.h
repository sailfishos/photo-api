/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_API_H
#define PHOTO_API_H

#include <photo-api-types.h>
#include <photo-controls.h>

typedef struct _PhotoStreamConfig PhotoStreamConfig;

typedef struct _PhotoStreamCallbacks {
    void (*buffer_created)(void *userdata, PhotoBuffer *buffer);
    void (*buffer_invalidated)(void *userdata, PhotoBuffer *buffer);
    void (*frame_available)(void *userdata);
    void (*stopped)(void *userdata);
} PhotoStreamCallbacks;

typedef struct _PhotoCameraCallbacks {
    void (*shutter)(void *userdata);
    void (*capture_done)(void *userdata, bool successful);
    void (*disconnected)(void *userdata);
} PhotoCameraCallbacks;

typedef void (*PhotoCameraIteratorCallback)(void *userdata, const PhotoCameraInfo *info);

#ifdef __cplusplus
extern "C" {
#endif

/* Camera access */
void photo_enumerate_cameras(PhotoCameraIteratorCallback cb, void *userdata);
PhotoCamera *photo_camera_open(const char *id, const PhotoCameraCallbacks *callbacks, void *userdata);
const PhotoCameraInfo *photo_camera_get_info(PhotoCamera *camera);
void photo_camera_close(PhotoCamera *camera);

PhotoControls *photo_camera_get_controls(PhotoCamera *camera);

/* Stream configuration template */
PhotoConfig *photo_config_new(void);

void photo_config_set_format(PhotoConfig *config, unsigned int fourcc);
void photo_config_set_size(PhotoConfig *config, int width, int height);
void photo_config_set_fps_range(PhotoConfig *config, int min_fps, int max_fps);

void photo_config_free(PhotoConfig *config);

/* Stream management */
PhotoStream *photo_camera_create_stream(PhotoCamera *camera, PhotoStreamUsage usage,
                                        const PhotoStreamCallbacks *callbacks, void *userdata);
void photo_stream_destroy(PhotoStream *stream);

bool photo_camera_request_streams(PhotoCamera *camera, int count, PhotoStream * const *streams);
void photo_camera_release_streams(PhotoCamera *camera);

void photo_stream_enumerate_configs(PhotoStream *stream, const PhotoConfig *filter,
                                    PhotoConfigIteratorCallback cb, void *userdata,
                                    PhotoStreamConfigFlags flags);

PhotoStreamConfig *photo_stream_configure(PhotoStream *stream, const PhotoConfig *config);
const PhotoFixedConfig *photo_stream_config_get_info(PhotoStreamConfig *config);
void photo_stream_config_apply(PhotoStreamConfig *config);
void photo_stream_config_discard(PhotoStreamConfig *config);

bool photo_stream_start(PhotoStream *stream);
void photo_stream_stop(PhotoStream *stream);

bool photo_stream_take_picture(PhotoStream *stream);

PhotoBuffer *photo_stream_get_next_buffer_with_metadata(PhotoStream *stream, int num_meta_reqs,
                                                        PhotoMetadataRequest *meta_reqs);
PhotoBuffer *photo_stream_get_last_buffer(PhotoStream *stream);

static inline PhotoBuffer *photo_stream_get_next_buffer(PhotoStream *stream)
{
    return photo_stream_get_next_buffer_with_metadata(stream, 0, NULL);
}

const PhotoBufferInfo *photo_buffer_get_info(PhotoBuffer *buffer);
void photo_buffer_set_userdata(PhotoBuffer *buffer, void *userdata);
void *photo_buffer_get_userdata(PhotoBuffer *buffer);

const PhotoBufferPlaneData *photo_buffer_map(PhotoBuffer *buffer);
void photo_buffer_release(PhotoBuffer *buffer);

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif
