#pragma once

#include <QEvent>
#include <QPoint>
#include <QString>
#include <QWidget>

#include <functional>

class DrawingCanvas;
class CommandManager;
class QLabel;
class QPushButton;
class QSlider;

/**
 * Floating SAM2 mask selection panel (child of DrawingCanvas).
 * Extracted from MainWindow (refactor A4).
 */
class FloatingMaskPanel : public QWidget
{
    Q_OBJECT

public:
    struct Host {
        DrawingCanvas *canvas = nullptr;
        CommandManager *commandManager = nullptr;
        std::function<void()> selectNextMask;
        std::function<void()> selectPreviousMask;
        std::function<void()> invertSelectedMask;
        std::function<void(int index)> onMaskSelectionChanged;
        std::function<void(const QString &)> setStatusText;
    };

    explicit FloatingMaskPanel(QWidget *canvasParent);
    void setHost(Host host);
    void refresh();
    void showAtDefaultPosition();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void buildUi();
    void setStatus(const QString &text);

    Host m_host;
    QPoint m_dragStartPosition;
    bool m_isDragging = false;

    QLabel *m_counterLabel = nullptr;
    QLabel *m_scoreLabel = nullptr;
    QLabel *m_stabilityLabel = nullptr;
    QLabel *m_areaLabel = nullptr;
    QLabel *m_iouLabel = nullptr;
    QWidget *m_scoreBar = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QSlider *m_slider = nullptr;
};
