/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GST_PHOTO_PLUGIN_BUFFER_POOL_H__
#define __GST_PHOTO_PLUGIN_BUFFER_POOL_H__

#include <gst/gst.h>

#include <photo-api.h>

#include "gstphotoallocator.h"

G_BEGIN_DECLS

#define GST_TYPE_PHOTO_BUFFER_POOL      (gst_photo_buffer_pool_get_type())
#define GST_PHOTO_BUFFER_POOL(obj)      (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PHOTO_BUFFER_POOL, GstPhotoBufferPool))
#define GST_IS_PHOTO_BUFFER_POOL(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PHOTO_BUFFER_POOL))

typedef struct _GstPhotoBufferPool GstPhotoBufferPool;
typedef struct _GstPhotoBufferPoolClass GstPhotoBufferPoolClass;

struct _GstPhotoBufferPool {
    GstBufferPool parent;

    GstPhotoAllocator *allocator;

    GMutex lock;
    GPtrArray *bound_buffers;
    GPtrArray *acquired_buffers;

    GstVideoFormat format;
    int width;
    int height;
};

struct _GstPhotoBufferPoolClass {
    GstBufferPoolClass parent_class;
};

GType gst_photo_buffer_pool_get_type(void);

gboolean gst_photo_buffer_pool_bind_photo_buffer(GstPhotoBufferPool *pool,
                                                 PhotoBuffer *pbuffer);
GstBuffer *gst_photo_buffer_pool_acquire_photo_buffer(GstPhotoBufferPool *pool,
                                                      PhotoBuffer *pbuffer);
void gst_photo_buffer_pool_unbind_photo_buffer(GstPhotoBufferPool *pool,
                                               PhotoBuffer *pbuffer);

void gst_photo_buffer_pool_set_format_from_config(GstPhotoBufferPool *pool,
                                                  const PhotoFixedConfig *config);

GstPhotoBufferPool *gst_photo_buffer_pool_new(GstPhotoAllocator *allocator);

G_END_DECLS

#endif
