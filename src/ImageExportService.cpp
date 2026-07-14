#include "ImageExportService.h"

#include "DrawingCanvas.h"
#include "DXFExporter.h"
#include "LayerManager.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QProgressDialog>
#include <QWidget>
QString ImageExportService::imageFormatFromPath(const QString &path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "jpg" || ext == "jpeg" || ext == "jfif") return QStringLiteral("JPEG");
    if (ext == "png") return QStringLiteral("PNG");
    if (ext == "bmp") return QStringLiteral("BMP");
    if (ext == "tif" || ext == "tiff") return QStringLiteral("TIFF");
    if (ext == "webp") return QStringLiteral("WEBP");
    if (ext == "gif") return QStringLiteral("GIF");
    if (ext == "ppm") return QStringLiteral("PPM");
    if (ext == "pbm") return QStringLiteral("PBM");
    if (ext == "pgm") return QStringLiteral("PGM");
    if (ext == "xbm") return QStringLiteral("XBM");
    if (ext == "xpm") return QStringLiteral("XPM");
    if (ext == "ico") return QStringLiteral("ICO");
    if (ext == "icns") return QStringLiteral("ICNS");
    if (ext == "jp2") return QStringLiteral("JP2");
    if (ext == "heic") return QStringLiteral("HEIC");
    if (ext == "heif") return QStringLiteral("HEIF");
    return QStringLiteral("PNG");
}

QString ImageExportService::defaultExtensionForFormat(const QString &format) {
    const QString fmt = format.toUpper();
    if (fmt == QLatin1String("JPEG") || fmt == QLatin1String("JPG"))
        return QStringLiteral(".jpg");
    if (fmt == QLatin1String("PNG")) return QStringLiteral(".png");
    if (fmt == QLatin1String("BMP")) return QStringLiteral(".bmp");
    if (fmt == QLatin1String("TIFF") || fmt == QLatin1String("TIF"))
        return QStringLiteral(".tiff");
    if (fmt == QLatin1String("WEBP")) return QStringLiteral(".webp");
    if (fmt == QLatin1String("GIF")) return QStringLiteral(".gif");
    if (fmt == QLatin1String("PPM")) return QStringLiteral(".ppm");
    if (fmt == QLatin1String("PBM")) return QStringLiteral(".pbm");
    if (fmt == QLatin1String("PGM")) return QStringLiteral(".pgm");
    if (fmt == QLatin1String("XBM")) return QStringLiteral(".xbm");
    if (fmt == QLatin1String("XPM")) return QStringLiteral(".xpm");
    if (fmt == QLatin1String("ICO")) return QStringLiteral(".ico");
    if (fmt == QLatin1String("ICNS")) return QStringLiteral(".icns");
    if (fmt == QLatin1String("JP2")) return QStringLiteral(".jp2");
    if (fmt == QLatin1String("HEIC")) return QStringLiteral(".heic");
    if (fmt == QLatin1String("HEIF")) return QStringLiteral(".heif");
    return QStringLiteral(".png");
}

QString ImageExportService::ensureImageExtension(const QString &path, const QString &format) {
    if (path.isEmpty()) return path;
    const QString expected = ImageExportService::defaultExtensionForFormat(format);
    const QString suffix = QFileInfo(path).suffix();
    if (suffix.isEmpty()) {
        return path + expected;
    }

    // If the existing suffix already maps to the requested format, keep it.
    if (ImageExportService::imageFormatFromPath(path).compare(format, Qt::CaseInsensitive) == 0) {
        return path;
    }

    // Replace mismatched extension with the format default.
    QFileInfo info(path);
    return info.path() + QLatin1Char('/') + info.completeBaseName() + expected;
}

QString ImageExportService::imageExportFilterString() {
    // Prefer formats Qt can actually write on this build.
    const QList<QByteArray> supported = QImageWriter::supportedImageFormats();
    auto has = [&](const char *fmt) {
        return supported.contains(QByteArray(fmt));
    };

    QStringList filters;
    filters << QStringLiteral("PNG (*.png)");
    if (has("jpg") || has("jpeg"))
        filters << QStringLiteral("JPEG (*.jpg *.jpeg)");
    if (has("bmp"))
        filters << QStringLiteral("BMP (*.bmp)");
    if (has("tif") || has("tiff"))
        filters << QStringLiteral("TIFF (*.tif *.tiff)");
    if (has("webp"))
        filters << QStringLiteral("WebP (*.webp)");
    if (has("gif"))
        filters << QStringLiteral("GIF (*.gif)");
    if (has("ppm"))
        filters << QStringLiteral("PPM (*.ppm)");
    if (has("jp2"))
        filters << QStringLiteral("JPEG 2000 (*.jp2)");
    if (has("ico"))
        filters << QStringLiteral("ICO (*.ico)");
    if (has("heic"))
        filters << QStringLiteral("HEIC (*.heic)");
    if (has("xbm"))
        filters << QStringLiteral("XBM (*.xbm)");
    if (has("xpm"))
        filters << QStringLiteral("XPM (*.xpm)");
    filters << QStringLiteral("All Files (*)");
    return filters.join(QStringLiteral(";;"));
}

QString ImageExportService::formatFromFilter(const QString &selectedFilter) {
    const QString f = selectedFilter.toLower();
    if (f.contains(QLatin1String("jpeg 2000")) || f.contains(QLatin1String("*.jp2")))
        return QStringLiteral("JP2");
    if (f.contains(QLatin1String("jpeg")) || f.contains(QLatin1String("*.jpg")))
        return QStringLiteral("JPEG");
    if (f.contains(QLatin1String("png"))) return QStringLiteral("PNG");
    if (f.contains(QLatin1String("bmp"))) return QStringLiteral("BMP");
    if (f.contains(QLatin1String("tiff")) || f.contains(QLatin1String("*.tif")))
        return QStringLiteral("TIFF");
    if (f.contains(QLatin1String("webp"))) return QStringLiteral("WEBP");
    if (f.contains(QLatin1String("gif"))) return QStringLiteral("GIF");
    if (f.contains(QLatin1String("ppm"))) return QStringLiteral("PPM");
    if (f.contains(QLatin1String("heic"))) return QStringLiteral("HEIC");
    if (f.contains(QLatin1String("xbm"))) return QStringLiteral("XBM");
    if (f.contains(QLatin1String("xpm"))) return QStringLiteral("XPM");
    if (f.contains(QLatin1String("ico"))) return QStringLiteral("ICO");
    return QString();
}

bool ImageExportService::exportCanvasToFile(const QString &path, const QString &format,
                                    int quality) {
    if (!m_host.canvas || path.isEmpty()) return false;

    QProgressDialog progress(QStringLiteral("Exporting image…"), QString(), 0, 0,
                             m_host.dialogParent);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(400);
    progress.setValue(0);
    progress.setLabelText(QStringLiteral("Rendering canvas…"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QImage img = m_host.canvas->renderToImage();
    if (img.isNull()) return false;

    progress.setLabelText(QStringLiteral("Writing file…"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QString fmt = format;
    if (fmt.isEmpty()) {
        fmt = ImageExportService::imageFormatFromPath(path);
    }
    fmt = fmt.toUpper();
    if (fmt == QLatin1String("JPG")) {
        fmt = QStringLiteral("JPEG");
    }

    // Formats that don't support alpha: flatten onto white.
    if (fmt == QLatin1String("JPEG") || fmt == QLatin1String("BMP") ||
        fmt == QLatin1String("PPM") || fmt == QLatin1String("GIF")) {
        if (img.hasAlphaChannel()) {
            QImage flat(img.size(), QImage::Format_RGB32);
            flat.fill(Qt::white);
            QPainter painter(&flat);
            painter.drawImage(0, 0, img);
            painter.end();
            img = flat;
        }
    }

    int q = quality;
    if (q < 0 && (fmt == QLatin1String("JPEG") || fmt == QLatin1String("WEBP"))) {
        q = 92;
    }

    bool ok = false;
    if (q >= 0) {
        ok = img.save(path, fmt.toUtf8().constData(), q);
    } else {
        ok = img.save(path, fmt.toUtf8().constData());
    }

    if (ok && m_host.setStatusText) {
        m_host.setStatusText(
            QStringLiteral("Exported %1 (%2)")
                .arg(QFileInfo(path).fileName(), fmt));
    }
    return ok;
}

bool ImageExportService::exportImageWithDialog(const QString &preferredFormat) {
    if (!m_host.canvas) return false;

    QString selectedFilter;
    if (!preferredFormat.isEmpty()) {
        const QString name = preferredFormat.toUpper();
        if (name == QLatin1String("JPEG") || name == QLatin1String("JPG")) {
            selectedFilter = QStringLiteral("JPEG (*.jpg *.jpeg)");
        } else if (name == QLatin1String("TIFF") || name == QLatin1String("TIF")) {
            selectedFilter = QStringLiteral("TIFF (*.tif *.tiff)");
        } else if (name == QLatin1String("WEBP")) {
            selectedFilter = QStringLiteral("WebP (*.webp)");
        } else if (name == QLatin1String("JP2")) {
            selectedFilter = QStringLiteral("JPEG 2000 (*.jp2)");
        } else if (name == QLatin1String("HEIC")) {
            selectedFilter = QStringLiteral("HEIC (*.heic)");
        } else {
            selectedFilter = QStringLiteral("%1 (*%2)")
                                 .arg(name, ImageExportService::defaultExtensionForFormat(name));
        }
    }

    QString fileName = QFileDialog::getSaveFileName(m_host.dialogParent, QStringLiteral("Export Image"), QString(),
        ImageExportService::imageExportFilterString(), &selectedFilter);

    if (fileName.isEmpty()) return false;

    QString format = preferredFormat;
    if (format.isEmpty()) {
        format = ImageExportService::formatFromFilter(selectedFilter);
    }
    if (format.isEmpty()) {
        format = ImageExportService::imageFormatFromPath(fileName);
    }

    fileName = ImageExportService::ensureImageExtension(fileName, format);

    int quality = -1;
    if (format.compare(QLatin1String("JPEG"), Qt::CaseInsensitive) == 0 ||
        format.compare(QLatin1String("WEBP"), Qt::CaseInsensitive) == 0) {
        bool ok = false;
        quality = QInputDialog::getInt(m_host.dialogParent, QStringLiteral("Export Quality"),
            QStringLiteral("Quality (1–100):"), 92, 1, 100, 1, &ok);
        if (!ok) return false;
    }

    if (!exportCanvasToFile(fileName, format, quality)) {
        QMessageBox::warning(m_host.dialogParent, QStringLiteral("Export Image"),
            QStringLiteral("Failed to export image to:\n%1\n\n"
                           "Supported writers: %2")
                .arg(fileName,
                     QString::fromLatin1(
                         QImageWriter::supportedImageFormats().join(", "))));
        return false;
    }
    return true;
}

void ImageExportService::exportImageAs() {
    exportImageWithDialog();
}

void ImageExportService::exportAsJPG() {
    exportImageWithDialog(QStringLiteral("JPEG"));
}

void ImageExportService::exportAsPNG() {
    exportImageWithDialog(QStringLiteral("PNG"));
}

void ImageExportService::exportAsBMP() {
    exportImageWithDialog(QStringLiteral("BMP"));
}

void ImageExportService::exportAsTIFF() {
    exportImageWithDialog(QStringLiteral("TIFF"));
}

void ImageExportService::exportAsWebP() {
    exportImageWithDialog(QStringLiteral("WEBP"));
}

void ImageExportService::exportAsGIF() {
    exportImageWithDialog(QStringLiteral("GIF"));
}

void ImageExportService::exportAsPPM() {
    exportImageWithDialog(QStringLiteral("PPM"));
}

void ImageExportService::exportAsDXF() {
    if (!m_host.layerManager) return;

    QString fileName = QFileDialog::getSaveFileName(m_host.dialogParent, "Export as DXF", "", "DXF Files (*.dxf)");
    if (fileName.isEmpty()) return;

    DXFExporter exporter;
    DXFExporter::Options options;
    if (m_host.canvas) {
        switch (m_host.canvas->getUnits()) {
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
        options.scaleFactor = 1.0 / m_host.canvas->pixelsPerUnit();
    }

    if (exporter.exportToFile(fileName, m_host.layerManager->layers(), options)) {
        if (m_host.setStatusText) m_host.setStatusText("Exported DXF to " + fileName);
    } else {
        QMessageBox::warning(m_host.dialogParent, "DXF Export", "Failed to export DXF file.");
    }
}

