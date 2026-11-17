/*
 * SPDX-FileCopyrightText: 2014 Mohammed Sameer
 * SPDX-FileCopyrightText: 2015-2021 Jolla Ltd.
 * SPDX-FileCopyrightText: 2010 Texas Instruments, Inc
 * SPDX-FileCopyrightText: 2021-2022 Open Mobile Platform LLC.
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GST_PHOTO_PLUGIN_CAMERASRC_H__
#define __GST_PHOTO_PLUGIN_CAMERASRC_H__

#include <gst/gst.h>
#include <gst/basecamerabinsrc/gstcamerabin-enum.h>
#include <gst/basecamerabinsrc/gstbasecamerasrc.h>

#include <photo-api.h>

#include "gstphotoallocator.h"
#include "gstphotosrcpad.h"

G_BEGIN_DECLS

#define GST_TYPE_PHOTO_CAMERA_SRC       (gst_photo_camera_src_get_type())
#define GST_PHOTO_CAMERA_SRC(obj)       (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PHOTO_CAMERA_SRC, GstPhotoCameraSrc))
#define GST_IS_PHOTO_CAMERA_SRC(obj)    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PHOTO_CAMERA_SRC))

typedef struct _GstPhotoCameraSrc GstPhotoCameraSrc;
typedef struct _GstPhotoCameraSrcClass GstPhotoCameraSrcClass;

struct _GstPhotoCameraSrc {
    GstElement parent;

    GstPhotoAllocator *allocator;
    GstPhotoSrcPad *vfsrc;
    GstPhotoSrcPad *imgsrc;
    GstPhotoSrcPad *vidsrc;

    GMutex camera_lock;
    char *camera_name;
    PhotoCamera *camera;
    float zoom;

    GMutex capture_lock;
    GstCameraBinMode mode;
    gboolean running;
    gboolean capturing;

    gboolean post_preview;
    GstCaps *preview_caps;
    GstElement *preview_filter;
    GstCameraBinPreviewPipelineData *preview_pipeline;
};

struct _GstPhotoCameraSrcClass {
    GstElementClass parent_class;
};

GType gst_photo_camera_src_get_type(void);

G_END_DECLS

#endif
