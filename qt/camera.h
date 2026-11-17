/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_API_QT_CAMERA_H
#define PHOTO_API_QT_CAMERA_H

#include <QObject>
#include <QSet>
#include <QVariant>

#include <photo-api.h>

#include "generated/ctl-classes.h"

namespace photo_api_qt {

class BaseCaptureTarget;

class Camera : public ControlContainer
{
    Q_OBJECT

    Q_PROPERTY(QString cameraId READ cameraId WRITE setCameraId NOTIFY cameraIdChanged)
    Q_PROPERTY(QVariantMap info READ info NOTIFY infoChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(bool configured READ isConfigured NOTIFY configuredChanged)

public:
    enum Facing {
        UnknownFacing = PHOTO_CAMERA_FACING_UNKNOWN,
        FrontFacing = PHOTO_CAMERA_FACING_FRONT,
        BackFacing = PHOTO_CAMERA_FACING_BACK,
    };
    Q_ENUM(Facing)

    enum LockBit {
        ExposureLock = PHOTO_CONTROL_LOCK_BIT_AE,
        FocusLock = PHOTO_CONTROL_LOCK_BIT_AF,
        WhiteBalanceLock = PHOTO_CONTROL_LOCK_BIT_AWB,
        AllLocks = PHOTO_CONTROL_LOCK_ALL
    };
    Q_DECLARE_FLAGS(LockMask, LockBit)
    Q_FLAG(LockMask)

    enum LockState {
        LockDisabled = PHOTO_CONTROL_STATE_DISABLED,
        Unlocked = PHOTO_CONTROL_STATE_UNLOCKED,
        LockPending = PHOTO_CONTROL_STATE_PENDING,
        Searching = PHOTO_CONTROL_STATE_SEARCHING,
        Locked = PHOTO_CONTROL_STATE_LOCKED,
        ForceLocking = PHOTO_CONTROL_STATE_FORCE_LOCKING,
        ForceLocked = PHOTO_CONTROL_STATE_FORCE_LOCKED,
        Unlocking = PHOTO_CONTROL_STATE_UNLOCKING,
    };
    Q_ENUM(LockState)

    Camera();
    ~Camera();

    QString cameraId() const;
    void setCameraId(const QString &cameraId);

    QVariantMap info() const;

    bool isActive() const;

    bool isConfigured() const;
    Q_INVOKABLE void configure();

    void addTarget(BaseCaptureTarget *target);
    void removeTarget(BaseCaptureTarget *target);

    PhotoControls *controls() const;

    Q_INVOKABLE void trigger(LockMask locks);
    Q_INVOKABLE void forceLock(LockMask locks);
    Q_INVOKABLE void unlock(LockMask locks);

signals:
    void cameraIdChanged();
    void infoChanged();
    void activeChanged();
    void configuredChanged();

    void initFailed();
    void configFailed();

    void shutter();
    void captureDone(bool successful);
    void disconnected();
    void lockStatusChanged(LockMask lockType, LockState oldState, LockState newState);

private:
    static void handleShutter(void *userdata);
    static void handleCaptureDone(void *userdata, bool successful);
    static void handleDisconnected(void *userdata);
    static void handleLockState(void *userdata, PhotoControlLockId lock_id,
                                PhotoControlLockState old_state,
                                PhotoControlLockState new_state);

    void openDevice();
    void resetConfig();
    void closeDevice();

    PhotoCamera *m_camera;
    QString m_cameraId;
    QSet<BaseCaptureTarget *> m_captureTargets;
    bool m_configured;
};

} /* namespace */

#endif
