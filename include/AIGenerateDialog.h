#pragma once

#include <QDialog>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QImage>

class AIImageClient;

/**
 * Prompt + provider/options dialog for AI image generate or edit.
 */
class AIGenerateDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { Generate, Edit };

    struct Result {
        QString provider;
        QString model;
        QString prompt;
        QString size;          // WxH
        QString aspectRatio;   // e.g. 1:1
        QString imageSize;     // e.g. 1K (Google)
        QString quality;
        bool replaceSelected = false;
    };

    explicit AIGenerateDialog(Mode mode, bool hasSelectedImage,
                              AIImageClient *client, QWidget *parent = nullptr);

    Result result() const { return m_result; }

private slots:
    void onProviderChanged();
    void onModelChanged();
    void testConnection();
    void acceptGenerate();

private:
    void rebuildOptionCombos();
    void updateNotes();
    void setConnectionStatus(const QString &text, bool ok);

    Mode m_mode;
    bool m_hasSelectedImage = false;
    AIImageClient *m_client = nullptr;
    Result m_result;

    QComboBox *m_provider = nullptr;
    QComboBox *m_model = nullptr;
    QComboBox *m_size = nullptr;
    QComboBox *m_aspect = nullptr;
    QComboBox *m_imageSize = nullptr;
    QComboBox *m_quality = nullptr;
    QPlainTextEdit *m_prompt = nullptr;
    QCheckBox *m_replace = nullptr;
    QLabel *m_notes = nullptr;
    QLabel *m_connectionStatus = nullptr;
    QPushButton *m_testBtn = nullptr;
    QLabel *m_sizeLabel = nullptr;
    QLabel *m_aspectLabel = nullptr;
    QLabel *m_imageSizeLabel = nullptr;
    QLabel *m_qualityLabel = nullptr;
};
