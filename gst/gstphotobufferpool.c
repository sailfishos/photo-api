// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "gstphotobufferpool.h"

GST_DEBUG_CATEGORY_EXTERN(gst_photoplugin_debug);
#define GST_CAT_DEFAULT gst_photoplugin_debug

#define gst_photo_buffer_pool_parent_class parent_class
G_DEFINE_TYPE(GstPhotoBufferPool, gst_photo_buffer_pool, GST_TYPE_BUFFER_POOL);

static void gst_photo_buffer_pool_init(GstPhotoBufferPool *pool)
{
    g_mutex_init(&pool->lock);
    pool->bound_buffers = g_ptr_array_new();
    pool->acquired_buffers = g_ptr_array_new();
}

static void gst_photo_buffer_pool_finalize(GObject *object)
{
    GstPhotoBufferPool *pool = GST_PHOTO_BUFFER_POOL(object);

    g_ptr_array_free(pool->bound_buffers, TRUE);
    g_ptr_array_free(pool->acquired_buffers, TRUE);
    g_mutex_clear(&pool->lock);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gst_photo_buffer_pool_release_buffer(GstBufferPool *gst_pool, GstBuffer *buffer)
{
    GstPhotoBufferPool *pool = GST_PHOTO_BUFFER_POOL(gst_pool);

    GST_LOG_OBJECT(pool, "release buffer %p", buffer);

    g_mutex_lock(&pool->lock);
    if (g_ptr_array_remove_fast(pool->acquired_buffers, buffer)) {
        g_ptr_array_add(pool->bound_buffers, buffer);
        buffer->pool = gst_object_ref(pool);
    }
    g_mutex_unlock(&pool->lock);

    GstMemory *mem = gst_buffer_peek_memory(buffer, 0);
    if (mem) {
        PhotoBuffer *pbuffer = gst_photo_buffer_memory_get_photo_buffer(mem);
        if (pbuffer) {
            GST_LOG_OBJECT(pool, "release PhotoBuffer %p", pbuffer);
            photo_buffer_release(pbuffer);
        }
    }

    if (!buffer->pool) {
        gst_buffer_unref(buffer);
    }
}

static void gst_photo_buffer_pool_class_init(GstPhotoBufferPoolClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBufferPoolClass *gstbufferpool_class = GST_BUFFER_POOL_CLASS(klass);

    gobject_class->finalize = gst_photo_buffer_pool_finalize;
    gstbufferpool_class->start = NULL;
    gstbufferpool_class->stop = NULL;
    gstbufferpool_class->set_config = NULL;
    gstbufferpool_class->acquire_buffer = NULL;
    gstbufferpool_class->reset_buffer = NULL;
    gstbufferpool_class->alloc_buffer = NULL;
    gstbufferpool_class->release_buffer = gst_photo_buffer_pool_release_buffer;
    gstbufferpool_class->free_buffer = NULL;
}

GstPhotoBufferPool *gst_photo_buffer_pool_new(GstPhotoAllocator *allocator)
{
    GstPhotoBufferPool *pool = GST_PHOTO_BUFFER_POOL(g_object_new(GST_TYPE_PHOTO_BUFFER_POOL, NULL));

    pool->allocator = allocator;

    return pool;
}

void gst_photo_buffer_pool_set_format_from_config(GstPhotoBufferPool *pool,
                                                  const PhotoFixedConfig *config)
{
    g_mutex_lock(&pool->lock);

    pool->format = gst_video_format_from_fourcc(config->format);
    if (pool->format == GST_VIDEO_FORMAT_UNKNOWN) {
        pool->format = GST_VIDEO_FORMAT_ENCODED;
    }

    pool->width = config->width;
    pool->height = config->height;

    g_mutex_unlock(&pool->lock);
}

gboolean gst_photo_buffer_pool_bind_photo_buffer(GstPhotoBufferPool *pool,
                                                 PhotoBuffer *pbuffer)
{
    GstBuffer *buffer = gst_buffer_new();

    GST_DEBUG_OBJECT(pool, "bind PhotoBuffer %p as buffer %p", pbuffer, buffer);

    g_mutex_lock(&pool->lock);

    gst_photo_allocator_setup_buffer(pool->allocator, buffer, pbuffer,
                                     pool->format, pool->width, pool->height);
    g_ptr_array_add(pool->bound_buffers, buffer);

    buffer->pool = gst_object_ref(pool);

    g_mutex_unlock(&pool->lock);

    photo_buffer_set_userdata(pbuffer, buffer);

    return TRUE;
}

GstBuffer *gst_photo_buffer_pool_acquire_photo_buffer(GstPhotoBufferPool *pool,
                                                      PhotoBuffer *pbuffer)
{
    GstBuffer *buffer = photo_buffer_get_userdata(pbuffer);

    g_mutex_lock(&pool->lock);

    if (buffer && g_ptr_array_remove_fast(pool->bound_buffers, buffer)) {
        g_ptr_array_add(pool->acquired_buffers, buffer);
    } else {
        buffer = NULL;
    }

    g_mutex_unlock(&pool->lock);

    return buffer;
}

void gst_photo_buffer_pool_unbind_photo_buffer(GstPhotoBufferPool *pool,
                                               PhotoBuffer *pbuffer)
{
    GstBuffer *buffer = photo_buffer_get_userdata(pbuffer);

    GST_DEBUG_OBJECT(pool, "unbind PhotoBuffer %p", pbuffer);

    if (buffer) {
        g_mutex_lock(&pool->lock);
        if (g_ptr_array_remove_fast(pool->bound_buffers, buffer)) {
            g_mutex_unlock(&pool->lock);

            // The buffer is already released, attempt to free it now
            gst_object_unref(buffer->pool);
            buffer->pool = NULL;
            gst_buffer_unref(buffer);
        } else {
            // The buffer will be released in release_buffer(), remove it
            // from the list to prevent it from being added back 
            g_ptr_array_remove_fast(pool->acquired_buffers, buffer);
            g_mutex_unlock(&pool->lock);
        }

        photo_buffer_set_userdata(pbuffer, NULL);
    }
}
