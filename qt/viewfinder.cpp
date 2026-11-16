// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include <QGuiApplication>
#include <QQuickWindow>
#include <qpa/qplatformnativeinterface.h>

#include <photo-egl.h>

#include "viewfinder.h"

namespace photo_api_qt {

static const PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES
    = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
static const PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR
    = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));

EglImageTexture::EglImageTexture(EGLImageKHR eglImage, const QSize &size)
    : m_eglImage(eglImage)
    , m_textureSize(size)
{
    glGenTextures(1, &m_textureId);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_textureId);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, eglImage);
}

EglImageTexture::~EglImageTexture()
{
    glDeleteTextures(1, &m_textureId);
}

int EglImageTexture::textureId() const
{
    return m_textureId;
}

QSize EglImageTexture::textureSize() const
{
    return m_textureSize;
}

bool EglImageTexture::hasAlphaChannel() const
{
    return false;
}

bool EglImageTexture::hasMipmaps() const
{
    return false;
}

void EglImageTexture::bind()
{
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_textureId);
}

// This is based on the GStreamerVideoMaterialShader from the Nemo QtMultimedia backend.
class EglImageMaterialShader : public QSGMaterialShader
{
public:
    static QSGMaterialType type;

    void updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect);
    char const *const *attributeNames() const;

protected:
    void initialize();

    const char *vertexShader() const;
    const char *fragmentShader() const;

private:
    int id_matrix;
    int id_subrect;
    int id_opacity;
    int id_texture;
};

void EglImageMaterialShader::updateState(
        const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    EglImageMaterial *material = static_cast<EglImageMaterial *>(newEffect);

    if (state.isMatrixDirty()) {
        program()->setUniformValue(id_matrix, state.combinedMatrix());
    }

    if (state.isOpacityDirty()) {
        program()->setUniformValue(id_opacity, state.opacity());
    }

    if (!oldEffect) {
        program()->setUniformValue(id_texture, 0);
    }

    if (!material->m_texture) {
        /* reset texture state */
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
        return;
    }

    const QRectF subRect = material->m_texture->normalizedTextureSubRect();
    program()->setUniformValue(
                id_subrect, QVector4D(subRect.x(), subRect.y(), subRect.width(), subRect.height()));

    glActiveTexture(GL_TEXTURE0);
    material->m_texture->bind();
}

char const *const *EglImageMaterialShader::attributeNames() const
{
    static char const *const attr[] = { "position", "texcoord", 0 };
    return attr;
}

void EglImageMaterialShader::initialize()
{
    id_matrix = program()->uniformLocation("matrix");
    id_subrect = program()->uniformLocation("subrect");
    id_opacity = program()->uniformLocation("opacity");
    id_texture = program()->uniformLocation("texture");
}

QSGMaterialType EglImageMaterialShader::type;

const char *EglImageMaterialShader::vertexShader() const
{
    return  "\n uniform highp mat4 matrix;"
            "\n uniform highp vec4 subrect;"
            "\n attribute highp vec4 position;"
            "\n attribute highp vec2 texcoord;"
            "\n varying highp vec2 frag_tx;"
            "\n void main(void)"
            "\n {"
            "\n     gl_Position = matrix * position;"
            "\n     frag_tx = (texcoord * subrect.zw) + subrect.xy;"
            "\n }";
}

const char *EglImageMaterialShader::fragmentShader() const
{
    return  "\n #extension GL_OES_EGL_image_external : require"
            "\n uniform samplerExternalOES texture;"
            "\n uniform lowp float opacity;"
            "\n varying highp vec2 frag_tx;"
            "\n void main(void)"
            "\n {"
            "\n     gl_FragColor = opacity * texture2D(texture, frag_tx.st);"
            "\n }";
}

EglImageMaterial::EglImageMaterial()
    : m_texture(nullptr)
{
}

QSGMaterialShader *EglImageMaterial::createShader() const
{
    return new EglImageMaterialShader;
}

QSGMaterialType *EglImageMaterial::type() const
{
    return &EglImageMaterialShader::type;
}

int EglImageMaterial::compare(const QSGMaterial *other) const
{
    return m_texture - static_cast<const EglImageMaterial *>(other)->m_texture;
}

ViewfinderNode::ViewfinderNode(StreamTarget *st, EGLDisplay eglDisplay)
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
    , m_stream(st)
    , m_eglDisplay(eglDisplay)
    , m_frontBuffer(nullptr)
    , m_waitStopped(false)
    , m_running(false)
{
    setMaterial(&m_material);
    setGeometry(&m_geometry);
}

ViewfinderNode::~ViewfinderNode()
{
    for (EglImageTexture *texture : m_cachedTextures) {
        eglDestroyImageKHR(m_eglDisplay, texture->eglImage());
        delete texture;
    }
}

void ViewfinderNode::setResolution(const QSize &resolution)
{
    if (m_resolution != resolution) {
        m_resolution = resolution;
    }
}

void ViewfinderNode::setRect(const QRectF &rect)
{
    if (m_rect != rect) {
        m_rect = rect;

        float left = rect.left();
        float top = rect.top();
        float right = rect.right();
        float bottom = rect.bottom();

        QSGGeometry::TexturedPoint2D vertices[] = {
            { left,  top,    0.0, 0.0 },
            { left,  bottom, 0.0, 1.0 },
            { right, top,    1.0, 0.0 },
            { right, bottom, 1.0, 1.0 }
        };

        memcpy(m_geometry.vertexDataAsTexturedPoint2D(), vertices, sizeof(vertices));
        markDirty(DirtyGeometry);
    }
}

void ViewfinderNode::releaseBuffer()
{
    if (m_frontBuffer) {
        photo_buffer_release(m_frontBuffer);
        m_frontBuffer = nullptr;
    }

    m_material.m_texture = nullptr;
    markDirty(DirtyMaterial);
}

void ViewfinderNode::update()
{
    // m_waitStopped is changed from the main thread, but this should be fine
    // since it blocked while this beforeRendering() handler runs.
    if (m_waitStopped) {
        if (m_running) {
            releaseBuffer();
            return;
        }

        m_waitStopped = false;
    }

    PhotoStream *stream = m_stream->stream();
    if (!stream) {
        releaseBuffer();
        return;
    }

    PhotoBuffer *buffer = photo_stream_get_last_buffer(stream);
    if (!buffer)
        return;

    m_running = true;

    auto cached = m_cachedTextures.find(buffer);
    if (cached != m_cachedTextures.end()) {
        m_material.m_texture = *cached;
    } else {
        EGLImageKHR image = photo_buffer_create_egl_image(buffer, m_eglDisplay, EGL_NO_CONTEXT);

        if (image == EGL_NO_IMAGE_KHR) {
            qWarning() << "Failed to create EGLImage from buffer";
            photo_buffer_release(buffer);
            return;
        }

        m_material.m_texture = new EglImageTexture(image, m_resolution);

        m_cachedTextures.insert(buffer, m_material.m_texture);
    }

    if (m_frontBuffer) {
        photo_buffer_release(m_frontBuffer);
    }

    m_frontBuffer = buffer;

    markDirty(DirtyMaterial);
}

void ViewfinderNode::handleBufferInvalidated(PhotoBuffer *buffer)
{
    if (buffer == m_frontBuffer) {
        releaseBuffer();
    }

    EglImageTexture *texture = m_cachedTextures.take(buffer);
    if (texture) {
        eglDestroyImageKHR(m_eglDisplay, texture->eglImage());
        delete texture;
    }
}

void ViewfinderNode::beginStop()
{
    // This is called from the main thread using a Qt::DirectConnection.
    m_waitStopped = true;
}

void ViewfinderNode::handleStopped()
{
    // Cannot change m_waitStopped here, since this is not the main thread.
    m_running = false;
}

Viewfinder::Viewfinder()
    : m_stream(PHOTO_STREAM_USAGE_PREVIEW)
{
    if (QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface()) {
        m_eglDisplay = nativeInterface->nativeResourceForIntegration("egldisplay");
    }
    if (m_eglDisplay == EGL_NO_DISPLAY) {
        m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    }

    connect(&m_stream, &StreamTarget::afterCameraRequest, this, &Viewfinder::handleConfigured);
    setFlag(ItemHasContents);
}

Viewfinder::~Viewfinder()
{
}

StreamTarget *Viewfinder::stream()
{
    return &m_stream;
}

QSGNode *Viewfinder::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    ViewfinderNode *node = static_cast<ViewfinderNode *>(oldNode);

    if (!node) {
        node = new ViewfinderNode(&m_stream, m_eglDisplay);
        connect(&m_stream, &StreamTarget::frameAvailable, window(), &QQuickWindow::update);
        connect(window(), &QQuickWindow::beforeRendering, node, &ViewfinderNode::update);
        connect(&m_stream, &StreamTarget::bufferInvalidated, node, &ViewfinderNode::handleBufferInvalidated);
        connect(&m_stream, &StreamTarget::beforeCameraRelease, node, &ViewfinderNode::beginStop, Qt::DirectConnection);
        connect(&m_stream, &StreamTarget::stopped, node, &ViewfinderNode::handleStopped);
    }

    node->setResolution(m_resolution);
    node->setRect(boundingRect());

    return node;
}

void Viewfinder::handleConfigured()
{
    const PhotoFixedConfig *info = m_stream.fixedConfig();

    m_resolution = QSize(info->width, info->height);

    photo_stream_start(m_stream.stream());
}

} /* namespace */
