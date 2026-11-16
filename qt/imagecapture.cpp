// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include <cstdio>

#include <QDebug>

#include "imagecapture.h"

namespace photo_api_qt {

ImageCapture::ImageCapture()
    : StreamTarget(PHOTO_STREAM_USAGE_STILL_CAPTURE)
{
}

ImageCapture::~ImageCapture()
{
}

void ImageCapture::takePicture()
{
    if (PhotoStream *s = stream()) {
        photo_stream_take_picture(s);
    }
}

void ImageCapture::saveFrame(const QString &path)
{
    if (PhotoStream *s = stream()) {
        if (PhotoBuffer *buffer = photo_stream_get_next_buffer(s)) {
            const PhotoBufferInfo *info = photo_buffer_get_info(buffer);
            if (info->num_planes != 1) {
                qWarning() << "Cannot save image with" << info->num_planes << "planes";
                photo_buffer_release(buffer);
                return;
            }

            const PhotoBufferPlaneData *planes = photo_buffer_map(buffer);
            if (!planes) {
                qWarning() << "Failed to map image buffer for saving to" << path;
                photo_buffer_release(buffer);
                return;
            }

            FILE *f = fopen(path.toLocal8Bit(), "wb");
            if (!f) {
                qWarning() << "Failed to open" << path << "for writing:" << strerror(errno);
                photo_buffer_release(buffer);
                return;
            }

            if (fwrite(planes[0].data, 1, planes[0].length, f) != planes[0].length) {
                qWarning() << "Failed to write image data to" << path;
            }

            fclose(f);

            photo_buffer_release(buffer);
        } else {
            qWarning() << "No image found to save to" << path;
        }
    }
}

} /* namespace */
