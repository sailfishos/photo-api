// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include "camera.h"
#include "streamtarget.h"

namespace photo_api_qt {

BaseCaptureTarget::BaseCaptureTarget()
    : m_enabled(true)
{
}

BaseCaptureTarget::~BaseCaptureTarget()
{
}

Camera *BaseCaptureTarget::camera() const
{
    return m_camera;
}

void BaseCaptureTarget::setCamera(Camera *camera)
{
    if (m_camera == camera)
        return;

    if (m_enabled && m_camera) {
        m_camera->removeTarget(this);
    }

    m_camera = camera;
    emit cameraChanged();

    if (m_enabled && camera) {
        camera->addTarget(this);
    }
}

bool BaseCaptureTarget::isEnabled() const
{
    return m_enabled;
}

void BaseCaptureTarget::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    if (m_camera) {
        if (enabled) {
            m_camera->addTarget(this);
        } else {
            m_camera->removeTarget(this);
        }
    }

    m_enabled = enabled;
    emit enabledChanged();
}

} /* namespace */
