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

// AIWorkflowController place/import/replace (refactor E27).

// imageLooksLikeCutout is file-local in AIWorkflowControllerComposite.cpp;
// placeSubjectInScene needs the same check — redefine locally.
namespace {

bool imageLooksLikeCutout(const QImage &im)
{
    if (im.isNull() || !im.hasAlphaChannel())
        return false;
    const QImage a = im.convertToFormat(QImage::Format_ARGB32);
    const int w = a.width();
    const int h = a.height();
    if (w < 1 || h < 1)
        return false;

    auto alphaAt = [&](int x, int y) -> int {
        return qAlpha(a.pixel(qBound(0, x, w - 1), qBound(0, y, h - 1)));
    };
    // Tight SAM2 crops almost always have transparent corners.
    if (alphaAt(0, 0) < 250 || alphaAt(w - 1, 0) < 250 ||
        alphaAt(0, h - 1) < 250 || alphaAt(w - 1, h - 1) < 250)
        return true;

    const int stepX = qMax(1, w / 128);
    const int stepY = qMax(1, h / 128);
    for (int y = 0; y < h; y += stepY) {
        const QRgb *line = reinterpret_cast<const QRgb *>(a.constScanLine(y));
        for (int x = 0; x < w; x += stepX) {
            if (qAlpha(line[x]) < 250)
                return true;
        }
    }
    return false;
}


} // namespace

// --- placeSubjectInScene ---
void AIWorkflowController::placeSubjectInScene()
{
    if (!m_host.canvas)
        return;

    connectAIImageClient();
    if (m_aiImageClient->isBusy() || m_aiJobKind != AIJobKind::None) {
        QMessageBox::information(dialogParent(), QStringLiteral("Place Subject in Scene"),
                                 QStringLiteral("An AI job is already running."));
        return;
    }

    bool hasMasked = false;
    bool hasCutout = false;
    bool hasDistinctScene = false;
    ImagePrimitive *maskedPrim = nullptr;

    // Pass 1: find the green-masked source (must not count as "scene")
    auto findMasked = [&](ImagePrimitive *img) {
        if (!img || maskedPrim)
            return;
        if (img->getMaskCandidateCount() > 0)
            maskedPrim = img;
    };
    for (auto *obj : m_host.canvas->selectedObjects())
        findMasked(dynamic_cast<ImagePrimitive *>(obj));
    if (!maskedPrim)
        findMasked((m_host.imageForDetection ? m_host.imageForDetection() : nullptr));
    hasMasked = maskedPrim != nullptr;

    auto checkImg = [&](ImagePrimitive *img) {
        if (!img)
            return;
        const QImage im = DrawingCanvas::getImageFromPrimitive(img);
        if (im.isNull())
            return;
        if (imageLooksLikeCutout(im)) {
            hasCutout = true;
            return;
        }
        // Distinct scene = full photo that is NOT the SAM2-masked source
        if (img != maskedPrim)
            hasDistinctScene = true;
    };

    for (auto *obj : m_host.canvas->selectedObjects())
        checkImg(dynamic_cast<ImagePrimitive *>(obj));
    if (ImagePrimitive *det = (m_host.imageForDetection ? m_host.imageForDetection() : nullptr))
        checkImg(det);

    // Also scan canvas for a separate opaque photo (for SceneOntoSubject)
    if (!hasDistinctScene && m_host.canvas->layerManager()) {
        for (const auto &layer : m_host.canvas->layerManager()->layers()) {
            if (!layer)
                continue;
            for (const auto &p : layer->primitives()) {
                auto *img = dynamic_cast<ImagePrimitive *>(p.get());
                if (!img || img == maskedPrim)
                    continue;
                const QImage im = DrawingCanvas::getImageFromPrimitive(img);
                if (im.isNull())
                    continue;
                if (!im.hasAlphaChannel()) {
                    hasDistinctScene = true;
                    break;
                }
            }
            if (hasDistinctScene)
                break;
        }
    }

    AICompositeDialog dlg(hasMasked, hasCutout, hasDistinctScene, m_aiImageClient, dialogParent());
    if (dlg.exec() != QDialog::Accepted)
        return;
    const auto dlgResult = dlg.result();
    m_compositeDlgResult = dlgResult;

    if (dlgResult.mode == AICompositeDialog::Mode::SubjectIntoScene) {
        QVector<CompositeHelper::SubjectSpec> subjects;
        QString err;
        if (!resolveCompositeSubjects(&subjects, &err)) {
            QMessageBox::warning(dialogParent(), QStringLiteral("Place Subject in Scene"), err);
            return;
        }
        m_compositeSubjects = subjects;

        // Preserve faces = empty AI background + exact local cutouts (no doubles).
        // Full AI redraw only when integrate is on AND preserve is off.
        if (dlgResult.aiIntegrateCutouts && !dlgResult.preserveOriginalFaces) {
            startAiIntegrateCutouts(subjects, QImage(), dlgResult);
            return;
        }

        m_aiJobKind = AIJobKind::CompositeBackground;
        setStatusText(
                QStringLiteral("Generating empty AI background for %1 subject%2…")
                    .arg(subjects.size())
                    .arg(subjects.size() == 1 ? QString() : QStringLiteral("s")));
        showStatusMessage(QStringLiteral("Generating background…"), 0);

        AIImageClient::Request req;
        req.prompt =
            QStringLiteral(
                "Photorealistic EMPTY background plate only. "
                "Scene description: ") +
            dlgResult.prompt +
            QStringLiteral(
                ". Strict rules: zero people, zero humans, zero faces, zero "
                "silhouettes, zero mannequins, zero statues of people. "
                "Clean environment only — %1 foreground subject cutout(s) "
                "will be composited later.")
                .arg(subjects.size());
        req.provider = dlgResult.provider;
        req.model = dlgResult.model;
        req.size = dlgResult.size;
        req.aspectRatio = dlgResult.aspectRatio;
        req.imageSize = dlgResult.imageSize;
        req.quality = dlgResult.quality;
        m_aiImageClient->generate(req);
        return;
    }

    // SceneOntoSubject — multi cutouts onto a separate photo background
    QVector<CompositeHelper::SubjectSpec> subjects;
    QImage scene;
    QString err;
    if (!resolveCompositeSubjects(&subjects, &err)) {
        QMessageBox::warning(dialogParent(), QStringLiteral("Place Subject in Scene"), err);
        return;
    }
    {
        QImage unusedSubject;
        QString sceneErr;
        if (!resolveCompositeInputs(false, /*placeCutoutOnCanvas=*/false,
                                    &unusedSubject, &scene, &sceneErr)) {
            QMessageBox::warning(dialogParent(), QStringLiteral("Place Subject in Scene"),
                                 sceneErr);
            return;
        }
    }
    if (dlgResult.aiIntegrateCutouts && !dlgResult.preserveOriginalFaces) {
        startAiIntegrateCutouts(subjects, scene, dlgResult);
        return;
    }
    startCompositeBlend(subjects, scene, dlgResult);
}


// --- importAIImageToCanvas ---
void AIWorkflowController::importAIImageToCanvas(const QImage &image, const QString &prompt,
                                       bool replaceSelectedImage,
                                       bool showSuccessDialog)
{
    if (!m_host.canvas || image.isNull()) {
        QMessageBox::warning(dialogParent(), QStringLiteral("AI Image"),
            QStringLiteral("AI returned an empty image — nothing to place on canvas."));
        return;
    }

    if (replaceSelectedImage) {
        replaceSelectedObjectWithImage(image, prompt, 0);
        return;
    }

    // Place near view center at a readable size — offset so it doesn't sit on
    // top of the original photo (keeps generated + source as separate objects).
    const float targetW = 400.0f;
    const float aspect =
        image.height() > 0
            ? static_cast<float>(image.width()) / static_cast<float>(image.height())
            : 1.0f;
    const float targetH = targetW / std::max(0.01f, aspect);
    const QVector2D center = m_host.canvas->viewCenter();
    QVector2D pos(center.x() - targetW * 0.5f + 420.0f,
                  center.y() - targetH * 0.5f);
    const QVector2D size(targetW, targetH);

    auto imagePrimitive = std::make_unique<ImagePrimitive>(image, pos, size);
    imagePrimitive->setVisible(true);
    imagePrimitive->setSelected(true);
    const QUuid id = imagePrimitive->id();

    // ImportImageCommand avoids AddPrimitiveCommand's full-image JSON clone
    if (m_host.commandManager) {
        auto cmd =
            std::make_unique<ImportImageCommand>(m_host.canvas, std::move(imagePrimitive));
        m_host.commandManager->executeCommand(std::move(cmd));
    } else {
        m_host.canvas->addPrimitive(std::move(imagePrimitive));
    }

    m_host.canvas->clearSelection();
    m_host.canvas->selectPrimitiveById(id);
    m_host.canvas->update();

    setStatusText(
            QStringLiteral("AI image on canvas (%1×%2)")
                .arg(image.width())
                .arg(image.height()));
    if (showSuccessDialog) {
        QMessageBox::information(dialogParent(), QStringLiteral("AI Image"),
            QStringLiteral("Image imported to canvas.\n\nPrompt: %1").arg(prompt));
    }
}


// --- replaceSelectedObjectWithImage ---
void AIWorkflowController::replaceSelectedObjectWithImage(const QImage &image, const QString &prompt, int mode)
{
    if (!m_host.canvas || !m_host.layerManager) return;
    
    if (mode == 0) {
        // Mode 0: Replace Selected Object
        auto selectedObjects = m_host.canvas->selectedObjects();
        if (selectedObjects.empty()) {
            // No selection — import as new instead of failing
            importAIImageToCanvas(image, prompt, false);
            return;
        }

        // Prefer replacing an ImagePrimitive in-place when possible
        if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(selectedObjects[0])) {
            const QRectF bounds = imgPrim->boundingRect();
            imgPrim->setImage(image);
            imgPrim->setPosition(QVector2D(bounds.left(), bounds.top()));
            imgPrim->setSize(QVector2D(bounds.width(), bounds.height()));
            m_host.canvas->update();
            QMessageBox::information(dialogParent(), QStringLiteral("Success"),
                QStringLiteral("Selected image updated with AI result.\n\nPrompt: %1")
                    .arg(prompt));
            return;
        }
        
        // Get the first selected object
        DrawingPrimitive* selectedObj = selectedObjects[0];
        QRectF bounds = selectedObj->boundingRect();
        
        // Create ImagePrimitive with the generated image
        QVector2D position(bounds.left(), bounds.top());
        QVector2D size(bounds.width(), bounds.height());
        
        auto imagePrimitive = std::make_unique<ImagePrimitive>(image, position, size);
        imagePrimitive->setVisible(true);
        
        // Get the layer of the selected object
        Layer* targetLayer = nullptr;
        const auto& layers = m_host.layerManager->layers();
        for (const auto& layer : layers) {
            const auto& primitives = layer->primitives();
            for (const auto& prim : primitives) {
                if (prim.get() == selectedObj) {
                    targetLayer = layer.get();
                    break;
                }
            }
            if (targetLayer) break;
        }
        
        // Delete the selected object
        m_host.canvas->deleteSelectedPrimitives();
        
        // Add the new image primitive to the same layer
        if (targetLayer) {
            targetLayer->addPrimitive(std::move(imagePrimitive));
        } else {
            // Fallback: add to active layer
            m_host.canvas->addPrimitive(std::move(imagePrimitive));
        }
        
        m_host.canvas->update();
        
        QMessageBox::information(dialogParent(), "Success!", 
            QString("✨ AI-generated image created and replaced selected object!\n\nPrompt: %1").arg(prompt));
            
    } else if (mode == 1) {
        // Mode 1: Fill Closed Shape
        auto selectedObjects = m_host.canvas->selectedObjects();
        if (selectedObjects.empty()) {
            QMessageBox::information(dialogParent(), "No Selection", 
                "No closed shape selected.\n\nTip: Select a rectangle, ellipse, or closed curve first.");
            return;
        }
        
        // Get the first selected object
        DrawingPrimitive* selectedObj = selectedObjects[0];
        QRectF bounds = selectedObj->boundingRect();
        
        // Create ImagePrimitive with the generated image (clipped to shape)
        QVector2D position(bounds.left(), bounds.top());
        QVector2D size(bounds.width(), bounds.height());
        
        auto imagePrimitive = std::make_unique<ImagePrimitive>(image, position, size);
        imagePrimitive->setVisible(true);
        
        // Get the layer of the selected object
        Layer* targetLayer = nullptr;
        const auto& layers = m_host.layerManager->layers();
        for (const auto& layer : layers) {
            const auto& primitives = layer->primitives();
            for (const auto& prim : primitives) {
                if (prim.get() == selectedObj) {
                    targetLayer = layer.get();
                    break;
                }
            }
            if (targetLayer) break;
        }
        
        // Add the new image primitive to the same layer (keep the original shape)
        if (targetLayer) {
            targetLayer->addPrimitive(std::move(imagePrimitive));
        } else {
            // Fallback: add to active layer
            m_host.canvas->addPrimitive(std::move(imagePrimitive));
        }
        
        m_host.canvas->update();
        
        QMessageBox::information(dialogParent(), "Success!", 
            QString("✨ AI-generated image created inside the closed shape!\n\nPrompt: %1\n\nNote: The original shape is preserved.").arg(prompt));
    }
}


