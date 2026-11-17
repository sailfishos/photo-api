// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#define G_LOG_DOMAIN "photo-controls"

#include <math.h>
#include <glib.h>

#include "photo-private.h"

#include "generated/ctl-list.h"

/* Called with camera lock held */
static const void *query_control(PhotoCamera *camera, PhotoControlId id)
{
    if (!photo_check_bit(camera->queried, id)) {
        if (id < camera->impl->num_controls && camera->impl->query_control[id])
            camera->impl->query_control[id](camera);
        photo_set_bit(camera->queried, id);
    }

    return camera->p._controls + photo_controls[id].info_offset;
}

/* Called with camera lock held */
static void *acquire_control(PhotoCamera *camera, PhotoControlId id)
{
    photo_set_bit(camera->dirty, id);

    return camera->p._controls + photo_controls[id].value_offset;
}

PhotoControlType photo_control_get_type(PhotoControlId id)
{
    if (id >= PHOTO_CONTROL_ID_NUM)
        return PHOTO_CONTROL_TYPE_INVALID;

    return photo_controls[id].type;
}

const PhotoControlFloatRange *photo_control_query_float(PhotoCamera *camera, PhotoControlId id)
{
    g_return_val_if_fail(camera, NULL);
    g_return_val_if_fail(id < PHOTO_CONTROL_ID_NUM, NULL);
    g_return_val_if_fail(photo_controls[id].type == PHOTO_CONTROL_TYPE_FLOAT, false);
    g_return_val_if_fail(!photo_controls[id].read_only, false);
    hold_lock_or_return_val(&camera->lock, lock, NULL);
    g_return_val_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                         camera->state == PHOTO_CAMERA_STATE_RUNNING, NULL);

    return query_control(camera, id);
}

const PhotoBitMask *photo_control_query_enum(PhotoCamera *camera, PhotoControlId id)
{
    g_return_val_if_fail(camera, NULL);
    g_return_val_if_fail(id < PHOTO_CONTROL_ID_NUM, NULL);
    g_return_val_if_fail(photo_controls[id].type == PHOTO_CONTROL_TYPE_ENUM, false);
    g_return_val_if_fail(!photo_controls[id].read_only, false);
    hold_lock_or_return_val(&camera->lock, lock, NULL);
    g_return_val_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                         camera->state == PHOTO_CAMERA_STATE_RUNNING, NULL);

    return query_control(camera, id);
}

bool photo_control_set_areas(PhotoCamera *camera, PhotoControlId id,
                             int count, PhotoControlMeteringRect *rects)
{
    g_return_val_if_fail(camera, false);
    g_return_val_if_fail(id < PHOTO_CONTROL_ID_NUM, NULL);
    g_return_val_if_fail(photo_controls[id].type == PHOTO_CONTROL_TYPE_AREAS, false);
    g_return_val_if_fail(!photo_controls[id].read_only, false);
    g_return_val_if_fail(count == 0 || (count > 0 && rects), false);
    hold_lock_or_return_val(&camera->lock, lock, false);
    g_return_val_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                         camera->state == PHOTO_CAMERA_STATE_RUNNING, false);

    PhotoControlMeteringAreas *dest = acquire_control(camera, id);
    if (!dest)
        return false;

    if (dest->count != count) {
        PhotoControlMeteringRect *tmp = g_try_new(PhotoControlMeteringRect, count);
        if (count > 0 && !tmp) {
            return false;
        }

        g_free(dest->rects);
        dest->count = count;
        dest->rects = tmp;
    }

    if (count > 0) {
        memcpy(dest->rects, rects, count * sizeof(PhotoControlMeteringRect));
    }

    return true;
}

bool photo_control_set_float(PhotoCamera *camera, PhotoControlId id, float value)
{
    g_return_val_if_fail(camera, false);
    g_return_val_if_fail(id < PHOTO_CONTROL_ID_NUM, false);
    g_return_val_if_fail(photo_controls[id].type == PHOTO_CONTROL_TYPE_FLOAT, false);
    g_return_val_if_fail(!photo_controls[id].read_only, false);
    g_return_val_if_fail(!isnan(value), false);
    hold_lock_or_return_val(&camera->lock, lock, false);
    g_return_val_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                         camera->state == PHOTO_CAMERA_STATE_RUNNING, false);

    const PhotoControlFloatRange *range = query_control(camera, id);
    if (!range)
        return false;

    float *dest = acquire_control(camera, id);
    if (!dest)
        return false;

    if (value < range->min) {
        g_warning("%s=%f out of range, clamping to %f",
                  photo_controls[id].name, value, range->min);
        value = range->min;
    }

    if (value > range->max) {
        g_warning("%s=%f out of range, clamping to %f",
                  photo_controls[id].name, value, range->max);
        value = range->max;
    }

    *dest = value;

    return true;
}

bool photo_control_set_enum(PhotoCamera *camera, PhotoControlId id, unsigned int value)
{
    g_return_val_if_fail(camera, false);
    g_return_val_if_fail(id < PHOTO_CONTROL_ID_NUM, false);
    g_return_val_if_fail(photo_controls[id].type == PHOTO_CONTROL_TYPE_ENUM, false);
    g_return_val_if_fail(!photo_controls[id].read_only, false);
    hold_lock_or_return_val(&camera->lock, lock, false);
    g_return_val_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                         camera->state == PHOTO_CAMERA_STATE_RUNNING, false);

    if (value >= photo_controls[id].enum_count) {
        g_warning("%d is an invalid enum value for %s", value, photo_controls[id].name);
        return false;
    }

    const PhotoBitMask *supported = query_control(camera, id);

    if (!photo_check_bit(supported, value)) {
        g_warning("%s=%s not supported by camera", photo_controls[id].name,
                  photo_controls[id].enum_value_names[value]);
        return false;
    }

    unsigned int *dest = acquire_control(camera, id);
    if (!dest)
        return false;

    *dest = value;

    return true;
}

void photo_controls_commit(PhotoCamera *camera)
{
    g_return_if_fail(camera);
    hold_lock_or_return(&camera->lock, lock);
    g_return_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                     camera->state == PHOTO_CAMERA_STATE_RUNNING);

    camera->impl->update_params(camera);

    memset(&camera->dirty, 0, sizeof(camera->dirty));
}

void photo_controls_trigger(PhotoCamera *camera, PhotoControlLockMask locks)
{
    g_return_if_fail(camera);
    hold_lock_or_return(&camera->lock, lock);
    g_return_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                     camera->state == PHOTO_CAMERA_STATE_RUNNING);

    camera->impl->trigger(camera, locks);
}

void photo_controls_force_lock(PhotoCamera *camera, PhotoControlLockMask locks)
{
    g_return_if_fail(camera);
    hold_lock_or_return(&camera->lock, lock);
    g_return_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                     camera->state == PHOTO_CAMERA_STATE_RUNNING);

    camera->impl->force_lock(camera, locks);
}

void photo_controls_unlock(PhotoCamera *camera, PhotoControlLockMask locks)
{
    g_return_if_fail(camera);
    hold_lock_or_return(&camera->lock, lock);
    g_return_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                     camera->state == PHOTO_CAMERA_STATE_RUNNING);

    camera->impl->unlock(camera, locks);
}

PhotoControlLockState photo_controls_get_lock_state(PhotoCamera *camera, PhotoControlLockId lock_id)
{
    g_return_val_if_fail(camera, PHOTO_CONTROL_STATE_DISABLED);
    hold_lock_or_return_val(&camera->lock, lock, PHOTO_CONTROL_STATE_DISABLED);
    hold_lock_or_return_val(&camera->precapture_lock, precapture_lock,
                            PHOTO_CONTROL_STATE_DISABLED);
    g_return_val_if_fail(camera->state == PHOTO_CAMERA_STATE_OPENED ||
                         camera->state == PHOTO_CAMERA_STATE_RUNNING,
                         PHOTO_CONTROL_STATE_DISABLED);

    g_return_val_if_fail(lock_id < PHOTO_CONTROL_LOCK_ID_NUM,
                         PHOTO_CONTROL_STATE_DISABLED);

    return camera->lock_states[lock_id];
}

void photo_controls_add_listener(PhotoCamera *camera, PhotoControlLockCallback cb,
                                 void *userdata, PhotoControlLockMask filter)
{
    g_return_if_fail(camera);
    hold_lock_or_return(&camera->lock, lock);
    hold_lock_or_return(&camera->precapture_lock, precapture_lock);
    g_return_if_fail(cb);

    PhotoControlListener *l = g_new(PhotoControlListener, 1);

    l->next = camera->lock_listeners;
    l->filter = filter;
    l->callback = cb;
    l->userdata = userdata;

    camera->lock_listeners = l;
}

void photo_controls_remove_listener(PhotoCamera *camera, PhotoControlLockCallback cb,
                                    void *userdata)
{
    g_return_if_fail(camera);
    hold_lock_or_return(&camera->lock, lock);
    hold_lock_or_return(&camera->precapture_lock, precapture_lock);

    PhotoControlListener *l, **p = &camera->lock_listeners;

    while (*p) {
        l = *p;
        if (l->callback == cb && l->userdata == userdata) {
            *p = l->next;
            g_free(l);
            break;
        }
        p = &l->next;
    }
}

