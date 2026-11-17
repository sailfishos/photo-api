// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include <QDebug>

#include <photo-api.h>

#include "basecapturetarget.h"
#include "camera.h"
#include "cameramanager.h"

namespace photo_api_qt {

Camera::Camera()
    : ControlContainer(this)
    , m_camera(nullptr)
    , m_configured(false)
{
}

Camera::~Camera()
{
    closeDevice();
}

QString Camera::cameraId() const
{
    return m_cameraId;
}

void Camera::setCameraId(const QString &cameraId)
{
    if (m_cameraId == cameraId)
        return;

    bool wasActive = isActive();

    closeDevice();
    m_cameraId = cameraId;
    emit cameraIdChanged();

    if (!cameraId.isEmpty()) {
        openDevice();
    }

    if (isActive() != wasActive) {
        emit activeChanged();
    }

    emit infoChanged();

    if (m_camera) {
        PhotoControls *controls = photo_camera_get_controls(m_camera);

        photo_controls_commit(controls);
        photo_controls_add_listener(controls, &Camera::handleLockState, this,
                                    PHOTO_CONTROL_LOCK_ALL);

        configure();
    }
}

QVariantMap Camera::info() const
{
    if (m_camera == nullptr)
        return QVariantMap();

    return CameraManager::constructInfoMap(photo_camera_get_info(m_camera));
}

bool Camera::isActive() const
{
    return m_camera != nullptr;
}

bool Camera::isConfigured() const
{
    return m_configured;
}

void Camera::configure()
{
    if (!m_camera || m_captureTargets.empty())
        return;

    if (m_configured) {
        photo_camera_release_streams(m_camera);
    }

    bool ok = false;

    for (BaseCaptureTarget *target : m_captureTargets) {
        ok = target->applyConfig();
        if (!ok) {
            break;
        }
    }

    if (ok) {
        PhotoStream **streams = new PhotoStream *[m_captureTargets.count()];
        int i = 0;
        for (BaseCaptureTarget *target : m_captureTargets) {
            streams[i] = target->stream();
            i++;
        }

        ok = photo_camera_request_streams(m_camera, m_captureTargets.count(), streams);

        delete[] streams;
    }

    if (ok) {
        for (BaseCaptureTarget *target : m_captureTargets) {
            emit target->afterCameraRequest();
        }
    } else {
        emit configFailed();
    }

    if (m_configured != ok) {
        m_configured = ok;
        emit configuredChanged();
    }
}

void Camera::addTarget(BaseCaptureTarget *target)
{
    m_captureTargets.insert(target);
    resetConfig();
    if (m_camera) {
        if (!target->initialize(m_camera)) {
            emit initFailed();
        }
    }
}

void Camera::removeTarget(BaseCaptureTarget *target)
{
    if (m_captureTargets.remove(target)) {
        resetConfig();
        target->cleanup();
    }
}

PhotoControls *Camera::controls() const
{
    return m_camera ? photo_camera_get_controls(m_camera) : nullptr;
}

void Camera::trigger(LockMask locks)
{
    if (PhotoControls *c = controls()) {
        photo_controls_trigger(c, locks);
    }
}

void Camera::forceLock(LockMask locks)
{
    if (PhotoControls *c = controls()) {
        photo_controls_force_lock(c, locks);
    }
}

void Camera::unlock(LockMask locks)
{
    if (PhotoControls *c = controls()) {
        photo_controls_unlock(c, locks);
    }
}

void Camera::handleShutter(void *userdata)
{
    emit static_cast<Camera *>(userdata)->shutter();
}

void Camera::handleCaptureDone(void *userdata, bool successful)
{
    emit static_cast<Camera *>(userdata)->captureDone(successful);
}

void Camera::handleDisconnected(void *userdata)
{
    emit static_cast<Camera *>(userdata)->disconnected();
}

void Camera::handleLockState(void *userdata, PhotoControlLockId lock_id,
                             PhotoControlLockState old_state,
                             PhotoControlLockState new_state)
{
    emit static_cast<Camera *>(userdata)->lockStatusChanged((LockBit)(1 << lock_id),
                                                            (LockState)old_state,
                                                            (LockState)new_state);
}

void Camera::openDevice()
{
    static const PhotoCameraCallbacks callbacks = {
        .shutter = Camera::handleShutter,
        .capture_done = Camera::handleCaptureDone,
        .disconnected = Camera::handleDisconnected,
    };

    m_camera = photo_camera_open(m_cameraId.toUtf8(), &callbacks, this);
    if (!m_camera) {
        emit initFailed();
        return;
    }

    for (BaseCaptureTarget *target : m_captureTargets) {
        if (!target->initialize(m_camera)) {
            emit initFailed();
            return;
        }
    }
}

void Camera::resetConfig()
{
    if (m_configured) {
        for (BaseCaptureTarget *target : m_captureTargets) {
            emit target->beforeCameraRelease();
        }

        photo_camera_release_streams(m_camera);
        m_configured = false;
        emit configuredChanged();
    }
}

void Camera::closeDevice()
{
    if (!m_camera)
        return;

    resetConfig();

    for (BaseCaptureTarget *target : m_captureTargets) {
        target->cleanup();
    }

    photo_camera_close(m_camera);

    m_camera = nullptr;
}

} /* namespace */
