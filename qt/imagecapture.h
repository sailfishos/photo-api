/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_API_QT_IMAGECAPTURE_H
#define PHOTO_API_QT_IMAGECAPTURE_H

#include "streamtarget.h"

namespace photo_api_qt {

class ImageCapture : public StreamTarget
{
    Q_OBJECT

public:
    ImageCapture();
    ~ImageCapture();

    Q_INVOKABLE void takePicture();
    Q_INVOKABLE void saveFrame(const QString &path);
};

} /* namespace */

#endif
