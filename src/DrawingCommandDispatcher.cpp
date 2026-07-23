#include "DrawingCommandDispatcher.h"
#include "AutomationCommandCatalog.h"

#include "DrawingPrimitive.h"
#include "DXFExporter.h"
#include "EdgeSelectionTool.h"
#include "ImagePrimitive.h"
#include "ImageToDrawingEngine.h"
#include "Layer.h"
#include "LayerManager.h"

#include <QDebug>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QVector2D>

#include <cmath>
#include <memory>
#include <vector>

// DrawingCommandDispatcher core + execute dispatch (refactor E29).

DrawingCommandDispatcher::DrawingCommandDispatcher() = default;

DrawingCommandDispatcher::~DrawingCommandDispatcher() = default;

void DrawingCommandDispatcher::setHost(Host host)
{
    m_ctx = std::move(host);
}

QColor DrawingCommandDispatcher::parseColor(const QJsonObject &p, const QString &key,
                                            const QColor &defaultColor)
{
    if (p.contains(key)) {
        const QColor parsed(p.value(key).toString());
        if (parsed.isValid())
            return parsed;
    }
    return defaultColor;
}

double DrawingCommandDispatcher::getDouble(const QJsonObject &p, const QString &key, double def)
{
    if (p.contains(key))
        return p.value(key).toDouble(def);
    return def;
}

bool DrawingCommandDispatcher::getBool(const QJsonObject &p, const QString &key, bool def)
{
    if (p.contains(key) && p.value(key).isBool())
        return p.value(key).toBool(def);
    return def;
}

QString DrawingCommandDispatcher::validateAutomationParams(
    const QJsonObject &params)
{
    constexpr int kMaxDepth = 16;
    constexpr qsizetype kMaxContainerItems = 10000;
    constexpr qsizetype kMaxStringCharacters = 1000000;
    constexpr double kMaxNumericMagnitude = 1.0e9;

    std::function<QString(const QJsonValue &, int)> validateValue;
    validateValue = [&](const QJsonValue &value, int depth) -> QString {
        if (depth > kMaxDepth)
            return QStringLiteral("parameter nesting exceeds the limit");
        if (value.isDouble()) {
            const double number = value.toDouble();
            if (!std::isfinite(number)
                || std::abs(number) > kMaxNumericMagnitude) {
                return QStringLiteral(
                    "numeric parameter is outside the supported range");
            }
        } else if (value.isString()) {
            if (value.toString().size() > kMaxStringCharacters)
                return QStringLiteral("string parameter exceeds the limit");
        } else if (value.isArray()) {
            const QJsonArray array = value.toArray();
            if (array.size() > kMaxContainerItems)
                return QStringLiteral("array parameter exceeds the limit");
            for (const QJsonValue &entry : array) {
                const QString error = validateValue(entry, depth + 1);
                if (!error.isEmpty())
                    return error;
            }
        } else if (value.isObject()) {
            const QJsonObject object = value.toObject();
            if (object.size() > kMaxContainerItems)
                return QStringLiteral("object parameter exceeds the limit");
            for (auto it = object.begin(); it != object.end(); ++it) {
                const QString error = validateValue(it.value(), depth + 1);
                if (!error.isEmpty())
                    return error;
            }
        }
        return {};
    };
    return validateValue(params, 0);
}

bool DrawingCommandDispatcher::exportCanvasToFileHost(const QString &path, const QString &format,
                                                      int quality)
{
    if (!m_ctx.exportCanvasToFile)
        return false;
    return m_ctx.exportCanvasToFile(path, format, quality);
}

QString DrawingCommandDispatcher::imageFormatFromPathHost(const QString &path) const
{
    if (!m_ctx.imageFormatFromPath)
        return QString();
    return m_ctx.imageFormatFromPath(path);
}

QString DrawingCommandDispatcher::ensureImageExtensionHost(const QString &path,
                                                          const QString &format) const
{
    if (!m_ctx.ensureImageExtension)
        return path;
    return m_ctx.ensureImageExtension(path, format);
}

QString DrawingCommandDispatcher::imageExportFilterStringHost() const
{
    if (!m_ctx.imageExportFilterString)
        return QString();
    return m_ctx.imageExportFilterString();
}

bool DrawingCommandDispatcher::saveProjectToFileHost(const QString &path)
{
    if (!m_ctx.saveProjectToFile)
        return false;
    return m_ctx.saveProjectToFile(path);
}

bool DrawingCommandDispatcher::loadProjectFromFileHost(const QString &path, bool waitUntilLoaded)
{
    if (!m_ctx.loadProjectFromFile)
        return false;
    return m_ctx.loadProjectFromFile(path, waitUntilLoaded);
}

QJsonObject DrawingCommandDispatcher::runImageToDrawing(const QString &action,
                                                       const QJsonObject &params)
{
    if (!m_ctx.imageToDrawingEngine)
        return QJsonObject();
    ImageToDrawingEngine *engine = m_ctx.imageToDrawingEngine();
    if (!engine)
        return QJsonObject();
    return engine->execute(action, params);
}

void DrawingCommandDispatcher::applyCommonParams(DrawingPrimitive *prim, const QJsonObject &params)
{

if (!prim) return;

// Opacity
if (params.contains("opacity"))
    prim->setOpacityMultiplier(params["opacity"].toDouble(1.0));

// Shadow
if (params.contains("shadow")) {
    QJsonObject s = params["shadow"].toObject();
    prim->setShadowEnabled(s["enabled"].toBool(true));
    prim->setShadowColor(QColor(s["color"].toString("#00000080")));
    prim->setShadowOffset(s["offsetX"].toDouble(5), s["offsetY"].toDouble(5));
    prim->setShadowBlur(s["blur"].toDouble(5));
}

// Gradient fill
if (params.contains("gradient")) {
    QJsonObject g = params["gradient"].toObject();
    QString type = g["type"].toString("linear");
    prim->setGradientFillType(type == "radial"
        ? DrawingPrimitive::GradientFillType::Radial
        : DrawingPrimitive::GradientFillType::Linear);
    prim->setGradientStartColor(QColor(g["startColor"].toString("#FFFFFF")));
    prim->setGradientEndColor(QColor(g["endColor"].toString("#000000")));
    prim->setGradientAngle(g["angle"].toDouble(0));
}
}

ImagePrimitive *DrawingCommandDispatcher::findImageByIndex(int index)
{

auto allPrims = m_ctx.canvas->layerManager()->getAllPrimitives();
std::vector<ImagePrimitive*> images;
for (auto* p : allPrims) {
    if (auto* img = dynamic_cast<ImagePrimitive*>(p))
        images.push_back(img);
}
if (images.empty()) return nullptr;
// Negative index: count from end (-1 = last)
int resolved = (index < 0) ? static_cast<int>(images.size()) + index : index;
if (resolved < 0 || resolved >= static_cast<int>(images.size())) return nullptr;
return images[resolved];
}

ImagePrimitive *DrawingCommandDispatcher::imageForDetection()
{

if (!m_ctx.canvas || !m_ctx.canvas->layerManager()) return nullptr;

for (auto *obj : m_ctx.canvas->selectedObjects()) {
    if (auto *ip = dynamic_cast<ImagePrimitive *>(obj)) {
        return ip;
    }
}

ImagePrimitive *sole = nullptr;
int count = 0;
for (auto *p : m_ctx.canvas->layerManager()->getAllPrimitives()) {
    if (auto *ip = dynamic_cast<ImagePrimitive *>(p)) {
        sole = ip;
        if (++count > 1) {
            return nullptr;
        }
    }
}
return count == 1 ? sole : nullptr;
}

void DrawingCommandDispatcher::selectImageForMaskUI(ImagePrimitive *image)
{

if (!m_ctx.canvas || !image) return;
// Keep other selected images — accumulate green-masked subjects
image->setSelected(true);
image->setMaskOverlayVisible(true);
m_ctx.canvas->addToSelection(image);
m_ctx.canvas->update();
}

ImagePrimitive *DrawingCommandDispatcher::selectedImageWithMasks()
{

if (!m_ctx.canvas) return nullptr;

ImagePrimitive *imgPrim = nullptr;
for (auto *obj : m_ctx.canvas->selectedObjects()) {
    if (auto *ip = dynamic_cast<ImagePrimitive *>(obj)) {
        imgPrim = ip;
        break;
    }
}

if ((!imgPrim || imgPrim->getMaskCandidateCount() == 0) &&
    m_ctx.canvas->layerManager()) {
    for (auto *p : m_ctx.canvas->layerManager()->getAllPrimitives()) {
        if (auto *ip = dynamic_cast<ImagePrimitive *>(p)) {
            if (ip->getMaskCandidateCount() > 0) {
                imgPrim = ip;
                break;
            }
        }
    }
}

return (imgPrim && imgPrim->getMaskCandidateCount() > 0) ? imgPrim : nullptr;
}

void DrawingCommandDispatcher::execute(const QString &action, const QJsonObject &params)
{
    // Clear previous result so analysis commands can populate it
    m_lastResult = QJsonObject();

    if (!AutomationCommandCatalog::containsAction(action)) {
        qWarning() << "CommandServer: Unknown action:" << action;
        m_lastResult["success"] = false;
        m_lastResult["error"] = QStringLiteral("Unknown action: %1").arg(action);
        return;
    }

    const QString parameterError = validateAutomationParams(params);
    if (!parameterError.isEmpty()) {
        m_lastResult["success"] = false;
        m_lastResult["error"] =
            QStringLiteral("Invalid parameters: ") + parameterError;
        return;
    }

    if (!m_ctx.canvas) {
        m_lastResult["success"] = false;
        m_lastResult["error"] = QStringLiteral("Canvas unavailable");
        return;
    }

    if (tryExecuteDraw(action, params)
        || tryExecuteEdit(action, params)
        || tryExecuteIO(action, params)
        || tryExecuteImage(action, params)) {
        m_ctx.canvas->update();
        return;
    }

    // Server-side rendering actions (photorealistic image-to-drawing)
    if (ImageToDrawingEngine::handlesAction(action)) {
        m_lastResult = runImageToDrawing(action, params);
        m_ctx.canvas->update();
        return;
    }

    qCritical() << "Automation catalog action has no dispatcher handler:" << action;
    m_lastResult["success"] = false;
    m_lastResult["error"] =
        QStringLiteral("Catalog mismatch: no handler for %1").arg(action);
}
