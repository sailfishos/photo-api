// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include <QtQml>

#include "camera.h"
#include "cameramanager.h"
#include "imagecapture.h"
#include "videocapture.h"
#include "viewfinder.h"

#include "generated/ctl-classes.h"

using namespace photo_api_qt;

template<class T>
static QObject *api_factory(QQmlEngine *, QJSEngine *)
{
    return new T();
}

class Q_DECL_EXPORT PhotoPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.sailfishos.PhotoApi")

public:
    virtual ~PhotoPlugin() {}

    void initializeEngine(QQmlEngine *, const char *uri)
    {
        Q_ASSERT(uri == QLatin1String("org.sailfishos.PhotoApi"));
    }

    void registerTypes(const char *uri)
    {
        qmlRegisterType<Camera>(uri, 0, 1, "Camera");
        qmlRegisterUncreatableType<StreamTarget>(uri, 0, 1, "StreamTarget", "use ImageCapture, VideoCapture or Viewfinder");
        qmlRegisterType<ImageCapture>(uri, 0, 1, "ImageCapture");
        qmlRegisterType<VideoCapture>(uri, 0, 1, "VideoCapture");
        qmlRegisterType<Viewfinder>(uri, 0, 1, "Viewfinder");
        qmlRegisterSingletonType<CameraManager>(uri, 0, 1, "CameraManager", api_factory<CameraManager>);
        qmlRegisterUncreatableType<MeteringAreas>(uri, 0, 1, "MeteringAreas", "control is part of camera");
#include "generated/ctl-enum-registration.h"
        qmlRegisterUncreatableType<FloatRangeControl>(uri, 0, 1, "FloatRangeControl", "control is part of camera");
    }
};

#include "qmlplugin.moc"
