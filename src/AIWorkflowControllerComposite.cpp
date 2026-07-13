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

// AIWorkflowController extract + composite blend (refactor E27).

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

// --- extractSelectedSubjects ---
void AIWorkflowController::extractSelectedSubjects()
{
    if (!m_host.canvas)
        return;

    // Snapshot sources first — selection will change as we add cutouts.
    QVector<ImagePrimitive *> sources;
    for (auto *obj : m_host.canvas->selectedObjects()) {
        auto *img = dynamic_cast<ImagePrimitive *>(obj);
        if (!img)
            continue;
        if (img->getMaskCandidateCount() > 0 ||
            !img->getEditableContour().empty())
            sources.append(img);
    }

    if (sources.isEmpty()) {
        QMessageBox::information(dialogParent(), QStringLiteral("Extract Subjects"),
            QStringLiteral(
                "Select one or more images with a SAM2 mask first "
                "(Detect Subjects → click a green outline)."));
        return;
    }

    QVector<ImagePrimitive *> created;
    int failed = 0;
    for (ImagePrimitive *src : sources) {
        auto extracted = src->extractDetectedSubject();
        if (!extracted || extracted->image().isNull()) {
            ++failed;
            continue;
        }
        ImagePrimitive *ptr = extracted.get();
        if (m_host.commandManager) {
            auto cmd =
                std::make_unique<ImportImageCommand>(m_host.canvas, std::move(extracted));
            m_host.commandManager->executeCommand(std::move(cmd));
        } else {
            m_host.canvas->addPrimitive(std::move(extracted));
        }
        created.append(ptr);
    }

    m_host.canvas->clearSelection();
    for (ImagePrimitive *cutout : created) {
        if (!cutout)
            continue;
        cutout->setSelected(true);
        m_host.canvas->addToSelection(cutout);
    }
    m_host.canvas->setCurrentTool(DrawingTool::Select);
    m_host.canvas->update();

    if (created.isEmpty()) {
        QMessageBox::warning(dialogParent(), QStringLiteral("Extract Subjects"),
            QStringLiteral(
                "Could not extract any subjects. Click a green mask outline "
                "on each image, then try again."));
        return;
    }

    const QString msg =
        QStringLiteral("Extracted %1 cutout%2 — all selected. "
                       "Shift-click to add/remove, then AI → Place Subject in Scene.")
            .arg(created.size())
            .arg(created.size() == 1 ? QString() : QStringLiteral("s"));
    setStatusText(msg);
    showStatusMessage(msg, 8000);
    if (failed > 0) {
        QMessageBox::information(dialogParent(), QStringLiteral("Extract Subjects"),
            QStringLiteral("Extracted %1 of %2 (some masks were missing).")
                .arg(created.size())
                .arg(sources.size()));
    }
}


// --- resolveCompositeSubjects ---
bool AIWorkflowController::resolveCompositeSubjects(
    QVector<CompositeHelper::SubjectSpec> *subjectsOut, QString *errorOut)
{
    if (!subjectsOut)
        return false;
    subjectsOut->clear();

    QVector<ImagePrimitive *> seen;

    auto appendCutout = [&](ImagePrimitive *img) -> bool {
        if (!img || seen.contains(img))
            return false;
        const QImage im = DrawingCanvas::getImageFromPrimitive(img);
        if (!imageLooksLikeCutout(im))
            return false;
        seen.append(img);
        CompositeHelper::SubjectSpec spec;
        spec.image = im;
        // Already-extracted cutouts: no source layout → auto pack
        subjectsOut->append(spec);
        return true;
    };

    auto appendFromMask = [&](ImagePrimitive *img) -> bool {
        if (!img || seen.contains(img))
            return false;
        if (img->getMaskCandidateCount() <= 0 &&
            img->getEditableContour().empty())
            return false;
        img->setMaskOverlayVisible(false);
        if (m_host.canvas)
            m_host.canvas->update();

        // Separate cutouts + original relative layout (not one glued crop)
        const auto pieces = img->extractSelectedMasksSeparately();
        if (pieces.isEmpty())
            return false;
        seen.append(img);
        for (const auto &piece : pieces) {
            if (piece.image.isNull())
                continue;
            CompositeHelper::SubjectSpec spec;
            spec.image = piece.image;
            spec.sourceNormRect = piece.sourceNormRect;
            subjectsOut->append(spec);
        }
        return !subjectsOut->isEmpty();
    };

    // Prefer already-extracted cutouts when multi-selected
    for (auto *obj : m_host.canvas->selectedObjects())
        appendCutout(dynamic_cast<ImagePrimitive *>(obj));

    // Also extract from any selected masked photos
    for (auto *obj : m_host.canvas->selectedObjects())
        appendFromMask(dynamic_cast<ImagePrimitive *>(obj));

    if (subjectsOut->isEmpty()) {
        if (ImagePrimitive *det = (m_host.imageForDetection ? m_host.imageForDetection() : nullptr)) {
            if (!appendCutout(det))
                appendFromMask(det);
        }
    }

    if (subjectsOut->isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                "No subjects found.\n\n"
                "• Extract cutouts (Image → Extract Selected Subject(s)), then "
                "Shift-click to select several, or\n"
                "• Select photos that still have a green SAM2 mask.");
        }
        return false;
    }
    return true;
}


// --- resolveCompositeInputs ---
bool AIWorkflowController::resolveCompositeInputs(bool preferMaskedSubject,
                                        bool placeCutoutOnCanvas,
                                        QImage *subjectOut, QImage *sceneOut,
                                        QString *errorOut)
{
    if (!subjectOut)
        return false;
    *subjectOut = QImage();
    if (sceneOut)
        *sceneOut = QImage();

    ImagePrimitive *masked = nullptr;
    ImagePrimitive *cutout = nullptr;
    ImagePrimitive *opaqueScene = nullptr;

    auto consider = [&](ImagePrimitive *img) {
        if (!img)
            return;
        const QImage im = DrawingCanvas::getImageFromPrimitive(img);
        if (im.isNull())
            return;
        if (img->getMaskCandidateCount() > 0 && !masked)
            masked = img;
        // Treat images with meaningful alpha as cutouts
        if (im.hasAlphaChannel()) {
            bool anyAlpha = false;
            const QImage a = im.convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < a.height() && !anyAlpha; y += qMax(1, a.height() / 32)) {
                const QRgb *line = reinterpret_cast<const QRgb *>(a.constScanLine(y));
                for (int x = 0; x < a.width(); x += qMax(1, a.width() / 32)) {
                    if (qAlpha(line[x]) < 250) {
                        anyAlpha = true;
                        break;
                    }
                }
            }
            if (anyAlpha && !cutout)
                cutout = img;
            else if (!anyAlpha && !opaqueScene && img != masked)
                opaqueScene = img;
        } else if (!opaqueScene && img != masked) {
            // Never use the green-masked source photo as the scene background —
            // that pastes the cutout onto itself and doubles the subject.
            opaqueScene = img;
        }
    };

    for (auto *obj : m_host.canvas->selectedObjects())
        consider(dynamic_cast<ImagePrimitive *>(obj));

    // Also allow mask on the detection target even if not selected alone
    if (!masked) {
        if (ImagePrimitive *det = (m_host.imageForDetection ? m_host.imageForDetection() : nullptr))
            consider(det);
    }

    // Fall back: scan layers for an opaque scene if needed
    if (sceneOut && !opaqueScene && m_host.canvas->layerManager()) {
        for (const auto &layer : m_host.canvas->layerManager()->layers()) {
            if (!layer)
                continue;
            for (const auto &p : layer->primitives()) {
                if (auto *img = dynamic_cast<ImagePrimitive *>(p.get())) {
                    if (img == cutout || img == masked)
                        continue;
                    const QImage im = DrawingCanvas::getImageFromPrimitive(img);
                    if (!im.isNull() && !im.hasAlphaChannel()) {
                        opaqueScene = img;
                        break;
                    }
                }
            }
            if (opaqueScene)
                break;
        }
    }

    // Final guard: never composite subject onto its own source photo
    if (opaqueScene && (opaqueScene == masked || opaqueScene == cutout))
        opaqueScene = nullptr;

    auto takeMaskSubject = [&](ImagePrimitive *src) -> bool {
        // Hide green overlay so it doesn't look like a second subject while composing
        src->setMaskOverlayVisible(false);
        if (m_host.canvas)
            m_host.canvas->update();
        auto extracted = src->extractDetectedSubject();
        if (!extracted) {
            if (errorOut)
                *errorOut = QStringLiteral("Could not extract the selected mask.");
            return false;
        }
        *subjectOut = extracted->image();
        // Never leave a cutout on canvas during compose unless explicitly requested.
        if (placeCutoutOnCanvas) {
            extracted->setMaskOverlayVisible(false);
            m_host.canvas->addPrimitiveWithCommand(std::move(extracted));
        }
        // else: discard the temporary cutout primitive — pixels already in subjectOut
        return true;
    };

    if (preferMaskedSubject && masked) {
        if (!takeMaskSubject(masked))
            return false;
    } else if (cutout) {
        *subjectOut = DrawingCanvas::getImageFromPrimitive(cutout);
    } else if (masked) {
        if (!takeMaskSubject(masked))
            return false;
    } else {
        if (errorOut)
            *errorOut = QStringLiteral(
                "No subject found. Detect Subjects on a photo, or select a cutout.");
        return false;
    }

    if (subjectOut->isNull()) {
        if (errorOut)
            *errorOut = QStringLiteral("Subject image is empty.");
        return false;
    }

    if (sceneOut) {
        if (!opaqueScene) {
            if (errorOut)
                *errorOut = QStringLiteral(
                    "No photo background found. Select a full image as the scene.");
            return false;
        }
        *sceneOut = DrawingCanvas::getImageFromPrimitive(opaqueScene);
        if (sceneOut->isNull()) {
            if (errorOut)
                *errorOut = QStringLiteral("Scene image is empty.");
            return false;
        }
    }
    return true;
}


// --- startCompositeBlend ---
void AIWorkflowController::startCompositeBlend(
    const QVector<CompositeHelper::SubjectSpec> &subjects,
    const QImage &background, const AICompositeDialog::Result &dlgResult)
{
    if (subjects.isEmpty()) {
        resetAIJobState();
        QMessageBox::warning(dialogParent(), QStringLiteral("Place Subject in Scene"),
                             QStringLiteral("No subject cutouts to composite."));
        return;
    }

    const QSize plateSize = CompositeHelper::platePixelSize(
        dlgResult.imageSize.isEmpty() ? QStringLiteral("1K") : dlgResult.imageSize,
        dlgResult.aspectRatio.isEmpty() ? QStringLiteral("1:1") : dlgResult.aspectRatio);

    // Original SAM2/cutout pixels with alpha — separate silhouettes, layout preserved.
    const QImage plate =
        CompositeHelper::buildPlate(background, subjects, plateSize,
                                    /*matchLighting=*/true);
    if (plate.isNull()) {
        resetAIJobState();
        QMessageBox::warning(dialogParent(), QStringLiteral("Place Subject in Scene"),
                             QStringLiteral("Could not build the composite plate."));
        return;
    }

    if (!dlgResult.aiLightingRefine) {
        finishCompositePipeline(
            plate, dlgResult.prompt.isEmpty()
                       ? QStringLiteral("Local composite (%1 subject%2 preserved)")
                             .arg(subjects.size())
                             .arg(subjects.size() == 1 ? QString() : QStringLiteral("s"))
                       : dlgResult.prompt);
        return;
    }

    QString blendPrompt =
        QStringLiteral(
            "This image already has the correct foreground subject cutout(s). "
            "ONLY adjust global lighting, color temperature, and soft contact "
            "shadows so the subjects match the scene. "
            "Do NOT redraw, duplicate, move, or change faces, bodies, clothing, "
            "poses, or silhouettes. Keep transparent-cutout edges — no boxes.");
    if (!dlgResult.prompt.isEmpty())
        blendPrompt += QStringLiteral(" Notes: ") + dlgResult.prompt;

    m_aiJobKind = AIJobKind::CompositeBlend;
    setStatusText(QStringLiteral("AI lighting refine (subject preserve)…"));
    showStatusMessage(QStringLiteral("AI lighting refine…"), 0);

    AIImageClient::Request req;
    req.prompt = blendPrompt;
    req.provider = dlgResult.provider;
    req.model = dlgResult.model;
    req.size = dlgResult.size;
    req.aspectRatio = dlgResult.aspectRatio;
    req.imageSize = dlgResult.imageSize;
    req.quality = dlgResult.quality;
    req.sourceImage = plate;
    m_aiImageClient->edit(req);
}


// --- finishCompositePipeline ---
void AIWorkflowController::finishCompositePipeline(const QImage &blended, const QString &prompt)
{
    resetAIJobState();
    clearStatusMessage();
    if (blended.isNull()) {
        QMessageBox::warning(dialogParent(), QStringLiteral("Place Subject in Scene"),
                             QStringLiteral("Blend returned an empty image."));
        return;
    }
    // Import only the final composite (no cutout clone; green overlay already hidden)
    importAIImageToCanvas(blended, prompt, false, true);
    setStatusText(
            QStringLiteral("Composite ready — original subject pixels preserved"));
}


// --- startAiIntegrateCutouts ---
void AIWorkflowController::startAiIntegrateCutouts(
    const QVector<CompositeHelper::SubjectSpec> &subjects,
    const QImage &optionalScene,
    const AICompositeDialog::Result &dlgResult)
{
    if (subjects.isEmpty()) {
        resetAIJobState();
        QMessageBox::warning(dialogParent(), QStringLiteral("Place Subject in Scene"),
                             QStringLiteral("No subject cutouts to send to AI."));
        return;
    }

    QString integratePrompt =
        QStringLiteral(
            "The attached image(s) are SUBJECT CUTOUTS (people/objects with "
            "transparent backgrounds). Create ONE new photorealistic photograph "
            "that implements these exact subjects into the scene described below. "
            "CRITICAL: keep the SAME pose, body orientation, scale, and framing "
            "as each cutout — do not rotate or re-pose them. "
            "Match lighting, color temperature, and soft contact shadows so they "
            "look naturally photographed there. Preserve faces, bodies, clothing, "
            "and silhouettes — do not invent different people. Relative positions "
            "between multiple cutouts should stay similar to the references. ");
    if (!optionalScene.isNull()) {
        integratePrompt +=
            QStringLiteral(
                "Also attached is a SCENE / BACKGROUND photo — place the "
                "subjects into that environment (you may extend or adjust the "
                "background for a natural fit). ");
    }
    if (!dlgResult.prompt.isEmpty()) {
        integratePrompt += QStringLiteral("Scene / notes: ") + dlgResult.prompt;
    } else if (optionalScene.isNull()) {
        integratePrompt +=
            QStringLiteral(
                "Scene: a natural outdoor or indoor environment that fits the "
                "subjects.");
    }

    AIImageClient::Request req;
    req.prompt = integratePrompt;
    req.provider = dlgResult.provider;
    req.model = dlgResult.model;
    req.size = dlgResult.size;
    req.aspectRatio = dlgResult.aspectRatio;
    req.imageSize = dlgResult.imageSize;
    req.quality = dlgResult.quality;

    for (const auto &spec : subjects) {
        if (!spec.image.isNull())
            req.referenceImages.append(spec.image);
    }
    if (!optionalScene.isNull()) {
        req.sourceImage = optionalScene;
    } else if (!req.referenceImages.isEmpty()) {
        // Providers that need a primary image (OpenAI) use the first cutout
        req.sourceImage = req.referenceImages.first();
    }

    // Non-Gemini providers only accept one image — pack cutouts onto a plate
    const QString provider = dlgResult.provider.isEmpty()
                                 ? QStringLiteral("nanobanana")
                                 : dlgResult.provider;
    if (provider != QLatin1String("nanobanana") && subjects.size() >= 1) {
        const QSize plateSize = CompositeHelper::platePixelSize(
            dlgResult.imageSize.isEmpty() ? QStringLiteral("1K")
                                          : dlgResult.imageSize,
            dlgResult.aspectRatio.isEmpty() ? QStringLiteral("1:1")
                                            : dlgResult.aspectRatio);
        QImage bg = optionalScene;
        if (bg.isNull()) {
            bg = QImage(plateSize, QImage::Format_RGB888);
            bg.fill(QColor(180, 190, 200)); // neutral stand-in scene
        }
        const QImage plate =
            CompositeHelper::buildPlate(bg, subjects, plateSize,
                                        /*matchLighting=*/true);
        if (!plate.isNull()) {
            req.sourceImage = plate;
            // Still keep cutouts as refs where supported; primary is the plate
        }
    }

    m_aiJobKind = AIJobKind::CompositeAiIntegrate;
    setStatusText(
            QStringLiteral("AI integrating %1 cutout(s) into new scene…")
                .arg(req.referenceImages.size()));
    showStatusMessage(QStringLiteral("AI integrating cutouts…"), 0);
    m_aiImageClient->edit(req);
}


