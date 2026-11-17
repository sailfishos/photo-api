/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GST_PHOTO_PLUGIN_SRCPAD_H__
#define __GST_PHOTO_PLUGIN_SRCPAD_H__

#include <gst/gst.h>

#include <photo-api.h>

#include "gstphotobufferpool.h"

G_BEGIN_DECLS

typedef struct _GstPhotoSrcPad {
    GstPad *pad;
    GstPhotoBufferPool *pool;
    GCond stop_cond;
    GMutex lock;
    gboolean start_stream;
    gboolean start_segment;
    gboolean stopping;
    GList *pending_events;
    PhotoStream *stream;
    PhotoStreamUsage usage;
} GstPhotoSrcPad;

gboolean gst_photo_src_pad_negotiate(GstPhotoSrcPad *pad_data);
void gst_photo_src_pad_init_stream(GstPhotoSrcPad *pad_data, PhotoCamera *camera);

GstPhotoSrcPad *gst_photo_src_pad_new(GstPhotoAllocator *allocator,
                                      GstElement *parent,
                                      const gchar *name);
void gst_photo_src_pad_destroy(GstPhotoSrcPad *pad_data);

G_END_DECLS

#endif
