#pragma once

#include <QDialog>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>

class AIImageClient;

/**
 * Guided dialog for Place Subject in Scene (SAM2 cutout + AI blend).
 */
class AICompositeDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        SubjectIntoScene,  // cutout/mask → generate bg → blend
        SceneOntoSubject   // photo bg + cutout → blend
    };

    struct Result {
        Mode mode = Mode::SubjectIntoScene;
        QString provider;
        QString model;
        QString prompt;       // scene prompt or placement notes
        QString aspectRatio;
        QString imageSize;
        QString quality;
        QString size; // WxH for non-Google providers
        bool placeCutoutOnCanvas = false;
        bool placeBackgroundOnCanvas = false;
        /** If true, AI may redraw subject (not recommended). Default: local composite only. */
        bool aiLightingRefine = false;
        /**
         * Special option: send selected cutouts to the AI so it generates a
         * new scene that implements them (lighting, shadows, integration).
         * Default off — local plate composite keeps subject pixels unchanged.
         * Ignored when preserveOriginalFaces is true (avoids double people).
         */
        bool aiIntegrateCutouts = false;
        /**
         * Keep exact cutout pixels (faces unchanged): AI builds an EMPTY
         * background, then cutouts are composited locally. No AI redraw of
         * people — correct character count, no mixed doubles.
         */
        bool preserveOriginalFaces = true;
    };

    /**
     * @param hasDistinctSceneImage true only if a full photo exists that is
     *        NOT the same image as the masked/cutout subject (needed for
     *        SceneOntoSubject). A single SAM2-masked photo must leave this false
     *        so we default to SubjectIntoScene (AI background).
     */
    explicit AICompositeDialog(bool hasMaskedSubject, bool hasCutoutSubject,
                               bool hasDistinctSceneImage, AIImageClient *client,
                               QWidget *parent = nullptr);

    Result result() const { return m_result; }

private slots:
    void onModeChanged();
    void onProviderChanged();
    void onModelChanged();
    void testConnection();
    void acceptDialog();

private:
    void rebuildOptionCombos();
    void updateHints();
    void setConnectionStatus(const QString &text, bool ok);

    AIImageClient *m_client = nullptr;
    Result m_result;
    bool m_hasMaskedSubject = false;
    bool m_hasCutoutSubject = false;
    bool m_hasSceneImage = false;

    QComboBox *m_mode = nullptr;
    QComboBox *m_provider = nullptr;
    QComboBox *m_model = nullptr;
    QComboBox *m_aspect = nullptr;
    QComboBox *m_imageSize = nullptr;
    QComboBox *m_size = nullptr;
    QComboBox *m_quality = nullptr;
    QPlainTextEdit *m_prompt = nullptr;
    QCheckBox *m_placeCutout = nullptr;
    QCheckBox *m_placeBackground = nullptr;
    QCheckBox *m_aiRefine = nullptr;
    QCheckBox *m_aiIntegrate = nullptr;
    QCheckBox *m_preserveFaces = nullptr;
    QLabel *m_hint = nullptr;
    QLabel *m_notes = nullptr;
    QLabel *m_connectionStatus = nullptr;
    QPushButton *m_testBtn = nullptr;
    QLabel *m_aspectLabel = nullptr;
    QLabel *m_imageSizeLabel = nullptr;
    QLabel *m_sizeLabel = nullptr;
    QLabel *m_qualityLabel = nullptr;
    QLabel *m_promptLabel = nullptr;
};
