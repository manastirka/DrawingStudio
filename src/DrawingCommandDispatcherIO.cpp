#include "DrawingCommandDispatcher.h"

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

#include <memory>
#include <vector>

// Import / export / project I/O (refactor E29).

bool DrawingCommandDispatcher::tryExecuteIO(const QString &action, const QJsonObject &params)
{
    if (action == "export_dxf") {
    QString path = params.value("path").toString();
    QJsonObject result;
    if (path.isEmpty() || !m_ctx.layerManager) {
        result["success"] = false;
        result["error"] = QStringLiteral("Missing path or layer manager");
    } else {
        if (!path.endsWith(".dxf", Qt::CaseInsensitive))
            path += ".dxf";
        DXFExporter exporter;
        DXFExporter::Options options;
        if (m_ctx.canvas) {
            switch (m_ctx.canvas->getUnits()) {
            case DrawingCanvas::Units::Millimeters:
                options.units = DXFExporter::Units::Millimeters;
                break;
            case DrawingCanvas::Units::Centimeters:
                options.units = DXFExporter::Units::Centimeters;
                break;
            case DrawingCanvas::Units::Inches:
                options.units = DXFExporter::Units::Inches;
                break;
            }
            options.scaleFactor = 1.0 / m_ctx.canvas->pixelsPerUnit();
        }
        const bool ok = exporter.exportToFile(path, m_ctx.layerManager->layers(), options);
        result["success"] = ok;
        result["path"] = path;
        if (!ok)
            result["error"] = QStringLiteral("DXF export failed");
    }
    m_lastResult = result;
        return true;
    }

    if (action == "export_png") {
    QString path = params["path"].toString();
    if (!path.isEmpty()) {
        const bool ok = exportCanvasToFileHost(path, "PNG");
        QJsonObject result;
        result["success"] = ok;
        result["path"] = path;
        result["format"] = "PNG";
        if (ok) {
            result["bytes"] = static_cast<qint64>(QFileInfo(path).size());
        }
        m_lastResult = result;
    }
        return true;
    }

    if (action == "export_image") {
    QString path = params["path"].toString();
    QString format = params["format"].toString();
    int quality = params.contains("quality") ? params["quality"].toInt(-1) : -1;
    if (!path.isEmpty()) {
        if (format.isEmpty()) {
            format = imageFormatFromPathHost(path);
        }
        path = ensureImageExtensionHost(path, format);
        const bool ok = exportCanvasToFileHost(path, format, quality);
        QJsonObject result;
        result["success"] = ok;
        result["path"] = path;
        result["format"] = format.toUpper();
        if (ok) {
            result["bytes"] = static_cast<qint64>(QFileInfo(path).size());
        }
        m_lastResult = result;
    }
        return true;
    }

    if (action == "list_export_formats") {
    QJsonArray formats;
    for (const QByteArray &fmt : QImageWriter::supportedImageFormats()) {
        formats.append(QString::fromLatin1(fmt));
    }
    QJsonObject result;
    result["formats"] = formats;
    result["filter"] = imageExportFilterStringHost();
    m_lastResult = result;
        return true;
    }

    if (action == "save_project") {
    QString path = params["path"].toString();
    if (path.isEmpty()) {
        qWarning() << "save_project: missing 'path' param";
    } else {
        QString savePath = path;
        if (!savePath.endsWith(".drawing", Qt::CaseInsensitive)) {
            savePath += ".drawing";
        }
        const bool ok = saveProjectToFileHost(savePath);
        QJsonObject result;
        result["success"] = ok;
        result["path"] = savePath;
        if (ok) {
            result["bytes"] = static_cast<qint64>(QFileInfo(savePath).size());
        }
        m_lastResult = result;
    }
        return true;
    }

    if (action == "open_project") {
    QString path = params["path"].toString();
    if (path.isEmpty()) {
        qWarning() << "open_project: missing 'path' param";
    } else {
        const bool ok = loadProjectFromFileHost(path, true);
        QJsonObject result;
        result["success"] = ok;
        result["path"] = path;
        if (ok && m_ctx.canvas && m_ctx.canvas->layerManager()) {
            int count = 0;
            for (auto *p : m_ctx.canvas->layerManager()->getAllPrimitives()) {
                if (p) ++count;
            }
            result["objectCount"] = count;
        }
        m_lastResult = result;
    }
        return true;
    }

    if (action == "import_image") {
    QString path = params["path"].toString();
    if (path.isEmpty()) {
        qWarning() << "import_image: missing 'path' param";
    } else {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        reader.setDecideFormatFromContent(true);

        // Scale down large images during load
        QSize originalSize = reader.size();
        const int MAX_LOAD_DIMENSION = 2048;
        if (originalSize.width() > MAX_LOAD_DIMENSION ||
            originalSize.height() > MAX_LOAD_DIMENSION) {
            QSize scaledSize = originalSize.scaled(
                MAX_LOAD_DIMENSION, MAX_LOAD_DIMENSION, Qt::KeepAspectRatio);
            reader.setScaledSize(scaledSize);
        }

        QImage image = reader.read();
        if (image.isNull()) {
            qWarning() << "import_image: failed to load" << path;
        } else {
            double x = getDouble(params, "x", 0.0);
            double y = getDouble(params, "y", 0.0);

            // Auto-size if width/height omitted: fit to 300 world units
            double w, h;
            float aspect = static_cast<float>(image.width()) / static_cast<float>(image.height());
            if (params.contains("width") || params.contains("height")) {
                if (params.contains("width") && params.contains("height")) {
                    w = getDouble(params, "width");
                    h = getDouble(params, "height");
                } else if (params.contains("width")) {
                    w = getDouble(params, "width");
                    h = w / aspect;
                } else {
                    h = getDouble(params, "height");
                    w = h * aspect;
                }
            } else {
                double maxSize = 300.0;
                if (aspect > 1.0f) {
                    w = maxSize;
                    h = maxSize / aspect;
                } else {
                    h = maxSize;
                    w = maxSize * aspect;
                }
            }

            auto imgPrim = std::make_unique<ImagePrimitive>(
                image, QVector2D(x, y), QVector2D(w, h));
            imgPrim->setColor(Qt::black);
            m_ctx.canvas->addPrimitiveWithCommand(std::move(imgPrim));

            // Count image primitives to determine index
            int imageIndex = 0;
            auto allPrims = m_ctx.canvas->layerManager()->getAllPrimitives();
            for (auto* p : allPrims) {
                if (dynamic_cast<ImagePrimitive*>(p)) imageIndex++;
            }
            imageIndex--; // zero-based: just-added is the last

            QJsonObject result;
            result["imageWidth"] = image.width();
            result["imageHeight"] = image.height();
            result["worldX"] = x;
            result["worldY"] = y;
            result["worldWidth"] = w;
            result["worldHeight"] = h;
            result["imageIndex"] = imageIndex;
            m_lastResult = result;
        }
    }
        return true;
    }

    return false;
}
