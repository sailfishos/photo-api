/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_API_QT_CAMERAMANAGER_H
#define PHOTO_API_QT_CAMERAMANAGER_H

#include <QObject>
#include <QVariant>

#include <photo-api.h>

namespace photo_api_qt {

class CameraManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList availableCameras READ availableCameras CONSTANT)

public:
    static QVariantMap constructInfoMap(const PhotoCameraInfo *info);

    CameraManager();
    ~CameraManager();

    QVariantList availableCameras() const;

private:
    static void addCamera(void *userdata, const PhotoCameraInfo *info);

    QVariantList m_cameras;
};

} /* namespace */

#endif
