/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_CONTROLS_H
#define PHOTO_CONTROLS_H

#include <photo-control-types.h>

/*
 * Internally, the PhotoControls are currently just an alias for the camera.
 * This is an implementation detail that may change in the future.
 * The API was split to allow access to controls in contexts where the camera
 * streams are managed separately, e.g. in the GStreamer plugin.
 */
#ifdef PHOTO_API_PRIVATE
typedef struct _PhotoCameraPrivate PhotoControls;
#else
typedef struct _PhotoControls PhotoControls;
#endif

typedef void (*PhotoControlLockCallback)(void *userdata, PhotoControlLockId lock_id,
                                         PhotoControlLockState old_state,
                                         PhotoControlLockState new_state);

#ifdef __cplusplus
extern "C" {
#endif

/* Generic controls */
PhotoControlType photo_control_get_type(PhotoControlId id);
const PhotoControlFloatRange *photo_control_query_float(PhotoControls *controls, PhotoControlId id);
const PhotoBitMask *photo_control_query_enum(PhotoControls *controls, PhotoControlId id);
bool photo_control_set_areas(PhotoControls *controls, PhotoControlId id,
                             int count, PhotoControlMeteringRect *rects);
bool photo_control_set_float(PhotoControls *controls, PhotoControlId id, float value);
bool photo_control_set_enum(PhotoControls *controls, PhotoControlId id, unsigned int value);

/* Precapture */
void photo_controls_commit(PhotoControls *controls);
void photo_controls_trigger(PhotoControls *controls, PhotoControlLockMask locks);
void photo_controls_force_lock(PhotoControls *controls, PhotoControlLockMask locks);
void photo_controls_unlock(PhotoControls *controls, PhotoControlLockMask locks);
PhotoControlLockState photo_controls_get_lock_state(PhotoControls *controls,
                                                    PhotoControlLockId lock_id);

void photo_controls_add_listener(PhotoControls *controls, PhotoControlLockCallback cb,
                                 void *userdata, PhotoControlLockMask filter);
void photo_controls_remove_listener(PhotoControls *controls, PhotoControlLockCallback cb,
                                    void *userdata);

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif
