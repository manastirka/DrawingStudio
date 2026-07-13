#include "PaperModel.h"

#include "CanvasRenderer.h"

#include <QVector2D>
#include <QVector4D>

PaperModel::PaperModel() = default;

QSizeF PaperModel::sizeForFormat(Format format)
{
    switch (format) {
    case Format::A4:
        return QSizeF(210.0f, 297.0f);
    case Format::A3:
        return QSizeF(297.0f, 420.0f);
    case Format::A2:
        return QSizeF(420.0f, 594.0f);
    case Format::A1:
        return QSizeF(594.0f, 841.0f);
    case Format::A0:
        return QSizeF(841.0f, 1189.0f);
    case Format::Letter:
        return QSizeF(216.0f, 279.0f);
    case Format::Legal:
        return QSizeF(216.0f, 356.0f);
    case Format::Tabloid:
        return QSizeF(279.0f, 432.0f);
    case Format::Custom:
    default:
        return QSizeF();
    }
}

void PaperModel::setFormat(Format format)
{
    m_format = format;
    const QSizeF s = sizeForFormat(format);
    if (s.width() > 0.0 && s.height() > 0.0) {
        m_sizeMm = s;
    }
}

QString PaperModel::formatName() const
{
    switch (m_format) {
    case Format::A4:
        return QStringLiteral("A4 (210×297mm)");
    case Format::A3:
        return QStringLiteral("A3 (297×420mm)");
    case Format::A2:
        return QStringLiteral("A2 (420×594mm)");
    case Format::A1:
        return QStringLiteral("A1 (594×841mm)");
    case Format::A0:
        return QStringLiteral("A0 (841×1189mm)");
    case Format::Letter:
        return QStringLiteral("Letter (8.5×11in)");
    case Format::Legal:
        return QStringLiteral("Legal (8.5×14in)");
    case Format::Tabloid:
        return QStringLiteral("Tabloid (11×17in)");
    case Format::Custom:
        return QStringLiteral("Custom");
    }
    return QStringLiteral("Unknown");
}

QRectF PaperModel::worldRect(float pixelsPerMM) const
{
    const float paperWidth = static_cast<float>(m_sizeMm.width()) * pixelsPerMM;
    const float paperHeight = static_cast<float>(m_sizeMm.height()) * pixelsPerMM;
    return QRectF(-paperWidth / 2.0, -paperHeight / 2.0, paperWidth, paperHeight);
}

void PaperModel::render(CanvasRenderer *renderer, float pixelsPerMM) const
{
    if (!renderer) {
        return;
    }

    const float paperWidth = static_cast<float>(m_sizeMm.width()) * pixelsPerMM;
    const float paperHeight = static_cast<float>(m_sizeMm.height()) * pixelsPerMM;
    const float left = -paperWidth / 2.0f;
    const float right = paperWidth / 2.0f;
    const float bottom = -paperHeight / 2.0f;
    const float top = paperHeight / 2.0f;

    const float shadowOffset = 3.0f;
    {
        auto shadowBatch = renderer->begin(CanvasRenderer::Mode::TriangleFan);
        QVector4D shadowColor(0.5f, 0.5f, 0.5f, 0.3f);
        renderer->addVertex(shadowBatch,
                            QVector2D(left + shadowOffset, bottom - shadowOffset),
                            shadowColor);
        renderer->addVertex(shadowBatch,
                            QVector2D(right + shadowOffset, bottom - shadowOffset),
                            shadowColor);
        renderer->addVertex(shadowBatch,
                            QVector2D(right + shadowOffset, top - shadowOffset),
                            shadowColor);
        renderer->addVertex(shadowBatch,
                            QVector2D(left + shadowOffset, top - shadowOffset),
                            shadowColor);
        renderer->submit(shadowBatch);
    }

    {
        auto paperBatch = renderer->begin(CanvasRenderer::Mode::TriangleFan);
        QVector4D paperColor(m_color.redF(), m_color.greenF(), m_color.blueF(),
                             1.0f);
        renderer->addVertex(paperBatch, QVector2D(left, bottom), paperColor);
        renderer->addVertex(paperBatch, QVector2D(right, bottom), paperColor);
        renderer->addVertex(paperBatch, QVector2D(right, top), paperColor);
        renderer->addVertex(paperBatch, QVector2D(left, top), paperColor);
        renderer->submit(paperBatch);
    }

    {
        auto borderBatch = renderer->begin(CanvasRenderer::Mode::LineLoop, 2.0f);
        QColor borderColor(179, 179, 179);
        renderer->addVertex(borderBatch, QVector2D(left, bottom), borderColor);
        renderer->addVertex(borderBatch, QVector2D(right, bottom), borderColor);
        renderer->addVertex(borderBatch, QVector2D(right, top), borderColor);
        renderer->addVertex(borderBatch, QVector2D(left, top), borderColor);
        renderer->submit(borderBatch);
    }
}
