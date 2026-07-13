#include "MainWindow.h"
#include "DrawingCanvas.h"
#include "DrawingTool.h"
#include "ImageExportService.h"
#include "ProjectFileService.h"
#include "TextEditingController.h"
#include "AIWorkflowController.h"
#include "ClassicTextTool.h"
#include "LayerManager.h"
#include <QLabel>
#include <QStatusBar>
#include <QDebug>

ImageExportService *MainWindow::imageExportService()
{
    if (!m_imageExportService)
        m_imageExportService = new ImageExportService();
    return m_imageExportService;
}

void MainWindow::ensureImageExportHost()
{
    ImageExportService::Host host;
    host.canvas = m_canvas;
    host.layerManager = m_layerManager;
    host.dialogParent = this;
    host.setStatusText = [this](const QString &msg) {
        if (m_statusLabel)
            m_statusLabel->setText(msg);
    };
    imageExportService()->setHost(std::move(host));
}

QString MainWindow::imageFormatFromPath(const QString &path)
{
    return ImageExportService::imageFormatFromPath(path);
}

QString MainWindow::defaultExtensionForFormat(const QString &format)
{
    return ImageExportService::defaultExtensionForFormat(format);
}

QString MainWindow::ensureImageExtension(const QString &path, const QString &format)
{
    return ImageExportService::ensureImageExtension(path, format);
}

QString MainWindow::imageExportFilterString()
{
    return ImageExportService::imageExportFilterString();
}

QString MainWindow::formatFromFilter(const QString &selectedFilter)
{
    return ImageExportService::formatFromFilter(selectedFilter);
}

bool MainWindow::exportCanvasToFile(const QString &path, const QString &format, int quality)
{
    ensureImageExportHost();
    return imageExportService()->exportCanvasToFile(path, format, quality);
}

bool MainWindow::exportImageWithDialog(const QString &preferredFormat)
{
    ensureImageExportHost();
    return imageExportService()->exportImageWithDialog(preferredFormat);
}

void MainWindow::exportImageAs() { ensureImageExportHost(); imageExportService()->exportImageAs(); }

void MainWindow::exportAsJPG() { ensureImageExportHost(); imageExportService()->exportAsJPG(); }

void MainWindow::exportAsPNG() { ensureImageExportHost(); imageExportService()->exportAsPNG(); }

void MainWindow::exportAsBMP() { ensureImageExportHost(); imageExportService()->exportAsBMP(); }

void MainWindow::exportAsTIFF() { ensureImageExportHost(); imageExportService()->exportAsTIFF(); }

void MainWindow::exportAsWebP() { ensureImageExportHost(); imageExportService()->exportAsWebP(); }

void MainWindow::exportAsGIF() { ensureImageExportHost(); imageExportService()->exportAsGIF(); }

void MainWindow::exportAsPPM() { ensureImageExportHost(); imageExportService()->exportAsPPM(); }

void MainWindow::exportAsDXF() { ensureImageExportHost(); imageExportService()->exportAsDXF(); }

void MainWindow::ensureTextEditingHost()
{
    TextEditingController::Host host;
    host.canvas = m_canvas;
    host.commandManager = m_commandManager;
    host.classicTextTool = m_classicTextTool;
    host.dialogParent = this;
    host.setStatusText = [this](const QString &msg) {
        if (m_statusLabel)
            m_statusLabel->setText(msg);
    };
    host.syncModifiedFlag = [this]() { syncModifiedFlag(); };
    host.showStatusMessage = [this](const QString &msg, int ms) {
        statusBar()->showMessage(msg, ms);
    };
    textEditingController()->setHost(std::move(host));
}

void MainWindow::commitTextPropertyEdits(const QString &description,
                                         const std::function<void(TextPrimitive *)> &mutate)
{
    ensureTextEditingHost();
    textEditingController()->commitTextPropertyEdits(description, mutate);
}

void MainWindow::setTextAlignLeft() { ensureTextEditingHost(); textEditingController()->setTextAlignLeft(); }

void MainWindow::setTextAlignCenter() { ensureTextEditingHost(); textEditingController()->setTextAlignCenter(); }

void MainWindow::setTextAlignRight() { ensureTextEditingHost(); textEditingController()->setTextAlignRight(); }

void MainWindow::setTextAlignJustify() { ensureTextEditingHost(); textEditingController()->setTextAlignJustify(); }

void MainWindow::showFontFamilyDialog() { ensureTextEditingHost(); textEditingController()->showFontFamilyDialog(); }

void MainWindow::showLetterSpacingDialog() { ensureTextEditingHost(); textEditingController()->showLetterSpacingDialog(); }

void MainWindow::showLineSpacingDialog() { ensureTextEditingHost(); textEditingController()->showLineSpacingDialog(); }

void MainWindow::showTrackingDialog() { ensureTextEditingHost(); textEditingController()->showTrackingDialog(); }

void MainWindow::showShadowDialog() { ensureTextEditingHost(); textEditingController()->showShadowDialog(); }

void MainWindow::showStrokeDialog() { ensureTextEditingHost(); textEditingController()->showStrokeDialog(); }

void MainWindow::showGradientDialog() { ensureTextEditingHost(); textEditingController()->showGradientDialog(); }

void MainWindow::showTextBoxDialog() { ensureTextEditingHost(); textEditingController()->showTextBoxDialog(); }

void MainWindow::showAdvancedTextEditor() { ensureTextEditingHost(); textEditingController()->showAdvancedTextEditor(); }

void MainWindow::addAutoShadow() { ensureTextEditingHost(); textEditingController()->addAutoShadow(); }

void MainWindow::insertMathFormula() { ensureTextEditingHost(); textEditingController()->insertMathFormula(); }

void MainWindow::insertMathGraph() { ensureTextEditingHost(); textEditingController()->insertMathGraph(); }

void MainWindow::showMathTutorial() { ensureTextEditingHost(); textEditingController()->showMathTutorial(); }

void MainWindow::insertPhysicsSolver() { ensureTextEditingHost(); textEditingController()->insertPhysicsSolver(); }


AIWorkflowController *MainWindow::aiWorkflow()
{
    if (!m_aiWorkflow)
        m_aiWorkflow = new AIWorkflowController(this);
    return m_aiWorkflow;
}

void MainWindow::ensureAIWorkflowHost()
{
    AIWorkflowController::Host host;
    host.canvas = m_canvas;
    host.layerManager = m_layerManager;
    host.commandManager = m_commandManager;
    host.dialogParent = this;
    host.setStatusText = [this](const QString &msg) {
        if (m_statusLabel)
            m_statusLabel->setText(msg);
    };
    host.showStatusMessage = [this](const QString &msg, int ms) {
        statusBar()->showMessage(msg, ms);
    };
    host.clearStatusMessage = [this]() {
        statusBar()->clearMessage();
    };
    host.imageForDetection = [this]() {
        return imageForDetection();
    };
    host.selectImageForMaskUI = [this](ImagePrimitive *img) {
        selectImageForMaskUI(img);
    };
    aiWorkflow()->setHost(std::move(host));
}

void MainWindow::showAISettings()
{
    ensureAIWorkflowHost();
    aiWorkflow()->showAISettings();
}

void MainWindow::generateAIImage()
{
    ensureAIWorkflowHost();
    aiWorkflow()->generateAIImage();
}

void MainWindow::editSelectedWithAI()
{
    ensureAIWorkflowHost();
    aiWorkflow()->editSelectedWithAI();
}

void MainWindow::extractSelectedSubjects()
{
    ensureAIWorkflowHost();
    aiWorkflow()->extractSelectedSubjects();
}

void MainWindow::placeSubjectInScene()
{
    ensureAIWorkflowHost();
    aiWorkflow()->placeSubjectInScene();
}

// --- recovered after A6 over-delete ---

TextEditingController *MainWindow::textEditingController()
{
    if (!m_textEditingController)
        m_textEditingController = new TextEditingController(this);
    return m_textEditingController;
}
