#include "MainWindow.h"
#include "DrawingCanvas.h"
#include "DrawingTool.h"
#include "ImagePrimitive.h"
#include "ImageToDrawingEngine.h"
#include "DrawingCommandDispatcher.h"
#include "FloatingMaskPanel.h"
#include "ImageAdjustmentController.h"
#include "ImageExportService.h"
#include "LayerManager.h"
#include "CommandManager.h"
#include "Commands.h"
#include "LineExtractor.h"
#include "DXFExporter.h"
#include "LineExtractionDialog.h"
#include "EdgeSelectionTool.h"
#include "DrawingPrimitive.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QJsonObject>
#include <QDebug>
#include <QImage>
#include <memory>

void MainWindow::ensureImageAdjustmentHost()
{
    ImageAdjustmentController::Host host;
    host.canvas = m_canvas;
    host.dialogParent = this;
    host.setStatusText = [this](const QString &msg) {
        if (m_statusLabel)
            m_statusLabel->setText(msg);
    };
    imageAdjustmentController()->setHost(std::move(host));
}

void MainWindow::createFloatingMaskPanel()
{
    if (!m_canvas) {
        qWarning() << "MainWindow: Cannot create floating mask panel - canvas not initialized";
        return;
    }
    if (m_floatingMaskPanel)
        return;
    m_floatingMaskPanel = new FloatingMaskPanel(m_canvas);
    ensureFloatingMaskHost();
}

void MainWindow::ensureFloatingMaskHost()
{
    if (!m_floatingMaskPanel)
        return;
    FloatingMaskPanel::Host host;
    host.canvas = m_canvas;
    host.commandManager = m_commandManager;
    host.selectNextMask = [this]() { selectNextMask(); };
    host.selectPreviousMask = [this]() { selectPreviousMask(); };
    host.invertSelectedMask = [this]() { invertSelectedMask(); };
    host.onMaskSelectionChanged = [this](int index) { onMaskSelectionChanged(index); };
    host.setStatusText = [this](const QString &msg) {
        if (m_statusLabel)
            m_statusLabel->setText(msg);
    };
    m_floatingMaskPanel->setHost(std::move(host));
}


// Helper function to load custom icons from file (Added by Rale)

void MainWindow::extractLinesFromImage() {
    if (!m_canvas || !m_layerManager) return;

    QImage sourceImage;

    // Try to get image from selected ImagePrimitive
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto *obj : selectedObjects) {
        if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
            sourceImage = DrawingCanvas::getImageFromPrimitive(imgPrim);
            break;
        }
    }

    // Fallback: find any ImagePrimitive on any layer
    if (sourceImage.isNull() && m_layerManager) {
        for (const auto &layer : m_layerManager->layers()) {
            for (const auto &prim : layer->primitives()) {
                if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(prim.get())) {
                    sourceImage = DrawingCanvas::getImageFromPrimitive(imgPrim);
                    break;
                }
            }
            if (!sourceImage.isNull()) break;
        }
    }

    // If still no image, open file dialog (does not place on canvas)
    if (sourceImage.isNull()) {
        QString fileName = QFileDialog::getOpenFileName(
            this, "Open Building Plan Image", "",
            "Images (*.png *.jpg *.jpeg *.bmp *.tiff *.tif)");
        if (fileName.isEmpty()) return;
        sourceImage.load(fileName);
        if (sourceImage.isNull()) {
            QMessageBox::warning(this, "Error", "Failed to load image file.");
            return;
        }
    }

    auto *dialog = new LineExtractionDialog(sourceImage, m_canvas, m_layerManager,
                                            m_commandManager, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->exec();
}

void MainWindow::importFloorPlan()
{
    if (!m_canvas || !m_layerManager)
        return;

    const QString fileName = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import Floor Plan"), QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.tiff *.tif)"));
    if (fileName.isEmpty())
        return;

    QImage sourceImage;
    if (!sourceImage.load(fileName) || sourceImage.isNull()) {
        QMessageBox::warning(this, QStringLiteral("Import Floor Plan"),
                             QStringLiteral("Failed to load image file."));
        return;
    }

    // Place image on canvas near view center
    const float maxW = 720.0f;
    const float aspect =
        sourceImage.height() > 0
            ? static_cast<float>(sourceImage.width()) /
                  static_cast<float>(sourceImage.height())
            : 1.0f;
    float targetW = maxW;
    float targetH = targetW / std::max(0.01f, aspect);
    if (targetH > 560.0f) {
        targetH = 560.0f;
        targetW = targetH * aspect;
    }
    const QVector2D center = m_canvas->viewCenter();
    const QVector2D pos(center.x() - targetW * 0.5f, center.y() - targetH * 0.5f);

    auto imagePrimitive =
        std::make_unique<ImagePrimitive>(sourceImage, pos, QVector2D(targetW, targetH));
    imagePrimitive->setVisible(true);
    imagePrimitive->setSelected(true);
    const QUuid id = imagePrimitive->id();

    if (m_commandManager) {
        m_commandManager->executeCommand(
            std::make_unique<ImportImageCommand>(m_canvas, std::move(imagePrimitive)));
    } else {
        m_canvas->addPrimitive(std::move(imagePrimitive));
    }

    m_canvas->clearSelection();
    m_canvas->selectPrimitiveById(id);
    m_canvas->setCurrentTool(DrawingTool::Select);
    m_canvas->update();

    if (m_statusLabel)
        m_statusLabel->setText(
            QStringLiteral("Floor plan imported — convert walls to lines"));

    auto *dialog = new LineExtractionDialog(sourceImage, m_canvas, m_layerManager,
                                            m_commandManager, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->exec();
}

// Image Helpers

QImage MainWindow::applyBoxBlur(const QImage &source, int radius) {
    if (radius <= 0) return source;
    QImage img = source.convertToFormat(QImage::Format_ARGB32);
    QImage result(img.size(), QImage::Format_ARGB32);
    int w = img.width();
    int h = img.height();

    // Horizontal pass
    QImage temp(img.size(), QImage::Format_ARGB32);
    for (int y = 0; y < h; y++) {
        int rSum = 0, gSum = 0, bSum = 0, aSum = 0;
        // Initialize window
        for (int x = -radius; x <= radius; x++) {
            int sx = qBound(0, x, w - 1);
            QRgb pixel = img.pixel(sx, y);
            rSum += qRed(pixel); gSum += qGreen(pixel); bSum += qBlue(pixel); aSum += qAlpha(pixel);
        }
        int windowSize = 2 * radius + 1;
        for (int x = 0; x < w; x++) {
            temp.setPixel(x, y, qRgba(rSum / windowSize, gSum / windowSize, bSum / windowSize, aSum / windowSize));
            // Slide window
            int removeX = qBound(0, x - radius, w - 1);
            int addX = qBound(0, x + radius + 1, w - 1);
            QRgb removePixel = img.pixel(removeX, y);
            QRgb addPixel = img.pixel(addX, y);
            rSum += qRed(addPixel) - qRed(removePixel);
            gSum += qGreen(addPixel) - qGreen(removePixel);
            bSum += qBlue(addPixel) - qBlue(removePixel);
            aSum += qAlpha(addPixel) - qAlpha(removePixel);
        }
    }

    // Vertical pass
    for (int x = 0; x < w; x++) {
        int rSum = 0, gSum = 0, bSum = 0, aSum = 0;
        for (int y = -radius; y <= radius; y++) {
            int sy = qBound(0, y, h - 1);
            QRgb pixel = temp.pixel(x, sy);
            rSum += qRed(pixel); gSum += qGreen(pixel); bSum += qBlue(pixel); aSum += qAlpha(pixel);
        }
        int windowSize = 2 * radius + 1;
        for (int y = 0; y < h; y++) {
            result.setPixel(x, y, qRgba(rSum / windowSize, gSum / windowSize, bSum / windowSize, aSum / windowSize));
            int removeY = qBound(0, y - radius, h - 1);
            int addY = qBound(0, y + radius + 1, h - 1);
            QRgb removePixel = temp.pixel(x, removeY);
            QRgb addPixel = temp.pixel(x, addY);
            rSum += qRed(addPixel) - qRed(removePixel);
            gSum += qGreen(addPixel) - qGreen(removePixel);
            bSum += qBlue(addPixel) - qBlue(removePixel);
            aSum += qAlpha(addPixel) - qAlpha(removePixel);
        }
    }

    return result;
}

// Shadows

void MainWindow::applyCommonParams(DrawingPrimitive* prim, const QJsonObject& params)
{
    ensureCommandDispatcherHost();
    commandDispatcher()->applyCommonParams(prim, params);
}

void MainWindow::executeDrawingCommand(const QString& action, const QJsonObject& params)
{
    ensureCommandDispatcherHost();
    commandDispatcher()->execute(action, params);
    m_lastCommandResult = commandDispatcher()->lastResult();
}



DrawingCommandDispatcher *MainWindow::commandDispatcher()
{
    if (!m_commandDispatcher) {
        m_commandDispatcher = new DrawingCommandDispatcher();
    }
    return m_commandDispatcher;
}

void MainWindow::ensureCommandDispatcherHost()
{
    DrawingCommandDispatcher *d = commandDispatcher();
    DrawingCommandDispatcher::Host host;
    host.canvas = m_canvas;
    host.layerManager = m_layerManager;
    host.selectAll = [this]() { selectAll(); };
    host.deleteSelected = [this]() { deleteSelected(); };
    host.copySelected = [this]() { copySelected(); };
    host.pasteClipboard = [this]() { pasteClipboard(); };
    host.duplicateSelected = [this]() { duplicateSelected(); };
    host.clipboardSize = [this]() { return static_cast<int>(m_clipboard.size()); };
    host.activateTool = [this](DrawingTool tool) { activateDrawingTool(tool); };
    host.undo = [this]() { undo(); };
    host.redo = [this]() { redo(); };
    host.exportCanvasToFile = [this](const QString &path, const QString &format, int quality) {
        return exportCanvasToFile(path, format, quality);
    };
    host.imageFormatFromPath = [](const QString &path) {
        return ImageExportService::imageFormatFromPath(path);
    };
    host.ensureImageExtension = [](const QString &path, const QString &format) {
        return ImageExportService::ensureImageExtension(path, format);
    };
    host.imageExportFilterString = []() {
        return ImageExportService::imageExportFilterString();
    };
    host.saveProjectToFile = [this](const QString &path) {
        return saveProjectToFile(path);
    };
    host.loadProjectFromFile = [this](const QString &path, bool wait) {
        return loadProjectFromFile(path, wait);
    };
    host.selectNextMask = [this]() { selectNextMask(); };
    host.selectPreviousMask = [this]() { selectPreviousMask(); };
    host.invertSelectedMask = [this]() { invertSelectedMask(); };
    host.setStatusText = [this](const QString &msg) {
        if (m_statusLabel)
            m_statusLabel->setText(msg);
    };
    host.imageToDrawingEngine = [this]() {
        return imageToDrawingEngine();
    };
    d->setHost(std::move(host));
}

ImageToDrawingEngine *MainWindow::imageToDrawingEngine()
{
    if (!m_imageToDrawingEngine) {
        m_imageToDrawingEngine = new ImageToDrawingEngine();
    }
    ImageToDrawingEngine::Context ctx;
    ctx.canvas = m_canvas;
    ctx.saveUndo = [this](const QString &description) {
        saveUndoState(description);
    };
    ctx.findImage = [this](int index) {
        return findImageByIndex(index);
    };
    m_imageToDrawingEngine->setContext(std::move(ctx));
    return m_imageToDrawingEngine;
}

ImagePrimitive* MainWindow::findImageByIndex(int index)
{
    ensureCommandDispatcherHost();
    return commandDispatcher()->findImageByIndex(index);
}

ImagePrimitive* MainWindow::imageForDetection()
{
    ensureCommandDispatcherHost();
    return commandDispatcher()->imageForDetection();
}

void MainWindow::selectImageForMaskUI(ImagePrimitive *image)
{
    ensureCommandDispatcherHost();
    commandDispatcher()->selectImageForMaskUI(image);
}

ImagePrimitive* MainWindow::selectedImageWithMasks()
{
    ensureCommandDispatcherHost();
    return commandDispatcher()->selectedImageWithMasks();
}


// Masks

void MainWindow::selectNextMask() {
    if (!m_canvas) return;

    ImagePrimitive *imgPrim = selectedImageWithMasks();
    if (!imgPrim) {
        if (m_statusLabel) {
            m_statusLabel->setText("No masks detected. Run detection first.");
        }
        return;
    }

    if (!imgPrim->isSelected()) {
        selectImageForMaskUI(imgPrim);
    }

    int count = imgPrim->getMaskCandidateCount();
    int current = imgPrim->getSelectedMaskIndex();
    int next = (current + 1) % count;
    if (m_commandManager) {
        m_commandManager->executeCommand(
            std::make_unique<SelectMaskCandidateCommand>(imgPrim, current, next));
    } else {
        imgPrim->selectMaskCandidate(next);
    }
    if (m_statusLabel) {
        m_statusLabel->setText(QString("Mask %1 of %2").arg(next + 1).arg(count));
    }
    updateMaskSelectionUI();
    updateFloatingMaskPanel();
    m_canvas->update();
}

void MainWindow::selectPreviousMask() {
    if (!m_canvas) return;

    ImagePrimitive *imgPrim = selectedImageWithMasks();
    if (!imgPrim) {
        if (m_statusLabel) {
            m_statusLabel->setText("No masks detected. Run detection first.");
        }
        return;
    }

    if (!imgPrim->isSelected()) {
        selectImageForMaskUI(imgPrim);
    }

    int count = imgPrim->getMaskCandidateCount();
    int current = imgPrim->getSelectedMaskIndex();
    int prev = current - 1;
    if (prev < 0) prev = count - 1;
    if (m_commandManager) {
        m_commandManager->executeCommand(
            std::make_unique<SelectMaskCandidateCommand>(imgPrim, current, prev));
    } else {
        imgPrim->selectMaskCandidate(prev);
    }
    if (m_statusLabel) {
        m_statusLabel->setText(QString("Mask %1 of %2").arg(prev + 1).arg(count));
    }
    updateMaskSelectionUI();
    updateFloatingMaskPanel();
    m_canvas->update();
}

void MainWindow::invertSelectedMask() {
    if (!m_canvas) return;

    ImagePrimitive *imgPrim = selectedImageWithMasks();
    if (!imgPrim) {
        if (m_statusLabel) {
            m_statusLabel->setText("No mask to invert. Run detection first.");
        }
        return;
    }

    if (!imgPrim->isSelected()) {
        selectImageForMaskUI(imgPrim);
    }

    imgPrim->invertMask();
    m_canvas->update();
    updateFloatingMaskPanel();
    if (m_statusLabel) {
        m_statusLabel->setText("Mask inverted");
    }
}

void MainWindow::updateMaskSelectionUI() {
    if (!m_canvas) return;
    ImagePrimitive* imgPrim = nullptr;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* ip = dynamic_cast<ImagePrimitive*>(obj)) {
            imgPrim = ip;
            break;
        }
    }

    bool hasMasks = imgPrim && imgPrim->getMaskCandidateCount() > 0;

    // The floating mask panel is the real UI; here we just keep the status bar
    // in sync with the current selection.
    if (hasMasks && m_statusLabel) {
        int count = imgPrim->getMaskCandidateCount();
        int current = imgPrim->getSelectedMaskIndex();
        m_statusLabel->setText(QString("Mask %1 of %2").arg(current + 1).arg(count));
    }
}

void MainWindow::onMaskSelectionChanged(int index) {
    if (!m_canvas) return;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            int current = imgPrim->getSelectedMaskIndex();
            if (index >= 0 && index < imgPrim->getMaskCandidateCount() &&
                index != current) {
                // Skip no-op selections (e.g. slider programmatically synced to
                // the current index) so they don't pollute the undo stack.
                if (m_commandManager) {
                    m_commandManager->executeCommand(
                        std::make_unique<SelectMaskCandidateCommand>(imgPrim, current, index));
                } else {
                    imgPrim->selectMaskCandidate(index);
                }
                updateMaskSelectionUI();
                m_canvas->update();
            }
            return;
        }
    }
}

void MainWindow::hideFloatingMaskPanel()
{
    if (m_floatingMaskPanel)
        m_floatingMaskPanel->hide();
}

void MainWindow::showFloatingMaskPanel()
{
    ensureFloatingMaskHost();
    if (m_floatingMaskPanel)
        m_floatingMaskPanel->showAtDefaultPosition();
}

void MainWindow::updateFloatingMaskPanel()
{
    ensureFloatingMaskHost();
    if (m_floatingMaskPanel)
        m_floatingMaskPanel->refresh();
}

void MainWindow::showLevelsAdjustment()
{
    ensureImageAdjustmentHost();
    imageAdjustmentController()->showLevelsAdjustment();
}

void MainWindow::showCurvesAdjustment()
{
    ensureImageAdjustmentHost();
    imageAdjustmentController()->showCurvesAdjustment();
}

void MainWindow::showShadowsAdjustment()
{
    ensureImageAdjustmentHost();
    imageAdjustmentController()->showShadowsAdjustment();
}

void MainWindow::showHighlightsAdjustment()
{
    ensureImageAdjustmentHost();
    imageAdjustmentController()->showHighlightsAdjustment();
}

void MainWindow::showBrightnessContrast()
{
    ensureImageAdjustmentHost();
    imageAdjustmentController()->showBrightnessContrast();
}

void MainWindow::showHueSaturation()
{
    ensureImageAdjustmentHost();
    imageAdjustmentController()->showHueSaturation();
}

void MainWindow::showGaussianBlur()
{
    ensureImageAdjustmentHost();
    imageAdjustmentController()->showGaussianBlur();
}

void MainWindow::showMotionBlur()
{
    ensureImageAdjustmentHost();
    imageAdjustmentController()->showMotionBlur();
}

void MainWindow::showRadialBlur()
{
    ensureImageAdjustmentHost();
    imageAdjustmentController()->showRadialBlur();
}

void MainWindow::showBokehBlur()
{
    ensureImageAdjustmentHost();
    imageAdjustmentController()->showBokehBlur();
}

void MainWindow::showSurfaceBlur()
{
    ensureImageAdjustmentHost();
    imageAdjustmentController()->showSurfaceBlur();
}

void MainWindow::autoEnhanceImage()
{
    ensureImageAdjustmentHost();
    imageAdjustmentController()->autoEnhanceImage();
}


int MainWindow::applyBlurToSelected(int radius) {
    if (!m_canvas || radius <= 0) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image();
            QImage blurred = applyBoxBlur(img, radius);
            imgPrim->setImage(blurred);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

int MainWindow::applySepiaToSelected() {
    if (!m_canvas) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image().convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < img.height(); y++) {
                QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
                for (int x = 0; x < img.width(); x++) {
                    int r = qRed(line[x]);
                    int g = qGreen(line[x]);
                    int b = qBlue(line[x]);
                    int a = qAlpha(line[x]);
                    int tr = qMin(255, (int)(0.393 * r + 0.769 * g + 0.189 * b));
                    int tg = qMin(255, (int)(0.349 * r + 0.686 * g + 0.168 * b));
                    int tb = qMin(255, (int)(0.272 * r + 0.534 * g + 0.131 * b));
                    line[x] = qRgba(tr, tg, tb, a);
                }
            }
            imgPrim->setImage(img);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

int MainWindow::applyInvertToSelected() {
    if (!m_canvas) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image().convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < img.height(); y++) {
                QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
                for (int x = 0; x < img.width(); x++) {
                    int a = qAlpha(line[x]);
                    line[x] = qRgba(255 - qRed(line[x]), 255 - qGreen(line[x]), 255 - qBlue(line[x]), a);
                }
            }
            imgPrim->setImage(img);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

int MainWindow::applyEdgeBlurToSelected(int radius) {
    if (!m_canvas || radius <= 0) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image().convertToFormat(QImage::Format_ARGB32);
            QImage blurred = applyBoxBlur(img, radius);
            // Only apply blur to edge pixels (where alpha gradient exists)
            for (int y = 0; y < img.height(); y++) {
                for (int x = 0; x < img.width(); x++) {
                    QColor orig = img.pixelColor(x, y);
                    if (orig.alpha() > 0 && orig.alpha() < 255) {
                        img.setPixelColor(x, y, blurred.pixelColor(x, y));
                    }
                }
            }
            imgPrim->setImage(img);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

int MainWindow::applyGrayscaleToSelected() {
    if (!m_canvas) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image().convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < img.height(); y++) {
                QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
                for (int x = 0; x < img.width(); x++) {
                    int gray = qGray(line[x]);
                    int a = qAlpha(line[x]);
                    line[x] = qRgba(gray, gray, gray, a);
                }
            }
            imgPrim->setImage(img);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

int MainWindow::applyFlipVerticalToSelected() {
    if (!m_canvas) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image().flipped(Qt::Vertical);
            imgPrim->setImage(img);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

int MainWindow::applyFlipHorizontalToSelected() {
    if (!m_canvas) return 0;
    int count = 0;
    auto selectedObjects = m_canvas->selectedObjects();
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            QImage img = imgPrim->image().flipped(Qt::Horizontal);
            imgPrim->setImage(img);
            count++;
        }
    }
    if (count > 0) {
        if (m_commandManager)
            m_commandManager->invalidateClean();
        m_isModified = true;
        updateWindowTitle();
        m_canvas->update();
    }
    return count;
}

// --- recovered icon helpers ---

