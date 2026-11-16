// SPDX-FileCopyrightText: 2014 Mohammed Sameer
// SPDX-FileCopyrightText: 2015-2021 Jolla Ltd.
// SPDX-FileCopyrightText: 2010 Texas Instruments, Inc
// SPDX-FileCopyrightText: 2021-2022 Open Mobile Platform LLC.
// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "gstphotocamerasrc.h"

GST_DEBUG_CATEGORY_EXTERN(gst_photoplugin_debug);
#define GST_CAT_DEFAULT gst_photoplugin_debug

#define gst_photo_camera_src_parent_class parent_class
G_DEFINE_TYPE(GstPhotoCameraSrc, gst_photo_camera_src, GST_TYPE_ELEMENT);

enum {
    PROP_0,
    PROP_CAMERA_NAME,
    PROP_MODE,
    PROP_READY_FOR_CAPTURE,
    PROP_ZOOM,
    PROP_MAX_ZOOM,
    PROP_PHOTO_CONTROLS,

    /* camerabin interface */
    PROP_POST_PREVIEW,
    PROP_PREVIEW_CAPS,
    PROP_PREVIEW_FILTER
};

enum {
    START_CAPTURE_SIGNAL,
    STOP_CAPTURE_SIGNAL,
    LAST_SIGNAL
};

static guint photo_camera_src_signals[LAST_SIGNAL];

#define DEFAULT_MODE                   MODE_IMAGE
#define DEFAULT_MAX_ZOOM               10.0f
#define DEFAULT_POST_PREVIEW           FALSE

static GstStaticPadTemplate vf_src_template_factory =
GST_STATIC_PAD_TEMPLATE(GST_BASE_CAMERA_SRC_VIEWFINDER_PAD_NAME,
                        GST_PAD_SRC,
                        GST_PAD_ALWAYS,
                        GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate img_src_template_factory =
GST_STATIC_PAD_TEMPLATE(GST_BASE_CAMERA_SRC_IMAGE_PAD_NAME,
                        GST_PAD_SRC,
                        GST_PAD_ALWAYS,
                        GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate video_src_template_factory =
GST_STATIC_PAD_TEMPLATE(GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME,
                        GST_PAD_SRC,
                        GST_PAD_ALWAYS,
                        GST_STATIC_CAPS_ANY);

static void gst_photo_camera_src_post_message(GstPhotoCameraSrc *src, GstStructure *s)
{
    GstMessage *msg;

    GST_DEBUG_OBJECT(src, "sending message %" GST_PTR_FORMAT, s);

    msg = gst_message_new_element(GST_OBJECT(src), s);
    if (!gst_element_post_message(GST_ELEMENT(src), msg)) {
        GST_WARNING_OBJECT(src, "failed to send message");
    }
}

static void handle_photo_camera_shutter(void *userdata)
{
    GstPhotoCameraSrc *src = userdata;

    GstStructure *s = gst_structure_new_empty("photo-capture-start");
    gst_photo_camera_src_post_message(src, s);
}

static void handle_photo_camera_capture_done(void *userdata, bool successful)
{
    GstPhotoCameraSrc *src = userdata;

    GST_DEBUG_OBJECT(src, "status: %d", successful);

    g_mutex_lock(&src->capture_lock);
    if (src->mode == MODE_IMAGE) {
        src->capturing = FALSE;
    }
    g_mutex_unlock(&src->capture_lock);

    g_object_notify (G_OBJECT (src), "ready-for-capture");
}

static void handle_photo_camera_disconnected(void *userdata)
{
    GstPhotoCameraSrc *src = userdata;

    GST_INFO_OBJECT(src, "camera disconnected!");
}

static const PhotoCameraCallbacks gst_photo_camera_callbacks = {
    .shutter = handle_photo_camera_shutter,
    .capture_done = handle_photo_camera_capture_done,
    .disconnected = handle_photo_camera_disconnected,
};

static gboolean gst_photo_camera_pad_is_significant(GstPhotoCameraSrc *src, GstPad *pad)
{
    GstPhotoSrcPad *mode_pad = src->mode == MODE_IMAGE ? src->imgsrc : src->vidsrc;

    return src->running && (src->vfsrc->pad == pad || mode_pad->pad == pad);
}

// called with capture_lock
static gboolean gst_photo_camera_src_configure(GstPhotoCameraSrc *src)
{
    GstPhotoSrcPad *mode_pad = src->mode == MODE_IMAGE ? src->imgsrc : src->vidsrc;

    photo_camera_release_streams(src->camera);

    if (!gst_photo_src_pad_negotiate(src->vfsrc)) {
        GST_ELEMENT_ERROR(src, STREAM, FORMAT,
            ("Failed to negotiate vfsrc."), (NULL));
        return FALSE;
    }

    if (!gst_photo_src_pad_negotiate(mode_pad)) {
        GST_ELEMENT_ERROR(src, STREAM, FORMAT,
            ("Failed to negotiate %s.", GST_PAD_NAME(mode_pad->pad)), (NULL));
        return FALSE;
    }

    PhotoStream *streams[2] = { src->vfsrc->stream, mode_pad->stream };

    if (!photo_camera_request_streams(src->camera, 2, streams)) {
        GST_ELEMENT_ERROR(src, STREAM, FORMAT,
            ("Camera rejected stream combination."), (NULL));
        return FALSE;
    }

    if (src->running) {
        GST_INFO_OBJECT(src, "starting viewfinder");
        photo_stream_start(src->vfsrc->stream);
    }

    return TRUE;
}

static gboolean
gst_photo_camera_pad_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    GstPhotoCameraSrc *src = GST_PHOTO_CAMERA_SRC(parent);
    gboolean ret = FALSE;

    GST_LOG_OBJECT(pad, "handle %" GST_PTR_FORMAT, event);

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_RECONFIGURE:
        g_mutex_lock(&src->capture_lock);
        if (gst_photo_camera_pad_is_significant(src, pad)) {
            ret = gst_photo_camera_src_configure(src);
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

static void gst_photo_camera_src_init(GstPhotoCameraSrc *src)
{
    g_mutex_init(&src->camera_lock);
    src->camera_name = NULL;
    src->camera = NULL;

    g_mutex_init(&src->capture_lock);
    src->mode = DEFAULT_MODE;
    src->zoom = 1.0f;

    src->allocator = gst_photo_allocator_new();

    src->vfsrc = gst_photo_src_pad_new(src->allocator, GST_ELEMENT(src),
                                       GST_BASE_CAMERA_SRC_VIEWFINDER_PAD_NAME);
    src->vfsrc->usage = PHOTO_STREAM_USAGE_PREVIEW;
    gst_pad_set_event_function(src->vfsrc->pad, gst_photo_camera_pad_event);

    src->imgsrc = gst_photo_src_pad_new(src->allocator, GST_ELEMENT(src),
                                        GST_BASE_CAMERA_SRC_IMAGE_PAD_NAME);
    src->imgsrc->usage = PHOTO_STREAM_USAGE_STILL_CAPTURE;
    gst_pad_set_event_function(src->imgsrc->pad, gst_photo_camera_pad_event);

    src->vidsrc = gst_photo_src_pad_new(src->allocator, GST_ELEMENT(src),
                                        GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME);
    src->vidsrc->usage = PHOTO_STREAM_USAGE_RECORDING;
    gst_pad_set_event_function(src->vidsrc->pad, gst_photo_camera_pad_event);

    src->post_preview = DEFAULT_POST_PREVIEW;
    src->preview_caps = NULL;
    src->preview_filter = NULL;
    src->preview_pipeline = gst_camerabin_create_preview_pipeline(GST_ELEMENT(src), NULL);

    GST_OBJECT_FLAG_SET(src, GST_ELEMENT_FLAG_SOURCE);
}

static void
gst_photo_camera_src_set_property(GObject *object, guint prop_id, const GValue *value,
                                  GParamSpec *pspec)
{
    GstPhotoCameraSrc *src = GST_PHOTO_CAMERA_SRC(object);

    switch (prop_id) {
    case PROP_CAMERA_NAME:
        g_mutex_lock(&src->camera_lock);
        g_free(src->camera_name);
        src->camera_name = g_value_dup_string(value);
        g_mutex_unlock(&src->camera_lock);
        break;

    case PROP_MODE: {
        GstCameraBinMode mode = g_value_get_enum(value);

        if (src->mode == mode) {
            break;
        }

        g_mutex_lock(&src->capture_lock);

        if (src->capturing) {
            GST_WARNING_OBJECT(src, "cannot set mode while capturing");
            g_mutex_unlock(&src->capture_lock);
            break;
        }

        src->mode = mode;

        if (src->running) {
            gst_photo_camera_src_configure(src);
        }

        g_mutex_unlock(&src->capture_lock);

        break;
    }

    case PROP_ZOOM:
        g_mutex_lock(&src->camera_lock);
        src->zoom = g_value_get_float(value);
        if (src->camera) {
            photo_control_set_float(src->camera, PHOTO_CONTROL_ZOOM, src->zoom);
            photo_controls_commit(src->camera);
        }
        g_mutex_unlock(&src->camera_lock);
        break;

    case PROP_POST_PREVIEW:
        src->post_preview = g_value_get_boolean(value);
        break;

    case PROP_PREVIEW_CAPS: {
        GstCaps *new_caps;

        new_caps = (GstCaps *) gst_value_get_caps(value);
        if (new_caps) {
            new_caps = gst_caps_ref(new_caps);
        } else {
            new_caps = gst_caps_new_any();
        }

        if (!src->preview_caps || !gst_caps_is_equal(src->preview_caps, new_caps)) {
            gst_caps_replace(&src->preview_caps, new_caps);

            if (src->preview_pipeline) {
                gst_camerabin_preview_set_caps(src->preview_pipeline, src->preview_caps);
            }
        }

        gst_caps_unref(new_caps);

        break;
    }

    case PROP_PREVIEW_FILTER:
        if (src->preview_filter) {
            gst_object_unref(src->preview_filter);
        }

        src->preview_filter = g_value_dup_object(value);

        if (src->preview_pipeline) {
            gst_camerabin_preview_set_filter(src->preview_pipeline, src->preview_filter);
        }

        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gst_photo_camera_src_get_property(GObject *object, guint prop_id, GValue *value,
                                  GParamSpec *pspec)
{
    GstPhotoCameraSrc *src = GST_PHOTO_CAMERA_SRC(object);

    switch (prop_id) {
    case PROP_CAMERA_NAME:
        g_value_set_string(value, src->camera_name);
        break;

    case PROP_MODE:
        g_value_set_enum(value, src->mode);
        break;

    case PROP_READY_FOR_CAPTURE:
        g_mutex_lock(&src->capture_lock);
        g_value_set_boolean(value, !src->capturing);
        g_mutex_unlock(&src->capture_lock);
        break;

    case PROP_ZOOM:
        g_value_set_float(value, src->zoom);
        break;

    case PROP_MAX_ZOOM:
        g_mutex_lock(&src->camera_lock);
        if (src->camera) {
            const PhotoControlFloatRange *zoom = photo_control_query_float(src->camera, PHOTO_CONTROL_ZOOM);
            g_value_set_float(value, zoom->max);
        } else {
            g_value_set_float(value, DEFAULT_MAX_ZOOM);
        }
        g_mutex_unlock(&src->camera_lock);
        break;

    case PROP_PHOTO_CONTROLS:
        g_mutex_lock(&src->camera_lock);
        if (src->camera) {
            g_value_set_pointer(value, photo_camera_get_controls(src->camera));
        } else {
            g_value_set_pointer(value, NULL);
        }
        g_mutex_unlock(&src->camera_lock);
        break;

    case PROP_POST_PREVIEW:
        g_value_set_boolean(value, src->post_preview);
        break;

    case PROP_PREVIEW_CAPS:
        if (src->preview_caps)
            gst_value_set_caps(value, src->preview_caps);
        break;

    case PROP_PREVIEW_FILTER:
        if (src->preview_filter)
            g_value_set_object(value, src->preview_filter);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_photo_camera_src_finalize(GObject *object)
{
    GstPhotoCameraSrc *src = GST_PHOTO_CAMERA_SRC(object);

    gst_photo_src_pad_destroy(src->vfsrc);
    gst_photo_src_pad_destroy(src->imgsrc);
    gst_photo_src_pad_destroy(src->vidsrc);

    g_mutex_clear(&src->capture_lock);
    g_mutex_clear(&src->camera_lock);

    if (src->preview_pipeline) {
        g_clear_pointer(&src->preview_pipeline, gst_camerabin_destroy_preview_pipeline);
    }

    if (src->preview_caps) {
        gst_caps_replace(&src->preview_caps, NULL);
    }

    g_object_unref(src->allocator);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

// called during state transition
static void gst_photo_camera_src_add_vfsrc_orientation_tag(GstPhotoCameraSrc *src)
{
    const gchar *tag;

    const PhotoCameraInfo *info = photo_camera_get_info(src->camera);

    switch (info->orientation) {
    case 0:
        tag = "rotate-0";
        break;
    case 90:
        tag = "rotate-90";
        break;
    case 180:
        tag = "rotate-180";
        break;
    case 270:
        tag = "rotate-270";
        break;
    default:
        GST_WARNING_OBJECT(src, "unknown orientation %d", info->orientation);
        return;
    }

    GstTagList *taglist = gst_tag_list_new(GST_TAG_IMAGE_ORIENTATION, tag, NULL);

    g_mutex_lock(&src->vfsrc->lock);
    src->vfsrc->pending_events =
        g_list_append(src->vfsrc->pending_events, gst_event_new_tag(taglist));
    g_mutex_unlock(&src->vfsrc->lock);
}

static GstStateChangeReturn
gst_photo_camera_src_change_state(GstElement *element, GstStateChange transition)
{
    GstPhotoCameraSrc *src = GST_PHOTO_CAMERA_SRC(element);
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        GST_INFO_OBJECT(src, "opening camera '%s'", src->camera_name);

        g_mutex_lock(&src->camera_lock);
        src->camera = photo_camera_open(src->camera_name, &gst_photo_camera_callbacks, src);
        if (!src->camera) {
            g_mutex_unlock(&src->camera_lock);
            GST_ERROR_OBJECT(src, "failed to open camera '%s'", src->camera_name);
            ret = GST_STATE_CHANGE_FAILURE;
            break;
        }

        src->zoom = 1.0f;
        g_mutex_unlock(&src->camera_lock);

        g_object_notify(G_OBJECT(src), "zoom");
        g_object_notify(G_OBJECT(src), "max-zoom");

        gst_photo_src_pad_init_stream(src->vfsrc, src->camera);
        gst_photo_src_pad_init_stream(src->imgsrc, src->camera);
        gst_photo_src_pad_init_stream(src->vidsrc, src->camera);
        if (!src->vfsrc->stream || !src->imgsrc->stream || !src->vidsrc->stream) {
            GST_ERROR_OBJECT(src, "failed to create camera streams");

            g_clear_pointer(&src->vfsrc->stream, photo_stream_destroy);
            g_clear_pointer(&src->imgsrc->stream, photo_stream_destroy);
            g_clear_pointer(&src->vidsrc->stream, photo_stream_destroy);

            g_clear_pointer(&src->camera, photo_camera_close);

            ret = GST_STATE_CHANGE_FAILURE;
        }

        break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:
        gst_photo_camera_src_add_vfsrc_orientation_tag(src);
        gst_element_set_state(src->preview_pipeline->pipeline, GST_STATE_PLAYING);
        break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        g_mutex_lock(&src->capture_lock);
        src->running = TRUE;
        if (!gst_photo_camera_src_configure(src)) {
            ret = GST_STATE_CHANGE_FAILURE;
        }
        g_mutex_unlock(&src->capture_lock);

        g_object_notify (G_OBJECT(src), "ready-for-capture");
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
        if (src->capturing) {
            GST_WARNING_OBJECT(src, "capture was not stopped");
            src->capturing = FALSE;
        }
        src->running = FALSE;
        g_mutex_unlock(&src->capture_lock);
        photo_stream_stop(src->vfsrc->stream);
        break;

    case GST_STATE_CHANGE_PAUSED_TO_READY:
        gst_element_set_state(src->preview_pipeline->pipeline, GST_STATE_READY);
        photo_camera_release_streams(src->camera);
        break;

    case GST_STATE_CHANGE_READY_TO_NULL:
        GST_INFO_OBJECT(src, "closing camera");

        g_clear_pointer(&src->vfsrc->stream, photo_stream_destroy);
        g_clear_pointer(&src->imgsrc->stream, photo_stream_destroy);
        g_clear_pointer(&src->vidsrc->stream, photo_stream_destroy);

        g_clear_pointer(&src->camera, photo_camera_close);
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

static gboolean
gst_photo_camera_src_send_event(GstElement *element, GstEvent *event)
{
    GstPhotoCameraSrc *src = GST_PHOTO_CAMERA_SRC(element);
    gboolean ret = FALSE;

    switch (GST_EVENT_TYPE(event)) {
    // serialized events
    case GST_EVENT_EOS:
    case GST_EVENT_TAG:
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    case GST_EVENT_CUSTOM_DOWNSTREAM_STICKY:
    case GST_EVENT_CUSTOM_BOTH:
        g_mutex_lock(&src->vfsrc->lock);
        src->vfsrc->pending_events =
            g_list_append(src->vfsrc->pending_events, event);
        g_mutex_unlock(&src->vfsrc->lock);
        event = NULL;
        ret = TRUE;
        break;

    // non-serialized events
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
    case GST_EVENT_CUSTOM_BOTH_OOB:
        ret = gst_pad_push_event(src->vfsrc->pad, event);
        event = NULL;
        break;

    default:
        break;
    }

    if (event) {
        gst_event_unref(event);
    }

    return ret;
}

static void gst_photo_camera_src_start_capture(GstPhotoCameraSrc *src)
{
    gboolean started = FALSE;

    GST_DEBUG_OBJECT(src, "start capture");

    g_mutex_lock(&src->capture_lock);

    if (!src->running) {
        GST_ERROR_OBJECT(src, "cannot capture while not running");
        goto out;
    }

    if (src->capturing) {
        GST_ERROR_OBJECT(src, "already capturing");
        goto out;
    }

    // This is needed to allow camerabin to switch filesink to PLAYING
    src->capturing = TRUE;
    g_mutex_unlock(&src->capture_lock);
    g_object_notify(G_OBJECT(src), "ready-for-capture");
    g_mutex_lock(&src->capture_lock);

    if (src->mode == MODE_IMAGE) {
        started = photo_stream_take_picture(src->imgsrc->stream);
    } else {
        started = photo_stream_start(src->vidsrc->stream);
    }

out:
    if (!started) {
        src->capturing = FALSE;
        GST_ELEMENT_WARNING(src, RESOURCE, FAILED, (NULL), (NULL));
        g_mutex_unlock(&src->capture_lock);
        g_object_notify (G_OBJECT (src), "ready-for-capture");
        return;
    }

    g_mutex_unlock(&src->capture_lock);
}

static void gst_photo_camera_src_stop_capture(GstPhotoCameraSrc *src)
{
    GST_DEBUG_OBJECT(src, "stop capture");

    g_mutex_lock(&src->capture_lock);

    if (!src->capturing) {
        GST_WARNING_OBJECT(src, "trying to stop capturing while nothing is being captured");
        goto out;
    }

    if (src->mode == MODE_IMAGE) {
        GST_WARNING_OBJECT(src, "trying to stop capturing in image mode");
        goto out;
    } else {
        photo_stream_stop(src->vidsrc->stream);

        g_mutex_lock(&src->vidsrc->lock);
        src->vidsrc->stopping = TRUE;
        while (src->vidsrc->stopping) {
            g_cond_wait(&src->vidsrc->stop_cond, &src->vidsrc->lock);
        }
        g_mutex_unlock(&src->vidsrc->lock);
    }

    src->capturing = FALSE;

out:
    g_mutex_unlock(&src->capture_lock);

    g_object_notify(G_OBJECT(src), "ready-for-capture");
}

static void gst_photo_camera_src_class_init(GstPhotoCameraSrcClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_set_static_metadata(gstelement_class,
        "Compatibility camera source", "Source/Video/Device",
        "Camera source compatible with camerabin",
        "Nikolai Burov <nikolai.burov@jolla.com>");

    gst_element_class_add_pad_template(gstelement_class,
        gst_static_pad_template_get(&vf_src_template_factory));

    gst_element_class_add_pad_template(gstelement_class,
        gst_static_pad_template_get(&img_src_template_factory));

    gst_element_class_add_pad_template(gstelement_class,
        gst_static_pad_template_get(&video_src_template_factory));

    gobject_class->set_property = gst_photo_camera_src_set_property;
    gobject_class->get_property = gst_photo_camera_src_get_property;
    gobject_class->finalize = gst_photo_camera_src_finalize;

    gstelement_class->change_state = gst_photo_camera_src_change_state;
    gstelement_class->send_event = gst_photo_camera_src_send_event;

    g_object_class_install_property(gobject_class, PROP_CAMERA_NAME,
        g_param_spec_string("camera-name", "Camera device name",
            "Defines which camera device should be used",
            NULL,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_MODE,
        g_param_spec_enum("mode", "Mode",
            "Capture mode (image or video)",
            GST_TYPE_CAMERABIN_MODE,
            DEFAULT_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_READY_FOR_CAPTURE,
        g_param_spec_boolean("ready-for-capture", "Ready for capture",
            "Element is ready for another capture",
            TRUE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_ZOOM,
        g_param_spec_float("zoom", "Zoom level",
            "Zoom level control", 1.0f, G_MAXFLOAT, 1.0f,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_MAX_ZOOM,
        g_param_spec_float("max-zoom", "Maximum zoom level",
            "Maximum zoom level (note: may change "
            "depending on resolution/implementation)", 1.0f, G_MAXFLOAT,
            DEFAULT_MAX_ZOOM, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_PHOTO_CONTROLS,
        g_param_spec_pointer("photo-controls", "Advanced camera controls",
            "Provides direct access to the PhotoControls API. "
            "Setting the pipeline to NULL state invalidates the pointer.",
            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

    /* camerabin interface */
    g_object_class_install_property(gobject_class, PROP_POST_PREVIEW,
        g_param_spec_boolean("post-previews", "Post Previews",
            "If capture preview images should be posted to the bus",
            DEFAULT_POST_PREVIEW, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_PREVIEW_CAPS,
        g_param_spec_boxed("preview-caps", "Preview caps",
            "The caps of the preview image to be posted (NULL means ANY)",
            GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_PREVIEW_FILTER,
        g_param_spec_object("preview-filter", "Preview filter",
            "A custom preview filter to process preview image data",
            GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    photo_camera_src_signals[START_CAPTURE_SIGNAL] =
        g_signal_new_class_handler("start-capture",
            G_TYPE_FROM_CLASS(klass),
            G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
            G_CALLBACK(gst_photo_camera_src_start_capture),
            NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    photo_camera_src_signals[STOP_CAPTURE_SIGNAL] =
        g_signal_new_class_handler("stop-capture",
            G_TYPE_FROM_CLASS(klass),
            G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
            G_CALLBACK(gst_photo_camera_src_stop_capture),
            NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}
