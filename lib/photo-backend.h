/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_BACKEND_H
#define PHOTO_BACKEND_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <photo-api-types.h>
#include <photo-control-types.h>

typedef struct _PhotoBackendBuffer {
    PhotoBuffer *photo_buffer;
} PhotoBackendBuffer;

#define PHOTO_BUFFER_LATEST_VERSION 1
typedef struct _PhotoBufferImpl {
    unsigned int version;

    bool (*map)(PhotoBuffer *buffer);
    void (*release)(PhotoBuffer *buffer);
    EGLImageKHR (*create_egl_image)(PhotoBuffer *buffer, EGLDisplay dpy, EGLContext ctx);
    void (*destroy)(PhotoBackendBuffer *buffer);
} PhotoBufferImpl;

struct _PhotoBuffer {
    PhotoBackendBuffer *backend;
    PhotoStream *stream;
    PhotoBufferInfo *info;
    PhotoBufferPlaneData *planes;
};

struct _PhotoConfig {
    unsigned int format;
    int width;
    int height;
    int min_fps;
    int max_fps;
};

struct _PhotoStreamConfig {
    PhotoStream *stream;
    PhotoFixedConfig info;
};

typedef struct _PhotoBackendStream {
    PhotoStream *photo_stream;
} PhotoBackendStream;

#define PHOTO_STREAM_LATEST_VERSION 1
typedef struct _PhotoStreamImpl {
    unsigned int version;

    void (*enumerate_configs)(PhotoStream *stream, const PhotoConfig *filter,
                              PhotoConfigIteratorCallback cb, void *userdata,
                              PhotoStreamConfigFlags flags, PhotoAvailableConfig *info);
    bool (*start)(PhotoStream *stream);
    void (*stop)(PhotoStream *stream);
    bool (*take_picture)(PhotoStream *stream);
    PhotoBuffer *(*get_next_buffer)(PhotoStream *stream, int num_meta_reqs,
                                    PhotoMetadataRequest *meta_reqs);
    void (*destroy)(PhotoBackendStream *stream);
} PhotoStreamImpl;

struct _PhotoStream {
    PhotoBackendStream *backend;
    PhotoCamera *camera;
    PhotoStreamUsage usage;
    PhotoFixedConfig *config;
    bool running;
};

typedef struct _PhotoControlMeteringAreas {
    int count;
    PhotoControlMeteringRect *rects;
} PhotoControlMeteringAreas;

typedef struct _PhotoBackendCamera {
    PhotoCamera *photo_camera;
} PhotoBackendCamera;

#define PHOTO_CAMERA_LATEST_VERSION 1
typedef struct _PhotoCameraImpl {
    unsigned int version;

    bool (*init)(PhotoCamera *camera);
    void (*destroy)(PhotoBackendCamera *camera);
    bool (*open)(PhotoCamera *camera);
    void (*close)(PhotoCamera *camera);
    void (*update_params)(PhotoCamera *camera);
    void (*trigger)(PhotoCamera *camera, PhotoControlLockMask locks);
    void (*force_lock)(PhotoCamera *camera, PhotoControlLockMask locks);
    void (*unlock)(PhotoCamera *camera, PhotoControlLockMask locks);
    PhotoStream *(*create_stream)(PhotoCamera *camera, PhotoStreamUsage usage);
    bool (*request_streams)(PhotoCamera *camera);
    void (*release_streams)(PhotoCamera *camera);

    unsigned int num_controls;
    void (**query_control)(PhotoCamera *camera);
} PhotoCameraImpl;

/*
 * Available controls are written by the backend during query. Control values
 * are read by the backend during update.
 * Changes to lock_states are done by the backend between begin_state_change()
 * and end_state_change() calls.
 */
struct _PhotoCamera {
    PhotoBackendCamera *backend;

    int num_streams;
    PhotoStream **streams;

    PhotoCameraInfo *info;
    PhotoBitMask *dirty;
    PhotoControlLockState *lock_states;

    union {
        struct {
#include "generated/ctl-data-fields.h"
        };
        unsigned char _controls[0];
    };
};

bool photo_backend_register_camera(PhotoBackendCamera *backend_camera,
                                   const char *id,
                                   const PhotoCameraImpl *impl);
bool photo_backend_init_stream(PhotoBackendCamera *backend_camera,
                               PhotoBackendStream *backend_stream,
                               PhotoStreamUsage usage, const PhotoStreamImpl *impl);
void photo_backend_begin_state_change(PhotoBackendCamera *backend_camera);
void photo_backend_end_state_change(PhotoBackendCamera *backend_camera, bool changed);
void photo_backend_notify_shutter(PhotoBackendCamera *backend_camera);
void photo_backend_notify_capture(PhotoBackendCamera *backend_camera, bool successful);
void photo_backend_notify_disconnected(PhotoBackendCamera *backend_camera);
void photo_backend_frame_available(PhotoBackendStream *backend_stream);
void photo_backend_stream_stopped(PhotoBackendStream *backend_stream);
bool photo_backend_init_buffer(PhotoBackendStream *backend_stream,
                               PhotoBackendBuffer *backend_buffer,
                               unsigned int num_planes, PhotoBufferPlaneInfo *planes,
                               const PhotoBufferImpl *impl);
void photo_backend_bind_buffer(PhotoBackendBuffer *backend_buffer);
void photo_backend_unbind_buffer(PhotoBackendBuffer *backend_buffer);

#endif
