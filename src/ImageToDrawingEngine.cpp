#include "ImageToDrawingEngine.h"

#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
#include "Layer.h"
#include "LayerManager.h"

#include <QDebug>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector2D>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

// ImageToDrawingEngine core dispatch (refactor E26).

// --- ImageToDrawingEngine ---
ImageToDrawingEngine::ImageToDrawingEngine() = default;


// --- ImageToDrawingEngine ---
ImageToDrawingEngine::ImageToDrawingEngine(Context ctx)
    : m_ctx(std::move(ctx))
{
}


// --- handlesAction ---
bool ImageToDrawingEngine::handlesAction(const QString &action)
{
    return action == QLatin1String("render_mosaic")
        || action == QLatin1String("auto_trace")
        || action == QLatin1String("render_photo_copy");
}


// --- execute ---
QJsonObject ImageToDrawingEngine::execute(const QString &action, const QJsonObject &params)
{
    if (action == QLatin1String("render_mosaic"))
        return renderMosaic(params);
    if (action == QLatin1String("auto_trace"))
        return autoTrace(params);
    if (action == QLatin1String("render_photo_copy"))
        return renderPhotoCopy(params);
    QJsonObject err;
    err.insert(QStringLiteral("success"), false);
    err.insert(QStringLiteral("error"),
               QStringLiteral("Unsupported image-to-drawing action: %1").arg(action));
    return err;
}


// --- parseColor ---
QColor ImageToDrawingEngine::parseColor(const QJsonObject &p, const QString &key,
                                        const QColor &defaultColor)
{
    if (p.contains(key))
        return QColor(p.value(key).toString());
    return defaultColor;
}


// --- getDouble ---
double ImageToDrawingEngine::getDouble(const QJsonObject &p, const QString &key, double def)
{
    if (p.contains(key))
        return p.value(key).toDouble(def);
    return def;
}


// --- getBool ---
bool ImageToDrawingEngine::getBool(const QJsonObject &p, const QString &key, bool def)
{
    if (p.contains(key))
        return p.value(key).toBool(def);
    return def;
}

// --- findImageByIndex ---
ImagePrimitive *ImageToDrawingEngine::findImageByIndex(int index) const
{
    if (m_ctx.findImage)
        return m_ctx.findImage(index);
    if (!m_ctx.canvas || !m_ctx.canvas->layerManager())
        return nullptr;

    auto allPrims = m_ctx.canvas->layerManager()->getAllPrimitives();
    std::vector<ImagePrimitive *> images;
    for (auto *p : allPrims) {
        if (auto *img = dynamic_cast<ImagePrimitive *>(p))
            images.push_back(img);
    }
    if (images.empty())
        return nullptr;
    int resolved = (index < 0) ? static_cast<int>(images.size()) + index : index;
    if (resolved < 0 || resolved >= static_cast<int>(images.size()))
        return nullptr;
    return images[resolved];
}

