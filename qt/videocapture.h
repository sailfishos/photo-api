/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_API_QT_VIDEOCAPTURE_H
#define PHOTO_API_QT_VIDEOCAPTURE_H

#include <QObject>
#include <QUrl>
#include <QVariant>

#include <gst/gst.h>

#include "basecapturetarget.h"

namespace photo_api_qt {

class VideoCapture : public BaseCaptureTarget
{
    Q_OBJECT

    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)
    Q_PROPERTY(QString outputPath MEMBER m_outputPath NOTIFY outputPathChanged)
    Q_PROPERTY(QString containerType MEMBER m_containerType NOTIFY containerTypeChanged)
    Q_PROPERTY(QString videoType MEMBER m_videoType NOTIFY videoTypeChanged)
    Q_PROPERTY(int videoBitrate MEMBER m_videoBitrate NOTIFY videoBitrateChanged)
    Q_PROPERTY(QString audioType MEMBER m_audioType NOTIFY audioTypeChanged)
    Q_PROPERTY(int audioBitrate MEMBER m_audioBitrate NOTIFY audioBitrateChanged)
    Q_PROPERTY(QVariantList recommendedConfigurations READ recommendedConfigurations NOTIFY recommendedConfigurationsChanged)
    Q_PROPERTY(QVariantMap configuration READ configuration WRITE setConfiguration NOTIFY configurationChanged)

public:
    VideoCapture();
    ~VideoCapture();

    QVariantList recommendedConfigurations() const;
    QVariantMap configuration() const;
    void setConfiguration(const QVariantMap &configuration);

    bool isRecording() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    bool initialize(PhotoCamera *camera) override;
    bool applyConfig() override;
    void cleanup() override;
    PhotoStream *stream() const override;

signals:
    void recordingChanged();
    void outputPathChanged();
    void containerTypeChanged();
    void videoTypeChanged();
    void videoBitrateChanged();
    void audioTypeChanged();
    void audioBitrateChanged();
    void recommendedConfigurationsChanged();
    void configurationChanged();
    void error(const QString &error);

private:
    struct EncoderPadData {
        GstPad *videoPad;
        GstPad *audioPad;
    };

    static void addConfiguration(void *userdata, const PhotoAvailableConfig *info);
    static void handleError(GstBus *bus, GstMessage *msg, VideoCapture *obj);
    static void handleEos(GstBus *bus, GstMessage *msg, VideoCapture *obj);
    static void setBitrate(GstElement *element, int bitrate);
    static gboolean foreachEncoderPad(GstElement *encodebin, GstPad *pad, void *user_data);
    static bool linkEncoderPad(GstElement *src, const char *srcPadName, GstPad *sinkPad);
    static void elementAdded(GstElement *encodebin, GstElement *element, VideoCapture *obj);
    static void elementRemoved(GstElement *encodebin, GstElement *element, VideoCapture *obj);

    bool initPipeline();
    void markProfileChanged();
    void applyCapsFilter();
    bool applyProfile();

    GstElement *m_pipeline;
    GstElement *m_streamsrc;
    GstElement *m_audiosrc;
    GstElement *m_capsfilter;
    GstElement *m_encodebin;
    GstElement *m_filesink;
    GstElement *m_videoEncoder;
    GstElement *m_audioEncoder;

    QVariantList m_recommendedConfigurations;
    QVariantMap m_configuration;

    QString m_outputPath;
    QString m_containerType;

    QString m_videoType;
    int m_videoBitrate;

    QString m_audioType;
    int m_audioBitrate;

    bool m_recording;
    bool m_profileChanged;
};

} /* namespace */

#endif
