#include "AIGenerateDialog.h"
#include "AIProviderCatalog.h"
#include "AIImageClient.h"
#include "AISettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSettings>
#include <QGroupBox>
#include <QScrollArea>

AIGenerateDialog::AIGenerateDialog(Mode mode, bool hasSelectedImage,
                                   AIImageClient *client, QWidget *parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_hasSelectedImage(hasSelectedImage)
    , m_client(client)
{
    setWindowTitle(mode == Mode::Edit ? QStringLiteral("Edit Image with AI")
                                      : QStringLiteral("Generate Image with AI"));
    setMinimumSize(560, 520);
    resize(600, 580);

    auto *root = new QVBoxLayout(this);

    auto *form = new QFormLayout();
    m_provider = new QComboBox(this);
    for (const auto &opt : AIProviderCatalog::all()) {
        if (mode == Mode::Edit && !opt.supportsEdit)
            continue;
        m_provider->addItem(opt.displayName, opt.id);
    }
    form->addRow(QStringLiteral("Image API"), m_provider);

    m_model = new QComboBox(this);
    m_model->setEditable(true);
    form->addRow(QStringLiteral("Model"), m_model);

    m_sizeLabel = new QLabel(QStringLiteral("Resolution"), this);
    m_size = new QComboBox(this);
    form->addRow(m_sizeLabel, m_size);

    m_aspectLabel = new QLabel(QStringLiteral("Aspect ratio"), this);
    m_aspect = new QComboBox(this);
    form->addRow(m_aspectLabel, m_aspect);

    m_imageSizeLabel = new QLabel(QStringLiteral("Output size"), this);
    m_imageSize = new QComboBox(this);
    form->addRow(m_imageSizeLabel, m_imageSize);

    m_qualityLabel = new QLabel(QStringLiteral("Quality"), this);
    m_quality = new QComboBox(this);
    form->addRow(m_qualityLabel, m_quality);
    root->addLayout(form);

    m_notes = new QLabel(this);
    m_notes->setWordWrap(true);
    m_notes->setStyleSheet(QStringLiteral(
        "QLabel { color: #9ca3af; font-size: 12px; padding: 6px; "
        "background: #1f2937; border-radius: 6px; }"));
    root->addWidget(m_notes);

    root->addWidget(new QLabel(
        mode == Mode::Edit ? QStringLiteral("Edit instruction:")
                           : QStringLiteral("Prompt:"),
        this));
    m_prompt = new QPlainTextEdit(this);
    m_prompt->setPlaceholderText(
        mode == Mode::Edit
            ? QStringLiteral("Describe how to change the selected image…")
            : QStringLiteral("Describe the image to generate…"));
    m_prompt->setMinimumHeight(120);
    root->addWidget(m_prompt);

    if (mode == Mode::Generate && hasSelectedImage) {
        m_replace = new QCheckBox(
            QStringLiteral("Replace selected canvas image with the result"), this);
        m_replace->setChecked(false);
        root->addWidget(m_replace);
    } else if (mode == Mode::Edit) {
        auto *hint = new QLabel(
            QStringLiteral("The currently selected canvas image will be sent as a reference."),
            this);
        hint->setStyleSheet(QStringLiteral("color: #93c5fd;"));
        root->addWidget(hint);
    }

    auto *connRow = new QHBoxLayout();
    m_testBtn = new QPushButton(QStringLiteral("Test API connection"), this);
    m_connectionStatus = new QLabel(QStringLiteral("Not tested"), this);
    m_connectionStatus->setStyleSheet(QStringLiteral("color: #9ca3af;"));
    connRow->addWidget(m_testBtn);
    connRow->addWidget(m_connectionStatus, 1);
    root->addLayout(connRow);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)
        ->setText(mode == Mode::Edit ? QStringLiteral("Edit")
                                     : QStringLiteral("Generate"));
    connect(buttons, &QDialogButtonBox::accepted, this, &AIGenerateDialog::acceptGenerate);
    connect(buttons, &QDialogButtonBox::rejected, this, &AIGenerateDialog::reject);
    root->addWidget(buttons);

    connect(m_provider, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AIGenerateDialog::onProviderChanged);
    connect(m_model, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AIGenerateDialog::onModelChanged);
    connect(m_testBtn, &QPushButton::clicked, this, &AIGenerateDialog::testConnection);

    // Prefill active provider from settings
    const QString active = AISettingsDialog::activeProvider();
    const int pIdx = m_provider->findData(active);
    m_provider->setCurrentIndex(pIdx >= 0 ? pIdx : 0);
    onProviderChanged();
}

void AIGenerateDialog::onProviderChanged()
{
    rebuildOptionCombos();
    updateNotes();
    setConnectionStatus(QStringLiteral("Not tested"), false);
}

void AIGenerateDialog::onModelChanged()
{
    const QString provider = m_provider->currentData().toString();
    if (provider == QLatin1String("nanobanana")) {
        const QString model = m_model->currentText().trimmed().toLower();
        const bool no512 =
            model.contains(QLatin1String("pro")) ||
            model.contains(QLatin1String("nano-banana-pro"));
        const QString cur = m_imageSize->currentText();
        m_imageSize->clear();
        QStringList sizes = {QStringLiteral("1K"), QStringLiteral("2K"),
                             QStringLiteral("4K")};
        if (!no512)
            sizes << QStringLiteral("512");
        m_imageSize->addItems(sizes);
        int idx = m_imageSize->findText(cur);
        if (idx < 0 || (no512 && cur == QLatin1String("512")))
            idx = m_imageSize->findText(QStringLiteral("1K"));
        m_imageSize->setCurrentIndex(idx >= 0 ? idx : 0);
        return;
    }

    if (provider != QLatin1String("openai"))
        return;
    const QString model = m_model->currentText().trimmed();
    QStringList sizes;
    if (model.contains(QLatin1String("dall-e-2"))) {
        sizes = {QStringLiteral("1024x1024"), QStringLiteral("512x512"),
                 QStringLiteral("256x256")};
    } else if (model.contains(QLatin1String("gpt-image"))) {
        sizes = {QStringLiteral("1024x1024"), QStringLiteral("1024x1536"),
                 QStringLiteral("1536x1024"), QStringLiteral("auto")};
    } else {
        // dall-e-3
        sizes = {QStringLiteral("1024x1024"), QStringLiteral("1792x1024"),
                 QStringLiteral("1024x1792")};
    }
    const QString cur = m_size->currentText();
    m_size->clear();
    m_size->addItems(sizes);
    const int idx = m_size->findText(cur);
    m_size->setCurrentIndex(idx >= 0 ? idx : 0);

    QStringList quals;
    if (model.contains(QLatin1String("gpt-image"))) {
        quals = {QStringLiteral("auto"), QStringLiteral("low"),
                 QStringLiteral("medium"), QStringLiteral("high")};
    } else if (model.contains(QLatin1String("dall-e-3"))) {
        quals = {QStringLiteral("standard"), QStringLiteral("hd")};
    }
    m_quality->clear();
    m_quality->addItems(quals);
    m_qualityLabel->setVisible(!quals.isEmpty());
    m_quality->setVisible(!quals.isEmpty());
}

void AIGenerateDialog::rebuildOptionCombos()
{
    const QString id = m_provider->currentData().toString();
    const AIProviderOption opt = AIProviderCatalog::byId(id);

    QSettings s;
    m_model->clear();
    m_model->addItems(opt.models);
    QString savedModel;
    if (id == QLatin1String("nanobanana"))
        savedModel = s.value(QStringLiteral("AI/nanoBananaModel")).toString();
    else if (id == QLatin1String("openai"))
        savedModel = s.value(QStringLiteral("AI/openaiModel"),
                             s.value(QStringLiteral("AI/model")).toString())
                         .toString();
    else if (id == QLatin1String("higgsfield"))
        savedModel = s.value(QStringLiteral("AI/higgsfieldModel")).toString();
    if (!savedModel.isEmpty()) {
        int mi = m_model->findText(savedModel);
        if (mi < 0) {
            m_model->addItem(savedModel);
            mi = m_model->findText(savedModel);
        }
        m_model->setCurrentIndex(mi);
    }

    m_size->clear();
    m_size->addItems(opt.sizes);
    const bool hasSize = !opt.sizes.isEmpty();
    m_sizeLabel->setVisible(hasSize);
    m_size->setVisible(hasSize);
    if (hasSize) {
        const QString savedSize =
            s.value(QStringLiteral("AI/size"), QStringLiteral("1024x1024")).toString();
        const int si = m_size->findText(savedSize);
        m_size->setCurrentIndex(si >= 0 ? si : 0);
    }

    m_aspect->clear();
    m_aspect->addItems(opt.aspectRatios);
    const bool hasAspect = !opt.aspectRatios.isEmpty();
    m_aspectLabel->setVisible(hasAspect);
    m_aspect->setVisible(hasAspect);
    if (hasAspect) {
        const QString savedAspect =
            s.value(QStringLiteral("AI/aspectRatio"), QStringLiteral("1:1")).toString();
        const int ai = m_aspect->findText(savedAspect);
        m_aspect->setCurrentIndex(ai >= 0 ? ai : 0);
    }

    m_imageSize->clear();
    m_imageSize->addItems(opt.imageSizes);
    const bool hasImgSize = !opt.imageSizes.isEmpty();
    m_imageSizeLabel->setVisible(hasImgSize);
    m_imageSize->setVisible(hasImgSize);
    if (hasImgSize) {
        const QString saved =
            s.value(QStringLiteral("AI/imageSize"), QStringLiteral("1K")).toString();
        const int ii = m_imageSize->findText(saved);
        m_imageSize->setCurrentIndex(ii >= 0 ? ii : 0);
    }

    m_quality->clear();
    m_quality->addItems(opt.qualities);
    const bool hasQ = !opt.qualities.isEmpty();
    m_qualityLabel->setVisible(hasQ);
    m_quality->setVisible(hasQ);

    onModelChanged();
}

void AIGenerateDialog::updateNotes()
{
    const AIProviderOption opt =
        AIProviderCatalog::byId(m_provider->currentData().toString());
    m_notes->setText(opt.notes);
}

void AIGenerateDialog::setConnectionStatus(const QString &text, bool ok)
{
    m_connectionStatus->setText(text);
    m_connectionStatus->setStyleSheet(
        ok ? QStringLiteral("color: #4ade80;")
           : QStringLiteral("color: #f87171;"));
    if (text == QLatin1String("Not tested"))
        m_connectionStatus->setStyleSheet(QStringLiteral("color: #9ca3af;"));
}

void AIGenerateDialog::testConnection()
{
    if (!m_client) {
        setConnectionStatus(QStringLiteral("No AI client"), false);
        return;
    }
    m_testBtn->setEnabled(false);
    setConnectionStatus(QStringLiteral("Testing…"), false);

    const QString provider = m_provider->currentData().toString();
    disconnect(m_client, &AIImageClient::connectionTestFinished, this, nullptr);
    connect(m_client, &AIImageClient::connectionTestFinished, this,
            [this](bool ok, const QString &message) {
                m_testBtn->setEnabled(true);
                setConnectionStatus(message, ok);
            });
    m_client->testConnection(provider);
}

void AIGenerateDialog::acceptGenerate()
{
    const QString prompt = m_prompt->toPlainText().trimmed();
    if (prompt.isEmpty()) {
        m_prompt->setFocus();
        return;
    }

    m_result.provider = m_provider->currentData().toString();
    m_result.model = m_model->currentText().trimmed();
    m_result.prompt = prompt;
    m_result.size = m_size->isVisible() ? m_size->currentText() : QString();
    m_result.aspectRatio =
        m_aspect->isVisible() ? m_aspect->currentText() : QString();
    m_result.imageSize =
        m_imageSize->isVisible() ? m_imageSize->currentText() : QString();
    m_result.quality =
        m_quality->isVisible() ? m_quality->currentText() : QString();
    m_result.replaceSelected =
        (m_mode == Mode::Edit) || (m_replace && m_replace->isChecked());

    // Remember last choices
    QSettings s;
    s.setValue(QStringLiteral("AI/provider"), m_result.provider);
    if (!m_result.size.isEmpty())
        s.setValue(QStringLiteral("AI/size"), m_result.size);
    if (!m_result.aspectRatio.isEmpty())
        s.setValue(QStringLiteral("AI/aspectRatio"), m_result.aspectRatio);
    if (!m_result.imageSize.isEmpty())
        s.setValue(QStringLiteral("AI/imageSize"), m_result.imageSize);
    if (m_result.provider == QLatin1String("nanobanana"))
        s.setValue(QStringLiteral("AI/nanoBananaModel"), m_result.model);
    else if (m_result.provider == QLatin1String("openai")) {
        s.setValue(QStringLiteral("AI/openaiModel"), m_result.model);
        s.setValue(QStringLiteral("AI/model"), m_result.model);
    } else if (m_result.provider == QLatin1String("higgsfield"))
        s.setValue(QStringLiteral("AI/higgsfieldModel"), m_result.model);

    accept();
}
