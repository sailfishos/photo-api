// SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
//
// SPDX-License-Identifier: BSD-3-Clause

#include "camera.h"
#include "controls.h"

namespace photo_api_qt {

MeteringAreas::MeteringAreas(Camera *camera, PhotoControlId id)
    : m_camera(camera)
    , m_id(id)
    , m_numRects(0)
    , m_rects(nullptr)
{
    connect(camera, &Camera::infoChanged, this, &MeteringAreas::update);
}

QVariantList MeteringAreas::rects() const
{
    QVariantList rects;

    for (int i = 0; i < m_numRects; i++) {
        rects << QVariant(QVariantList{
            m_rects[i].left,
            m_rects[i].top,
            m_rects[i].right,
            m_rects[i].bottom,
            m_rects[i].weight
        });
    }

    return rects;
}

void MeteringAreas::setRects(const QVariantList &rects)
{
    delete[] m_rects;

    m_numRects = rects.count();

    if (m_numRects == 0) {
        m_rects = nullptr;
        return;
    }

    m_rects = new PhotoControlMeteringRect[rects.count()];

    for (int i = 0; i < rects.count(); i++) {
        QList list = rects.at(i).toList();

        bool ok = list.count() == 5;
        if (ok)
            m_rects[i].left = list.at(0).toFloat(&ok);
        if (ok)
            m_rects[i].top = list.at(1).toFloat(&ok);
        if (ok)
            m_rects[i].right = list.at(2).toFloat(&ok);
        if (ok)
            m_rects[i].bottom = list.at(3).toFloat(&ok);
        if (ok)
            m_rects[i].weight = list.at(4).toInt(&ok);

        if (!ok) {
            qWarning("MeteringAreas::rects must be a list of (left, top, right, bottom, weight) pairs!");
            delete[] m_rects;
            m_numRects = 0;
            m_rects = nullptr;
            break;
        }
    }

    if (PhotoControls *controls = m_camera->controls()) {
        photo_control_set_areas(controls, m_id, m_numRects, m_rects);
        photo_controls_commit(controls);
    }

    emit rectsChanged();
}

void MeteringAreas::update()
{
    if (PhotoControls *controls = m_camera->controls()) {
        photo_control_set_areas(controls, m_id, m_numRects, m_rects);
    }
}

EnumControl::EnumControl(Camera *camera, PhotoControlId id, unsigned int count)
    : m_value(0)
    , m_camera(camera)
    , m_id(id)
    , m_count(count)
{
    connect(camera, &Camera::infoChanged, this, &EnumControl::update);
}

QVariantList EnumControl::supported() const
{
    return m_supported;
}

void EnumControl::setValue(unsigned int value)
{
    if (m_value != value) {
        m_value = value;
        emit valueChanged();

        if (PhotoControls *controls = m_camera->controls()) {
            photo_control_set_enum(controls, m_id, value);
            photo_controls_commit(controls);
        }
    }
}

void EnumControl::update()
{
    if (PhotoControls *controls = m_camera->controls()) {
        const PhotoBitMask *supported = photo_control_query_enum(controls, m_id);

        m_supported.clear();
        for (unsigned int i = 0; i < m_count; i++) {
            if (photo_check_bit(supported, i)) {
                m_supported << i;
                if (m_value == i) {
                    photo_control_set_enum(controls, m_id, m_value);
                }
            }
        }
        emit supportedChanged();
    }
}

FloatRangeControl::FloatRangeControl(Camera *camera, PhotoControlId id)
    : m_camera(camera)
    , m_id(id)
    , m_range{0.0, 0.0, 0.0}
    , m_value(0.0)
{
    connect(camera, &Camera::infoChanged, this, &FloatRangeControl::update);
}

float FloatRangeControl::min() const
{
    return m_range.min;
}

float FloatRangeControl::max() const
{
    return m_range.max;
}

float FloatRangeControl::step() const
{
    return m_range.step;
}

float FloatRangeControl::value() const
{
    return m_value;
}

void FloatRangeControl::setValue(float value)
{
    if (m_value != value) {
        m_value = value;
        emit valueChanged();

        if (PhotoControls *controls = m_camera->controls()) {
            photo_control_set_float(controls, m_id, value);
            photo_controls_commit(controls);
        }
    }
}

void FloatRangeControl::update()
{
    if (PhotoControls *controls = m_camera->controls()) {
        m_range = *photo_control_query_float(controls, m_id);
        emit supportedChanged();
        if (m_value > m_range.max) {
            m_value = m_range.max;
            emit valueChanged();
        } else if (m_value < m_range.min) {
            m_value = m_range.min;
            emit valueChanged();
        }
        photo_control_set_float(controls, m_id, m_value);
    }
}

} /* namespace */
