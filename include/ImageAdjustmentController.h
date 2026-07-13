#pragma once

#include <QObject>
#include <QString>

#include <functional>

class DrawingCanvas;
class QWidget;

/**
 * Image adjustment dialogs (levels, curves, blur suite, auto-enhance).
 * Extracted from MainWindow:: methods in ImageAdjustments*.cpp (refactor A5).
 */
class ImageAdjustmentController : public QObject
{
    Q_OBJECT

public:
    struct Host {
        DrawingCanvas *canvas = nullptr;
        QWidget *dialogParent = nullptr;
        std::function<void(const QString &)> setStatusText;
    };

    explicit ImageAdjustmentController(QObject *parent = nullptr);

    void setHost(Host host);
    const Host &host() const { return m_host; }

    void showLevelsAdjustment();
    void showCurvesAdjustment();
    void showShadowsAdjustment();
    void showHighlightsAdjustment();
    void showBrightnessContrast();
    void showHueSaturation();

    void showGaussianBlur();
    void showMotionBlur();
    void showRadialBlur();
    void showBokehBlur();
    void showSurfaceBlur();
    void autoEnhanceImage();

private:
    Host m_host;
};
