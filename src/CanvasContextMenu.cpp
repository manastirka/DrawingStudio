#include "CanvasContextMenu.h"

#include <QAction>
#include <QMenu>

namespace CanvasContextMenu {

const char *styleSheet()
{
    return R"(
        QMenu {
            background-color: rgba(30, 30, 30, 0.5);
            color: #e8e8e8;
            border: 1px solid rgba(100, 149, 237, 0.3);
            border-radius: 8px;
            padding: 4px;
        }
        QMenu::item {
            background-color: transparent;
            padding: 8px 16px;
            border-radius: 4px;
            margin: 1px;
        }
        QMenu::item:selected {
            background-color: rgba(100, 149, 237, 0.2);
        }
        QMenu::separator {
            height: 1px;
            background-color: rgba(100, 149, 237, 0.2);
            margin: 4px 8px;
        }
    )";
}

QMenu *build(QWidget *parent, bool hasSelection, const Actions &actions)
{
    auto *menu = new QMenu(parent);

    if (hasSelection) {
        if (actions.deleteSelected) {
            menu->addAction(QStringLiteral("Delete Selected"),
                            actions.deleteSelected);
        }
        menu->addSeparator();

        QMenu *orderMenu = menu->addMenu(QStringLiteral("Order"));
        if (actions.bringToFront) {
            orderMenu->addAction(QStringLiteral("Bring to Front"),
                                 actions.bringToFront);
        }
        if (actions.sendToBack) {
            orderMenu->addAction(QStringLiteral("Send to Back"),
                                 actions.sendToBack);
        }
        orderMenu->addSeparator();
        if (actions.bringForward) {
            orderMenu->addAction(QStringLiteral("Bring Forward"),
                                 actions.bringForward);
        }
        if (actions.sendBackward) {
            orderMenu->addAction(QStringLiteral("Send Backward"),
                                 actions.sendBackward);
        }
        menu->addSeparator();
    }

    if (actions.zoomFit) {
        menu->addAction(QStringLiteral("Zoom to Fit"), actions.zoomFit);
    }
    if (actions.zoomActual) {
        menu->addAction(QStringLiteral("Zoom to Actual Size"),
                        actions.zoomActual);
    }

    menu->setStyleSheet(QString::fromUtf8(styleSheet()));
    return menu;
}

MaskEditAction execMaskEditMenu(QWidget *parent, const QPoint &globalPos,
                                bool canDelete, bool canInsert)
{
    if (!canDelete && !canInsert) {
        return MaskEditAction::None;
    }

    QMenu menu(parent);
    QAction *deleteAct = nullptr;
    QAction *insertAct = nullptr;
    if (canDelete) {
        deleteAct =
            menu.addAction(QStringLiteral("🗑️ Delete Control Point"));
    }
    if (canInsert) {
        insertAct =
            menu.addAction(QStringLiteral("➕ Add Control Point Here"));
    }
    menu.setStyleSheet(QString::fromUtf8(styleSheet()));

    QAction *selected = menu.exec(globalPos);
    if (!selected) {
        return MaskEditAction::None;
    }
    if (selected == deleteAct) {
        return MaskEditAction::DeleteControlPoint;
    }
    if (selected == insertAct) {
        return MaskEditAction::InsertControlPoint;
    }
    // Fallback: match by label (locale-stable emoji strings)
    const QString text = selected->text();
    if (text.contains(QStringLiteral("Delete"))) {
        return MaskEditAction::DeleteControlPoint;
    }
    if (text.contains(QStringLiteral("Add"))) {
        return MaskEditAction::InsertControlPoint;
    }
    return MaskEditAction::None;
}

} // namespace CanvasContextMenu
