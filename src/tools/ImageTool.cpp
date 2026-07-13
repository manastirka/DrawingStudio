#include "tools/ImageTool.h"

#include "ImagePrimitive.h"

#include <QDebug>
#include <QFileDialog>
#include <QImage>
#include <QImageReader>
#include <QSize>
#include <QString>
#include <QWidget>

void ImageTool::onPress(ToolHost &host, QMouseEvent *event)
{
    // Only react to press — opening a native file dialog synchronously makes
    // the subsequent mouse-up land on the dialog and dismiss it (macOS).
    if (event->type() != QEvent::MouseButtonPress ||
        event->button() != Qt::LeftButton) {
        return;
    }
    if (!host.runDeferred || !host.screenToWorld || !host.commitPrimitive) {
        return;
    }

    const QPoint clickPos = event->pos();
    // Capture callbacks by value for the deferred lambda.
    auto screenToWorld = host.screenToWorld;
    auto snapToGrid = host.snapToGrid;
    auto commit = host.commitPrimitive;
    auto dialogParent = host.dialogParent;
    auto requestUpdate = host.requestUpdate;

    host.runDeferred([screenToWorld, snapToGrid, commit, dialogParent,
                      requestUpdate, clickPos]() {
        QWidget *parent =
            dialogParent ? dialogParent() : static_cast<QWidget *>(nullptr);
        const QString fileName = QFileDialog::getOpenFileName(
            parent, QStringLiteral("Select Image File"), QString(),
            QStringLiteral("Image Files (*.png *.jpg *.jpeg *.bmp *.gif *.tiff "
                           "*.webp);;All Files (*)"));
        if (fileName.isEmpty()) {
            return;
        }

        qDebug() << "ImageTool: loading" << fileName;

        QImageReader reader(fileName);
        reader.setAutoTransform(true);
        reader.setDecideFormatFromContent(true);

        QSize originalSize = reader.size();
        constexpr int kMaxDim = 2048;
        if (originalSize.width() > kMaxDim || originalSize.height() > kMaxDim) {
            const QSize scaled =
                originalSize.scaled(kMaxDim, kMaxDim, Qt::KeepAspectRatio);
            reader.setScaledSize(scaled);
            qDebug() << "ImageTool: scale" << originalSize << "->" << scaled;
        }

        const QImage image = reader.read();
        if (image.isNull()) {
            qDebug() << "ImageTool: failed to load" << fileName;
            return;
        }

        QVector2D worldPos = screenToWorld(clickPos);
        if (snapToGrid) {
            worldPos = snapToGrid(worldPos);
        }

        constexpr float kMaxSize = 200.0f;
        const float aspect = static_cast<float>(image.width()) /
                             static_cast<float>(image.height());
        QVector2D size;
        if (aspect > 1.0f) {
            size = QVector2D(kMaxSize, kMaxSize / aspect);
        } else {
            size = QVector2D(kMaxSize * aspect, kMaxSize);
        }

        auto imagePrimitive =
            std::make_unique<ImagePrimitive>(image, worldPos, size);
        imagePrimitive->setColor(Qt::black);
        commit(std::move(imagePrimitive));
        qDebug() << "ImageTool: placed at" << worldPos << "size" << size;
        if (requestUpdate) {
            requestUpdate();
        }
    });
}

void ImageTool::onMove(ToolHost & /*host*/, QMouseEvent * /*event*/) {}

void ImageTool::onRelease(ToolHost & /*host*/, QMouseEvent * /*event*/) {}
