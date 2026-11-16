/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_API_TYPES_H
#define PHOTO_API_TYPES_H

#include <stdbool.h>
#include <stddef.h>

typedef enum _PhotoStreamUsage {
    PHOTO_STREAM_USAGE_PREVIEW,
    PHOTO_STREAM_USAGE_STILL_CAPTURE,
    PHOTO_STREAM_USAGE_RECORDING,
    PHOTO_STREAM_USAGE_WEBCAM,
    PHOTO_STREAM_USAGE_RAW,
} PhotoStreamUsage;

typedef unsigned int PhotoStreamConfigFlags;
enum {
    PHOTO_STREAM_CONFIG_RECOMMENDED = (1 << 0),
};

typedef enum _PhotoCameraFacing {
    PHOTO_CAMERA_FACING_UNKNOWN,
    PHOTO_CAMERA_FACING_FRONT,
    PHOTO_CAMERA_FACING_BACK,
} PhotoCameraFacing;

typedef struct _PhotoConfig PhotoConfig;

#ifdef PHOTO_API_PRIVATE
typedef struct _PhotoBufferPrivate PhotoBuffer;
typedef struct _PhotoStreamPrivate PhotoStream;
typedef struct _PhotoCameraPrivate PhotoCamera;
#else
typedef struct _PhotoBuffer PhotoBuffer;
typedef struct _PhotoStream PhotoStream;
typedef struct _PhotoCamera PhotoCamera;
#endif

typedef struct _PhotoBufferPlaneInfo {
    size_t stride;
    size_t max_length;
} PhotoBufferPlaneInfo;

typedef struct _PhotoBufferInfo {
    unsigned int num_planes;
    PhotoBufferPlaneInfo *planes;
    void *native_metadata;
    size_t native_metadata_size;
} PhotoBufferInfo;

typedef struct _PhotoBufferPlaneData {
    const void *data;
    size_t length;
} PhotoBufferPlaneData;

typedef struct _PhotoConfigIntRange {
    int min;
    int max;
    int step;
} PhotoConfigIntRange;

typedef struct _PhotoAvailableConfig {
    unsigned int format;
    PhotoConfigIntRange width;
    PhotoConfigIntRange height;
    PhotoConfigIntRange min_fps;
    PhotoConfigIntRange max_fps;
    const char *native_format;
} PhotoAvailableConfig;

typedef void (*PhotoConfigIteratorCallback)(void *userdata, const PhotoAvailableConfig *info);

typedef struct _PhotoFixedConfig {
    unsigned int format;
    int width;
    int height;
    int min_fps;
    int max_fps;
    const char *native_format;
} PhotoFixedConfig;

typedef struct _PhotoCameraInfo {
    const char *id;
    PhotoCameraFacing facing;
    int orientation;
    int active_array_width;
    int active_array_height;
} PhotoCameraInfo;

#endif
