// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <photo-egl.h>

#include "gstphotoallocator.h"

GST_DEBUG_CATEGORY_EXTERN(gst_photoplugin_debug);
#define GST_CAT_DEFAULT gst_photoplugin_debug

typedef struct {
    GstMemory mem;

    PhotoBuffer *buffer;
    int plane_idx;
} GstPhotoBufferMemory;

G_DEFINE_TYPE(GstPhotoAllocator, gst_photo_allocator, NEMO_GST_TYPE_EGL_IMAGE_ALLOCATOR);

static gpointer
gst_photo_buffer_memory_map(GstMemory *mem, gsize maxsize, GstMapFlags flags)
{
    GstPhotoBufferMemory *m = (GstPhotoBufferMemory *)mem;

    if (m->plane_idx == -1) {
        GST_ERROR("Cannot map opaque PhotoBuffer memory");
        return NULL;
    }

    if (flags & GST_MAP_WRITE) {
        GST_ERROR("Cannot write to PhotoBuffer memory");
        return NULL;
    }

    const PhotoBufferPlaneData *planes = photo_buffer_map(m->buffer);
    if (!planes) {
        GST_ERROR("Failed to map PhotoBuffer memory");
        return NULL;
    }

    return (gpointer)planes[m->plane_idx].data;
}

static void gst_photo_buffer_memory_unmap(GstMemory *mem G_GNUC_UNUSED)
{
    // no-op, buffer will stay mapped until released
}

static EGLImageKHR
gst_photo_buffer_create_image(GstMemory *mem, EGLDisplay dpy, EGLContext ctx)
{
    GstPhotoBufferMemory *m = (GstPhotoBufferMemory *)mem;

    return photo_buffer_create_egl_image(m->buffer, dpy, ctx);
}

static void
gst_photo_buffer_allocator_free(GstAllocator *gst_alloc G_GNUC_UNUSED, GstMemory *mem)
{
    GstPhotoBufferMemory *m = (GstPhotoBufferMemory *)mem;

    g_free(m);
}

static void gst_photo_allocator_init(GstPhotoAllocator *allocator)
{
    GstAllocator *gst_alloc = GST_ALLOCATOR(allocator);
    NemoGstEglImageAllocator *egl_alloc = NEMO_GST_EGL_IMAGE_ALLOCATOR(allocator);

    gst_alloc->mem_type = "photobuffer";
    gst_alloc->mem_map = gst_photo_buffer_memory_map;
    gst_alloc->mem_unmap = gst_photo_buffer_memory_unmap;
    gst_alloc->mem_share = NULL;
    gst_alloc->mem_is_span = NULL;

    egl_alloc->create_image = gst_photo_buffer_create_image;

    GST_OBJECT_FLAG_SET(allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

static void gst_photo_allocator_class_init(GstPhotoAllocatorClass *klass)
{
    GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS(klass);

    allocator_class->alloc = NULL;
    allocator_class->free = gst_photo_buffer_allocator_free;
}

GstPhotoAllocator *gst_photo_allocator_new(void)
{
    return g_object_new(GST_TYPE_PHOTO_ALLOCATOR, NULL);
}

void gst_photo_allocator_setup_buffer(GstPhotoAllocator *allocator,
                                      GstBuffer *buffer, PhotoBuffer *pbuffer,
                                      GstVideoFormat format, int width, int height)
{
    const PhotoBufferInfo *info = photo_buffer_get_info(pbuffer);

    GstCustomMeta *meta = gst_buffer_add_custom_meta(buffer, "photo-buffer");
    if (meta) {
        GstStructure *s = gst_custom_meta_get_structure(meta);

        if (info->native_metadata) {
            gst_structure_set(s, "native-metadata", G_TYPE_POINTER,
                              info->native_metadata, NULL);
            gst_structure_set(s, "native-metadata-size", G_TYPE_UINT,
                              (guint)info->native_metadata_size, NULL);
        }
    }

    // Opaque buffers don't have any mappable planes, but they still need
    // at least one GstMemory for things like create_image() to work.
    if (info->num_planes == 0) {
        GstPhotoBufferMemory *m = g_new0(GstPhotoBufferMemory, 1);

        m->buffer = pbuffer;
        m->plane_idx = -1;

        gst_memory_init(&m->mem, GST_MEMORY_FLAG_NO_SHARE,
                        GST_ALLOCATOR(allocator), NULL, 0, 0, 0, 0);

        gst_buffer_append_memory(buffer, &m->mem);

        return;
    }

    gint strides[GST_VIDEO_MAX_PLANES];
    gsize offsets[GST_VIDEO_MAX_PLANES];
    gsize offset = 0;

    for (int i = 0; i < info->num_planes; i++) {
        GstPhotoBufferMemory *m = g_new0(GstPhotoBufferMemory, 1);

        m->buffer = pbuffer;
        m->plane_idx = i;

        size_t size = info->planes[i].max_length;
        strides[i] = info->planes[i].stride;
        offsets[i] = offset;

        gst_memory_init(&m->mem, GST_MEMORY_FLAG_NO_SHARE,
                        GST_ALLOCATOR(allocator), NULL,
                        size, 0, 0, size);

        gst_buffer_append_memory(buffer, &m->mem);

        offset += size;
    }

    gst_buffer_add_video_meta_full(buffer, GST_VIDEO_FRAME_FLAG_NONE,
                                   format, width, height,
                                   info->num_planes, offsets, strides);
}

PhotoBuffer *gst_photo_buffer_memory_get_photo_buffer(GstMemory *mem)
{
    GstPhotoBufferMemory *m = (GstPhotoBufferMemory *)mem;

    return m->buffer;
}
