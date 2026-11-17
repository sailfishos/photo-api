/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GST_PHOTO_PLUGIN_BUFFER_ALLOCATOR_H__
#define __GST_PHOTO_PLUGIN_BUFFER_ALLOCATOR_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/interfaces/nemoeglimagememory.h>

#include <photo-api.h>

G_BEGIN_DECLS

#define GST_TYPE_PHOTO_ALLOCATOR    (gst_photo_allocator_get_type())
#define GST_PHOTO_ALLOCATOR(obj)    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_PHOTO_ALLOCATOR, GstPhotoAllocator))
#define GST_IS_PHOTO_ALLOCATOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_PHOTO_ALLOCATOR))

typedef struct _GstPhotoAllocator GstPhotoAllocator;
typedef struct _GstPhotoAllocatorClass GstPhotoAllocatorClass;

struct _GstPhotoAllocator {
    NemoGstEglImageAllocator parent;
};

struct _GstPhotoAllocatorClass {
    NemoGstEglImageAllocatorClass parent_class;
};

GType gst_photo_allocator_get_type(void);

GstPhotoAllocator *gst_photo_allocator_new(void);

void gst_photo_allocator_setup_buffer(GstPhotoAllocator *allocator,
                                      GstBuffer *buffer, PhotoBuffer *pbuffer,
                                      GstVideoFormat format, int width, int height);

PhotoBuffer *gst_photo_buffer_memory_get_photo_buffer(GstMemory *mem);

G_END_DECLS

#endif
