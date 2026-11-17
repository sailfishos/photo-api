/*
 * SPDX-FileCopyrightText: 2014 Mohammed Sameer
 * SPDX-FileCopyrightText: 2015-2021 Jolla Ltd.
 * SPDX-FileCopyrightText: 2010 Texas Instruments, Inc
 * SPDX-FileCopyrightText: 2021-2022 Open Mobile Platform LLC.
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GST_PHOTO_PLUGIN_STREAMSRC_H__
#define __GST_PHOTO_PLUGIN_STREAMSRC_H__

#include <gst/gst.h>

#include <photo-api.h>

#include "gstphotoallocator.h"
#include "gstphotosrcpad.h"

G_BEGIN_DECLS

#define GST_TYPE_PHOTO_STREAM_SRC       (gst_photo_stream_src_get_type())
#define GST_PHOTO_STREAM_SRC(obj)       (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PHOTO_STREAM_SRC, GstPhotoStreamSrc))
#define GST_IS_PHOTO_STREAM_SRC(obj)    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PHOTO_STREAM_SRC))

typedef struct _GstPhotoStreamSrc GstPhotoStreamSrc;
typedef struct _GstPhotoStreamSrcClass GstPhotoStreamSrcClass;

struct _GstPhotoStreamSrc {
    GstElement parent;

    GstPhotoAllocator *allocator;
    GstPhotoSrcPad *pad;

    GMutex stream_lock;
    PhotoCamera *camera;

    GMutex capture_lock;
    gboolean running;
};

struct _GstPhotoStreamSrcClass {
    GstElementClass parent_class;
};

GType gst_photo_stream_src_get_type(void);

G_END_DECLS

#endif
