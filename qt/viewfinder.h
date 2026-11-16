/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_API_QT_VIEWFINDER_H
#define PHOTO_API_QT_VIEWFINDER_H

#include <QHash>
#include <QOpenGLContext>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGTexture>
#include <QQuickItem>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <photo-api.h>

#include "streamtarget.h"

Q_DECLARE_OPAQUE_POINTER(PhotoBuffer *);

namespace photo_api_qt {

class EglImageTexture : public QSGTexture
{
    Q_OBJECT

public:
    EglImageTexture(EGLImageKHR eglImage, const QSize &size);
    ~EglImageTexture();

    int textureId() const override;
    QSize textureSize() const override;
    bool hasAlphaChannel() const override;
    bool hasMipmaps() const override;

    void bind() override;

    EGLImageKHR eglImage() const { return m_eglImage; }

private:
    EGLImageKHR m_eglImage;
    GLuint m_textureId;
    QSize m_textureSize;
};

class EglImageMaterial : public QSGMaterial
{
public:
    EglImageMaterial();

    QSGMaterialShader *createShader() const override;
    QSGMaterialType *type() const override;
    int compare(const QSGMaterial *other) const override;

private:
    friend class EglImageMaterialShader;
    friend class ViewfinderNode;

    EglImageTexture *m_texture;
};

class ViewfinderNode : public QObject, public QSGGeometryNode
{
    Q_OBJECT

public:
    ViewfinderNode(StreamTarget *st, EGLDisplay eglDisplay);
    ~ViewfinderNode();

    void setResolution(const QSize &resolution);
    void setRect(const QRectF &rect);

public slots:
    void update();
    void handleBufferInvalidated(PhotoBuffer *buffer);
    void beginStop();
    void handleStopped();

private:
    void releaseBuffer();
    void updateGeometry();

    QSGGeometry m_geometry;
    EglImageMaterial m_material;
    StreamTarget *m_stream;
    EGLDisplay m_eglDisplay;
    PhotoBuffer *m_frontBuffer;
    QHash<PhotoBuffer *, EglImageTexture *> m_cachedTextures;
    QSize m_resolution;
    QRectF m_rect;
    bool m_waitStopped;
    bool m_running;
};

class Viewfinder : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(photo_api_qt::StreamTarget *stream READ stream CONSTANT)

public:
    Viewfinder();
    ~Viewfinder();

    StreamTarget *stream();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private slots:
    void handleConfigured();

private:
    EGLDisplay m_eglDisplay;
    StreamTarget m_stream;
    QSize m_resolution;
};

} /* namespace */

#endif
