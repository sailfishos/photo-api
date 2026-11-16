/*
 * SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PHOTO_API_QT_CONTROLS_H
#define PHOTO_API_QT_CONTROLS_H

#include <QObject>
#include <QRect>
#include <QVariant>

#include <photo-controls.h>

namespace photo_api_qt {

class Camera;

class MeteringAreas : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList rects READ rects WRITE setRects NOTIFY rectsChanged)

public:
    MeteringAreas(Camera *camera, PhotoControlId id);

    QVariantList rects() const;
    void setRects(const QVariantList &rects);

signals:
    void rectsChanged();

private slots:
    void update();

private:
    Camera *m_camera;
    PhotoControlId m_id;
    int m_numRects;
    PhotoControlMeteringRect *m_rects;
};

class EnumControl : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList supported READ supported NOTIFY supportedChanged)

public:
    EnumControl(Camera *camera, PhotoControlId id, unsigned int count);

    QVariantList supported() const;

    void setValue(unsigned int value);

    virtual void valueChanged() = 0;

signals:
    void supportedChanged();

protected:
    unsigned int m_value;

private slots:
    void update();

private:
    Camera *m_camera;
    PhotoControlId m_id;
    unsigned int m_count;
    QVariantList m_supported;
};

class FloatRangeControl : public QObject
{
    Q_OBJECT

    Q_PROPERTY(float min READ min NOTIFY supportedChanged)
    Q_PROPERTY(float max READ max NOTIFY supportedChanged)
    Q_PROPERTY(float step READ step NOTIFY supportedChanged)
    Q_PROPERTY(float value READ value WRITE setValue NOTIFY valueChanged)

public:
    FloatRangeControl(Camera *camera, PhotoControlId id);

    float min() const;
    float max() const;
    float step() const;

    float value() const;
    void setValue(float value);

signals:
    void supportedChanged();
    void valueChanged();

private slots:
    void update();

private:
    Camera *m_camera;
    PhotoControlId m_id;
    PhotoControlFloatRange m_range;
    float m_value;
};

} /* namespace */

#endif
