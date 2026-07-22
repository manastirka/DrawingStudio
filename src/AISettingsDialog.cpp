#include "AISettingsDialog.h"
#include "AIImageClient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSettings>
#include <QGroupBox>
#include <QApplication>

AISettingsDialog::AISettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("AI API Settings"));
    setMinimumWidth(480);
    resize(520, 620);

    auto *root = new QVBoxLayout(this);

    auto *form = new QFormLayout();
    m_providerCombo = new QComboBox(this);
    m_providerCombo->setObjectName(QStringLiteral("aiProviderCombo"));
    m_providerCombo->addItem(QStringLiteral("OpenAI (DALL·E)"), QStringLiteral("openai"));
    m_providerCombo->addItem(QStringLiteral("Stability AI"), QStringLiteral("stability"));
    m_providerCombo->addItem(QStringLiteral("Nano Banana (Google Gemini)"), QStringLiteral("nanobanana"));
    m_providerCombo->addItem(QStringLiteral("Higgsfield"), QStringLiteral("higgsfield"));
    m_providerCombo->addItem(QStringLiteral("Remote Stable Diffusion"), QStringLiteral("remotesd"));
    form->addRow(QStringLiteral("Active provider"), m_providerCombo);

    m_sizeCombo = new QComboBox(this);
    m_sizeCombo->addItems({QStringLiteral("1024x1024"), QStringLiteral("1024x1792"),
                           QStringLiteral("1792x1024"), QStringLiteral("512x512"),
                           QStringLiteral("768x768")});
    form->addRow(QStringLiteral("Default size"), m_sizeCombo);
    root->addLayout(form);

    auto *openaiBox = new QGroupBox(QStringLiteral("OpenAI"), this);
    auto *openaiForm = new QFormLayout(openaiBox);
    m_openaiKey = new QLineEdit(openaiBox);
    m_openaiKey->setObjectName(QStringLiteral("openaiApiKeyEdit"));
    m_openaiKey->setEchoMode(QLineEdit::Password);
    m_openaiKey->setPlaceholderText(QStringLiteral("sk-..."));
    m_openaiModel = new QLineEdit(openaiBox);
    m_openaiModel->setPlaceholderText(QStringLiteral("dall-e-3"));
    openaiForm->addRow(QStringLiteral("API key"), m_openaiKey);
    openaiForm->addRow(QStringLiteral("Model"), m_openaiModel);
    root->addWidget(openaiBox);

    auto *stabilityBox = new QGroupBox(QStringLiteral("Stability AI"), this);
    auto *stabilityForm = new QFormLayout(stabilityBox);
    m_stabilityKey = new QLineEdit(stabilityBox);
    m_stabilityKey->setObjectName(QStringLiteral("stabilityApiKeyEdit"));
    m_stabilityKey->setEchoMode(QLineEdit::Password);
    m_stabilityKey->setPlaceholderText(QStringLiteral("sk-..."));
    stabilityForm->addRow(QStringLiteral("API key"), m_stabilityKey);
    root->addWidget(stabilityBox);

    auto *nbBox = new QGroupBox(QStringLiteral("Nano Banana (Google AI)"), this);
    auto *nbForm = new QFormLayout(nbBox);
    m_nanoBananaKey = new QLineEdit(nbBox);
    m_nanoBananaKey->setObjectName(QStringLiteral("nanoBananaApiKeyEdit"));
    m_nanoBananaKey->setEchoMode(QLineEdit::Password);
    m_nanoBananaKey->setPlaceholderText(QStringLiteral("AIza... (Google AI Studio key)"));
    m_nanoBananaModel = new QComboBox(nbBox);
    m_nanoBananaModel->setEditable(true);
    m_nanoBananaModel->addItems({
        QStringLiteral("gemini-3.1-flash-image"),
        QStringLiteral("gemini-3.1-flash-lite-image"),
        QStringLiteral("gemini-3-pro-image"),
        QStringLiteral("gemini-2.5-flash-image"),
        QStringLiteral("nano-banana-pro-preview"),
    });
    nbForm->addRow(QStringLiteral("API key"), m_nanoBananaKey);
    nbForm->addRow(QStringLiteral("Model"), m_nanoBananaModel);
    root->addWidget(nbBox);

    auto *hfBox = new QGroupBox(QStringLiteral("Higgsfield"), this);
    auto *hfForm = new QFormLayout(hfBox);
    m_higgsfieldUseCli = new QCheckBox(QStringLiteral("Use local higgsfield CLI (recommended)"), hfBox);
    m_higgsfieldUseCli->setChecked(true);
    m_higgsfieldCliPath = new QLineEdit(hfBox);
    m_higgsfieldCliPath->setObjectName(QStringLiteral("higgsfieldCliPathEdit"));
    m_higgsfieldCliPath->setPlaceholderText(QStringLiteral("higgsfield (or full path)"));
    m_higgsfieldKey = new QLineEdit(hfBox);
    m_higgsfieldKey->setObjectName(QStringLiteral("higgsfieldApiKeyEdit"));
    m_higgsfieldKey->setEchoMode(QLineEdit::Password);
    m_higgsfieldKey->setPlaceholderText(QStringLiteral("Optional cloud API key"));
    m_higgsfieldModel = new QComboBox(hfBox);
    m_higgsfieldModel->setEditable(true);
    m_higgsfieldModel->addItems({
        QStringLiteral("gpt_image_2"),
        QStringLiteral("soul_2_0"),
        QStringLiteral("seedream_4_5"),
        QStringLiteral("z_image"),
    });
    hfForm->addRow(m_higgsfieldUseCli);
    hfForm->addRow(QStringLiteral("CLI path"), m_higgsfieldCliPath);
    hfForm->addRow(QStringLiteral("API key"), m_higgsfieldKey);
    hfForm->addRow(QStringLiteral("Model"), m_higgsfieldModel);
    root->addWidget(hfBox);

    auto *remoteBox = new QGroupBox(QStringLiteral("Remote Stable Diffusion"), this);
    auto *remoteForm = new QFormLayout(remoteBox);
    m_remoteSdUrl = new QLineEdit(remoteBox);
    m_remoteSdUrl->setObjectName(QStringLiteral("remoteSdUrlEdit"));
    m_remoteSdUrl->setPlaceholderText(AIImageClient::defaultRemoteSdUrl());
    remoteForm->addRow(QStringLiteral("Server URL"), m_remoteSdUrl);
    root->addWidget(remoteBox);

    m_helpLabel = new QLabel(this);
    m_helpLabel->setWordWrap(true);
    m_helpLabel->setStyleSheet(QStringLiteral("color: #9ca3af; font-size: 12px;"));
    root->addWidget(m_helpLabel);

    auto *testRow = new QHBoxLayout();
    m_testConnectionButton = new QPushButton(QStringLiteral("Test Connection"), this);
    m_testConnectionButton->setObjectName(QStringLiteral("testAiConnectionButton"));
    m_testConnectionButton->setToolTip(
        QStringLiteral("Probe the active provider with the keys currently shown "
                       "(does not require Save)."));
    m_connectionStatusLabel = new QLabel(QStringLiteral("Not tested"), this);
    m_connectionStatusLabel->setWordWrap(true);
    m_connectionStatusLabel->setStyleSheet(QStringLiteral("color: #9ca3af; font-size: 12px;"));
    testRow->addWidget(m_testConnectionButton);
    testRow->addWidget(m_connectionStatusLabel, 1);
    root->addLayout(testRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &AISettingsDialog::saveAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &AISettingsDialog::reject);
    root->addWidget(buttons);

    connect(m_providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AISettingsDialog::onProviderChanged);
    connect(m_testConnectionButton, &QPushButton::clicked,
            this, &AISettingsDialog::onTestConnection);

    m_testClient = new AIImageClient(this);
    connect(m_testClient, &AIImageClient::connectionTestFinished,
            this, &AISettingsDialog::onConnectionTestFinished);

    loadSettings();
    updateVisibility();
}

QString AISettingsDialog::activeProvider()
{
    return QSettings().value(QStringLiteral("AI/provider"), QStringLiteral("nanobanana")).toString();
}

QString AISettingsDialog::providerDisplayName(const QString &id)
{
    if (id == QLatin1String("openai"))
        return QStringLiteral("OpenAI");
    if (id == QLatin1String("stability"))
        return QStringLiteral("Stability AI");
    if (id == QLatin1String("higgsfield"))
        return QStringLiteral("Higgsfield");
    if (id == QLatin1String("nanobanana"))
        return QStringLiteral("Nano Banana");
    if (id == QLatin1String("remotesd"))
        return QStringLiteral("Remote SD");
    return id;
}

void AISettingsDialog::loadSettings()
{
    QSettings s;
    const QString provider =
        s.value(QStringLiteral("AI/provider"), QStringLiteral("nanobanana")).toString();
    const int idx = m_providerCombo->findData(provider);
    m_providerCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    m_openaiKey->setText(s.value(QStringLiteral("AI/openaiApiKey"),
                                 s.value(QStringLiteral("AI/apiKey")).toString()).toString());
    m_openaiModel->setText(s.value(QStringLiteral("AI/openaiModel"),
                                   s.value(QStringLiteral("AI/model"), QStringLiteral("dall-e-3")).toString()).toString());
    m_stabilityKey->setText(s.value(QStringLiteral("AI/stabilityApiKey")).toString());

    m_nanoBananaKey->setText(
        s.value(QStringLiteral("AI/nanoBananaApiKey"),
                s.value(QStringLiteral("AI/googleApiKey")).toString())
            .toString());
    const QString nbModel = s.value(QStringLiteral("AI/nanoBananaModel"),
                                    QStringLiteral("gemini-3.1-flash-image")).toString();
    int nbIdx = m_nanoBananaModel->findText(nbModel);
    if (nbIdx < 0) {
        m_nanoBananaModel->addItem(nbModel);
        nbIdx = m_nanoBananaModel->findText(nbModel);
    }
    m_nanoBananaModel->setCurrentIndex(nbIdx);

    m_higgsfieldKey->setText(s.value(QStringLiteral("AI/higgsfieldApiKey")).toString());
    m_higgsfieldUseCli->setChecked(s.value(QStringLiteral("AI/higgsfieldUseCli"), true).toBool());
    m_higgsfieldCliPath->setText(s.value(QStringLiteral("AI/higgsfieldCliPath"),
                                         QStringLiteral("higgsfield")).toString());

    const QString hfModel = s.value(QStringLiteral("AI/higgsfieldModel"),
                                    QStringLiteral("gpt_image_2")).toString();
    int hfIdx = m_higgsfieldModel->findText(hfModel);
    if (hfIdx < 0) {
        m_higgsfieldModel->addItem(hfModel);
        hfIdx = m_higgsfieldModel->findText(hfModel);
    }
    m_higgsfieldModel->setCurrentIndex(hfIdx);

    m_remoteSdUrl->setText(
        s.value(QStringLiteral("AI/remoteSDUrl"), AIImageClient::defaultRemoteSdUrl())
            .toString());

    const QString size = s.value(QStringLiteral("AI/size"), QStringLiteral("1024x1024")).toString();
    const int sizeIdx = m_sizeCombo->findText(size);
    m_sizeCombo->setCurrentIndex(sizeIdx >= 0 ? sizeIdx : 0);
}

void AISettingsDialog::onProviderChanged(int)
{
    updateVisibility();
}

void AISettingsDialog::updateVisibility()
{
    const QString id = m_providerCombo->currentData().toString();
    if (id == QLatin1String("openai")) {
        m_helpLabel->setText(QStringLiteral(
            "OpenAI DALL·E for text-to-image. Image edit uses images/edits when a canvas image is selected."));
    } else if (id == QLatin1String("stability")) {
        m_helpLabel->setText(QStringLiteral(
            "Stability AI text-to-image. For edits, the selected canvas image is sent as an init image when supported."));
    } else if (id == QLatin1String("higgsfield")) {
        m_helpLabel->setText(QStringLiteral(
            "Higgsfield via local CLI (higgsfield auth login) or API key. Generate imports to canvas; edit passes the selected image as --image."));
    } else if (id == QLatin1String("nanobanana")) {
        m_helpLabel->setText(QStringLiteral(
            "Google Gemini Nano Banana image models. Uses your Google AI Studio key. Generate creates a new image; Edit sends the selected canvas image as a reference."));
    } else {
        m_helpLabel->setText(QStringLiteral(
            "Local/remote Stable Diffusion HTTP server (text-to-image)."));
    }
}

void AISettingsDialog::writeFormToSettings() const
{
    QSettings s;
    const QString provider = m_providerCombo->currentData().toString();
    s.setValue(QStringLiteral("AI/provider"), provider);
    s.setValue(QStringLiteral("AI/openaiApiKey"), m_openaiKey->text().trimmed());
    if (provider == QLatin1String("openai"))
        s.setValue(QStringLiteral("AI/apiKey"), m_openaiKey->text().trimmed());
    s.setValue(QStringLiteral("AI/openaiModel"), m_openaiModel->text().trimmed());
    s.setValue(QStringLiteral("AI/model"), m_openaiModel->text().trimmed());
    s.setValue(QStringLiteral("AI/stabilityApiKey"), m_stabilityKey->text().trimmed());
    s.setValue(QStringLiteral("AI/nanoBananaApiKey"), m_nanoBananaKey->text().trimmed());
    s.setValue(QStringLiteral("AI/googleApiKey"), m_nanoBananaKey->text().trimmed());
    s.setValue(QStringLiteral("AI/nanoBananaModel"), m_nanoBananaModel->currentText().trimmed());
    s.setValue(QStringLiteral("AI/higgsfieldApiKey"), m_higgsfieldKey->text().trimmed());
    s.setValue(QStringLiteral("AI/higgsfieldUseCli"), m_higgsfieldUseCli->isChecked());
    s.setValue(QStringLiteral("AI/higgsfieldCliPath"),
               m_higgsfieldCliPath->text().trimmed().isEmpty()
                   ? QStringLiteral("higgsfield")
                   : m_higgsfieldCliPath->text().trimmed());
    s.setValue(QStringLiteral("AI/higgsfieldModel"), m_higgsfieldModel->currentText().trimmed());
    const QString remoteSdUrl = m_remoteSdUrl->text().trimmed();
    s.setValue(QStringLiteral("AI/remoteSDUrl"),
               remoteSdUrl.isEmpty() ? AIImageClient::defaultRemoteSdUrl()
                                     : remoteSdUrl);
    s.setValue(QStringLiteral("AI/size"), m_sizeCombo->currentText());
    s.sync();
}

void AISettingsDialog::onTestConnection()
{
    if (!m_testClient || !m_testConnectionButton)
        return;

    const QString provider = m_providerCombo->currentData().toString();
    AIImageClient::ConnectionTestConfig config;
    if (provider == QLatin1String("openai")) {
        config.apiKey = m_openaiKey->text();
    } else if (provider == QLatin1String("stability")) {
        config.apiKey = m_stabilityKey->text();
    } else if (provider == QLatin1String("nanobanana")) {
        config.apiKey = m_nanoBananaKey->text();
    } else if (provider == QLatin1String("higgsfield")) {
        config.apiKey = m_higgsfieldKey->text();
        config.higgsfieldCliPath = m_higgsfieldCliPath->text();
    } else if (provider == QLatin1String("remotesd")) {
        config.remoteSdUrl = m_remoteSdUrl->text();
    }
    m_testConnectionButton->setEnabled(false);
    m_connectionStatusLabel->setStyleSheet(QStringLiteral("color: #9ca3af; font-size: 12px;"));
    m_connectionStatusLabel->setText(
        QStringLiteral("Testing %1…").arg(providerDisplayName(provider)));
    QApplication::processEvents();

    m_testClient->testConnection(provider, config);
}

void AISettingsDialog::onConnectionTestFinished(bool ok, const QString &message)
{
    if (m_testConnectionButton)
        m_testConnectionButton->setEnabled(true);

    if (!m_connectionStatusLabel)
        return;

    if (ok) {
        m_connectionStatusLabel->setStyleSheet(
            QStringLiteral("color: #34d399; font-size: 12px;"));
        m_connectionStatusLabel->setText(QStringLiteral("✓ %1").arg(message));
    } else {
        m_connectionStatusLabel->setStyleSheet(
            QStringLiteral("color: #f87171; font-size: 12px;"));
        m_connectionStatusLabel->setText(QStringLiteral("✗ %1").arg(message));
    }

    // Remember last health probe for status bar / debugging.
    QSettings s;
    s.setValue(QStringLiteral("AI/lastConnectionOk"), ok);
    s.setValue(QStringLiteral("AI/lastConnectionMessage"), message);
    s.setValue(QStringLiteral("AI/lastConnectionProvider"),
               m_providerCombo ? m_providerCombo->currentData().toString() : QString());
}

void AISettingsDialog::saveAndAccept()
{
    writeFormToSettings();
    accept();
}
