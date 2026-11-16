// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include <glib.h>

#include "photo-egl.h"
#include "photo-private.h"

EGLImageKHR photo_buffer_create_egl_image(PhotoBuffer *buffer, EGLDisplay dpy, EGLContext ctx)
{
    g_return_val_if_fail(buffer, EGL_NO_IMAGE_KHR);
    hold_lock(&buffer->lock, lock);

    if (buffer->p.stream && buffer->impl->create_egl_image) {
        return buffer->impl->create_egl_image(buffer, dpy, ctx);
    }

    return EGL_NO_IMAGE_KHR;
}
