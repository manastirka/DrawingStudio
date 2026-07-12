// Image adjustment implementations for MainWindow
// This file contains the implementation of image adjustment features

#include "MainWindow.h"
#include "ImagePrimitive.h"
#include "DrawingCanvas.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QtMath>
#include <QImage>
#include <QColor>
#include <QPainter>
#include <QPolygonF>

// Helper function to create mask image from contour
static QImage createMaskImage(const std::vector<QPointF>& contour, const QSize& size, bool inverted) {
    QImage maskImage(size, QImage::Format_Grayscale8);
    maskImage.fill(inverted ? Qt::white : Qt::black);
    
    if (contour.empty()) {
        return maskImage;
    }
    
    QPainter maskPainter(&maskImage);
    maskPainter.setRenderHint(QPainter::Antialiasing);
    maskPainter.setBrush(inverted ? Qt::black : Qt::white);
    maskPainter.setPen(Qt::NoPen);
    
    QPolygonF polygon;
    for (const auto& point : contour) {
        polygon << point;
    }
    maskPainter.drawPolygon(polygon);
    maskPainter.end();
    
    return maskImage;
}

void MainWindow::showLevelsAdjustment()
{
    if (!m_canvas) return;
    
    auto selectedObjects = m_canvas->selectedObjects();
    ImagePrimitive* selectedImage = nullptr;
    
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            selectedImage = imgPrim;
            break;
        }
    }
    
    if (!selectedImage) {
        QMessageBox::information(this, "Levels", "Please select an image first");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Levels Adjustment");
    dialog.setMinimumWidth(450);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QSlider* blackSlider = new QSlider(Qt::Horizontal);
    blackSlider->setRange(0, 255);
    blackSlider->setValue(0);
    QLabel* blackValue = new QLabel("0");
    
    QSlider* whiteSlider = new QSlider(Qt::Horizontal);
    whiteSlider->setRange(0, 255);
    whiteSlider->setValue(255);
    QLabel* whiteValue = new QLabel("255");
    
    QSlider* gammaSlider = new QSlider(Qt::Horizontal);
    gammaSlider->setRange(10, 300);
    gammaSlider->setValue(100);
    QLabel* gammaValue = new QLabel("1.00");
    
    QImage originalImage = selectedImage->image();
    
    // Check if image has a mask
    bool hasMask = selectedImage->getMaskCandidateCount() > 0;
    QImage maskImage;
    if (hasMask) {
        auto contour = selectedImage->getEditableContour();
        bool inverted = selectedImage->isMaskInverted();
        maskImage = createMaskImage(contour, originalImage.size(), inverted);
    }
    
    auto applyLevels = [&]() {
        int inBlack = blackSlider->value();
        int inWhite = whiteSlider->value();
        float gamma = gammaSlider->value() / 100.0f;
        
        blackValue->setText(QString::number(inBlack));
        whiteValue->setText(QString::number(inWhite));
        gammaValue->setText(QString::number(gamma, 'f', 2));
        
        QImage adjusted = originalImage.copy();
        for (int y = 0; y < adjusted.height(); ++y) {
            for (int x = 0; x < adjusted.width(); ++x) {
                // Check if pixel is in masked area
                if (hasMask) {
                    int maskValue = qGray(maskImage.pixel(x, y));
                    if (maskValue < 128) continue; // Skip pixels outside mask
                }
                
                QColor color = adjusted.pixelColor(x, y);
                
                float r = qBound(0.0f, (color.red() - inBlack) / float(inWhite - inBlack), 1.0f);
                float g = qBound(0.0f, (color.green() - inBlack) / float(inWhite - inBlack), 1.0f);
                float b = qBound(0.0f, (color.blue() - inBlack) / float(inWhite - inBlack), 1.0f);
                
                r = qPow(r, 1.0f / gamma);
                g = qPow(g, 1.0f / gamma);
                b = qPow(b, 1.0f / gamma);
                
                int finalR = qBound(0, int(r * 255), 255);
                int finalG = qBound(0, int(g * 255), 255);
                int finalB = qBound(0, int(b * 255), 255);
                
                adjusted.setPixelColor(x, y, QColor(finalR, finalG, finalB, color.alpha()));
            }
        }
        
        selectedImage->setImage(adjusted);
        m_canvas->update();
    };
    
    connect(blackSlider, &QSlider::valueChanged, applyLevels);
    connect(whiteSlider, &QSlider::valueChanged, applyLevels);
    connect(gammaSlider, &QSlider::valueChanged, applyLevels);
    
    layout->addWidget(new QLabel("<b>Input Levels:</b>"));
    layout->addWidget(new QLabel("Black Point:"));
    QHBoxLayout* blackLayout = new QHBoxLayout();
    blackLayout->addWidget(blackSlider);
    blackLayout->addWidget(blackValue);
    layout->addLayout(blackLayout);
    
    layout->addWidget(new QLabel("White Point:"));
    QHBoxLayout* whiteLayout = new QHBoxLayout();
    whiteLayout->addWidget(whiteSlider);
    whiteLayout->addWidget(whiteValue);
    layout->addLayout(whiteLayout);
    
    layout->addWidget(new QLabel("Gamma:"));
    QHBoxLayout* gammaLayout = new QHBoxLayout();
    gammaLayout->addWidget(gammaSlider);
    gammaLayout->addWidget(gammaValue);
    layout->addLayout(gammaLayout);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog.exec() != QDialog::Accepted) {
        selectedImage->setImage(originalImage);
        m_canvas->update();
        m_statusLabel->setText("Levels cancelled");
    } else {
        m_statusLabel->setText("✓ Levels adjusted");
    }
}

void MainWindow::showCurvesAdjustment()
{
    if (!m_canvas) return;
    
    auto selectedObjects = m_canvas->selectedObjects();
    ImagePrimitive* selectedImage = nullptr;
    
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            selectedImage = imgPrim;
            break;
        }
    }
    
    if (!selectedImage) {
        QMessageBox::information(this, "Curves", "Please select an image first");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Curves Adjustment");
    dialog.setMinimumWidth(400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* info = new QLabel("Adjust tone curve (S-curve):");
    layout->addWidget(info);
    
    QSlider* curveSlider = new QSlider(Qt::Horizontal);
    curveSlider->setRange(-100, 100);
    curveSlider->setValue(0);
    QLabel* curveValue = new QLabel("0");
    
    QImage originalImage = selectedImage->image();
    
    // Check if image has a mask
    bool hasMask = selectedImage->getMaskCandidateCount() > 0;
    QImage maskImage;
    if (hasMask) {
        auto contour = selectedImage->getEditableContour();
        bool inverted = selectedImage->isMaskInverted();
        maskImage = createMaskImage(contour, originalImage.size(), inverted);
    }
    
    auto applyCurve = [&]() {
        int curveAmount = curveSlider->value();
        curveValue->setText(QString::number(curveAmount));
        
        QImage adjusted = originalImage.copy();
        for (int y = 0; y < adjusted.height(); ++y) {
            for (int x = 0; x < adjusted.width(); ++x) {
                // Check if pixel is in masked area
                if (hasMask) {
                    int maskValue = qGray(maskImage.pixel(x, y));
                    if (maskValue < 128) continue;
                }
                
                QColor color = adjusted.pixelColor(x, y);
                
                float factor = curveAmount / 100.0f;
                auto adjustChannel = [factor](int value) {
                    float normalized = value / 255.0f;
                    float adjusted = normalized + factor * (normalized - 0.5f) * (1.0f - normalized) * normalized * 4.0f;
                    return qBound(0, int(adjusted * 255), 255);
                };
                
                int r = adjustChannel(color.red());
                int g = adjustChannel(color.green());
                int b = adjustChannel(color.blue());
                
                adjusted.setPixelColor(x, y, QColor(r, g, b, color.alpha()));
            }
        }
        
        selectedImage->setImage(adjusted);
        m_canvas->update();
    };
    
    connect(curveSlider, &QSlider::valueChanged, applyCurve);
    
    QHBoxLayout* curveLayout = new QHBoxLayout();
    curveLayout->addWidget(curveSlider);
    curveLayout->addWidget(curveValue);
    layout->addLayout(curveLayout);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog.exec() != QDialog::Accepted) {
        selectedImage->setImage(originalImage);
        m_canvas->update();
        m_statusLabel->setText("Curves cancelled");
    } else {
        m_statusLabel->setText("✓ Curves adjusted");
    }
}

void MainWindow::showShadowsAdjustment()
{
    if (!m_canvas) return;
    
    auto selectedObjects = m_canvas->selectedObjects();
    ImagePrimitive* selectedImage = nullptr;
    
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            selectedImage = imgPrim;
            break;
        }
    }
    
    if (!selectedImage) {
        QMessageBox::information(this, "Shadows", "Please select an image first");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Shadows Adjustment");
    dialog.setMinimumWidth(400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* label = new QLabel("Shadow Amount:");
    QSlider* slider = new QSlider(Qt::Horizontal);
    slider->setRange(-100, 100);
    slider->setValue(0);
    QLabel* valueLabel = new QLabel("0");
    
    QImage originalImage = selectedImage->image();
    
    // Check if image has a mask
    bool hasMask = selectedImage->getMaskCandidateCount() > 0;
    QImage maskImage;
    if (hasMask) {
        auto contour = selectedImage->getEditableContour();
        bool inverted = selectedImage->isMaskInverted();
        maskImage = createMaskImage(contour, originalImage.size(), inverted);
    }
    
    auto applyShadows = [&]() {
        int amount = slider->value();
        valueLabel->setText(QString::number(amount));
        
        QImage adjusted = originalImage.copy();
        float factor = amount / 100.0f;
        
        for (int y = 0; y < adjusted.height(); ++y) {
            for (int x = 0; x < adjusted.width(); ++x) {
                // Check if pixel is in masked area
                if (hasMask) {
                    int maskValue = qGray(maskImage.pixel(x, y));
                    if (maskValue < 128) continue;
                }
                
                QColor color = adjusted.pixelColor(x, y);
                int luminance = qGray(color.rgb());
                
                if (luminance < 128) {
                    float shadowFactor = (128 - luminance) / 128.0f;
                    int adjustment = factor * shadowFactor * 80;
                    
                    int r = qBound(0, color.red() + adjustment, 255);
                    int g = qBound(0, color.green() + adjustment, 255);
                    int b = qBound(0, color.blue() + adjustment, 255);
                    
                    adjusted.setPixelColor(x, y, QColor(r, g, b, color.alpha()));
                }
            }
        }
        
        selectedImage->setImage(adjusted);
        m_canvas->update();
    };
    
    connect(slider, &QSlider::valueChanged, applyShadows);
    
    layout->addWidget(label);
    QHBoxLayout* sliderLayout = new QHBoxLayout();
    sliderLayout->addWidget(slider);
    sliderLayout->addWidget(valueLabel);
    layout->addLayout(sliderLayout);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog.exec() != QDialog::Accepted) {
        selectedImage->setImage(originalImage);
        m_canvas->update();
        m_statusLabel->setText("Shadows cancelled");
    } else {
        m_statusLabel->setText("✓ Shadows adjusted");
    }
}

void MainWindow::showHighlightsAdjustment()
{
    if (!m_canvas) return;
    
    auto selectedObjects = m_canvas->selectedObjects();
    ImagePrimitive* selectedImage = nullptr;
    
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            selectedImage = imgPrim;
            break;
        }
    }
    
    if (!selectedImage) {
        QMessageBox::information(this, "Highlights", "Please select an image first");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Highlights Adjustment");
    dialog.setMinimumWidth(400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* label = new QLabel("Highlight Amount:");
    QSlider* slider = new QSlider(Qt::Horizontal);
    slider->setRange(-100, 100);
    slider->setValue(0);
    QLabel* valueLabel = new QLabel("0");
    
    QImage originalImage = selectedImage->image();
    
    // Check if image has a mask
    bool hasMask = selectedImage->getMaskCandidateCount() > 0;
    QImage maskImage;
    if (hasMask) {
        auto contour = selectedImage->getEditableContour();
        bool inverted = selectedImage->isMaskInverted();
        maskImage = createMaskImage(contour, originalImage.size(), inverted);
    }
    
    auto applyHighlights = [&]() {
        int amount = slider->value();
        valueLabel->setText(QString::number(amount));
        
        QImage adjusted = originalImage.copy();
        float factor = amount / 100.0f;
        
        for (int y = 0; y < adjusted.height(); ++y) {
            for (int x = 0; x < adjusted.width(); ++x) {
                // Check if pixel is in masked area
                if (hasMask) {
                    int maskValue = qGray(maskImage.pixel(x, y));
                    if (maskValue < 128) continue;
                }
                
                QColor color = adjusted.pixelColor(x, y);
                int luminance = qGray(color.rgb());
                
                if (luminance > 128) {
                    float highlightFactor = (luminance - 128) / 127.0f;
                    int adjustment = factor * highlightFactor * 80;
                    
                    int r = qBound(0, color.red() + adjustment, 255);
                    int g = qBound(0, color.green() + adjustment, 255);
                    int b = qBound(0, color.blue() + adjustment, 255);
                    
                    adjusted.setPixelColor(x, y, QColor(r, g, b, color.alpha()));
                }
            }
        }
        
        selectedImage->setImage(adjusted);
        m_canvas->update();
    };
    
    connect(slider, &QSlider::valueChanged, applyHighlights);
    
    layout->addWidget(label);
    QHBoxLayout* sliderLayout = new QHBoxLayout();
    sliderLayout->addWidget(slider);
    sliderLayout->addWidget(valueLabel);
    layout->addLayout(sliderLayout);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog.exec() != QDialog::Accepted) {
        selectedImage->setImage(originalImage);
        m_canvas->update();
        m_statusLabel->setText("Highlights cancelled");
    } else {
        m_statusLabel->setText("✓ Highlights adjusted");
    }
}

void MainWindow::showBrightnessContrast()
{
    if (!m_canvas) return;
    
    auto selectedObjects = m_canvas->selectedObjects();
    ImagePrimitive* selectedImage = nullptr;
    
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            selectedImage = imgPrim;
            break;
        }
    }
    
    if (!selectedImage) {
        QMessageBox::information(this, "Brightness/Contrast", "Please select an image first");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Brightness/Contrast");
    dialog.setMinimumWidth(400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* brightnessLabel = new QLabel("Brightness:");
    QSlider* brightnessSlider = new QSlider(Qt::Horizontal);
    brightnessSlider->setRange(-100, 100);
    brightnessSlider->setValue(0);
    QLabel* brightnessValue = new QLabel("0");
    
    QLabel* contrastLabel = new QLabel("Contrast:");
    QSlider* contrastSlider = new QSlider(Qt::Horizontal);
    contrastSlider->setRange(-100, 100);
    contrastSlider->setValue(0);
    QLabel* contrastValue = new QLabel("0");
    
    QImage originalImage = selectedImage->image();
    
    // Check if image has a mask
    bool hasMask = selectedImage->getMaskCandidateCount() > 0;
    QImage maskImage;
    if (hasMask) {
        auto contour = selectedImage->getEditableContour();
        bool inverted = selectedImage->isMaskInverted();
        maskImage = createMaskImage(contour, originalImage.size(), inverted);
    }
    
    auto applyAdjustment = [&]() {
        int brightness = brightnessSlider->value();
        int contrast = contrastSlider->value();
        
        brightnessValue->setText(QString::number(brightness));
        contrastValue->setText(QString::number(contrast));
        
        QImage adjusted = originalImage.copy();
        float contrastFactor = (259.0f * (contrast + 255.0f)) / (255.0f * (259.0f - contrast));
        
        for (int y = 0; y < adjusted.height(); ++y) {
            for (int x = 0; x < adjusted.width(); ++x) {
                // Check if pixel is in masked area
                if (hasMask) {
                    int maskValue = qGray(maskImage.pixel(x, y));
                    if (maskValue < 128) continue;
                }
                
                QColor color = adjusted.pixelColor(x, y);
                
                int r = qBound(0, color.red() + brightness, 255);
                int g = qBound(0, color.green() + brightness, 255);
                int b = qBound(0, color.blue() + brightness, 255);
                
                r = qBound(0, int(contrastFactor * (r - 128) + 128), 255);
                g = qBound(0, int(contrastFactor * (g - 128) + 128), 255);
                b = qBound(0, int(contrastFactor * (b - 128) + 128), 255);
                
                adjusted.setPixelColor(x, y, QColor(r, g, b, color.alpha()));
            }
        }
        
        selectedImage->setImage(adjusted);
        m_canvas->update();
    };
    
    connect(brightnessSlider, &QSlider::valueChanged, applyAdjustment);
    connect(contrastSlider, &QSlider::valueChanged, applyAdjustment);
    
    layout->addWidget(brightnessLabel);
    QHBoxLayout* brightnessLayout = new QHBoxLayout();
    brightnessLayout->addWidget(brightnessSlider);
    brightnessLayout->addWidget(brightnessValue);
    layout->addLayout(brightnessLayout);
    
    layout->addWidget(contrastLabel);
    QHBoxLayout* contrastLayout = new QHBoxLayout();
    contrastLayout->addWidget(contrastSlider);
    contrastLayout->addWidget(contrastValue);
    layout->addLayout(contrastLayout);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog.exec() != QDialog::Accepted) {
        selectedImage->setImage(originalImage);
        m_canvas->update();
        m_statusLabel->setText("Brightness/Contrast cancelled");
    } else {
        m_statusLabel->setText("✓ Brightness/Contrast adjusted");
    }
}

void MainWindow::showHueSaturation()
{
    if (!m_canvas) return;
    
    auto selectedObjects = m_canvas->selectedObjects();
    ImagePrimitive* selectedImage = nullptr;
    
    for (auto* obj : selectedObjects) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            selectedImage = imgPrim;
            break;
        }
    }
    
    if (!selectedImage) {
        QMessageBox::information(this, "Hue/Saturation", "Please select an image first");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Hue/Saturation");
    dialog.setMinimumWidth(400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* hueLabel = new QLabel("Hue:");
    QSlider* hueSlider = new QSlider(Qt::Horizontal);
    hueSlider->setRange(-180, 180);
    hueSlider->setValue(0);
    QLabel* hueValue = new QLabel("0°");
    
    QLabel* saturationLabel = new QLabel("Saturation:");
    QSlider* saturationSlider = new QSlider(Qt::Horizontal);
    saturationSlider->setRange(-100, 100);
    saturationSlider->setValue(0);
    QLabel* saturationValue = new QLabel("0");
    
    QImage originalImage = selectedImage->image();
    
    // Check if image has a mask
    bool hasMask = selectedImage->getMaskCandidateCount() > 0;
    QImage maskImage;
    if (hasMask) {
        auto contour = selectedImage->getEditableContour();
        bool inverted = selectedImage->isMaskInverted();
        maskImage = createMaskImage(contour, originalImage.size(), inverted);
    }
    
    auto applyAdjustment = [&]() {
        int hue = hueSlider->value();
        int saturation = saturationSlider->value();
        
        hueValue->setText(QString::number(hue) + "°");
        saturationValue->setText(QString::number(saturation));
        
        QImage adjusted = originalImage.copy();
        
        for (int y = 0; y < adjusted.height(); ++y) {
            for (int x = 0; x < adjusted.width(); ++x) {
                // Check if pixel is in masked area
                if (hasMask) {
                    int maskValue = qGray(maskImage.pixel(x, y));
                    if (maskValue < 128) continue;
                }
                
                QColor color = adjusted.pixelColor(x, y);
                int h, s, v, a;
                color.getHsv(&h, &s, &v, &a);
                
                if (h != -1) {
                    h = (h + hue + 360) % 360;
                }
                
                s = qBound(0, s + saturation * 255 / 100, 255);
                
                adjusted.setPixelColor(x, y, QColor::fromHsv(h, s, v, a));
            }
        }
        
        selectedImage->setImage(adjusted);
        m_canvas->update();
    };
    
    connect(hueSlider, &QSlider::valueChanged, applyAdjustment);
    connect(saturationSlider, &QSlider::valueChanged, applyAdjustment);
    
    layout->addWidget(hueLabel);
    QHBoxLayout* hueLayout = new QHBoxLayout();
    hueLayout->addWidget(hueSlider);
    hueLayout->addWidget(hueValue);
    layout->addLayout(hueLayout);
    
    layout->addWidget(saturationLabel);
    QHBoxLayout* saturationLayout = new QHBoxLayout();
    saturationLayout->addWidget(saturationSlider);
    saturationLayout->addWidget(saturationValue);
    layout->addLayout(saturationLayout);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog.exec() != QDialog::Accepted) {
        selectedImage->setImage(originalImage);
        m_canvas->update();
        m_statusLabel->setText("Hue/Saturation cancelled");
    } else {
        m_statusLabel->setText("✓ Hue/Saturation adjusted");
    }
}

// Blur and auto-enhance functions are in ImageAdjustments_Part2.cpp
