#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>

/**
 * Configure AI image providers (OpenAI, Stability, Higgsfield, Nano Banana, Remote SD).
 * Persists to QSettings under the "AI/" group.
 */
class AISettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AISettingsDialog(QWidget *parent = nullptr);

    static QString activeProvider();
    static QString providerDisplayName(const QString &id);

private slots:
    void onProviderChanged(int index);
    void saveAndAccept();

private:
    void loadSettings();
    void updateVisibility();

    QComboBox *m_providerCombo = nullptr;
    QLineEdit *m_openaiKey = nullptr;
    QLineEdit *m_stabilityKey = nullptr;
    QLineEdit *m_nanoBananaKey = nullptr;
    QLineEdit *m_higgsfieldKey = nullptr;
    QCheckBox *m_higgsfieldUseCli = nullptr;
    QLineEdit *m_higgsfieldCliPath = nullptr;
    QComboBox *m_higgsfieldModel = nullptr;
    QComboBox *m_nanoBananaModel = nullptr;
    QLineEdit *m_openaiModel = nullptr;
    QLineEdit *m_remoteSdUrl = nullptr;
    QComboBox *m_sizeCombo = nullptr;
    QLabel *m_helpLabel = nullptr;
};
