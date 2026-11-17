// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include <photo-api.h>

#include "cameramanager.h"

namespace photo_api_qt {

CameraManager::CameraManager()
{
    photo_enumerate_cameras(CameraManager::addCamera, this);
}

CameraManager::~CameraManager()
{
}

QVariantList CameraManager::availableCameras() const
{
    return m_cameras;
}

QVariantMap CameraManager::constructInfoMap(const PhotoCameraInfo *info)
{
    QVariantMap v;

    v.insert(QStringLiteral("id"), QString::fromUtf8(info->id));
    v.insert(QStringLiteral("facing"), info->facing);
    v.insert(QStringLiteral("orientation"), info->orientation);
    v.insert(QStringLiteral("activeArrayWidth"), info->active_array_width);
    v.insert(QStringLiteral("activeArrayHeight"), info->active_array_height);

    return v;
}

void CameraManager::addCamera(void *userdata, const PhotoCameraInfo *info)
{
    CameraManager *mgr = static_cast<CameraManager *>(userdata);

    mgr->m_cameras.append(constructInfoMap(info));
}

} /* namespace */
