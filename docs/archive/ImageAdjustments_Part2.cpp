// Blur effects implementation - Part 2

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
#include <QPainter>
#include <QPolygonF>
#include <QTimer>

// Performance optimization: Downsample large images for real-time preview
namespace {
    constexpr int MAX_PREVIEW_DIMENSION = 800;  // Max size for real-time preview
    
    struct PreviewHelper {
        QImage previewImage;
        QImage previewMask;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        bool needsDownsample = false;
        
        void prepare(const QImage& original, const QImage& mask) {
            needsDownsample = original.width() > MAX_PREVIEW_DIMENSION || 
                            original.height() > MAX_PREVIEW_DIMENSION;
            
            if (needsDownsample) {
                // Downsample for faster preview
                previewImage = original.scaled(MAX_PREVIEW_DIMENSION, MAX_PREVIEW_DIMENSION, 
                                              Qt::KeepAspectRatio, Qt::FastTransformation);
                scaleX = (float)previewImage.width() / original.width();
                scaleY = (float)previewImage.height() / original.height();
                
                if (!mask.isNull()) {
                    previewMask = mask.scaled(previewImage.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
                }
            } else {
                previewImage = original;
                previewMask = mask;
                scaleX = scaleY = 1.0f;
            }
        }
        
        QImage upscale(const QImage& processed, const QSize& targetSize) const {
            if (needsDownsample) {
                return processed.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
            return processed;
        }
    };
    
    // Debounce helper for smooth slider interaction
    class DebouncedUpdate : public QObject {
        Q_OBJECT
    public:
        DebouncedUpdate(int delayMs = 50) : m_timer(new QTimer(this)) {
            m_timer->setSingleShot(true);
            m_timer->setInterval(delayMs);
            connect(m_timer, &QTimer::timeout, this, &DebouncedUpdate::triggered);
        }
        
        void trigger() {
            m_timer->start();
        }
        
    signals:
        void triggered();
        
    private:
        QTimer* m_timer;
    };
}

void MainWindow::showGaussianBlur()
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
        QMessageBox::information(this, "Gaussian Blur", "Please select an image first");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Gaussian Blur");
    dialog.setMinimumWidth(400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* label = new QLabel("Blur Radius:");
    QSlider* slider = new QSlider(Qt::Horizontal);
    slider->setRange(1, 25);
    slider->setValue(5);
    QLabel* valueLabel = new QLabel("5");
    
    QImage originalImage = selectedImage->image();
    
    // Check if image has a mask
    bool hasMask = selectedImage->getMaskCandidateCount() > 0;
    QImage maskImage;
    if (hasMask) {
        auto contour = selectedImage->getEditableContour();
        bool inverted = selectedImage->isMaskInverted();
        
        // Create mask
        maskImage = QImage(originalImage.size(), QImage::Format_Grayscale8);
        maskImage.fill(inverted ? Qt::white : Qt::black);
        
        if (!contour.empty()) {
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
        }
    }
    
    // Setup preview optimization
    PreviewHelper preview;
    preview.prepare(originalImage, maskImage);
    
    // Fast separable box blur implementation
    auto fastBoxBlur = [](QImage& img, int radius) {
        if (radius < 1) return;
        
        int width = img.width();
        int height = img.height();
        
        // Horizontal pass
        QImage temp(width, height, img.format());
        for (int y = 0; y < height; ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
            QRgb* tempLine = reinterpret_cast<QRgb*>(temp.scanLine(y));
            
            int r = 0, g = 0, b = 0, a = 0;
            int count = 0;
            
            // Initialize window
            for (int x = 0; x <= qMin(radius, width - 1); ++x) {
                QRgb pixel = line[x];
                r += qRed(pixel);
                g += qGreen(pixel);
                b += qBlue(pixel);
                a += qAlpha(pixel);
                count++;
            }
            
            // Process pixels
            for (int x = 0; x < width; ++x) {
                tempLine[x] = qRgba(r / count, g / count, b / count, a / count);
                
                // Remove left pixel
                if (x - radius >= 0) {
                    QRgb pixel = line[x - radius];
                    r -= qRed(pixel);
                    g -= qGreen(pixel);
                    b -= qBlue(pixel);
                    a -= qAlpha(pixel);
                    count--;
                }
                
                // Add right pixel
                if (x + radius + 1 < width) {
                    QRgb pixel = line[x + radius + 1];
                    r += qRed(pixel);
                    g += qGreen(pixel);
                    b += qBlue(pixel);
                    a += qAlpha(pixel);
                    count++;
                }
            }
        }
        
        // Vertical pass
        for (int x = 0; x < width; ++x) {
            int r = 0, g = 0, b = 0, a = 0;
            int count = 0;
            
            // Initialize window
            for (int y = 0; y <= qMin(radius, height - 1); ++y) {
                QRgb pixel = reinterpret_cast<QRgb*>(temp.scanLine(y))[x];
                r += qRed(pixel);
                g += qGreen(pixel);
                b += qBlue(pixel);
                a += qAlpha(pixel);
                count++;
            }
            
            // Process pixels
            for (int y = 0; y < height; ++y) {
                reinterpret_cast<QRgb*>(img.scanLine(y))[x] = qRgba(r / count, g / count, b / count, a / count);
                
                // Remove top pixel
                if (y - radius >= 0) {
                    QRgb pixel = reinterpret_cast<QRgb*>(temp.scanLine(y - radius))[x];
                    r -= qRed(pixel);
                    g -= qGreen(pixel);
                    b -= qBlue(pixel);
                    a -= qAlpha(pixel);
                    count--;
                }
                
                // Add bottom pixel
                if (y + radius + 1 < height) {
                    QRgb pixel = reinterpret_cast<QRgb*>(temp.scanLine(y + radius + 1))[x];
                    r += qRed(pixel);
                    g += qGreen(pixel);
                    b += qBlue(pixel);
                    a += qAlpha(pixel);
                    count++;
                }
            }
        }
    };
    
    // Debounced update for smooth interaction
    DebouncedUpdate* debouncer = new DebouncedUpdate(30);  // 30ms debounce
    bool isPreview = true;
    
    auto applyBlur = [&](bool usePreview) {
        int radius = slider->value();
        valueLabel->setText(QString::number(radius));
        
        // Use preview image for real-time updates, full res on apply
        QImage sourceImage = usePreview ? preview.previewImage : originalImage;
        QImage sourceMask = usePreview ? preview.previewMask : maskImage;
        int scaledRadius = usePreview ? qMax(1, (int)(radius * preview.scaleX)) : radius;
        
        QImage blurred = sourceImage.copy();
        
        // Apply fast box blur (2 passes for better Gaussian approximation)
        fastBoxBlur(blurred, scaledRadius);
        fastBoxBlur(blurred, scaledRadius);
        
        // Apply mask if present
        if (hasMask && !sourceMask.isNull()) {
            for (int y = 0; y < blurred.height(); ++y) {
                QRgb* blurredLine = reinterpret_cast<QRgb*>(blurred.scanLine(y));
                const QRgb* originalLine = reinterpret_cast<const QRgb*>(sourceImage.scanLine(y));
                const uchar* maskLine = sourceMask.scanLine(y);
                
                for (int x = 0; x < blurred.width(); ++x) {
                    if (maskLine[x] < 128) {
                        blurredLine[x] = originalLine[x];
                    }
                }
            }
        }
        
        // Upscale if needed
        if (usePreview && preview.needsDownsample) {
            blurred = preview.upscale(blurred, originalImage.size());
        }
        
        selectedImage->setImage(blurred);
        m_canvas->update();
    };
    
    // Connect slider with debouncing for smooth preview
    connect(slider, &QSlider::valueChanged, [debouncer]() {
        debouncer->trigger();
    });
    
    connect(debouncer, &DebouncedUpdate::triggered, [&]() {
        applyBlur(true);  // Preview mode
    });
    
    layout->addWidget(label);
    QHBoxLayout* sliderLayout = new QHBoxLayout();
    sliderLayout->addWidget(slider);
    sliderLayout->addWidget(valueLabel);
    layout->addLayout(sliderLayout);
    
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    // Initial preview
    applyBlur(true);
    
    if (dialog.exec() != QDialog::Accepted) {
        selectedImage->setImage(originalImage);
        m_canvas->update();
        m_statusLabel->setText("Gaussian blur cancelled");
    } else {
        // Apply at full resolution on accept
        applyBlur(false);
        m_statusLabel->setText("✓ Gaussian blur applied");
    }
    
    delete debouncer;
}

// Macro to setup common preview infrastructure
#define SETUP_BLUR_PREVIEW() \
    PreviewHelper preview; \
    preview.prepare(originalImage, maskImage); \
    DebouncedUpdate* debouncer = new DebouncedUpdate(30);

#define CLEANUP_BLUR_PREVIEW() \
    delete debouncer;

#include "ImageAdjustments_Part2.moc"

void MainWindow::showMotionBlur()
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
        QMessageBox::information(this, "Motion Blur", "Please select an image first");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Motion Blur");
    dialog.setMinimumWidth(450);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // Distance slider
    QLabel* distLabel = new QLabel("Distance:");
    QSlider* distSlider = new QSlider(Qt::Horizontal);
    distSlider->setRange(1, 50);
    distSlider->setValue(15);
    QLabel* distValue = new QLabel("15");
    
    // Angle slider
    QLabel* angleLabel = new QLabel("Angle:");
    QSlider* angleSlider = new QSlider(Qt::Horizontal);
    angleSlider->setRange(0, 359);
    angleSlider->setValue(0);
    QLabel* angleValue = new QLabel("0°");
    
    QImage originalImage = selectedImage->image();
    
    // Check if image has a mask
    bool hasMask = selectedImage->getMaskCandidateCount() > 0;
    QImage maskImage;
    if (hasMask) {
        auto contour = selectedImage->getEditableContour();
        bool inverted = selectedImage->isMaskInverted();
        
        // Create mask
        maskImage = QImage(originalImage.size(), QImage::Format_Grayscale8);
        maskImage.fill(inverted ? Qt::white : Qt::black);
        
        if (!contour.empty()) {
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
        }
    }
    
    SETUP_BLUR_PREVIEW();
    
    auto applyMotionBlur = [&](bool usePreview) {
        int distance = distSlider->value();
        int angle = angleSlider->value();
        distValue->setText(QString::number(distance));
        angleValue->setText(QString("%1°").arg(angle));
        
        QImage sourceImage = usePreview ? preview.previewImage : originalImage;
        QImage sourceMask = usePreview ? preview.previewMask : maskImage;
        int scaledDist = usePreview ? qMax(1, (int)(distance * preview.scaleX)) : distance;
        
        QImage blurred = sourceImage.copy();
        
        // Convert angle to radians
        double rad = angle * M_PI / 180.0;
        double dx = cos(rad);
        double dy = sin(rad);
        
        // Optimized motion blur with scanLine access
        for (int y = 0; y < blurred.height(); ++y) {
            QRgb* blurredLine = reinterpret_cast<QRgb*>(blurred.scanLine(y));
            
            for (int x = 0; x < blurred.width(); ++x) {
                int r = 0, g = 0, b = 0, a = 0;
                int count = 0;
                
                // Sample along motion direction
                for (int i = 0; i < scaledDist; ++i) {
                    int sx = x + (int)(dx * i);
                    int sy = y + (int)(dy * i);
                    
                    if (sx >= 0 && sx < blurred.width() && sy >= 0 && sy < blurred.height()) {
                        QRgb pixel = reinterpret_cast<QRgb*>(sourceImage.scanLine(sy))[sx];
                        r += qRed(pixel);
                        g += qGreen(pixel);
                        b += qBlue(pixel);
                        a += qAlpha(pixel);
                        count++;
                    }
                }
                
                if (count > 0) {
                    blurredLine[x] = qRgba(r/count, g/count, b/count, a/count);
                }
            }
        }
        
        // Apply mask if present
        if (hasMask && !sourceMask.isNull()) {
            for (int y = 0; y < blurred.height(); ++y) {
                QRgb* blurredLine = reinterpret_cast<QRgb*>(blurred.scanLine(y));
                const QRgb* originalLine = reinterpret_cast<const QRgb*>(sourceImage.scanLine(y));
                const uchar* maskLine = sourceMask.scanLine(y);
                
                for (int x = 0; x < blurred.width(); ++x) {
                    if (maskLine[x] < 128) {
                        blurredLine[x] = originalLine[x];
                    }
                }
            }
        }
        
        if (usePreview && preview.needsDownsample) {
            blurred = preview.upscale(blurred, originalImage.size());
        }
        
        selectedImage->setImage(blurred);
        m_canvas->update();
    };
    
    connect(distSlider, &QSlider::valueChanged, [debouncer]() { debouncer->trigger(); });
    connect(angleSlider, &QSlider::valueChanged, [debouncer]() { debouncer->trigger(); });
    connect(debouncer, &DebouncedUpdate::triggered, [&]() { applyMotionBlur(true); });
    
    layout->addWidget(distLabel);
    QHBoxLayout* distLayout = new QHBoxLayout();
    distLayout->addWidget(distSlider);
    distLayout->addWidget(distValue);
    layout->addLayout(distLayout);
    
    layout->addWidget(angleLabel);
    QHBoxLayout* angleLayout = new QHBoxLayout();
    angleLayout->addWidget(angleSlider);
    angleLayout->addWidget(angleValue);
    layout->addLayout(angleLayout);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    applyMotionBlur(true);
    
    if (dialog.exec() != QDialog::Accepted) {
        selectedImage->setImage(originalImage);
        m_canvas->update();
        m_statusLabel->setText("Motion blur cancelled");
    } else {
        applyMotionBlur(false);  // Full resolution on accept
        m_statusLabel->setText("✓ Motion blur applied");
    }
    
    CLEANUP_BLUR_PREVIEW();
}

void MainWindow::showRadialBlur()
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
        QMessageBox::information(this, "Radial Blur", "Please select an image first");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Radial Blur");
    dialog.setMinimumWidth(400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* label = new QLabel("Blur Amount:");
    QSlider* slider = new QSlider(Qt::Horizontal);
    slider->setRange(1, 50);
    slider->setValue(10);
    QLabel* valueLabel = new QLabel("10");
    
    QImage originalImage = selectedImage->image();
    
    // Check if image has a mask
    bool hasMask = selectedImage->getMaskCandidateCount() > 0;
    QImage maskImage;
    if (hasMask) {
        auto contour = selectedImage->getEditableContour();
        bool inverted = selectedImage->isMaskInverted();
        
        // Create mask
        maskImage = QImage(originalImage.size(), QImage::Format_Grayscale8);
        maskImage.fill(inverted ? Qt::white : Qt::black);
        
        if (!contour.empty()) {
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
        }
    }
    
    SETUP_BLUR_PREVIEW();
    
    auto applyRadialBlur = [&](bool usePreview) {
        int amount = slider->value();
        valueLabel->setText(QString::number(amount));
        
        QImage sourceImage = usePreview ? preview.previewImage : originalImage;
        QImage sourceMask = usePreview ? preview.previewMask : maskImage;
        int scaledAmount = usePreview ? qMax(1, (int)(amount * preview.scaleX)) : amount;
        
        QImage blurred = sourceImage.copy();
        
        int cx = blurred.width() / 2;
        int cy = blurred.height() / 2;
        
        // Optimized radial blur with scanLine access
        for (int y = 0; y < blurred.height(); ++y) {
            QRgb* blurredLine = reinterpret_cast<QRgb*>(blurred.scanLine(y));
            
            for (int x = 0; x < blurred.width(); ++x) {
                int r = 0, g = 0, b = 0, a = 0;
                int count = 0;
                
                // Sample along line from center
                for (int i = 0; i < scaledAmount; ++i) {
                    float t = 1.0f - (float)i / scaledAmount;
                    int sx = cx + (int)((x - cx) * t);
                    int sy = cy + (int)((y - cy) * t);
                    
                    if (sx >= 0 && sx < blurred.width() && sy >= 0 && sy < blurred.height()) {
                        QRgb pixel = reinterpret_cast<QRgb*>(sourceImage.scanLine(sy))[sx];
                        r += qRed(pixel);
                        g += qGreen(pixel);
                        b += qBlue(pixel);
                        a += qAlpha(pixel);
                        count++;
                    }
                }
                
                if (count > 0) {
                    blurredLine[x] = qRgba(r/count, g/count, b/count, a/count);
                }
            }
        }
        
        // Apply mask if present
        if (hasMask && !sourceMask.isNull()) {
            for (int y = 0; y < blurred.height(); ++y) {
                QRgb* blurredLine = reinterpret_cast<QRgb*>(blurred.scanLine(y));
                const QRgb* originalLine = reinterpret_cast<const QRgb*>(sourceImage.scanLine(y));
                const uchar* maskLine = sourceMask.scanLine(y);
                
                for (int x = 0; x < blurred.width(); ++x) {
                    if (maskLine[x] < 128) {
                        blurredLine[x] = originalLine[x];
                    }
                }
            }
        }
        
        if (usePreview && preview.needsDownsample) {
            blurred = preview.upscale(blurred, originalImage.size());
        }
        
        selectedImage->setImage(blurred);
        m_canvas->update();
    };
    
    connect(slider, &QSlider::valueChanged, [debouncer]() { debouncer->trigger(); });
    connect(debouncer, &DebouncedUpdate::triggered, [&]() { applyRadialBlur(true); });
    
    layout->addWidget(label);
    QHBoxLayout* sliderLayout = new QHBoxLayout();
    sliderLayout->addWidget(slider);
    sliderLayout->addWidget(valueLabel);
    layout->addLayout(sliderLayout);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    applyRadialBlur(true);
    
    if (dialog.exec() != QDialog::Accepted) {
        selectedImage->setImage(originalImage);
        m_canvas->update();
        m_statusLabel->setText("Radial blur cancelled");
    } else {
        applyRadialBlur(false);  // Full resolution on accept
        m_statusLabel->setText("✓ Radial blur applied");
    }
    
    CLEANUP_BLUR_PREVIEW();
}

void MainWindow::showBokehBlur()
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
        QMessageBox::information(this, "Bokeh Blur", "Please select an image first");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Bokeh Blur");
    dialog.setMinimumWidth(400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* label = new QLabel("Bokeh Size:");
    QSlider* slider = new QSlider(Qt::Horizontal);
    slider->setRange(3, 25);
    slider->setValue(9);
    QLabel* valueLabel = new QLabel("9");
    
    QImage originalImage = selectedImage->image();
    
    // Check if image has a mask
    bool hasMask = selectedImage->getMaskCandidateCount() > 0;
    QImage maskImage;
    if (hasMask) {
        auto contour = selectedImage->getEditableContour();
        bool inverted = selectedImage->isMaskInverted();
        
        // Create mask
        maskImage = QImage(originalImage.size(), QImage::Format_Grayscale8);
        maskImage.fill(inverted ? Qt::white : Qt::black);
        
        if (!contour.empty()) {
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
        }
    }
    
    SETUP_BLUR_PREVIEW();
    
    auto applyBokehBlur = [&](bool usePreview) {
        int radius = slider->value();
        valueLabel->setText(QString::number(radius));
        
        QImage sourceImage = usePreview ? preview.previewImage : originalImage;
        QImage sourceMask = usePreview ? preview.previewMask : maskImage;
        int scaledRadius = usePreview ? qMax(1, (int)(radius * preview.scaleX)) : radius;
        
        QImage blurred = sourceImage.copy();
        
        // Create circular kernel (bokeh shape) - precompute once
        int kernelSize = scaledRadius * 2 + 1;
        std::vector<std::vector<bool>> kernel(kernelSize, std::vector<bool>(kernelSize, false));
        
        for (int ky = 0; ky < kernelSize; ++ky) {
            for (int kx = 0; kx < kernelSize; ++kx) {
                int dx = kx - scaledRadius;
                int dy = ky - scaledRadius;
                if (dx*dx + dy*dy <= scaledRadius*scaledRadius) {
                    kernel[ky][kx] = true;
                }
            }
        }
        
        // Optimized circular blur with scanLine access
        for (int y = 0; y < blurred.height(); ++y) {
            QRgb* blurredLine = reinterpret_cast<QRgb*>(blurred.scanLine(y));
            
            for (int x = 0; x < blurred.width(); ++x) {
                int r = 0, g = 0, b = 0, a = 0;
                int count = 0;
                
                for (int ky = 0; ky < kernelSize; ++ky) {
                    int sy = y + ky - scaledRadius;
                    if (sy < 0 || sy >= blurred.height()) continue;
                    
                    const QRgb* srcLine = reinterpret_cast<const QRgb*>(sourceImage.scanLine(sy));
                    
                    for (int kx = 0; kx < kernelSize; ++kx) {
                        if (!kernel[ky][kx]) continue;
                        
                        int sx = x + kx - scaledRadius;
                        if (sx < 0 || sx >= blurred.width()) continue;
                        
                        QRgb pixel = srcLine[sx];
                        r += qRed(pixel);
                        g += qGreen(pixel);
                        b += qBlue(pixel);
                        a += qAlpha(pixel);
                        count++;
                    }
                }
                
                if (count > 0) {
                    blurredLine[x] = qRgba(r/count, g/count, b/count, a/count);
                }
            }
        }
        
        // Apply mask if present
        if (hasMask && !sourceMask.isNull()) {
            for (int y = 0; y < blurred.height(); ++y) {
                QRgb* blurredLine = reinterpret_cast<QRgb*>(blurred.scanLine(y));
                const QRgb* originalLine = reinterpret_cast<const QRgb*>(sourceImage.scanLine(y));
                const uchar* maskLine = sourceMask.scanLine(y);
                
                for (int x = 0; x < blurred.width(); ++x) {
                    if (maskLine[x] < 128) {
                        blurredLine[x] = originalLine[x];
                    }
                }
            }
        }
        
        if (usePreview && preview.needsDownsample) {
            blurred = preview.upscale(blurred, originalImage.size());
        }
        
        selectedImage->setImage(blurred);
        m_canvas->update();
    };
    
    connect(slider, &QSlider::valueChanged, [debouncer]() { debouncer->trigger(); });
    connect(debouncer, &DebouncedUpdate::triggered, [&]() { applyBokehBlur(true); });
    
    layout->addWidget(label);
    QHBoxLayout* sliderLayout = new QHBoxLayout();
    sliderLayout->addWidget(slider);
    sliderLayout->addWidget(valueLabel);
    layout->addLayout(sliderLayout);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    applyBokehBlur(true);
    
    if (dialog.exec() != QDialog::Accepted) {
        selectedImage->setImage(originalImage);
        m_canvas->update();
        m_statusLabel->setText("Bokeh blur cancelled");
    } else {
        applyBokehBlur(false);  // Full resolution on accept
        m_statusLabel->setText("✓ Bokeh blur applied");
    }
    
    CLEANUP_BLUR_PREVIEW();
}

void MainWindow::showSurfaceBlur()
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
        QMessageBox::information(this, "Surface Blur", "Please select an image first");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("Surface Blur");
    dialog.setMinimumWidth(450);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // Radius slider
    QLabel* radiusLabel = new QLabel("Radius:");
    QSlider* radiusSlider = new QSlider(Qt::Horizontal);
    radiusSlider->setRange(1, 15);
    radiusSlider->setValue(5);
    QLabel* radiusValue = new QLabel("5");
    
    // Threshold slider
    QLabel* thresholdLabel = new QLabel("Threshold:");
    QSlider* thresholdSlider = new QSlider(Qt::Horizontal);
    thresholdSlider->setRange(1, 100);
    thresholdSlider->setValue(20);
    QLabel* thresholdValue = new QLabel("20");
    
    QImage originalImage = selectedImage->image();
    
    // Check if image has a mask
    bool hasMask = selectedImage->getMaskCandidateCount() > 0;
    QImage maskImage;
    if (hasMask) {
        auto contour = selectedImage->getEditableContour();
        bool inverted = selectedImage->isMaskInverted();
        
        // Create mask
        maskImage = QImage(originalImage.size(), QImage::Format_Grayscale8);
        maskImage.fill(inverted ? Qt::white : Qt::black);
        
        if (!contour.empty()) {
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
        }
    }
    
    SETUP_BLUR_PREVIEW();
    
    auto applySurfaceBlur = [&](bool usePreview) {
        int radius = radiusSlider->value();
        int threshold = thresholdSlider->value();
        radiusValue->setText(QString::number(radius));
        thresholdValue->setText(QString::number(threshold));
        
        QImage sourceImage = usePreview ? preview.previewImage : originalImage;
        QImage sourceMask = usePreview ? preview.previewMask : maskImage;
        int scaledRadius = usePreview ? qMax(1, (int)(radius * preview.scaleX)) : radius;
        
        QImage blurred = sourceImage.copy();
        
        // Optimized surface blur with scanLine access
        for (int y = 0; y < blurred.height(); ++y) {
            QRgb* blurredLine = reinterpret_cast<QRgb*>(blurred.scanLine(y));
            const QRgb* centerLine = reinterpret_cast<const QRgb*>(sourceImage.scanLine(y));
            
            for (int x = 0; x < blurred.width(); ++x) {
                QRgb centerPixel = centerLine[x];
                int centerR = qRed(centerPixel);
                int centerG = qGreen(centerPixel);
                int centerB = qBlue(centerPixel);
                
                int r = 0, g = 0, b = 0, a = 0;
                int count = 0;
                
                // Sample neighborhood
                for (int dy = -scaledRadius; dy <= scaledRadius; ++dy) {
                    int sy = y + dy;
                    if (sy < 0 || sy >= blurred.height()) continue;
                    
                    const QRgb* srcLine = reinterpret_cast<const QRgb*>(sourceImage.scanLine(sy));
                    
                    for (int dx = -scaledRadius; dx <= scaledRadius; ++dx) {
                        int sx = x + dx;
                        if (sx < 0 || sx >= blurred.width()) continue;
                        
                        QRgb pixel = srcLine[sx];
                        int pr = qRed(pixel);
                        int pg = qGreen(pixel);
                        int pb = qBlue(pixel);
                        
                        // Calculate color difference
                        int diff = abs(pr - centerR) + abs(pg - centerG) + abs(pb - centerB);
                        
                        // Only include similar colors (edge-preserving)
                        if (diff < threshold) {
                            r += pr;
                            g += pg;
                            b += pb;
                            a += qAlpha(pixel);
                            count++;
                        }
                    }
                }
                
                if (count > 0) {
                    blurredLine[x] = qRgba(r/count, g/count, b/count, a/count);
                } else {
                    blurredLine[x] = centerPixel;
                }
            }
        }
        
        // Apply mask if present
        if (hasMask && !sourceMask.isNull()) {
            for (int y = 0; y < blurred.height(); ++y) {
                QRgb* blurredLine = reinterpret_cast<QRgb*>(blurred.scanLine(y));
                const QRgb* originalLine = reinterpret_cast<const QRgb*>(sourceImage.scanLine(y));
                const uchar* maskLine = sourceMask.scanLine(y);
                
                for (int x = 0; x < blurred.width(); ++x) {
                    if (maskLine[x] < 128) {
                        blurredLine[x] = originalLine[x];
                    }
                }
            }
        }
        
        if (usePreview && preview.needsDownsample) {
            blurred = preview.upscale(blurred, originalImage.size());
        }
        
        selectedImage->setImage(blurred);
        m_canvas->update();
    };
    
    connect(radiusSlider, &QSlider::valueChanged, [debouncer]() { debouncer->trigger(); });
    connect(thresholdSlider, &QSlider::valueChanged, [debouncer]() { debouncer->trigger(); });
    connect(debouncer, &DebouncedUpdate::triggered, [&]() { applySurfaceBlur(true); });
    
    layout->addWidget(radiusLabel);
    QHBoxLayout* radiusLayout = new QHBoxLayout();
    radiusLayout->addWidget(radiusSlider);
    radiusLayout->addWidget(radiusValue);
    layout->addLayout(radiusLayout);
    
    layout->addWidget(thresholdLabel);
    QHBoxLayout* thresholdLayout = new QHBoxLayout();
    thresholdLayout->addWidget(thresholdSlider);
    thresholdLayout->addWidget(thresholdValue);
    layout->addLayout(thresholdLayout);
    
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);
    
    applySurfaceBlur(true);
    
    if (dialog.exec() != QDialog::Accepted) {
        selectedImage->setImage(originalImage);
        m_canvas->update();
        m_statusLabel->setText("Surface blur cancelled");
    } else {
        applySurfaceBlur(false);  // Full resolution on accept
        m_statusLabel->setText("✓ Surface blur applied");
    }
    
    CLEANUP_BLUR_PREVIEW();
}

void MainWindow::autoEnhanceImage()
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
        QMessageBox::information(this, "Auto Enhance", "Please select an image first");
        return;
    }
    
    QImage original = selectedImage->image();
    QImage enhanced = original.copy();
    
    // Auto levels
    int minR = 255, minG = 255, minB = 255;
    int maxR = 0, maxG = 0, maxB = 0;
    
    for (int y = 0; y < enhanced.height(); ++y) {
        for (int x = 0; x < enhanced.width(); ++x) {
            QColor c = enhanced.pixelColor(x, y);
            minR = qMin(minR, c.red());
            minG = qMin(minG, c.green());
            minB = qMin(minB, c.blue());
            maxR = qMax(maxR, c.red());
            maxG = qMax(maxG, c.green());
            maxB = qMax(maxB, c.blue());
        }
    }
    
    // Apply auto levels
    for (int y = 0; y < enhanced.height(); ++y) {
        for (int x = 0; x < enhanced.width(); ++x) {
            QColor c = enhanced.pixelColor(x, y);
            
            int r = (maxR > minR) ? ((c.red() - minR) * 255 / (maxR - minR)) : c.red();
            int g = (maxG > minG) ? ((c.green() - minG) * 255 / (maxG - minG)) : c.green();
            int b = (maxB > minB) ? ((c.blue() - minB) * 255 / (maxB - minB)) : c.blue();
            
            enhanced.setPixelColor(x, y, QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255), c.alpha()));
        }
    }
    
    selectedImage->setImage(enhanced);
    m_canvas->update();
    m_statusLabel->setText("✓ Image auto-enhanced");
}
