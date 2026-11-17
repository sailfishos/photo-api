// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include <QDebug>

#include <gst/pbutils/encoding-profile.h>

#include "videocapture.h"

namespace photo_api_qt {

VideoCapture::VideoCapture()
    : m_pipeline(nullptr)
    , m_streamsrc(nullptr)
    , m_audiosrc(nullptr)
    , m_capsfilter(nullptr)
    , m_encodebin(nullptr)
    , m_filesink(nullptr)
    , m_videoEncoder(nullptr)
    , m_audioEncoder(nullptr)
    , m_videoBitrate(-1)
    , m_audioBitrate(-1)
    , m_recording(false)
    , m_profileChanged(false)
{
    connect(this, &VideoCapture::containerTypeChanged, this, &VideoCapture::markProfileChanged);
    connect(this, &VideoCapture::audioTypeChanged, this, &VideoCapture::markProfileChanged);
    connect(this, &VideoCapture::videoTypeChanged, this, &VideoCapture::markProfileChanged);
}

VideoCapture::~VideoCapture()
{
    setCamera(nullptr);

    if (m_pipeline) {
        GstBus *bus = gst_element_get_bus(m_pipeline);
        gst_bus_remove_signal_watch(bus);
        gst_object_unref(bus);
        gst_object_unref(m_pipeline);
    }
}

bool VideoCapture::isRecording() const
{
    return m_recording;
}

QVariantList VideoCapture::recommendedConfigurations() const
{
    return m_recommendedConfigurations;
}

QVariantMap VideoCapture::configuration() const
{
    return m_configuration;
}

void VideoCapture::setConfiguration(const QVariantMap &configuration)
{
    if (m_configuration != configuration) {
        m_configuration = configuration;
        emit configurationChanged();
    }
}

void VideoCapture::start()
{
    if (m_recording || !m_pipeline)
        return;

    g_object_set(m_filesink, "location", m_outputPath.toLocal8Bit().constData(), nullptr);

    if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        qWarning() << "pipeline failed to start";
        return;
    }

    if (gst_element_set_state(m_filesink, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        qWarning() << "filesink failed to start";
        return;
    }

    photo_stream_start(stream());

    m_recording = true;
    emit recordingChanged();
}

void VideoCapture::stop()
{
    if (!m_recording)
        return;

    photo_stream_stop(stream());
    gst_element_send_event(m_audiosrc, gst_event_new_eos());
}

bool VideoCapture::initialize(PhotoCamera *camera)
{
    if (!initPipeline())
        return false;

    m_profileChanged = true;

    g_object_set(m_streamsrc, "photo-camera", camera, nullptr);

    /* Prevent file from being created before recording starts */
    gst_element_set_locked_state(m_filesink, TRUE);

    if (gst_element_set_state(m_pipeline, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE) {
        qWarning() << "pipeline failed to initialize";
        return false;
    }

    m_recommendedConfigurations.clear();
    photo_stream_enumerate_configs(stream(), NULL, &VideoCapture::addConfiguration,
                                   &m_recommendedConfigurations, PHOTO_STREAM_CONFIG_RECOMMENDED);
    emit recommendedConfigurationsChanged();

    return true;
}

bool VideoCapture::applyConfig()
{
    if (!m_pipeline)
        return false;

    if (m_recording) {
        m_recording = false;
        emit recordingChanged();
    }

    /* Stop to reconfigure in case we were in PLAYING state */
    gst_element_set_locked_state(m_streamsrc, FALSE);
    gst_element_set_state(m_pipeline, GST_STATE_READY);

    applyCapsFilter();

    if (!applyProfile()) {
        return false;
    }

    setBitrate(m_videoEncoder, m_videoBitrate);
    setBitrate(m_audioEncoder, m_audioBitrate);

    if (gst_element_set_state(m_streamsrc, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        qWarning() << "photostreamsrc failed to configure";
        return false;
    }

    /* videosrc must remain in PLAYING state to retain configuration */
    gst_element_set_locked_state(m_streamsrc, TRUE);

    return true;
}

void VideoCapture::cleanup()
{
    if (!m_pipeline)
        return;

    if (m_recording) {
        m_recording = false;
        emit recordingChanged();
    }

    gst_element_set_locked_state(m_filesink, FALSE);
    gst_element_set_locked_state(m_streamsrc, FALSE);

    if (gst_element_set_state(m_pipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE) {
        qWarning() << "pipeline failed to deinitialize";
    }

    gst_element_unlink(m_capsfilter, m_encodebin);
    gst_element_unlink(m_audiosrc, m_encodebin);
}

PhotoStream *VideoCapture::stream() const
{
    if (!m_pipeline)
        return nullptr;

    PhotoStream *stream = nullptr;
    g_object_get(m_streamsrc, "photo-stream", &stream, nullptr);
    return stream;
}

void VideoCapture::handleError(GstBus *bus, GstMessage *msg, VideoCapture *obj)
{
    GError *err = nullptr;
    gchar *debug_info = nullptr;

    gst_message_parse_error(msg, &err, &debug_info);
    QString errorStr = QString::fromUtf8(err->message);

    qWarning() << "Error:" << errorStr;
    qDebug() << "Debug info:" << (debug_info ? debug_info : "(none)");

    emit obj->error(errorStr);

    /* Try to stop immediately */
    if (obj->m_recording) {
        photo_stream_stop(obj->stream());
        obj->m_recording = false;
        emit obj->recordingChanged();
    }

    GstState state;
    gst_element_get_state(obj->m_pipeline, &state, NULL, 0);
    if (state > GST_STATE_READY)
        gst_element_set_state(obj->m_pipeline, GST_STATE_READY);
}

void VideoCapture::handleEos(GstBus *bus, GstMessage *msg, VideoCapture *obj)
{
    if (obj->m_recording) {
        /* Close resources */
        gst_element_set_state(obj->m_filesink, GST_STATE_NULL);
        gst_element_set_state(obj->m_pipeline, GST_STATE_READY);
        obj->m_recording = false;
        emit obj->recordingChanged();
    }
}

void VideoCapture::setBitrate(GstElement *element, int bitrate)
{
    if (!element || bitrate < 0)
        return;

    GObjectClass *objectClass = G_OBJECT_GET_CLASS(element);

    if (g_object_class_find_property(objectClass, "bitrate")) {
        g_object_set(element, "bitrate", bitrate, NULL);
    } else if (g_object_class_find_property(objectClass, "target-bitrate")) {
        g_object_set(element, "target-bitrate", bitrate, NULL);
    }
}

void VideoCapture::elementAdded(GstElement *encodebin, GstElement *element, VideoCapture *obj)
{
    if (GST_IS_BIN(element)) {
        g_signal_connect(element, "element-added", G_CALLBACK(&VideoCapture::elementAdded), obj);
        g_signal_connect(element, "element-removed", G_CALLBACK(&VideoCapture::elementRemoved), obj);
        return;
    }

    GstElementFactory *factory = gst_element_get_factory(element);

    if (!factory)
        return;

    if (gst_element_factory_list_is_type(factory, GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER)) {
        obj->m_audioEncoder = element;
        setBitrate(element, obj->m_audioBitrate);
    } else if (gst_element_factory_list_is_type(factory, GST_ELEMENT_FACTORY_TYPE_VIDEO_ENCODER)) {
        obj->m_videoEncoder = element;
        setBitrate(element, obj->m_videoBitrate);
    } else if (strcmp(GST_OBJECT_NAME(factory), "pulsesrc") == 0) {
        GstStructure *properties = gst_structure_from_string("props,audio.source=camcorder", nullptr);
        g_object_set(element, "stream-properties", properties, nullptr);
        gst_structure_free(properties);
    }
}

void VideoCapture::elementRemoved(GstElement *encodebin, GstElement *element, VideoCapture *obj)
{
    if (element == obj->m_audioEncoder) {
        obj->m_audioEncoder = nullptr;
    } else if (element == obj->m_videoEncoder) {
        obj->m_videoEncoder = nullptr;
    }
}

gboolean VideoCapture::foreachEncoderPad(GstElement *encodebin, GstPad *pad, void *user_data)
{
    EncoderPadData *data = static_cast<EncoderPadData *>(user_data);

    if (g_str_has_prefix(GST_PAD_NAME(pad), "video")) {
        data->videoPad = pad;
    } else if (g_str_has_prefix(GST_PAD_NAME(pad), "audio")) {
        data->audioPad = pad;
    }

    return TRUE;
}

bool VideoCapture::linkEncoderPad(GstElement *src, const char *srcPadName, GstPad *sinkPad)
{
    GstPad *srcPad = gst_element_get_static_pad(src, srcPadName);
    g_return_val_if_fail(srcPad, false);
    g_return_val_if_fail(sinkPad, false);

    bool ok = gst_pad_link(srcPad, sinkPad) == GST_PAD_LINK_OK;

    gst_object_unref(srcPad);
    return ok;
}

bool VideoCapture::initPipeline()
{
    if (m_pipeline)
        return true;

    GError *err = nullptr;

    if (!gst_init_check(nullptr, nullptr, &err)) {
        qWarning() << "Failed to initialize GStreamer:" << err->message;
    }

    m_pipeline = gst_pipeline_new("recorder");
    if (!m_pipeline) {
        qWarning() << "Failed to create GStreamer pipeline";
        return false;
    }

    g_signal_connect(m_pipeline, "element-added", G_CALLBACK(&VideoCapture::elementAdded), this);
    g_signal_connect(m_pipeline, "element-removed", G_CALLBACK(&VideoCapture::elementRemoved), this);

    GstBus *bus = gst_element_get_bus(m_pipeline);
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message::error",
                     G_CALLBACK(&VideoCapture::handleError), this);
    g_signal_connect(G_OBJECT(bus), "message::eos",
                     G_CALLBACK(&VideoCapture::handleEos), this);
    gst_object_unref(bus);

    m_streamsrc = gst_element_factory_make("photostreamsrc", "recorder-videosrc");
    if (!m_streamsrc) {
        qWarning() << "Failed to create photostreamsrc element";
    }

    m_audiosrc = gst_element_factory_make("autoaudiosrc", "recorder-audiosrc");
    if (!m_audiosrc) {
        qWarning() << "Failed to create autoaudiosrc element";
    }

    m_capsfilter = gst_element_factory_make("capsfilter", "recorder-capsfilter");
    if (!m_capsfilter) {
        qWarning() << "Failed to create capsfilter element";
    }

    m_encodebin = gst_element_factory_make("encodebin", "recorder-encodebin");
    if (!m_encodebin) {
        qWarning() << "Failed to create encodebin element";
    }

    m_filesink = gst_element_factory_make("filesink", "recorder-filesink");
    if (!m_filesink) {
        qWarning() << "Failed to create filesink element";
    }

    if (m_streamsrc && m_audiosrc && m_capsfilter && m_encodebin && m_filesink) {
        gst_bin_add_many(GST_BIN(m_pipeline), m_streamsrc, m_audiosrc,
                         m_capsfilter, m_encodebin, m_filesink, nullptr);
        gst_element_link_pads(m_streamsrc, "src", m_capsfilter, "sink");
        gst_element_link_pads(m_encodebin, "src", m_filesink, "sink");

        /* Disable audio and video converters */
        g_object_set(m_encodebin, "flags", 0x3, NULL);

        return true;
    } else {
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        if (m_streamsrc) {
            gst_object_unref(m_streamsrc);
            m_streamsrc = nullptr;
        }
        if (m_audiosrc) {
            gst_object_unref(m_audiosrc);
            m_audiosrc = nullptr;
        }
        if (m_capsfilter) {
            gst_object_unref(m_capsfilter);
            m_capsfilter = nullptr;
        }
        if (m_encodebin) {
            gst_object_unref(m_encodebin);
            m_encodebin = nullptr;
        }
        if (m_filesink) {
            gst_object_unref(m_filesink);
            m_filesink = nullptr;
        }
        return false;
    }
}

void VideoCapture::addConfiguration(void *userdata, const PhotoAvailableConfig *info)
{
    QVariantMap v;

    // Skip configurations that require variable-framerate capture.
    if (info->min_fps.max < info->max_fps.min)
        return;

    v.insert(QStringLiteral("width"), QVariantMap{
        {QStringLiteral("min"), info->width.min},
        {QStringLiteral("max"), info->width.max}
    });
    v.insert(QStringLiteral("height"), QVariantMap{
        {QStringLiteral("min"), info->height.min},
        {QStringLiteral("max"), info->height.max}
    });
    // Ignore min_fps since only constant-framerate capture is used for videos.
    v.insert(QStringLiteral("fps"), QVariantMap{
        {QStringLiteral("min"), info->max_fps.min},
        {QStringLiteral("max"), info->max_fps.max}
    });

    static_cast<QVariantList *>(userdata)->append(v);
}

void VideoCapture::markProfileChanged()
{
    m_profileChanged = true;
}

void VideoCapture::applyCapsFilter()
{
    GstCaps *caps;

    if (m_configuration.contains("width") && m_configuration.contains("height")) {
        int width = m_configuration["width"].toInt();
        int height = m_configuration["height"].toInt();

        caps = gst_caps_from_string("video/x-raw(ANY)");
        gst_structure_set(gst_caps_get_structure(caps, 0),
                          "width", G_TYPE_INT, width,
                          "height", G_TYPE_INT, height,
                          nullptr);

        if (m_configuration.contains("fps")) {
            int fps = m_configuration["fps"].toInt();
            gst_structure_set(gst_caps_get_structure(caps, 0),
                              "framerate", GST_TYPE_FRACTION, fps, 1, nullptr);
        }
    } else {
        caps = gst_caps_new_any();
    }

    g_object_set(m_capsfilter, "caps", caps, nullptr);
    gst_caps_unref(caps);
}

bool VideoCapture::applyProfile()
{
    if (!m_profileChanged)
        return true;

    gst_element_unlink(m_audiosrc, m_encodebin);

    GstCaps *caps = gst_caps_from_string(m_containerType.toLatin1());
    GstEncodingContainerProfile *container =
        gst_encoding_container_profile_new("camera", "camera recording profile", caps, nullptr);
    gst_caps_unref(caps);

    caps = gst_caps_from_string(m_videoType.toLatin1());
    gst_encoding_container_profile_add_profile(container,
        (GstEncodingProfile *) gst_encoding_video_profile_new(caps, nullptr, nullptr, 1));
    gst_caps_unref(caps);

    caps = gst_caps_from_string(m_audioType.toLatin1());
    gst_encoding_container_profile_add_profile(container,
        (GstEncodingProfile *) gst_encoding_audio_profile_new(caps, nullptr, nullptr, 1));
    gst_caps_unref(caps);

    g_object_set(m_encodebin, "profile", container, nullptr);

    EncoderPadData data = {};

    gst_element_foreach_sink_pad(m_encodebin, &VideoCapture::foreachEncoderPad, &data);

    if (!linkEncoderPad(m_capsfilter, "src", data.videoPad)) {
        qWarning() << "Failed to link capsfilter to encoder";
        return false;
    }

    if (!linkEncoderPad(m_audiosrc, "src", data.audioPad)) {
        qWarning() << "Failed to link audio source to encoder";
        return false;
    }

    m_profileChanged = false;

    return true;
}

} /* namespace */
