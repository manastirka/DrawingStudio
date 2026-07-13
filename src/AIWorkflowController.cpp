#include "AIWorkflowController.h"

#include "AIGenerateDialog.h"
#include "AIImageClient.h"
#include "AISettingsDialog.h"
#include "CommandManager.h"
#include "Commands.h"
#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
#include "LayerManager.h"
#include "RemoteSDHelper.h"

#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QUuid>
#include <QVector2D>
#include <QtMath>

#include <algorithm>
#include <memory>
#include <vector>

// AIWorkflowController core + generate/edit (refactor E27).

// --- setStatusText ---
void AIWorkflowController::setStatusText(const QString &msg)
{
    if (m_host.setStatusText)
        m_host.setStatusText(msg);
}


// --- showStatusMessage ---
void AIWorkflowController::showStatusMessage(const QString &msg, int ms)
{
    if (m_host.showStatusMessage)
        m_host.showStatusMessage(msg, ms);
}


// --- clearStatusMessage ---
void AIWorkflowController::clearStatusMessage()
{
    if (m_host.clearStatusMessage)
        m_host.clearStatusMessage();
}

// --- dialogParent ---
QWidget *AIWorkflowController::dialogParent() const
{
    return m_host.dialogParent;
}


// --- selectImageForMaskUIHost ---
void AIWorkflowController::selectImageForMaskUIHost(ImagePrimitive *image)
{
    if (m_host.selectImageForMaskUI)
        m_host.selectImageForMaskUI(image);
}


// --- isBusy ---
bool AIWorkflowController::isBusy() const
{
    if (m_aiJobKind != AIJobKind::None)
        return true;
    return m_aiImageClient && m_aiImageClient->isBusy();
}


// --- AIWorkflowController ---
AIWorkflowController::AIWorkflowController(QObject *parent)
    : QObject(parent)
{
}


// --- ~AIWorkflowController ---
AIWorkflowController::~AIWorkflowController() = default;


// --- setHost ---
void AIWorkflowController::setHost(Host host)
{
    m_host = std::move(host);
}


// --- resetAIJobState ---
void AIWorkflowController::resetAIJobState()
{
    m_aiJobKind = AIJobKind::None;
    m_aiReplaceSelected = false;
    m_compositeSubjects.clear();
}


// --- onAIImageFinished ---
void AIWorkflowController::onAIImageFinished(const QImage &image, const QString &prompt)
{
    const AIJobKind job = m_aiJobKind;
    clearStatusMessage();

    if (job == AIJobKind::Generate) {
        const bool replace = m_aiReplaceSelected;
        resetAIJobState();
        importAIImageToCanvas(image, prompt, replace, true);
        return;
    }
    if (job == AIJobKind::Edit) {
        resetAIJobState();
        importAIImageToCanvas(image, prompt, true, true);
        return;
    }
    if (job == AIJobKind::CompositeBackground) {
        if (image.isNull()) {
            resetAIJobState();
            QMessageBox::warning(dialogParent(), QStringLiteral("Place Subject in Scene"),
                                 QStringLiteral("Background generation failed."));
            return;
        }
        const auto dlg = m_compositeDlgResult;
        const QVector<CompositeHelper::SubjectSpec> subjects = m_compositeSubjects;
        m_aiJobKind = AIJobKind::None;
        if (dlg.placeBackgroundOnCanvas)
            importAIImageToCanvas(image, dlg.prompt, false, false);
        startCompositeBlend(subjects, image, dlg);
        return;
    }
    if (job == AIJobKind::CompositeBlend) {
        finishCompositePipeline(image, prompt);
        return;
    }
    if (job == AIJobKind::CompositeAiIntegrate) {
        resetAIJobState();
        clearStatusMessage();
        if (image.isNull()) {
            QMessageBox::warning(dialogParent(), QStringLiteral("Place Subject in Scene"),
                                 QStringLiteral("AI integrate returned an empty image."));
            return;
        }
        // Pure AI result only — never paste cutouts on top (that doubles people)
        importAIImageToCanvas(image, prompt, false, true);
        setStatusText(
                QStringLiteral("AI integrate ready — subjects redrawn by AI"));
        return;
    }

    // Stale/unknown finished signal — ignore safely
    resetAIJobState();
}


// --- connectAIImageClient ---
void AIWorkflowController::connectAIImageClient()
{
    if (m_aiImageClient)
        return;
    m_aiImageClient = new AIImageClient(this);
    connect(m_aiImageClient, &AIImageClient::started, this, [this](const QString &msg) {
        setStatusText(msg);
        showStatusMessage(msg, 0);
    });
    connect(m_aiImageClient, &AIImageClient::progress, this, [this](const QString &msg) {
        setStatusText(msg);
        showStatusMessage(msg, 0);
    });
    // Single permanent finished handler — no disconnect/reconnect races.
    connect(m_aiImageClient, &AIImageClient::finished, this,
            &AIWorkflowController::onAIImageFinished);
    connect(m_aiImageClient, &AIImageClient::failed, this, [this](const QString &err) {
        resetAIJobState();
        clearStatusMessage();
        const QString firstLine = err.section(QLatin1Char('\n'), 0, 0).trimmed();
        setStatusText(
                firstLine.isEmpty() ? QStringLiteral("AI failed")
                                    : firstLine.left(120));
        QMessageBox::critical(dialogParent(), QStringLiteral("AI Image"), err);
    });
}


// --- showAISettings ---
void AIWorkflowController::showAISettings()
{
    AISettingsDialog dlg(dialogParent());
    if (dlg.exec() == QDialog::Accepted) {
        setStatusText(
            QStringLiteral("AI provider: %1")
                .arg(AISettingsDialog::providerDisplayName(
                    AISettingsDialog::activeProvider())));
    }
}


// --- generateAIImage ---
void AIWorkflowController::generateAIImage()
{
    if (!m_host.canvas)
        return;

    connectAIImageClient();
    if (m_aiImageClient->isBusy() || m_aiJobKind != AIJobKind::None) {
        QMessageBox::information(dialogParent(), QStringLiteral("AI Image"),
                                 QStringLiteral("An AI job is already running."));
        return;
    }

    bool hasSelectedImage = false;
    for (auto *obj : m_host.canvas->selectedObjects()) {
        if (dynamic_cast<ImagePrimitive *>(obj)) {
            hasSelectedImage = true;
            break;
        }
    }

    AIGenerateDialog dlg(AIGenerateDialog::Mode::Generate, hasSelectedImage,
                         m_aiImageClient, dialogParent());
    if (dlg.exec() != QDialog::Accepted)
        return;
    const auto result = dlg.result();

    m_aiJobKind = AIJobKind::Generate;
    m_aiReplaceSelected = result.replaceSelected;

    AIImageClient::Request req;
    req.prompt = result.prompt;
    req.provider = result.provider;
    req.model = result.model;
    req.size = result.size;
    req.aspectRatio = result.aspectRatio;
    req.imageSize = result.imageSize;
    req.quality = result.quality;
    m_aiImageClient->generate(req);
}


// --- editSelectedWithAI ---
void AIWorkflowController::editSelectedWithAI()
{
    if (!m_host.canvas)
        return;

    ImagePrimitive *selectedImage = nullptr;
    for (auto *obj : m_host.canvas->selectedObjects()) {
        if ((selectedImage = dynamic_cast<ImagePrimitive *>(obj)))
            break;
    }
    if (!selectedImage) {
        QMessageBox::information(dialogParent(), QStringLiteral("Edit with AI"),
            QStringLiteral("Select an image on the canvas first, then choose "
                           "AI → Edit Selected Image."));
        return;
    }

    connectAIImageClient();
    if (m_aiImageClient->isBusy() || m_aiJobKind != AIJobKind::None) {
        QMessageBox::information(dialogParent(), QStringLiteral("AI Image"),
                                 QStringLiteral("An AI job is already running."));
        return;
    }

    const QImage source = DrawingCanvas::getImageFromPrimitive(selectedImage);
    if (source.isNull()) {
        QMessageBox::warning(dialogParent(), QStringLiteral("Edit with AI"),
                             QStringLiteral("Could not read the selected image."));
        return;
    }

    AIGenerateDialog dlg(AIGenerateDialog::Mode::Edit, true, m_aiImageClient, dialogParent());
    if (dlg.exec() != QDialog::Accepted)
        return;
    const auto result = dlg.result();

    m_aiJobKind = AIJobKind::Edit;

    AIImageClient::Request req;
    req.prompt = result.prompt;
    req.provider = result.provider;
    req.model = result.model;
    req.size = result.size;
    req.aspectRatio = result.aspectRatio;
    req.imageSize = result.imageSize;
    req.quality = result.quality;
    req.sourceImage = source;
    m_aiImageClient->edit(req);
}


// --- ensureRemoteSDHelper ---
bool AIWorkflowController::ensureRemoteSDHelper() {
    if (!m_remoteSDHelper) {
        m_remoteSDHelper = new RemoteSDHelper(this);
        connect(m_remoteSDHelper, &RemoteSDHelper::imageGenerated,
                this, &AIWorkflowController::onImageGenerated);
        connect(m_remoteSDHelper, &RemoteSDHelper::errorOccurred, this, [this](const QString& err) {
            setStatusText("Stable Diffusion error: " + err);
        });
    }
    // initialize() probes the server (5s timeout); only succeeds when reachable.
    if (!m_remoteSDHelper->isInitialized()) {
        QSettings settings;
        QString url = settings.value("AI/remoteSDUrl", "http://192.168.1.58:8000").toString();
        m_remoteSDHelper->initialize(url);
    }
    return m_remoteSDHelper->isInitialized();
}


// --- onImageGenerated ---
void AIWorkflowController::onImageGenerated(const QImage& image, const QString& prompt) {
    // Add image to canvas
    if (m_host.canvas) {
        auto img = std::make_unique<ImagePrimitive>(image, QVector2D(0,0), QVector2D(512,512));
        m_host.canvas->addPrimitiveWithCommand(std::move(img));
    }
}


