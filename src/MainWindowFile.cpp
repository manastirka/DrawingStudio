#include "MainWindow.h"
#include "DrawingCanvas.h"
#include "DrawingTool.h"
#include "ProjectFileService.h"
#include "CommandManager.h"
#include "DrawingProject.h"
#include "ClassicTextTool.h"
#include "LayerManager.h"
#include "LayerPanel.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QSettings>
#include <QDebug>
#include <QTimer>

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        // Save window settings
        QSettings settings;
        settings.setValue("geometry", saveGeometry());
        settings.setValue("windowState", saveState(DOCK_LAYOUT_VERSION));
        event->accept();
    } else {
        event->ignore();
    }
}

bool MainWindow::maybeSave()
{
    if (!m_isModified) {
        return true;
    }
    
    QMessageBox::StandardButton ret;
    ret = QMessageBox::warning(this, "Drawing Studio",
                              "The document has been modified.\n"
                              "Do you want to save your changes?",
                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    
    if (ret == QMessageBox::Save) {
        return saveProject();
    } else if (ret == QMessageBox::Cancel) {
        return false;
    }
    return true;
}

void MainWindow::updateWindowTitle()
{
    QString title = "Drawing Studio";
    if (!m_currentFile.isEmpty()) {
        QFileInfo fileInfo(m_currentFile);
        title = fileInfo.baseName() + " - " + title;
    }
    if (m_isModified) {
        title = "*" + title;
    }
    setWindowTitle(title);
}

void MainWindow::setCurrentFile(const QString &fileName)
{
    m_currentFile = fileName;
    m_isModified = false;
    if (m_commandManager)
        m_commandManager->markClean();
    updateWindowTitle();
}

void MainWindow::syncModifiedFlag()
{
    m_isModified = !(m_commandManager && m_commandManager->isClean());
    updateWindowTitle();
}

void MainWindow::addToRecentFiles(const QString &fileName)
{
    if (fileName.isEmpty())
        return;

    QSettings settings;
    QStringList files = settings.value(QStringLiteral("recentFileList")).toStringList();
    files.removeAll(fileName);
    files.prepend(fileName);
    while (files.size() > MaxRecentFiles)
        files.removeLast();
    settings.setValue(QStringLiteral("recentFileList"), files);
    updateRecentFileActions();
}

void MainWindow::updateRecentFileActions()
{
    QSettings settings;
    const QStringList files = settings.value(QStringLiteral("recentFileList")).toStringList();
    const int count = qMin(files.size(), static_cast<int>(MaxRecentFiles));
    for (int i = 0; i < count; ++i) {
        const QString text = QStringLiteral("&%1 %2").arg(i + 1).arg(QFileInfo(files[i]).fileName());
        m_recentFileActions[i]->setText(text);
        m_recentFileActions[i]->setData(files[i]);
        m_recentFileActions[i]->setVisible(true);
    }
    for (int i = count; i < MaxRecentFiles; ++i)
        m_recentFileActions[i]->setVisible(false);
    if (m_recentFilesSeparator)
        m_recentFilesSeparator->setVisible(count > 0);
}

void MainWindow::openRecentFile()
{
    if (auto *action = qobject_cast<QAction *>(sender())) {
        const QString fileName = action->data().toString();
        if (fileName.isEmpty() || !maybeSave())
            return;
        if (!loadProjectFromFile(fileName, true)) {
            QMessageBox::warning(this, QStringLiteral("Drawing Studio"),
                                 QStringLiteral("Could not open \"%1\".").arg(fileName));
            QSettings settings;
            QStringList files = settings.value(QStringLiteral("recentFileList")).toStringList();
            files.removeAll(fileName);
            settings.setValue(QStringLiteral("recentFileList"), files);
            updateRecentFileActions();
        }
    }
}

// Slot implementations

void MainWindow::newProject()
{
    if (!maybeSave()) {
        return;
    }

    if (m_classicTextTool) {
        m_classicTextTool->deactivate();
    }

    if (m_layerManager) {
        m_layerManager->clearLayers();
    }

    if (m_canvas) {
        m_canvas->clearSelection();
        m_canvas->clearPrimitives();
        if (m_project) {
            m_project->clearSelection();
            m_project->clearPrimitives();
        }
        m_canvas->zoomActual();
        m_canvas->update();
    }

    if (m_commandManager) {
        m_commandManager->clear();
    }

    hideFloatingMaskPanel();
    updatePropertyPanel();
    if (m_layerPanel) {
        m_layerPanel->refresh();
    }
    updateUndoHistoryPanel();

    setCurrentFile(QString());
    if (m_statusLabel) {
        m_statusLabel->setText("New project created");
    }
}

void MainWindow::openProject()
{
    if (maybeSave()) {
        QString fileName = QFileDialog::getOpenFileName(this,
            "Open Drawing Project", m_currentFile,
            "Drawing Project (*.drawing);;All Files (*)");
        if (!fileName.isEmpty()) {
            if (!loadProjectFromFile(fileName, true)) {
                QMessageBox::warning(this, "Open Project",
                                     "Failed to open project file:\n" + fileName);
            }
        }
    }
}

bool MainWindow::saveProject()
{
    if (m_currentFile.isEmpty()) {
        return saveProjectAs();
    }
    return saveProjectToFile(m_currentFile);
}

bool MainWindow::saveProjectAs()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Drawing Project", m_currentFile,
        "Drawing Project (*.drawing);;All Files (*)");
    if (fileName.isEmpty()) {
        return false;
    }

    if (!fileName.endsWith(".drawing", Qt::CaseInsensitive)) {
        fileName += ".drawing";
    }

    return saveProjectToFile(fileName);
}

void MainWindow::exitApplication()
{
    close();
}

void MainWindow::ensureProjectFileHost()
{
    ProjectFileService::Host host;
    host.canvas = m_canvas;
    host.layerManager = m_layerManager;
    host.commandManager = m_commandManager;
    host.dialogParent = this;
    host.setStatusText = [this](const QString &msg) {
        if (m_statusLabel)
            m_statusLabel->setText(msg);
    };
    host.setCurrentFile = [this](const QString &path) {
        setCurrentFile(path);
    };
    host.addToRecentFiles = [this](const QString &path) {
        addToRecentFiles(path);
    };
    host.refreshLayerPanel = [this]() {
        if (m_layerPanel)
            m_layerPanel->refresh();
    };
    projectFileService()->setHost(std::move(host));
}

bool MainWindow::saveProjectToFile(const QString &fileName)
{
    ensureProjectFileHost();
    return projectFileService()->saveToFile(fileName);
}

bool MainWindow::loadProjectFromFile(const QString &fileName)
{
    return loadProjectFromFile(fileName, false);
}

bool MainWindow::loadProjectFromFile(const QString &fileName, bool waitUntilLoaded)
{
    ensureProjectFileHost();
    return projectFileService()->loadFromFile(fileName, waitUntilLoaded);
}

ProjectFileService *MainWindow::projectFileService()
{
    if (!m_projectFileService)
        m_projectFileService = new ProjectFileService(this);
    return m_projectFileService;
}
