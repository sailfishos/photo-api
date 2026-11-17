
/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_PRIVATE_H
#define PHOTO_PRIVATE_H

#include <pthread.h>

#include "photo-api.h"
#include "photo-backend.h"
#include "photo-controls.h"

static inline void unlocker_cleanup(pthread_mutex_t **ptr)
{
    if (*ptr) {
        pthread_mutex_unlock(*ptr);
    }
}

#define define_unlocker(mutex, var) \
    __attribute__((unused)) __attribute__((cleanup(unlocker_cleanup))) pthread_mutex_t *var = (mutex)

#define hold_lock(mutex, var) define_unlocker(mutex, var); pthread_mutex_lock(var)

#define hold_lock_or_return(mutex, var)                                             \
    define_unlocker(mutex, var);                                                    \
    do {                                                                            \
        if (pthread_mutex_lock(var)) {                                              \
            g_critical("%s called with lock held", __func__);                       \
            return;                                                                 \
        }                                                                           \
    } while(0)

#define hold_lock_or_return_val(mutex, var, ret)                                    \
    define_unlocker(mutex, var);                                                    \
    do {                                                                            \
        if (pthread_mutex_lock(var)) {                                              \
            g_critical("%s called with lock held", __func__);                       \
            return (ret);                                                           \
        }                                                                           \
    } while(0)

typedef enum _PhotoCameraState {
    PHOTO_CAMERA_STATE_INVALID,
    PHOTO_CAMERA_STATE_CLOSED,
    PHOTO_CAMERA_STATE_OPENED,
    PHOTO_CAMERA_STATE_RUNNING,
} PhotoCameraState;

#define PHOTO_BUFFER_MAX_PLANES 3

struct _PhotoBufferPrivate {
    struct _PhotoBuffer p;

    pthread_mutex_t lock;

    const PhotoBufferImpl *impl;
    void *userdata;

    PhotoBufferInfo info;
    PhotoBufferPlaneInfo plane_info[PHOTO_BUFFER_MAX_PLANES];
    PhotoBufferPlaneData plane_data[PHOTO_BUFFER_MAX_PLANES];

    bool mapped : 1;
    bool acquired : 1;
};

struct _PhotoStreamPrivate {
    struct _PhotoStream p;

    const PhotoStreamImpl *impl;
    const PhotoStreamCallbacks *callbacks;
    void *userdata;

    PhotoStreamConfig *config;

    bool active : 1;
};

typedef struct _PhotoControlListener {
    struct _PhotoControlListener *next;
    PhotoControlLockMask filter;
    PhotoControlLockCallback callback;
    void *userdata;
} PhotoControlListener;

struct _PhotoCameraPrivate {
    struct _PhotoCamera p;

    pthread_mutex_t lock;

    const PhotoCameraImpl *impl;
    const PhotoCameraCallbacks *callbacks;
    void *userdata;

    PhotoCamera *next;
    PhotoCameraState state;

    pthread_mutex_t precapture_lock;
    PhotoStream *still_capture_stream;
    bool still_capture_started;

    PhotoCameraInfo info;

    PhotoControlLockState old_lock_states[PHOTO_CONTROL_LOCK_ID_NUM];
    PhotoControlLockState lock_states[PHOTO_CONTROL_LOCK_ID_NUM];
    PhotoControlListener *lock_listeners;

    PhotoBitMask queried[PHOTO_BITMASK_SIZE(PHOTO_CONTROL_ID_NUM)];
    PhotoBitMask dirty[PHOTO_BITMASK_SIZE(PHOTO_CONTROL_ID_NUM)];
};

#endif
