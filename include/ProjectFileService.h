#pragma once

#include <QObject>
#include <QString>
#include <functional>

class DrawingCanvas;
class LayerManager;
class CommandManager;
class QWidget;

/**
 * .drawing project save/load.
 * Extracted from MainWindow (refactor A7).
 */
class ProjectFileService : public QObject {
    Q_OBJECT
public:
    // Keep JSON DOM construction and background primitive creation within a
    // predictable memory/time budget. Save and load use the same limits so
    // DrawingStudio never writes a project that it will refuse to reopen.
    static constexpr qint64 kMaxProjectFileBytes = 256LL * 1024LL * 1024LL;
    static constexpr qsizetype kMaxProjectLayers = 4096;
    static constexpr qsizetype kMaxProjectPrimitives = 100000;
    static constexpr qsizetype kMaxProjectGeometryPoints = 100000;

    struct Host {
        DrawingCanvas *canvas = nullptr;
        LayerManager *layerManager = nullptr;
        CommandManager *commandManager = nullptr;
        QWidget *dialogParent = nullptr;
        std::function<void(const QString &)> setStatusText;
        std::function<void(const QString &)> setCurrentFile;
        std::function<void(const QString &)> addToRecentFiles;
        std::function<void()> refreshLayerPanel;
    };

    explicit ProjectFileService(QObject *parent = nullptr);

    void setHost(Host host) { m_host = std::move(host); }
    const Host &host() const { return m_host; }

    /** @param updateSession when false, skip setCurrentFile / recent-files (autosave). */
    bool saveToFile(const QString &fileName, bool updateSession = true);
    bool loadFromFile(const QString &fileName);
    bool loadFromFile(const QString &fileName, bool waitUntilLoaded);
    /** Suppress session/recent-file callbacks when restoring a recovery snapshot. */
    bool loadFromFile(const QString &fileName, bool waitUntilLoaded,
                      bool updateSession);
    bool isLoadInProgress() const { return m_loadInProgress; }

private:
    Host m_host;
    bool m_loadInProgress = false;
};
