// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "gstphotosrcpad.h"

GST_DEBUG_CATEGORY_EXTERN(gst_photoplugin_debug);
#define GST_CAT_DEFAULT gst_photoplugin_debug

static void gst_photo_src_pad_loop(gpointer user_data);

static void handle_photo_stream_buffer_created(void *userdata, PhotoBuffer *pbuffer)
{
    GstPhotoSrcPad *pad_data = userdata;

    if (!gst_photo_buffer_pool_bind_photo_buffer(pad_data->pool, pbuffer)) {
        photo_buffer_release(pbuffer);
    }
}

static void handle_photo_stream_buffer_invalidated(void *userdata, PhotoBuffer *pbuffer)
{
    GstPhotoSrcPad *pad_data = userdata;

    gst_photo_buffer_pool_unbind_photo_buffer(pad_data->pool, pbuffer);
}

static void handle_photo_stream_frame_available(void *userdata)
{
    GstPhotoSrcPad *pad_data = userdata;
    GstPad *pad = pad_data->pad;

    GST_LOG_OBJECT(pad, "frame available");

    // Resume the task
    if (gst_pad_get_task_state(pad) != GST_TASK_STOPPED) {
        gst_pad_start_task(pad, gst_photo_src_pad_loop, pad_data, NULL);
    }
}

static void handle_photo_stream_stopped(void *userdata)
{
    GstPhotoSrcPad *pad_data = userdata;
    GstPad *pad = pad_data->pad;

    GST_LOG_OBJECT(pad, "stream stopped");

    g_mutex_lock(&pad_data->lock);
    if (pad_data->stopping) {
        pad_data->stopping = FALSE;
        g_cond_signal(&pad_data->stop_cond);
    }

    // restart stream for next video
    pad_data->start_stream = TRUE;
    pad_data->start_segment = TRUE;

    GST_DEBUG_OBJECT(pad_data->pad, "pushing EOS");
    if (!gst_pad_push_event(pad_data->pad, gst_event_new_eos())) {
        GST_ERROR_OBJECT(pad_data->pad, "failed to push EOS event");
    }

    g_mutex_unlock(&pad_data->lock);
}

static const PhotoStreamCallbacks gst_photo_stream_callbacks = {
    .buffer_created = handle_photo_stream_buffer_created,
    .buffer_invalidated = handle_photo_stream_buffer_invalidated,
    .frame_available = handle_photo_stream_frame_available,
    .stopped = handle_photo_stream_stopped,
};

static void merge_available_config(void *userdata, const PhotoAvailableConfig *info)
{
    GstCaps **caps = userdata;
    bool opaque = false;
    GstStructure *s;

    if (info->format == GST_STR_FOURCC("JPEG")) {
        s = gst_structure_new_empty("image/jpeg");
    } else {
        GstVideoFormat fmt = gst_video_format_from_fourcc(info->format);
        if (fmt == GST_VIDEO_FORMAT_UNKNOWN) {
            if (!info->native_format) {
                return;
            }
            opaque = true;
            fmt = GST_VIDEO_FORMAT_ENCODED;
        }

        s = gst_structure_new("video/x-raw", "format", G_TYPE_STRING,
                              gst_video_format_to_string(fmt), NULL);
    }

    GValue val = G_VALUE_INIT;

    if (info->width.min == info->width.max) {
        gst_structure_set(s, "width", G_TYPE_INT, info->width.min, NULL);
    } else {
        gst_value_set_int_range_step(&val, info->width.min, info->width.max, info->width.step);
        gst_structure_set_value(s, "width", &val);
    }

    if (info->height.min == info->height.max) {
        gst_structure_set(s, "height", G_TYPE_INT, info->height.min, NULL);
    } else {
        gst_value_set_int_range_step(&val, info->height.min, info->height.max, info->height.step);
        gst_structure_set_value(s, "height", &val);
    }

    if (info->max_fps.min == info->max_fps.max) {
        gst_structure_set(s, "framerate", GST_TYPE_FRACTION, info->max_fps.max, 1, NULL);
    } else {
        gst_value_set_fraction_range_full(&val, info->max_fps.min, 1, info->max_fps.max, 1);
        gst_structure_set_value(s, "framerate", &val);
    }

    if (info->native_format) {
        GstCapsFeatures *feat = gst_caps_features_new_single("memory:PhotoBuffer");
        GstStructure *opaque_s = opaque ? s : gst_structure_copy(s);

        // TODO Existing buffer memory types supported by GStreamer should
        // be integrated at some point, especially if we ever get backends
        // that use DMA-bufs.
        gst_structure_set(opaque_s, "photo-native-format", G_TYPE_STRING,
                          info->native_format, NULL);

        *caps = gst_caps_merge_structure_full(*caps, opaque_s, feat);
    }

    if (!opaque) {
        *caps = gst_caps_merge_structure(*caps, s);
    }
}

static GstCaps *build_stream_caps(PhotoStream *stream)
{
    GstCaps *caps = gst_caps_new_empty();

    photo_stream_enumerate_configs(stream, NULL, merge_available_config, &caps,
                                   PHOTO_STREAM_CONFIG_RECOMMENDED);
    photo_stream_enumerate_configs(stream, NULL, merge_available_config, &caps, 0);

    return caps;
}

typedef struct {
    PhotoStreamUsage usage;
    GstCaps *caps;
    GstCaps *fixed_caps;
    unsigned int format;
    int width;
    int height;
    int size;
    int min_fps;
    int max_fps;
} GstPhotoConfigData;

static void find_best_config(void *userdata, const PhotoAvailableConfig *info)
{
    GstPhotoConfigData *data = userdata;
    GstCaps *caps, *avail_caps;

    avail_caps = gst_caps_new_empty();

    merge_available_config(&avail_caps, info);

    GST_LOG("avail caps %" GST_PTR_FORMAT, avail_caps);

    caps = gst_caps_intersect_full(data->caps, avail_caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref(avail_caps);

    caps = gst_caps_make_writable(caps);

    int best_idx = -1;

    for (int i = 0; i < gst_caps_get_size(caps); i++) {
        GstStructure *s = gst_caps_get_structure(caps, i);
        gint width, height;
        gint fps_n, fps_d;

        GST_LOG("candidate %" GST_PTR_FORMAT, s);

        gst_structure_fixate_field_nearest_int(s, "width", G_MAXINT);
        gst_structure_fixate_field_nearest_int(s, "height", G_MAXINT);
        gst_structure_fixate_field_nearest_fraction(s, "framerate", G_MAXINT, 1);
        gst_structure_get_int(s, "width", &width);
        gst_structure_get_int(s, "height", &height);
        gst_structure_get_fraction(s, "framerate", &fps_n, &fps_d);

        if (fps_d != 1) {
            GST_WARNING("Fractional framerate %d/%d not supported", fps_n, fps_d);
            continue;
        }

        int size = width * height;
        int min_fps = fps_n < info->min_fps.max ? fps_n : info->min_fps.max;
        // When multiple options are found, the order of preference depends on
        // the stream type:
        //  - Image: size, then max framerate, then min framerate
        //  - Video/viewfinder: max framerate, min framerate, size
        // This is intended as a reasonable default for compatibility purposes.
        bool is_better = data->usage == PHOTO_STREAM_USAGE_STILL_CAPTURE
            ? (size == data->size && fps_n == data->max_fps && min_fps > data->min_fps) ||
              (size == data->size && fps_n > data->max_fps) || size > data->size
            : (fps_n == data->max_fps && min_fps == data->min_fps && size > data->size) ||
              (fps_n == data->max_fps && min_fps > data->min_fps) || fps_n > data->max_fps;
        if (is_better) {
            data->format = info->format;
            data->width = width;
            data->height = height;
            data->size = size;
            data->min_fps = min_fps;
            data->max_fps = fps_n;
            best_idx = i;
        }
    }

    if (best_idx >= 0) {
        GstCapsFeatures *feat = gst_caps_get_features(caps, best_idx);
        if (feat) {
            // FIXME: we shouldn't need to copy here, but there is
            // currently no way to "steal" the features together with
            // the structure.
            feat = gst_caps_features_copy(feat);
        }

        GstStructure *s = gst_caps_steal_structure(caps, best_idx);

        if (data->fixed_caps) {
            gst_caps_unref(data->fixed_caps);
        }

        data->fixed_caps = gst_caps_new_full(s, NULL);
        gst_caps_set_features(data->fixed_caps, 0, feat);
    }

    gst_caps_unref(caps);
}

static void gst_photo_src_pad_send_stream_start(GstPad *pad)
{
    GstElement *src = GST_PAD_PARENT(pad);
    gchar *stream_id = gst_pad_create_stream_id(pad, src, GST_PAD_NAME(pad));

    gst_pad_push_event(pad, gst_event_new_stream_start(stream_id));

    g_free(stream_id);
}

gboolean gst_photo_src_pad_negotiate(GstPhotoSrcPad *pad_data)
{
    PhotoStream *stream = pad_data->stream;
    GstPad *pad = pad_data->pad;
    GstCaps *our_caps, *caps;

    our_caps = gst_pad_get_current_caps(pad);
    if (our_caps) {
        caps = gst_pad_peer_query_caps(pad, our_caps);
        if (!gst_caps_is_empty(caps)) {
            GST_DEBUG_OBJECT(pad, "already negotiated");
            return TRUE;
        }

        gst_caps_unref(caps);
        gst_caps_unref(our_caps);
    }

    GST_DEBUG_OBJECT(pad, "negotiate");

    caps = gst_pad_peer_query_caps(pad, NULL);
    GST_DEBUG_OBJECT(pad, "caps %" GST_PTR_FORMAT, caps);

    if (!caps || gst_caps_is_empty(caps)) {
        GST_ERROR_OBJECT(pad, "no caps found");
        gst_caps_unref(caps);
        return FALSE;
    }

    GstPhotoConfigData data = {
        .usage = pad_data->usage,
        .caps = caps,
        .fixed_caps = NULL,
        .format = 0,
        .width = 0,
        .height = 0,
        .size = 0,
        .min_fps = 0,
        .max_fps = 0,
    };

    photo_stream_enumerate_configs(stream, NULL, find_best_config, &data,
                                   PHOTO_STREAM_CONFIG_RECOMMENDED);
    if (!data.fixed_caps) {
        photo_stream_enumerate_configs(stream, NULL, find_best_config, &data, 0);
    }

    gst_caps_unref(caps);
    caps = data.fixed_caps;

    if (!caps) {
        GST_ERROR_OBJECT(pad, "no suitable configuration found");
        return FALSE;
    }

    GST_LOG_OBJECT(pad, "best caps %" GST_PTR_FORMAT, caps);

    PhotoConfig *config = photo_config_new();
    photo_config_set_format(config, data.format);
    photo_config_set_size(config, data.width, data.height);
    photo_config_set_fps_range(config, data.min_fps, data.max_fps);

    PhotoStreamConfig *stream_config = photo_stream_configure(stream, config);
    if (!stream_config) {
        GST_ERROR_OBJECT(pad, "camera rejected configuration");
        gst_caps_unref(caps);
        return FALSE;
    }

    // GStreamer requires stream start before caps,
    // so send it either here or in the pad task
    g_mutex_lock(&pad_data->lock);
    if (pad_data->start_stream) {
        gst_photo_src_pad_send_stream_start(pad);
        pad_data->start_stream = FALSE;
    }
    g_mutex_unlock(&pad_data->lock);

    if (!gst_pad_push_event(pad, gst_event_new_caps(caps))) {
        GST_ERROR_OBJECT(pad, "failed to push caps event");
    }

    gst_caps_unref(caps);

    gst_photo_buffer_pool_set_format_from_config(pad_data->pool,
            photo_stream_config_get_info(stream_config));

    photo_stream_config_apply(stream_config);

    return TRUE;
}

static void gst_photo_src_pad_loop(gpointer user_data)
{
    GstPhotoSrcPad *pad_data = user_data;
    GstPad *pad = pad_data->pad;
    GstElement *src = GST_PAD_PARENT(pad);

    g_mutex_lock(&pad_data->lock);
    if (pad_data->start_stream) {
        gst_photo_src_pad_send_stream_start(pad);
        pad_data->start_stream = FALSE;
    }
    g_mutex_unlock(&pad_data->lock);

    PhotoBuffer *pbuffer = photo_stream_get_next_buffer(pad_data->stream);
    if (!pbuffer) {
        gst_pad_pause_task(pad);
        return;
    }

    // TODO: the camera should be providing a timestamp, add API for that
    GstClockTime base_time, ts;

    GST_OBJECT_LOCK(src);
    base_time = src->base_time;
    ts = gst_clock_get_time(GST_ELEMENT_CLOCK(src)) - base_time;
    GST_OBJECT_UNLOCK(src);

    g_mutex_lock(&pad_data->lock);
    if (pad_data->stopping) {
        g_mutex_unlock(&pad_data->lock);

        GST_LOG_OBJECT(pad, "stream is stopping");
        photo_buffer_release(pbuffer);
        return;
    }

    if (pad_data->start_segment) {
        g_mutex_unlock(&pad_data->lock);

        GstSegment segment;
        gst_segment_init(&segment, GST_FORMAT_TIME);
        segment.start = ts;
        gst_pad_push_event(pad, gst_event_new_segment(&segment));

        g_mutex_lock(&pad_data->lock);
        pad_data->start_segment = FALSE;
    }

    GList *events = pad_data->pending_events;
    pad_data->pending_events = NULL;
    g_mutex_unlock(&pad_data->lock);

    if (G_UNLIKELY(events)) {
        for (GList *tmp = events; tmp; tmp = g_list_next(tmp)) {
            GstEvent *ev = tmp->data;
            GST_LOG_OBJECT(pad, "pushing pending %" GST_PTR_FORMAT, ev);
            gst_pad_push_event(pad, ev);
        }
        g_list_free(events);
    }

    GstBuffer *buffer = gst_photo_buffer_pool_acquire_photo_buffer(pad_data->pool, pbuffer);
    if (G_UNLIKELY(!buffer)) {
        GST_ERROR_OBJECT(pad, "unable to acquire a GstBuffer for a PhotoBuffer");
        photo_buffer_release(pbuffer);
        return;
    }

    GST_BUFFER_DTS(buffer) = ts;
    GST_BUFFER_PTS(buffer) = ts;

    GST_LOG_OBJECT(pad, "pushing buffer %p", buffer);

    GstFlowReturn ret = gst_pad_push(pad, buffer);
    if (G_UNLIKELY(ret != GST_FLOW_OK)) {
        if (ret == GST_FLOW_EOS) {
            gst_pad_push_event(pad, gst_event_new_eos());
        } else if (ret < GST_FLOW_OK && ret != GST_FLOW_FLUSHING) {
            GST_ELEMENT_ERROR(src, STREAM, FAILED,
                ("Internal data stream error."),
                ("stream stopped, reason %s", gst_flow_get_name(ret)));
        }
    }
}

static gboolean
gst_photo_src_pad_activate_mode(GstPad *pad, GstObject *parent, GstPadMode mode,
                                gboolean active)
{
    GstPhotoSrcPad *pad_data = gst_pad_get_element_private(pad);

    GST_DEBUG_OBJECT(pad, "activate: %d", active);

    if (mode != GST_PAD_MODE_PUSH) {
        GST_ERROR_OBJECT(pad, "only push mode is supported");
        return FALSE;
    }

    if (active) {
        // No need for locking here since the task is not running
        pad_data->start_stream = TRUE;
        pad_data->start_segment = TRUE;

        if (!gst_pad_start_task(pad, gst_photo_src_pad_loop, pad_data, NULL)) {
            GST_ERROR_OBJECT(pad, "failed to start pad task");
            return FALSE;
        }
    } else {
        if (!gst_pad_stop_task(pad)) {
            GST_ERROR_OBJECT(pad, "failed to stop pad task");
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
gst_photo_src_pad_query(GstPad *pad, GstObject *parent, GstQuery *query)
{
    GstPhotoSrcPad *pad_data = gst_pad_get_element_private(pad);
    gboolean ret = FALSE;

    GST_LOG_OBJECT(pad, "handle %" GST_PTR_FORMAT, query);

    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_SCHEDULING:
        gst_query_add_scheduling_mode(query, GST_PAD_MODE_PUSH);
        ret = TRUE;
        break;

    case GST_QUERY_FORMATS:
        gst_query_set_formats(query, 1, GST_FORMAT_TIME);
        ret = TRUE;
        break;

    case GST_QUERY_CAPS:
        if (pad_data->stream) {
            GstCaps *caps, *filter = NULL;

            caps = build_stream_caps(pad_data->stream);
            gst_query_parse_caps(query, &filter);
            if (filter) {
                GstCaps *intersection =
                    gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
                gst_caps_unref(caps);
                caps = intersection;
            }

            gst_query_set_caps_result(query, caps);
            ret = TRUE;
        }

        break;

    default:
        break;
    }

    return ret;
}

void gst_photo_src_pad_init_stream(GstPhotoSrcPad *pad_data, PhotoCamera *camera)
{
    pad_data->stream = photo_camera_create_stream(camera, pad_data->usage,
                                                  &gst_photo_stream_callbacks, pad_data);
}

GstPhotoSrcPad *
gst_photo_src_pad_new(GstPhotoAllocator *allocator, GstElement *parent, const gchar *name)
{
    GstPhotoSrcPad *pad_data = g_new0(GstPhotoSrcPad, 1);
    GstPadTemplate *tpl = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(parent), name);
    GstPad *pad = gst_pad_new_from_template(tpl, name);

    gst_pad_set_element_private(pad, pad_data);
    gst_pad_set_activatemode_function(pad, gst_photo_src_pad_activate_mode);
    gst_pad_set_query_function(pad, gst_photo_src_pad_query);

    pad_data->pool = gst_photo_buffer_pool_new(allocator);
    pad_data->pad = pad;
    g_mutex_init(&pad_data->lock);

    gst_element_add_pad(parent, pad);

    return pad_data;
}

void gst_photo_src_pad_destroy(GstPhotoSrcPad *pad_data)
{
    g_mutex_clear(&pad_data->lock);
    g_object_unref(pad_data->pool);
    g_free(pad_data);
}
