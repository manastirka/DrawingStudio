#include "AICompositeDialog.h"
#include "AIProviderCatalog.h"
#include "AIImageClient.h"
#include "AISettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSettings>
#include <QMessageBox>

AICompositeDialog::AICompositeDialog(bool hasMaskedSubject, bool hasCutoutSubject,
                                     bool hasDistinctSceneImage, AIImageClient *client,
                                     QWidget *parent)
    : QDialog(parent)
    , m_client(client)
    , m_hasMaskedSubject(hasMaskedSubject)
    , m_hasCutoutSubject(hasCutoutSubject)
    , m_hasSceneImage(hasDistinctSceneImage)
{
    setWindowTitle(QStringLiteral("Place Subject in Scene"));
    setMinimumSize(560, 560);
    resize(600, 600);

    auto *root = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_mode = new QComboBox(this);
    m_mode->addItem(QStringLiteral("Photo subject → AI background"),
                    static_cast<int>(Mode::SubjectIntoScene));
    m_mode->addItem(QStringLiteral("Cutout / AI subject → photo background"),
                    static_cast<int>(Mode::SceneOntoSubject));
    form->addRow(QStringLiteral("Mode"), m_mode);

    m_provider = new QComboBox(this);
    for (const auto &opt : AIProviderCatalog::all()) {
        if (!opt.supportsEdit)
            continue;
        m_provider->addItem(opt.displayName, opt.id);
    }
    form->addRow(QStringLiteral("Blend API"), m_provider);

    m_model = new QComboBox(this);
    m_model->setEditable(true);
    form->addRow(QStringLiteral("Model"), m_model);

    m_aspectLabel = new QLabel(QStringLiteral("Aspect ratio"), this);
    m_aspect = new QComboBox(this);
    form->addRow(m_aspectLabel, m_aspect);

    m_imageSizeLabel = new QLabel(QStringLiteral("Output size"), this);
    m_imageSize = new QComboBox(this);
    form->addRow(m_imageSizeLabel, m_imageSize);

    m_sizeLabel = new QLabel(QStringLiteral("Resolution"), this);
    m_size = new QComboBox(this);
    form->addRow(m_sizeLabel, m_size);

    m_qualityLabel = new QLabel(QStringLiteral("Quality"), this);
    m_quality = new QComboBox(this);
    form->addRow(m_qualityLabel, m_quality);
    root->addLayout(form);

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    m_hint->setStyleSheet(QStringLiteral(
        "QLabel { color: #93c5fd; font-size: 12px; padding: 6px; }"));
    root->addWidget(m_hint);

    m_notes = new QLabel(this);
    m_notes->setWordWrap(true);
    m_notes->setStyleSheet(QStringLiteral(
        "QLabel { color: #9ca3af; font-size: 12px; padding: 6px; "
        "background: #1f2937; border-radius: 6px; }"));
    root->addWidget(m_notes);

    m_promptLabel = new QLabel(QStringLiteral("Scene / placement prompt:"), this);
    root->addWidget(m_promptLabel);
    m_prompt = new QPlainTextEdit(this);
    m_prompt->setMinimumHeight(110);
    root->addWidget(m_prompt);

    m_placeCutout = new QCheckBox(
        QStringLiteral("Also place subject cutout on canvas"),
        this);
    m_placeCutout->setChecked(false);
    m_placeCutout->setToolTip(
        QStringLiteral("Off by default — avoids duplicating the green-selected subject."));
    root->addWidget(m_placeCutout);

    m_placeBackground = new QCheckBox(
        QStringLiteral("Also place generated background on canvas"),
        this);
    m_placeBackground->setChecked(false);
    root->addWidget(m_placeBackground);

    m_aiRefine = new QCheckBox(
        QStringLiteral("AI lighting refine (can change the subject — not recommended)"),
        this);
    m_aiRefine->setChecked(false);
    m_aiRefine->setToolTip(
        QStringLiteral(
            "Default keeps the SAM2 subject pixels unchanged and only tints "
            "lighting locally. Enable only if you want a full AI re-blend."));
    root->addWidget(m_aiRefine);

    m_aiIntegrate = new QCheckBox(
        QStringLiteral("AI redraw subjects into scene (may change faces)"),
        this);
    m_aiIntegrate->setChecked(
        QSettings().value(QStringLiteral("AI/compositeAiIntegrate"), false)
            .toBool());
    m_aiIntegrate->setToolTip(
        QStringLiteral(
            "Sends cutouts to the AI so it paints them into a new scene "
            "(lighting/shadows). Faces can change. Turn OFF \"Preserve original "
            "faces\" to use this. When preserve is on, this is ignored."));
    root->addWidget(m_aiIntegrate);

    m_preserveFaces = new QCheckBox(
        QStringLiteral("Preserve original faces (exact cutouts on empty AI background)"),
        this);
    m_preserveFaces->setChecked(
        QSettings().value(QStringLiteral("AI/compositePreserveFaces"), true)
            .toBool());
    m_preserveFaces->setToolTip(
        QStringLiteral(
            "Recommended. AI generates an empty background only, then your "
            "exact cutouts are placed on it — same faces, same character count. "
            "Does not paste cutouts on top of AI-drawn people."));
    root->addWidget(m_preserveFaces);

    auto *connRow = new QHBoxLayout();
    m_testBtn = new QPushButton(QStringLiteral("Test API connection"), this);
    m_connectionStatus = new QLabel(QStringLiteral("Not tested"), this);
    m_connectionStatus->setStyleSheet(QStringLiteral("color: #9ca3af;"));
    connRow->addWidget(m_testBtn);
    connRow->addWidget(m_connectionStatus, 1);
    root->addLayout(connRow);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Compose"));
    connect(buttons, &QDialogButtonBox::accepted, this, &AICompositeDialog::acceptDialog);
    connect(buttons, &QDialogButtonBox::rejected, this, &AICompositeDialog::reject);
    root->addWidget(buttons);

    connect(m_mode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AICompositeDialog::onModeChanged);
    connect(m_provider, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AICompositeDialog::onProviderChanged);
    connect(m_model, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AICompositeDialog::onModelChanged);
    connect(m_testBtn, &QPushButton::clicked, this, &AICompositeDialog::testConnection);

    const QString active = AISettingsDialog::activeProvider();
    int pIdx = m_provider->findData(active);
    if (pIdx < 0)
        pIdx = m_provider->findData(QStringLiteral("nanobanana"));
    m_provider->setCurrentIndex(pIdx >= 0 ? pIdx : 0);

    // Only default to photo-background mode when a SEPARATE scene photo exists.
    // A single green-masked photo must use SubjectIntoScene (AI generates bg).
    if (m_hasSceneImage && (m_hasCutoutSubject || m_hasMaskedSubject))
        m_mode->setCurrentIndex(1);
    else
        m_mode->setCurrentIndex(0);

    onProviderChanged();
    onModeChanged();
}

void AICompositeDialog::onModeChanged()
{
    updateHints();
    const Mode mode =
        static_cast<Mode>(m_mode->currentData().toInt());
    if (mode == Mode::SubjectIntoScene) {
        m_promptLabel->setText(QStringLiteral("Background scene prompt (no people):"));
        m_prompt->setPlaceholderText(
            QStringLiteral(
                "Describe ONLY the empty environment — e.g. sunny park path, "
                "modern kitchen, rainy street at dusk. Do not mention people."));
        if (m_placeBackground)
            m_placeBackground->setVisible(true);
        if (m_placeCutout)
            m_placeCutout->setVisible(false);
    } else {
        m_promptLabel->setText(QStringLiteral("Placement notes (optional):"));
        m_prompt->setPlaceholderText(
            QStringLiteral("e.g. standing on the left, warm evening light…"));
        if (m_placeBackground)
            m_placeBackground->setVisible(false);
        if (m_placeCutout)
            m_placeCutout->setVisible(false); // never place cutout — causes duplicates
    }
}

void AICompositeDialog::onProviderChanged()
{
    rebuildOptionCombos();
    const AIProviderOption opt =
        AIProviderCatalog::byId(m_provider->currentData().toString());
    m_notes->setText(opt.notes);
    setConnectionStatus(QStringLiteral("Not tested"), false);
}

void AICompositeDialog::onModelChanged()
{
    const QString provider = m_provider->currentData().toString();
    if (provider != QLatin1String("nanobanana"))
        return;
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
}

void AICompositeDialog::rebuildOptionCombos()
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
        savedModel = s.value(QStringLiteral("AI/openaiModel")).toString();
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

    m_aspect->clear();
    m_aspect->addItems(opt.aspectRatios);
    const bool hasAspect = !opt.aspectRatios.isEmpty();
    m_aspectLabel->setVisible(hasAspect);
    m_aspect->setVisible(hasAspect);
    if (hasAspect) {
        const QString saved =
            s.value(QStringLiteral("AI/aspectRatio"), QStringLiteral("1:1")).toString();
        const int ai = m_aspect->findText(saved);
        m_aspect->setCurrentIndex(ai >= 0 ? ai : 0);
    }

    m_imageSize->clear();
    m_imageSize->addItems(opt.imageSizes);
    const bool hasImg = !opt.imageSizes.isEmpty();
    m_imageSizeLabel->setVisible(hasImg);
    m_imageSize->setVisible(hasImg);
    if (hasImg) {
        const QString saved =
            s.value(QStringLiteral("AI/imageSize"), QStringLiteral("1K")).toString();
        const int ii = m_imageSize->findText(saved);
        m_imageSize->setCurrentIndex(ii >= 0 ? ii : 0);
    }

    m_size->clear();
    m_size->addItems(opt.sizes);
    const bool hasSize = !opt.sizes.isEmpty();
    m_sizeLabel->setVisible(hasSize);
    m_size->setVisible(hasSize);
    if (hasSize) {
        const QString saved =
            s.value(QStringLiteral("AI/size"), QStringLiteral("1024x1024")).toString();
        const int si = m_size->findText(saved);
        m_size->setCurrentIndex(si >= 0 ? si : 0);
    }

    m_quality->clear();
    m_quality->addItems(opt.qualities);
    const bool hasQ = !opt.qualities.isEmpty();
    m_qualityLabel->setVisible(hasQ);
    m_quality->setVisible(hasQ);

    onModelChanged();
}

void AICompositeDialog::updateHints()
{
    const Mode mode =
        static_cast<Mode>(m_mode->currentData().toInt());
    if (mode == Mode::SubjectIntoScene) {
        const bool ok = m_hasMaskedSubject || m_hasCutoutSubject;
        m_hint->setText(
            ok ? QStringLiteral(
                     "Select one or more extracted cutouts (Shift-click) and/or "
                     "masked photos. Each is used as a transparent cutout on one "
                     "EMPTY AI background. Tip: Image → Extract Selected Subject(s) "
                     "extracts all and keeps them selected.")
               : QStringLiteral(
                     "Need selected cutouts or SAM2-masked photos. "
                     "Extract subjects first, then Shift-click to multi-select."));
        m_hint->setStyleSheet(
            ok ? QStringLiteral("QLabel { color: #93c5fd; font-size: 12px; padding: 6px; }")
               : QStringLiteral("QLabel { color: #f87171; font-size: 12px; padding: 6px; }"));
    } else {
        const bool ok =
            m_hasSceneImage && (m_hasCutoutSubject || m_hasMaskedSubject);
        m_hint->setText(
            ok ? QStringLiteral(
                     "Places the subject onto a SEPARATE photo background "
                     "(not the same image the mask came from).")
               : QStringLiteral(
                     "Need a separate photo background PLUS a cutout or "
                     "SAM2-masked subject. A single masked photo is not enough — "
                     "use “Photo subject → AI background” instead."));
        m_hint->setStyleSheet(
            ok ? QStringLiteral("QLabel { color: #93c5fd; font-size: 12px; padding: 6px; }")
               : QStringLiteral("QLabel { color: #f87171; font-size: 12px; padding: 6px; }"));
    }
}

void AICompositeDialog::setConnectionStatus(const QString &text, bool ok)
{
    m_connectionStatus->setText(text);
    m_connectionStatus->setStyleSheet(
        ok ? QStringLiteral("color: #4ade80;")
           : QStringLiteral("color: #f87171;"));
    if (text == QLatin1String("Not tested"))
        m_connectionStatus->setStyleSheet(QStringLiteral("color: #9ca3af;"));
}

void AICompositeDialog::testConnection()
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

void AICompositeDialog::acceptDialog()
{
    const Mode mode =
        static_cast<Mode>(m_mode->currentData().toInt());
    if (mode == Mode::SubjectIntoScene) {
        if (!m_hasMaskedSubject && !m_hasCutoutSubject) {
            QMessageBox::warning(
                this, QStringLiteral("Place Subject in Scene"),
                QStringLiteral(
                    "Select an image with a detected mask or an ARGB cutout."));
            return;
        }
        if (m_prompt->toPlainText().trimmed().isEmpty()) {
            m_prompt->setFocus();
            QMessageBox::information(
                this, QStringLiteral("Place Subject in Scene"),
                QStringLiteral("Enter a background scene prompt."));
            return;
        }
    } else {
        if (!m_hasSceneImage ||
            (!m_hasCutoutSubject && !m_hasMaskedSubject)) {
            QMessageBox::warning(
                this, QStringLiteral("Place Subject in Scene"),
                QStringLiteral(
                    "Select both a photo background and a subject "
                    "(cutout or masked image)."));
            return;
        }
    }

    m_result.mode = mode;
    m_result.provider = m_provider->currentData().toString();
    m_result.model = m_model->currentText().trimmed();
    m_result.prompt = m_prompt->toPlainText().trimmed();
    m_result.aspectRatio =
        m_aspect->isVisible() ? m_aspect->currentText() : QString();
    m_result.imageSize =
        m_imageSize->isVisible() ? m_imageSize->currentText() : QString();
    m_result.size = m_size->isVisible() ? m_size->currentText() : QString();
    m_result.quality =
        m_quality->isVisible() ? m_quality->currentText() : QString();
    m_result.placeCutoutOnCanvas = m_placeCutout && m_placeCutout->isChecked();
    m_result.placeBackgroundOnCanvas =
        m_placeBackground && m_placeBackground->isVisible() &&
        m_placeBackground->isChecked();
    m_result.aiLightingRefine = m_aiRefine && m_aiRefine->isChecked();
    m_result.aiIntegrateCutouts = m_aiIntegrate && m_aiIntegrate->isChecked();
    m_result.preserveOriginalFaces =
        m_preserveFaces && m_preserveFaces->isChecked();

    QSettings s;
    s.setValue(QStringLiteral("AI/provider"), m_result.provider);
    s.setValue(QStringLiteral("AI/compositePlaceCutout"), false); // never persist on
    s.setValue(QStringLiteral("AI/compositePlaceBackground"),
               m_result.placeBackgroundOnCanvas);
    s.setValue(QStringLiteral("AI/compositeAiRefine"), m_result.aiLightingRefine);
    s.setValue(QStringLiteral("AI/compositeAiIntegrate"),
               m_result.aiIntegrateCutouts);
    s.setValue(QStringLiteral("AI/compositePreserveFaces"),
               m_result.preserveOriginalFaces);
    if (!m_result.aspectRatio.isEmpty())
        s.setValue(QStringLiteral("AI/aspectRatio"), m_result.aspectRatio);
    if (!m_result.imageSize.isEmpty())
        s.setValue(QStringLiteral("AI/imageSize"), m_result.imageSize);
    if (!m_result.size.isEmpty())
        s.setValue(QStringLiteral("AI/size"), m_result.size);
    if (m_result.provider == QLatin1String("nanobanana"))
        s.setValue(QStringLiteral("AI/nanoBananaModel"), m_result.model);

    accept();
}
