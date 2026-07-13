#pragma once

#include "AICompositeDialog.h"
#include "CompositeHelper.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

class AIImageClient;
class CommandManager;
class DrawingCanvas;
class ImagePrimitive;
class LayerManager;
class RemoteSDHelper;
class QWidget;

/**
 * AI generate / edit / composite / subject extract orchestration.
 * Extracted from MainWindow (refactor A6).
 */
class AIWorkflowController : public QObject
{
    Q_OBJECT

public:
    enum class AIJobKind {
        None,
        Generate,
        Edit,
        CompositeBackground,
        CompositeBlend,
        CompositeAiIntegrate
    };

    struct Host {
        DrawingCanvas *canvas = nullptr;
        LayerManager *layerManager = nullptr;
        CommandManager *commandManager = nullptr;
        QWidget *dialogParent = nullptr;
        std::function<void(const QString &)> setStatusText;
        std::function<void(const QString &, int ms)> showStatusMessage;
        std::function<void()> clearStatusMessage;
        std::function<ImagePrimitive *()> imageForDetection;
        std::function<void(ImagePrimitive *)> selectImageForMaskUI;
    };

    explicit AIWorkflowController(QObject *parent = nullptr);
    ~AIWorkflowController() override;

    void setHost(Host host);
    const Host &host() const { return m_host; }

    bool isBusy() const;

    void showAISettings();
    void generateAIImage();
    void editSelectedWithAI();
    void extractSelectedSubjects();
    void placeSubjectInScene();

    void importAIImageToCanvas(const QImage &image, const QString &prompt,
                               bool replaceSelectedImage,
                               bool showSuccessDialog = true);
    void replaceSelectedObjectWithImage(const QImage &image, const QString &prompt,
                                        int mode = 0);
    void onImageGenerated(const QImage &image, const QString &prompt);
    bool ensureRemoteSDHelper();

private:
    void resetAIJobState();
    void onAIImageFinished(const QImage &image, const QString &prompt);
    void connectAIImageClient();

    bool resolveCompositeSubjects(QVector<CompositeHelper::SubjectSpec> *subjectsOut,
                                  QString *errorOut);
    bool resolveCompositeInputs(bool preferMaskedSubject, bool placeCutoutOnCanvas,
                                QImage *subjectOut, QImage *sceneOut, QString *errorOut);
    void startCompositeBlend(const QVector<CompositeHelper::SubjectSpec> &subjects,
                             const QImage &background,
                             const AICompositeDialog::Result &dlgResult);
    void startAiIntegrateCutouts(const QVector<CompositeHelper::SubjectSpec> &subjects,
                                 const QImage &optionalScene,
                                 const AICompositeDialog::Result &dlgResult);
    void finishCompositePipeline(const QImage &blended, const QString &prompt);

    void setStatusText(const QString &msg);
    void showStatusMessage(const QString &msg, int ms = 0);
    void clearStatusMessage();
    QWidget *dialogParent() const;
    void selectImageForMaskUIHost(ImagePrimitive *image);

    Host m_host;
    AIImageClient *m_aiImageClient = nullptr;
    RemoteSDHelper *m_remoteSDHelper = nullptr;
    AIJobKind m_aiJobKind = AIJobKind::None;
    bool m_aiReplaceSelected = false;
    QVector<CompositeHelper::SubjectSpec> m_compositeSubjects;
    AICompositeDialog::Result m_compositeDlgResult;
};
