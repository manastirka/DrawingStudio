#pragma once

#include <QString>
#include <functional>

class DrawingCanvas;
class LayerManager;
class QWidget;

/**
 * Image / DXF export helpers and dialogs.
 * Extracted from MainWindow (refactor A7).
 */
class ImageExportService {
public:
    struct Host {
        DrawingCanvas *canvas = nullptr;
        LayerManager *layerManager = nullptr;
        QWidget *dialogParent = nullptr;
        std::function<void(const QString &)> setStatusText;
    };

    ImageExportService() = default;

    void setHost(Host host) { m_host = std::move(host); }
    const Host &host() const { return m_host; }

    static QString imageFormatFromPath(const QString &path);
    static QString defaultExtensionForFormat(const QString &format);
    static QString ensureImageExtension(const QString &path, const QString &format);
    static QString imageExportFilterString();
    static QString formatFromFilter(const QString &selectedFilter);

    bool exportCanvasToFile(const QString &path, const QString &format = QString(),
                            int quality = -1);
    bool exportImageWithDialog(const QString &preferredFormat = QString());
    void exportImageAs();
    void exportAsJPG();
    void exportAsPNG();
    void exportAsBMP();
    void exportAsTIFF();
    void exportAsWebP();
    void exportAsGIF();
    void exportAsPPM();
    void exportAsDXF();

private:
    Host m_host;
};
