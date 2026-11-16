// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "gstphotostreamsrc.h"

GST_DEBUG_CATEGORY_EXTERN(gst_photoplugin_debug);
#define GST_CAT_DEFAULT gst_photoplugin_debug

#define gst_photo_stream_src_parent_class parent_class
G_DEFINE_TYPE(GstPhotoStreamSrc, gst_photo_stream_src, GST_TYPE_ELEMENT);

enum {
    PROP_0,
    PROP_PHOTO_CAMERA,
    PROP_PHOTO_STREAM,
};

static GstStaticPadTemplate any_src_template_factory =
GST_STATIC_PAD_TEMPLATE("src",
                        GST_PAD_SRC,
                        GST_PAD_ALWAYS,
                        GST_STATIC_CAPS_ANY);

static gboolean
gst_photo_stream_pad_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    GstPhotoStreamSrc *src = GST_PHOTO_STREAM_SRC(parent);
    gboolean ret = FALSE;

    GST_LOG_OBJECT(pad, "handle %" GST_PTR_FORMAT, event);

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_RECONFIGURE:
        g_mutex_lock(&src->capture_lock);
        if (src->running) {
            if (!gst_photo_src_pad_negotiate(src->pad)) {
                GST_ELEMENT_ERROR(src, STREAM, FORMAT,
                    ("Failed to negotiate stream."), (NULL));
            } else {
                ret = TRUE;
            }
        } else {
            // pad will negotiate later
            ret = TRUE;
        }
        g_mutex_unlock(&src->capture_lock);

    default:
        break;
    }

    return ret;
}

static void gst_photo_stream_src_init(GstPhotoStreamSrc *src)
{
    g_mutex_init(&src->stream_lock);
    g_mutex_init(&src->capture_lock);

    src->allocator = gst_photo_allocator_new();
    src->pad = gst_photo_src_pad_new(src->allocator, GST_ELEMENT(src), "src");
    src->pad->usage = PHOTO_STREAM_USAGE_RECORDING;
    gst_pad_set_event_function(src->pad->pad, gst_photo_stream_pad_event);

    GST_OBJECT_FLAG_SET(src, GST_ELEMENT_FLAG_SOURCE);
}

static void
gst_photo_stream_src_set_property(GObject *object, guint prop_id, const GValue *value,
                                  GParamSpec *pspec)
{
    GstPhotoStreamSrc *src = GST_PHOTO_STREAM_SRC(object);

    switch (prop_id) {
    case PROP_PHOTO_CAMERA:
        g_mutex_lock(&src->stream_lock);
        src->camera = g_value_get_pointer(value);
        g_mutex_unlock(&src->stream_lock);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gst_photo_stream_src_get_property(GObject *object, guint prop_id, GValue *value,
                                  GParamSpec *pspec)
{
    GstPhotoStreamSrc *src = GST_PHOTO_STREAM_SRC(object);

    switch (prop_id) {
    case PROP_PHOTO_CAMERA:
        g_value_set_pointer(value, src->camera);
        break;

    case PROP_PHOTO_STREAM:
        g_mutex_lock(&src->stream_lock);
        g_value_set_pointer(value, src->pad->stream);
        g_mutex_unlock(&src->stream_lock);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_photo_stream_src_finalize(GObject *object)
{
    GstPhotoStreamSrc *src = GST_PHOTO_STREAM_SRC(object);

    gst_photo_src_pad_destroy(src->pad);

    g_mutex_clear(&src->capture_lock);
    g_mutex_clear(&src->stream_lock);

    g_object_unref(src->allocator);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static GstStateChangeReturn
gst_photo_stream_src_change_state(GstElement *element, GstStateChange transition)
{
    GstPhotoStreamSrc *src = GST_PHOTO_STREAM_SRC(element);
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        g_mutex_lock(&src->stream_lock);
        gst_photo_src_pad_init_stream(src->pad, src->camera);
        if (!src->pad->stream) {
            g_mutex_unlock(&src->stream_lock);
            GST_ELEMENT_ERROR(src, STREAM, FORMAT,
                ("Failed to create camera stream."), (NULL));
            ret = GST_STATE_CHANGE_FAILURE;
            break;
        }
        g_mutex_unlock(&src->stream_lock);
        break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        g_mutex_lock(&src->capture_lock);
        src->running = TRUE;
        if (!gst_photo_src_pad_negotiate(src->pad)) {
            GST_ELEMENT_ERROR(src, STREAM, FORMAT,
                ("Failed to negotiate stream."), (NULL));
            ret = GST_STATE_CHANGE_FAILURE;
        }
        g_mutex_unlock(&src->capture_lock);
        break;

    default:
        break;
    }

    if (ret == GST_STATE_CHANGE_FAILURE) {
        return ret;
    }

    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        return ret;
    }

    switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        g_mutex_lock(&src->capture_lock);
        src->running = FALSE;
        g_mutex_unlock(&src->capture_lock);
        break;

    case GST_STATE_CHANGE_READY_TO_NULL:
        g_mutex_lock(&src->stream_lock);
        g_clear_pointer(&src->pad->stream, photo_stream_destroy);
        g_mutex_unlock(&src->stream_lock);
        break;

    default:
        break;
    }

    if (ret == GST_STATE_CHANGE_SUCCESS
            && (transition == GST_STATE_CHANGE_READY_TO_PAUSED
                || transition == GST_STATE_CHANGE_PLAYING_TO_PAUSED)) {
        ret = GST_STATE_CHANGE_NO_PREROLL;
    }

    return ret;
}

static void gst_photo_stream_src_class_init(GstPhotoStreamSrcClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_static_metadata(gstelement_class,
        "Camera stream source", "Source/Video/Device",
        "Stream source for applications that already use the photo-api directly",
        "Nikolai Burov <nikolai.burov@jolla.com>");

    gst_element_class_add_pad_template(gstelement_class,
        gst_static_pad_template_get(&any_src_template_factory));

    gobject_class->set_property = gst_photo_stream_src_set_property;
    gobject_class->get_property = gst_photo_stream_src_get_property;
    gobject_class->finalize = gst_photo_stream_src_finalize;

    gstelement_class->change_state = gst_photo_stream_src_change_state;

    g_object_class_install_property(gobject_class, PROP_PHOTO_CAMERA,
        g_param_spec_pointer("photo-camera", "Camera device object",
            "The PhotoCamera object on which the stream should be created",
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_PHOTO_STREAM,
        g_param_spec_pointer("photo-stream", "Stream object",
            "Provides a pointer to the configured PhotoStream. Use only with "
            "photo_camera_request_streams() and photo_stream_{start,stop}().",
            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

