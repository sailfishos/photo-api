/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_API_QT_BASECAPTURETARGET_H
#define PHOTO_API_QT_BASECAPTURETARGET_H

#include <QObject>
#include <QPointer>

#include <photo-api.h>

namespace photo_api_qt {

class Camera;

class BaseCaptureTarget : public QObject
{
    Q_OBJECT

    Q_PROPERTY(photo_api_qt::Camera *camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)

public:
    BaseCaptureTarget();
    ~BaseCaptureTarget();

    Camera *camera() const;
    void setCamera(Camera *camera);

    bool isEnabled() const;
    void setEnabled(bool enabled);

    virtual bool initialize(PhotoCamera *camera) = 0;
    virtual bool applyConfig() = 0;
    virtual void cleanup() = 0;
    virtual PhotoStream *stream() const = 0;

signals:
    void cameraChanged();
    void enabledChanged();

    void afterCameraRequest();
    void beforeCameraRelease();

private:
    QPointer<Camera> m_camera;
    bool m_enabled;
};

} /* namespace */

#endif
