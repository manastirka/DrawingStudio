#include "MainWindow.h"
#include "DrawingCanvas.h"
#include "DrawingTool.h"
#include "IconFactory.h"
#include "ToolOptionsBar.h"
#include "ToolIconProvider.h"
#include "ToolSuggestionService.h"
#include "ClassicTextTool.h"
#include "CommandManager.h"
#include "Commands.h"
#include "ImagePrimitive.h"
#include "DrawingPrimitive.h"
#include "LayerManager.h"
#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QSettings>
#include <QKeySequence>
#include <QWidgetAction>
#include <QKeySequenceEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QIcon>
#include <QSize>
#include <QMap>
#include <QList>
#include <QDebug>
#include <memory>

void MainWindow::selectTool() { activateDrawingTool(DrawingTool::Select); }

void MainWindow::lineTool() { activateDrawingTool(DrawingTool::Line); }

void MainWindow::curveTool() { activateDrawingTool(DrawingTool::Curve); }

void MainWindow::bezierTool() { activateDrawingTool(DrawingTool::BezierCurve); }

void MainWindow::splineTool() { activateDrawingTool(DrawingTool::Spline); }

void MainWindow::polygonTool() { activateDrawingTool(DrawingTool::Polygon); }

void MainWindow::rectangleTool() { activateDrawingTool(DrawingTool::Rectangle); }

void MainWindow::ellipseTool() { activateDrawingTool(DrawingTool::Ellipse); }

void MainWindow::circleTool() { activateDrawingTool(DrawingTool::Circle); }

void MainWindow::arcTool() { activateDrawingTool(DrawingTool::Arc); }

void MainWindow::angleLineTool() { activateDrawingTool(DrawingTool::AngleLine); }

void MainWindow::eraserTool() { activateDrawingTool(DrawingTool::Eraser); }

void MainWindow::fillTool() { activateDrawingTool(DrawingTool::Fill); }

void MainWindow::brushTool() { activateDrawingTool(DrawingTool::Brush); }

void MainWindow::blurTool() { activateDrawingTool(DrawingTool::Blur); }

void MainWindow::measureTool() { activateDrawingTool(DrawingTool::Measure); }

void MainWindow::imageTool() { activateDrawingTool(DrawingTool::Image); }

void MainWindow::handTool() { activateDrawingTool(DrawingTool::Move); }

void MainWindow::activateDrawingTool(DrawingTool tool)
{
    if (!m_canvas) {
        return;
    }

    if (m_classicTextTool && tool != DrawingTool::Text) {
        m_classicTextTool->deactivate();
    }

    m_canvas->setCurrentTool(tool);

    if (tool == DrawingTool::Text && m_classicTextTool) {
        m_classicTextTool->activate();
    }

    syncToolSlotAppearance(tool);
    persistToolSlotChoice(tool);
    updateToolSettings(tool);
    refreshSmartSuggestions();
}

void MainWindow::setupToolFlyoutSlot(const QList<DrawingTool> &tools, DrawingTool defaultTool)
{
    if (tools.isEmpty() || !m_leftToolbar) {
        return;
    }

    QSettings settings;
    DrawingTool current = defaultTool;
    const QString saved = settings.value(
        QStringLiteral("ToolFlyouts/%1").arg(toolKey(defaultTool))).toString();
    for (DrawingTool t : tools) {
        if (toolKey(t) == saved) {
            current = t;
            break;
        }
    }

    auto *slotAction = new QAction(iconForTool(current), displayNameForTool(current), this);
    slotAction->setCheckable(true);
    slotAction->setToolTip(QStringLiteral("%1 — hold for related tools")
                               .arg(displayNameForTool(current)));
    slotAction->setProperty("currentTool", static_cast<int>(current));
    if (defaultTool == DrawingTool::Select) {
        slotAction->setChecked(true);
    }
    m_toolActionGroup->addAction(slotAction);
    m_leftToolbar->addAction(slotAction);

    auto *menu = new QMenu(m_leftToolbar);
    menu->setToolTipsVisible(true);
    menu->setStyleSheet(QStringLiteral(
        "QMenu { background:#2a2d32; border:1px solid rgba(255,255,255,0.12); padding:4px; }"
        "QMenu::item { padding:6px 18px 6px 8px; color:#e7eaf0; border-radius:4px; }"
        "QMenu::item:selected { background:rgba(138,180,255,0.22); }"));

    for (DrawingTool t : tools) {
        QAction *item = menu->addAction(iconForTool(t), displayNameForTool(t));
        item->setData(static_cast<int>(t));
        const QKeySequence seq = defaultShortcutForTool(t);
        if (!seq.isEmpty()) {
            item->setShortcut(seq);
            item->setShortcutVisibleInContextMenu(true);

            auto *shortcutAction = new QAction(this);
            shortcutAction->setShortcut(seq);
            shortcutAction->setShortcutContext(Qt::ApplicationShortcut);
            connect(shortcutAction, &QAction::triggered, this, [this, t]() {
                activateDrawingTool(t);
            });
            addAction(shortcutAction);
        }
        connect(item, &QAction::triggered, this, [this, t]() {
            activateDrawingTool(t);
        });

        m_toolActions.insert(t, item);
        m_toolSlotActions.insert(t, slotAction);
        m_toolSlotLeader.insert(t, current);
    }

    if (QToolButton *btn = qobject_cast<QToolButton *>(m_leftToolbar->widgetForAction(slotAction))) {
        if (tools.size() > 1) {
            btn->setPopupMode(QToolButton::DelayedPopup);
            btn->setMenu(menu);
        } else {
            // Single-tool slots: no flyout menu
            menu->deleteLater();
        }
        btn->setAutoRaise(true);
    }

    connect(slotAction, &QAction::triggered, this, [this, slotAction]() {
        const QVariant v = slotAction->property("currentTool");
        const int stored = v.isValid() ? v.toInt() : static_cast<int>(DrawingTool::Select);
        activateDrawingTool(static_cast<DrawingTool>(stored));
    });
}

void MainWindow::syncToolSlotAppearance(DrawingTool tool)
{
    QAction *slot = m_toolSlotActions.value(tool, nullptr);
    if (!slot) {
        return;
    }

    slot->setIcon(iconForTool(tool));
    slot->setText(displayNameForTool(tool));
    slot->setToolTip(QStringLiteral("%1 — hold for related tools").arg(displayNameForTool(tool)));
    slot->setProperty("currentTool", static_cast<int>(tool));
    slot->setChecked(true);

    for (auto it = m_toolSlotActions.begin(); it != m_toolSlotActions.end(); ++it) {
        if (it.value() == slot) {
            m_toolSlotLeader[it.key()] = tool;
        }
    }
}

void MainWindow::persistToolSlotChoice(DrawingTool tool)
{
    QAction *slot = m_toolSlotActions.value(tool, nullptr);
    if (!slot || !m_leftToolbar) {
        return;
    }

    DrawingTool familyKey = tool;
    if (QToolButton *btn = qobject_cast<QToolButton *>(m_leftToolbar->widgetForAction(slot))) {
        if (QMenu *menu = btn->menu()) {
            const auto acts = menu->actions();
            if (!acts.isEmpty()) {
                familyKey = static_cast<DrawingTool>(acts.first()->data().toInt());
            }
        }
    }

    QSettings settings;
    settings.setValue(QStringLiteral("ToolFlyouts/%1").arg(toolKey(familyKey)), toolKey(tool));
}

void MainWindow::refreshSmartSuggestions()
{
    if (!m_canvas || !m_toolOptionsBar || !m_toolOptionsBar->suggestionStrip() || !m_toolOptionsBar->suggestionStripLayout()) {
        return;
    }

    ToolSuggestionContext ctx;
    ctx.tool = m_canvas->currentTool();
    ctx.snapEnabled = m_canvas->isSnapEnabled();
    ctx.magneticEnabled = m_canvas->isMagneticConnectionEnabled();
    ctx.gridVisible = m_canvas->isGridVisible();
    ctx.liveDrawHint = m_liveSmartHint;

    const auto &selected = m_canvas->selectedObjects();
    ctx.selectionCount = static_cast<int>(selected.size());
    for (auto *obj : selected) {
        if (auto *img = dynamic_cast<ImagePrimitive *>(obj)) {
            ctx.hasImage = true;
            if (img->getMaskCandidateCount() > 0) {
                ctx.imageHasMasks = true;
            }
        }
        if (dynamic_cast<TextPrimitive *>(obj)) {
            ctx.hasText = true;
        }
    }

    int primitiveCount = 0;
    if (m_layerManager) {
        for (const auto &layer : m_layerManager->layers()) {
            if (layer) {
                primitiveCount += static_cast<int>(layer->primitives().size());
            }
        }
    } else {
        primitiveCount = static_cast<int>(m_canvas->primitives().size());
    }
    ctx.canvasEmpty = (primitiveCount == 0);

    const ToolSuggestionResult result = ToolSuggestionService::resolve(ctx);

    if (m_statusLabel && !result.statusHint.isEmpty()) {
        m_statusLabel->setText(result.statusHint);
    }

    while (QLayoutItem *item = m_toolOptionsBar->suggestionStripLayout()->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }

    const QString chipStyle = QStringLiteral(
        "QToolButton {"
        "  background: rgba(138, 180, 255, 0.14);"
        "  border: 1px solid rgba(138, 180, 255, 0.45);"
        "  border-radius: 11px;"
        "  padding: 2px 10px;"
        "  color: #dce6ff;"
        "  font-size: 11px;"
        "}"
        "QToolButton:hover {"
        "  background: rgba(138, 180, 255, 0.28);"
        "}");

    for (const auto &action : result.chips) {
        auto *btn = new QToolButton(m_toolOptionsBar->suggestionStrip());
        btn->setText(action.label);
        btn->setToolTip(action.tip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(chipStyle);
        btn->setAutoRaise(true);
        const QString actionId = action.id;
        connect(btn, &QToolButton::clicked, this, [this, actionId]() {
            applySmartSuggestion(actionId);
        });
        m_toolOptionsBar->suggestionStripLayout()->addWidget(btn);
    }

    m_toolOptionsBar->suggestionStrip()->setVisible(!result.chips.empty());
}

void MainWindow::applySmartSuggestion(const QString &actionId)
{
    if (!m_canvas) {
        return;
    }

    if (actionId == QLatin1String("detect_subjects")) {
        if (ImagePrimitive *img = imageForDetection()) {
            selectImageForMaskUI(img);
            img->startSubjectDetection();
            if (m_statusLabel) {
                m_statusLabel->setText(QStringLiteral("Detecting subjects…"));
            }
            if (m_suggestionRestoreTimer) {
                m_suggestionRestoreTimer->start(2500);
            }
            m_canvas->update();
        }
        return;
    }

    if (actionId == QLatin1String("extract_subject")) {
        for (auto *obj : m_canvas->selectedObjects()) {
            if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
                auto extracted = imgPrim->extractDetectedSubject();
                if (extracted) {
                    m_canvas->addPrimitiveWithCommand(std::move(extracted));
                    if (m_statusLabel) {
                        m_statusLabel->setText(QStringLiteral("Subject extracted."));
                    }
                }
                m_canvas->update();
                refreshSmartSuggestions();
                return;
            }
        }
        return;
    }

    if (actionId == QLatin1String("remove_background")) {
        for (auto *obj : m_canvas->selectedObjects()) {
            if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
                if (imgPrim->getEditableContour().empty()) {
                    return;
                }
                auto compound = std::make_unique<CompoundCommand>("Remove Background");
                compound->addCommand(std::make_unique<ExtractSubjectCommand>(m_canvas, imgPrim));
                std::vector<DrawingPrimitive *> toDelete = {imgPrim};
                compound->addCommand(std::make_unique<DeletePrimitivesCommand>(m_canvas, toDelete));
                if (m_commandManager) {
                    m_commandManager->executeCommand(std::move(compound));
                }
                refreshSmartSuggestions();
                return;
            }
        }
        return;
    }

    if (actionId == QLatin1String("mask_settings")) {
        showMaskSettingsPopup();
        return;
    }

    if (actionId == QLatin1String("enable_snap")) {
        m_canvas->setSnapEnabled(true);
        refreshSmartSuggestions();
        return;
    }

    if (actionId == QLatin1String("enable_magnetic")) {
        m_canvas->setMagneticConnectionEnabled(true);
        refreshSmartSuggestions();
        return;
    }

    if (actionId == QLatin1String("show_grid")) {
        if (!m_canvas->isGridVisible()) {
            toggleGrid();
        }
        refreshSmartSuggestions();
        return;
    }

    if (actionId == QLatin1String("import_image") || actionId == QLatin1String("switch_image")) {
        imageTool();
        if (m_statusLabel) {
            m_statusLabel->setText(QStringLiteral("Image tool — click the canvas to choose a file"));
        }
        return;
    }

    if (actionId == QLatin1String("switch_text")) {
        textTool();
        return;
    }

    if (actionId == QLatin1String("switch_rectangle")) {
        rectangleTool();
        return;
    }

    if (actionId == QLatin1String("select_all")) {
        selectAll();
        refreshSmartSuggestions();
        return;
    }

    // Informational chips — just reinforce the hint
    if (actionId.startsWith(QLatin1String("hint_"))) {
        refreshSmartSuggestions();
    }
}

void MainWindow::textTool() { activateDrawingTool(DrawingTool::Text); }

QString MainWindow::toolKey(DrawingTool tool) const
{
    switch (tool) {
        case DrawingTool::Select: return "select";
        case DrawingTool::Move: return "hand";
        case DrawingTool::Line: return "line";
        case DrawingTool::Curve: return "curve";
        case DrawingTool::BezierCurve: return "bezier";
        case DrawingTool::Spline: return "spline";
        case DrawingTool::Polygon: return "polygon";
        case DrawingTool::Rectangle: return "rectangle";
        case DrawingTool::Ellipse: return "ellipse";
        case DrawingTool::Circle: return "circle";
        case DrawingTool::Arc: return "arc";
        case DrawingTool::AngleLine: return "angleline";
        case DrawingTool::Eraser: return "eraser";
        case DrawingTool::Fill: return "fill";
        case DrawingTool::Brush: return "brush";
        case DrawingTool::Blur: return "blur";
        case DrawingTool::Measure: return "measure";
        case DrawingTool::Image: return "image";
        case DrawingTool::Text: return "text";
        default: return "tool";
    }
}

void MainWindow::updateFavoritesToolbar()
{
    if (!m_favoritesToolbar) {
        return;
    }

    m_favoritesToolbar->clear();

    if (m_favoriteTools.isEmpty()) {
        m_favoritesToolbar->hide();
        return;
    }

    m_favoritesToolbar->show();
    QLabel *label = new QLabel("★");
    label->setAlignment(Qt::AlignCenter);
    QWidgetAction *labelAction = new QWidgetAction(m_favoritesToolbar);
    labelAction->setDefaultWidget(label);
    m_favoritesToolbar->addAction(labelAction);

    QMap<QString, QAction*> byKey;
    for (auto it = m_toolActions.begin(); it != m_toolActions.end(); ++it) {
        byKey.insert(toolKey(it.key()), it.value());
    }

    for (const auto &key : m_favoriteTools) {
        if (!byKey.contains(key)) {
            continue;
        }
        QAction *source = byKey.value(key);
        QAction *fav = m_favoritesToolbar->addAction(source->icon(), source->text());
        fav->setToolTip(source->toolTip());
        connect(fav, &QAction::triggered, source, &QAction::trigger);
    }
}

void MainWindow::toggleFavoriteTool(DrawingTool tool)
{
    QString key = toolKey(tool);
    if (m_favoriteTools.contains(key)) {
        m_favoriteTools.removeAll(key);
    } else {
        m_favoriteTools.append(key);
    }

    QSettings settings;
    settings.setValue("Favorites/Tools", m_favoriteTools);
    updateFavoritesToolbar();
}

void MainWindow::updateToolTooltips()
{
    for (auto it = m_toolActions.begin(); it != m_toolActions.end(); ++it) {
        QAction *action = it.value();
        if (!action) {
            continue;
        }
        QString tip = action->text();
        if (!action->shortcut().isEmpty()) {
            tip += QString(" (%1)").arg(action->shortcut().toString(QKeySequence::NativeText));
        }
        action->setToolTip(tip);
    }
}

void MainWindow::loadToolShortcuts()
{
    QSettings settings;
    settings.beginGroup("Shortcuts");
    for (auto it = m_toolActions.begin(); it != m_toolActions.end(); ++it) {
        QAction *action = it.value();
        QString key = toolKey(it.key());
        QString stored = settings.value(key).toString();
        if (!stored.isEmpty()) {
            action->setShortcut(QKeySequence(stored));
            action->setShortcutContext(Qt::ApplicationShortcut);
            addAction(action);
        }
    }
    settings.endGroup();
}

void MainWindow::saveToolShortcuts(const QMap<QString, QKeySequence>& shortcuts)
{
    QSettings settings;
    settings.beginGroup("Shortcuts");
    for (auto it = shortcuts.begin(); it != shortcuts.end(); ++it) {
        settings.setValue(it.key(), it.value().toString(QKeySequence::NativeText));
    }
    settings.endGroup();
}

void MainWindow::showShortcutEditor()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Customize Shortcuts");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QWidget *container = new QWidget();
    QFormLayout *form = new QFormLayout(container);
    QMap<QString, QKeySequenceEdit*> edits;

    for (auto it = m_toolActions.begin(); it != m_toolActions.end(); ++it) {
        QAction *action = it.value();
        QString key = toolKey(it.key());
        QKeySequenceEdit *edit = new QKeySequenceEdit(action->shortcut());
        edit->setClearButtonEnabled(true);
        form->addRow(action->text(), edit);
        edits.insert(key, edit);
    }

    QScrollArea *scroll = new QScrollArea();
    scroll->setWidget(container);
    scroll->setWidgetResizable(true);
    layout->addWidget(scroll);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        QMap<QString, QKeySequence> shortcuts;
        for (auto it = edits.begin(); it != edits.end(); ++it) {
            shortcuts.insert(it.key(), it.value()->keySequence());
        }
        saveToolShortcuts(shortcuts);
        for (auto it = m_toolActions.begin(); it != m_toolActions.end(); ++it) {
            QString key = toolKey(it.key());
            if (shortcuts.contains(key)) {
                it.value()->setShortcut(shortcuts.value(key));
                it.value()->setShortcutContext(Qt::ApplicationShortcut);
                addAction(it.value());
            }
        }
        updateToolTooltips();
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
}

void MainWindow::onToolChanged()
{
    if (m_canvas) {
        updateToolSettings(m_canvas->currentTool());
    }
}

QIcon MainWindow::iconForTool(DrawingTool tool)
{
    return ToolIconProvider::iconForTool(tool);
}

QString MainWindow::displayNameForTool(DrawingTool tool) const
{
    return ToolIconProvider::displayNameForTool(tool);
}

QKeySequence MainWindow::defaultShortcutForTool(DrawingTool tool) const
{
    return ToolIconProvider::defaultShortcutForTool(tool);
}

void MainWindow::ensureToolOptionsHost()
{
    if (!m_toolOptionsBar)
        return;
    ToolOptionsBar::Host host;
    host.canvas = m_canvas;
    host.classicTextTool = m_classicTextTool;
    host.commandManager = m_commandManager;
    host.dialogParent = this;
    host.extractSelectedSubjects = [this]() { extractSelectedSubjects(); };
    host.setStatusText = [this](const QString &msg) {
        if (m_statusLabel)
            m_statusLabel->setText(msg);
    };
    host.refreshSmartSuggestions = [this]() { refreshSmartSuggestions(); };
    host.toolKey = [this](DrawingTool tool) { return toolKey(tool); };
    host.imageForDetection = [this]() { return imageForDetection(); };
    host.selectImageForMaskUI = [this](ImagePrimitive *img) { selectImageForMaskUI(img); };
    host.updatePropertyPanel = [this]() { updatePropertyPanel(); };
    m_toolOptionsBar->setHost(std::move(host));
}

void MainWindow::updateToolSettings(DrawingTool tool)
{
    ensureToolOptionsHost();
    if (m_toolOptionsBar)
        m_toolOptionsBar->updateForTool(tool);
}

void MainWindow::showMaskSettingsPopup()
{
    ensureToolOptionsHost();
    if (m_toolOptionsBar)
        m_toolOptionsBar->showMaskSettingsPopup();
}

void MainWindow::updatePresetList(DrawingTool tool)
{
    ensureToolOptionsHost();
    if (m_toolOptionsBar)
        m_toolOptionsBar->updatePresetList(tool);
}

void MainWindow::applyPreset(const QString &presetName)
{
    ensureToolOptionsHost();
    if (m_toolOptionsBar)
        m_toolOptionsBar->applyPreset(presetName);
}

void MainWindow::saveCurrentPreset()
{
    ensureToolOptionsHost();
    if (m_toolOptionsBar)
        m_toolOptionsBar->saveCurrentPreset();
}

