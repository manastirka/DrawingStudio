#include "MainWindow.h"
#include "DrawingCanvas.h"
#include "DrawingTool.h"
#include "ImagePrimitive.h"
#include "LayerPanel.h"
#include "CommandManager.h"
#include <QLabel>
#include <QStatusBar>
#include <QTimer>
#include <QVector2D>

// Canvas / layer panel signal wiring (refactor E14).

void MainWindow::connectSignals()
{
// Connect canvas signals
    if (m_canvas) {
        connect(m_canvas, &DrawingCanvas::commandRequested,
                this, &MainWindow::executeCommand);

        connect(m_canvas, &DrawingCanvas::maskDetectionComplete, this,
                [this](ImagePrimitive *imgPrim) {
                    if (m_canvas && imgPrim) {
                        selectImageForMaskUI(imgPrim);
                    }
                    updateMaskSelectionUI();
                    updateFloatingMaskPanel();
                    showFloatingMaskPanel();
                    if (m_statusLabel && imgPrim &&
                        imgPrim->getMaskCandidateCount() > 0) {
                        m_statusLabel->setText(
                            QString("Detected %1 masks")
                                .arg(imgPrim->getMaskCandidateCount()));
                    }
                    if (m_suggestionRestoreTimer) {
                        m_suggestionRestoreTimer->start(1800);
                    } else {
                        refreshSmartSuggestions();
                    }
                });

        connect(m_canvas, &DrawingCanvas::maskDetectionFailed, this,
                [this](const QString &error) {
                    if (m_statusLabel) {
                        m_statusLabel->setText("Mask detection failed: " + error);
                    }
                });

        connect(m_canvas, &DrawingCanvas::maskDetectionProgress, this,
                [this](int progress, const QString &message) {
                    if (m_statusLabel) {
                        m_statusLabel->setText(
                            QString("SAM2: %1 %2%").arg(message).arg(progress));
                    }
                });

        connect(m_canvas, &DrawingCanvas::maskNavigationRequested, this,
                [this](int direction) {
                    if (direction < 0) {
                        selectPreviousMask();
                    } else {
                        selectNextMask();
                    }
                });

        connect(m_canvas, &DrawingCanvas::maskInvertRequested, this,
                &MainWindow::invertSelectedMask);

        connect(m_canvas, &DrawingCanvas::smartHintChanged, this,
                [this](const QString &hint) {
                    m_liveSmartHint = hint;
                    refreshSmartSuggestions();
                });

        connect(m_canvas, &DrawingCanvas::advancedTextEditorRequested, this,
                &MainWindow::showAdvancedTextEditor);
        connect(m_canvas, &DrawingCanvas::toolChangeRequested, this,
                &MainWindow::activateDrawingTool);

        connect(m_canvas, &DrawingCanvas::coordinatesChanged,
                this, [this](const QVector2D &coords) {
                    if (m_canvas) {
                        float x = m_canvas->worldToUnits(coords.x());
                        float y = m_canvas->worldToUnits(coords.y());
                        QString units = m_canvas->getUnitsString();
                        m_coordsLabel->setText(QString("X: %1%3, Y: %2%3")
                                              .arg(x, 0, 'f', 1)
                                              .arg(y, 0, 'f', 1)
                                              .arg(units));
                    }
                });
        
        connect(m_canvas, &DrawingCanvas::zoomChanged,
                this, [this](float zoom) {
                    m_zoomLabel->setText(QString("Zoom: %1%")
                                        .arg(zoom * 100, 0, 'f', 0));
                });
        
        connect(m_canvas, &DrawingCanvas::selectionChanged,
                this, [this]() {
                    updatePropertyPanel();
                    refreshSmartSuggestions();

                    const int selCount =
                        m_canvas ? static_cast<int>(m_canvas->selectedObjects().size())
                                 : 0;
                    if (selCount > 1) {
                        statusBar()->showMessage(
                            QStringLiteral("%1 objects selected — Shift/⌘-click to add/remove")
                                .arg(selCount),
                            4000);
                    }

                    bool hasSelection = selCount > 0;

                    // Check for ImagePrimitive with detected masks
                    bool showMask = false;
                    if (hasSelection) {
                        for (auto *obj : m_canvas->selectedObjects()) {
                            if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
                                if (imgPrim->getMaskCandidateCount() > 0) {
                                    showMask = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (showMask) {
                        updateFloatingMaskPanel();
                        showFloatingMaskPanel();
                    } else {
                        hideFloatingMaskPanel();
                    }

                });
    }
    
    // Connect layer panel signals
    if (m_layerPanel) {
        connect(m_layerPanel, &LayerPanel::commandRequested,
                this, &MainWindow::executeCommand);
        connect(m_layerPanel, &LayerPanel::layerPropertyChanged,
                this, [this]() {
                    if (m_canvas) {
                        m_canvas->update();
                    }
                });
        connect(m_layerPanel, &LayerPanel::documentModified,
                this, [this]() {
                    if (m_commandManager)
                        m_commandManager->invalidateClean();
                    m_isModified = true;
                    updateWindowTitle();
                });
    }
    
}
