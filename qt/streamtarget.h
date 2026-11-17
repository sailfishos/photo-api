/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_API_QT_STREAMTARGET_H
#define PHOTO_API_QT_STREAMTARGET_H

#include <QObject>
#include <QVariant>

#include <photo-api.h>

#include "basecapturetarget.h"

namespace photo_api_qt {

class Camera;

class StreamTarget : public BaseCaptureTarget
{
    Q_OBJECT

    Q_PROPERTY(QVariantList availableConfigurations READ availableConfigurations NOTIFY availableConfigurationsChanged)
    Q_PROPERTY(QVariantList recommendedConfigurations READ recommendedConfigurations NOTIFY recommendedConfigurationsChanged)
    Q_PROPERTY(QVariantMap configuration READ configuration WRITE setConfiguration NOTIFY configurationChanged)
    Q_PROPERTY(bool hasConfig READ hasConfig NOTIFY fixedConfigChanged)
    Q_PROPERTY(int width READ width NOTIFY fixedConfigChanged)
    Q_PROPERTY(int height READ height NOTIFY fixedConfigChanged)

public:
    StreamTarget(PhotoStreamUsage usage);
    ~StreamTarget();

    QVariantList availableConfigurations() const;
    QVariantList recommendedConfigurations() const;

    QVariantMap configuration() const;
    void setConfiguration(const QVariantMap &configuration);

    bool initialize(PhotoCamera *camera) override;
    bool applyConfig() override;
    void cleanup() override;
    PhotoStream *stream() const override;

    const PhotoFixedConfig *fixedConfig() const;

    bool hasConfig() const;
    int width() const;
    int height() const;

signals:
    void availableConfigurationsChanged();
    void recommendedConfigurationsChanged();
    void configurationChanged();
    void fixedConfigChanged();

    void bufferCreated(PhotoBuffer *buffer);
    void bufferInvalidated(PhotoBuffer *buffer);
    void frameAvailable();
    void stopped();

private:
    static void addConfiguration(void *userdata, const PhotoAvailableConfig *info);

    static void handleBufferCreated(void *userdata, PhotoBuffer *buffer);
    static void handleBufferInvalidated(void *userdata, PhotoBuffer *buffer);
    static void handleFrameAvailable(void *userdata);
    static void handleStopped(void *userdata);

    PhotoStream *m_stream;
    QVariantList m_availableConfigurations;
    QVariantList m_recommendedConfigurations;
    QVariantMap m_configuration;
    PhotoStreamUsage m_usage;
    PhotoFixedConfig m_fixedConfig;
    bool m_hasConfig;
};

} /* namespace */

#endif
