/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_EGL_H
#define PHOTO_EGL_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <photo-api-types.h>

#ifdef __cplusplus
extern "C" {
#endif

EGLImageKHR photo_buffer_create_egl_image(PhotoBuffer *buffer, EGLDisplay dpy, EGLContext ctx);

#ifdef __cplusplus
}; /* extern "C" */
#endif

#endif
