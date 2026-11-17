// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include "camera.h"
#include "streamtarget.h"

namespace photo_api_qt {

StreamTarget::StreamTarget(PhotoStreamUsage usage)
    : m_stream(nullptr)
    , m_usage(usage)
    , m_hasConfig(false)
{
}

StreamTarget::~StreamTarget()
{
    setCamera(nullptr);
}

QVariantList StreamTarget::recommendedConfigurations() const
{
    return m_recommendedConfigurations;
}

QVariantList StreamTarget::availableConfigurations() const
{
    return m_availableConfigurations;
}

void StreamTarget::addConfiguration(void *userdata, const PhotoAvailableConfig *info)
{
    QVariantMap v;

    v.insert(QStringLiteral("width"), QVariantMap{
        {QStringLiteral("min"), info->width.min},
        {QStringLiteral("max"), info->width.max}
    });
    v.insert(QStringLiteral("height"), QVariantMap{
        {QStringLiteral("min"), info->height.min},
        {QStringLiteral("max"), info->height.max}
    });
    v.insert(QStringLiteral("minFps"), QVariantMap{
        {QStringLiteral("min"), info->min_fps.min},
        {QStringLiteral("max"), info->min_fps.max}
    });
    v.insert(QStringLiteral("maxFps"), QVariantMap{
        {QStringLiteral("min"), info->max_fps.min},
        {QStringLiteral("max"), info->max_fps.max}
    });

    // We currently don't expect QML to care about the pixel format

    static_cast<QVariantList *>(userdata)->append(v);
}

QVariantMap StreamTarget::configuration() const
{
    return m_configuration;
}

void StreamTarget::setConfiguration(const QVariantMap &configuration)
{
    if (m_configuration != configuration) {
        m_configuration = configuration;
        emit configurationChanged();
    }
}

bool StreamTarget::initialize(PhotoCamera *camera)
{
    static const PhotoStreamCallbacks callbacks = {
        .buffer_created = StreamTarget::handleBufferCreated,
        .buffer_invalidated = StreamTarget::handleBufferInvalidated,
        .frame_available = StreamTarget::handleFrameAvailable,
        .stopped = StreamTarget::handleStopped,
    };

    m_stream = photo_camera_create_stream(camera, m_usage, &callbacks, this);
    if (!m_stream)
        return false;

    m_recommendedConfigurations.clear();
    m_availableConfigurations.clear();

    photo_stream_enumerate_configs(m_stream, NULL, &StreamTarget::addConfiguration,
                                   &m_recommendedConfigurations, PHOTO_STREAM_CONFIG_RECOMMENDED);
    photo_stream_enumerate_configs(m_stream, NULL, &StreamTarget::addConfiguration,
                                   &m_availableConfigurations, 0);

    emit recommendedConfigurationsChanged();
    emit availableConfigurationsChanged();

    return true;
}

bool StreamTarget::applyConfig()
{
    if (!m_stream)
        return false;

    PhotoConfig *config = photo_config_new();

    if (m_configuration.contains("width") && m_configuration.contains("height")) {
        int width = m_configuration["width"].toInt();
        int height = m_configuration["height"].toInt();
        photo_config_set_size(config, width, height);
    }

    if (m_configuration.contains("minFps") && m_configuration.contains("maxFps")) {
        int min = m_configuration["minFps"].toInt();
        int max = m_configuration["maxFps"].toInt();
        photo_config_set_fps_range(config, min, max);
    }

    PhotoStreamConfig *stream_cfg = photo_stream_configure(m_stream, config);
    photo_config_free(config);
    if (!stream_cfg)
        return false;

    m_fixedConfig = *photo_stream_config_get_info(stream_cfg);
    photo_stream_config_apply(stream_cfg);

    m_hasConfig = true;
    emit fixedConfigChanged();

    return true;
}

void StreamTarget::cleanup()
{
    photo_stream_destroy(m_stream);
    m_stream = nullptr;
}

PhotoStream *StreamTarget::stream() const
{
    return m_stream;
}

const PhotoFixedConfig *StreamTarget::fixedConfig() const
{
    return m_hasConfig ? &m_fixedConfig : nullptr;
}

bool StreamTarget::hasConfig() const
{
    return m_hasConfig;
}

int StreamTarget::width() const
{
    return m_hasConfig ? m_fixedConfig.width : 0;
}

int StreamTarget::height() const
{
    return m_hasConfig ? m_fixedConfig.height : 0;
}

void StreamTarget::handleBufferCreated(void *userdata, PhotoBuffer *buffer)
{
    emit static_cast<StreamTarget *>(userdata)->bufferCreated(buffer);
}

void StreamTarget::handleBufferInvalidated(void *userdata, PhotoBuffer *buffer)
{
    emit static_cast<StreamTarget *>(userdata)->bufferInvalidated(buffer);
}

void StreamTarget::handleFrameAvailable(void *userdata)
{
    emit static_cast<StreamTarget *>(userdata)->frameAvailable();
}

void StreamTarget::handleStopped(void *userdata)
{
    emit static_cast<StreamTarget *>(userdata)->stopped();
}

} /* namespace */
