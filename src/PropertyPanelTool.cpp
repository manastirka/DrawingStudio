#include "PropertyPanel.h"
#include "DrawingPrimitive.h"
#include "DrawingCanvas.h"
#include "ImagePrimitive.h"
#include "SimpleTextPanel.h"
#include "ClassicTextTool.h"
#include "BrushStrokePrimitive.h"
#include "DrawingTool.h"

#include <QColorDialog>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QCheckBox>
#include <QGroupBox>
#include <QDebug>
#include <QFontDatabase>
#include <QLabel>
#include <QSlider>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QMap>
#include <functional>
#include <cmath>
#include <algorithm>
#include <vector>

// Tool-mode property UI (refactor E17).

void PropertyPanel::showToolProperties(DrawingTool tool, DrawingCanvas* canvas)
{
    m_currentPrimitive = nullptr;
    m_currentPrimitives.clear();
    m_currentCanvas = canvas;

    clearLayout();
    m_propertyEditors.clear();

    // Title
    QLabel* title = new QLabel("Tool");
    title->setStyleSheet("font-size: 12px; font-weight: 700; margin: 3px 0 8px 0; color: rgba(231,234,240,0.9);");
    m_contentLayout->addWidget(title);

    auto addBool = [&](const QString& key, const QString& label, bool value,
                       std::function<void(bool)> apply) {
        QCheckBox* cb = new QCheckBox(label);
        cb->setChecked(value);
        connect(cb, &QCheckBox::toggled, this, [this, apply](bool v){
            if (apply) apply(v);
            if (m_currentCanvas) m_currentCanvas->update();
        });
        m_contentLayout->addWidget(cb);
        m_propertyEditors[key] = cb;
    };

    auto addIntSlider = [&](const QString& key, const QString& label, int minV, int maxV, int value,
                            std::function<void(int)> apply) {
        QWidget* row = new QWidget();
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(0,0,0,0);
        QLabel* l = new QLabel(label);
        QSlider* s = new QSlider(Qt::Horizontal);
        s->setRange(minV, maxV);
        s->setValue(value);
        QLabel* v = new QLabel(QString::number(value));
        v->setMinimumWidth(32);
        h->addWidget(l);
        h->addWidget(s, 1);
        h->addWidget(v);
        connect(s, &QSlider::valueChanged, this, [this, v, apply](int nv){
            v->setText(QString::number(nv));
            if (apply) apply(nv);
            if (m_currentCanvas) m_currentCanvas->update();
        });
        m_contentLayout->addWidget(row);
        m_propertyEditors[key] = s;
    };

    auto addFloatSpin = [&](const QString& key, const QString& label, double minV, double maxV,
                            double step, double value,
                            std::function<void(double)> apply) {
        QWidget* row = new QWidget();
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(0,0,0,0);
        QLabel* l = new QLabel(label);
        QDoubleSpinBox* sp = new QDoubleSpinBox();
        sp->setRange(minV, maxV);
        sp->setSingleStep(step);
        sp->setDecimals(1);
        sp->setValue(value);
        h->addWidget(l);
        h->addWidget(sp, 1);
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, apply](double nv){
                    if (apply) apply(nv);
                    if (m_currentCanvas) m_currentCanvas->update();
                });
        m_contentLayout->addWidget(row);
        m_propertyEditors[key] = sp;
    };

    auto addLineStyleCombo = [&](const QString& key, const QString& label, Qt::PenStyle current,
                                 std::function<void(Qt::PenStyle)> apply) {
        QWidget* row = new QWidget();
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(0,0,0,0);
        QLabel* l = new QLabel(label);
        QComboBox* combo = new QComboBox();
        struct Item { const char* name; Qt::PenStyle style; };
        const Item items[] = {
            {"Solid", Qt::SolidLine},
            {"Dash", Qt::DashLine},
            {"Dot", Qt::DotLine},
            {"Dash Dot", Qt::DashDotLine},
            {"Dash Dot Dot", Qt::DashDotDotLine}
        };
        int idx = 0;
        int currentIdx = 0;
        for (const auto& it : items) {
            combo->addItem(it.name, static_cast<int>(it.style));
            if (it.style == current) currentIdx = idx;
            ++idx;
        }
        combo->setCurrentIndex(currentIdx);
        h->addWidget(l);
        h->addWidget(combo, 1);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, combo, apply](int){
                    Qt::PenStyle st = static_cast<Qt::PenStyle>(combo->currentData().toInt());
                    if (apply) apply(st);
                    if (m_currentCanvas) m_currentCanvas->update();
                });
        m_contentLayout->addWidget(row);
        m_propertyEditors[key] = combo;
    };

    auto addColorButton = [&](const QString& key, const QString& label, const QColor& color,
                              std::function<void(const QColor&)> apply) {
        QWidget* row = new QWidget();
        QHBoxLayout* h = new QHBoxLayout(row);
        h->setContentsMargins(0,0,0,0);
        QLabel* l = new QLabel(label);
        ColorButton* btn = new ColorButton(color);
        btn->setMinimumWidth(44);
        h->addWidget(l);
        h->addWidget(btn, 1);
        connect(btn, &ColorButton::colorChanged, this, [this, apply](const QColor& c){
            if (apply) apply(c);
            if (m_currentCanvas) m_currentCanvas->update();
        });
        m_contentLayout->addWidget(row);
        m_propertyEditors[key] = btn;
    };

    const bool isStrokeTool = (tool == DrawingTool::Line || tool == DrawingTool::Rectangle ||
                               tool == DrawingTool::Ellipse || tool == DrawingTool::Circle ||
                               tool == DrawingTool::Polygon || tool == DrawingTool::Arc ||
                               tool == DrawingTool::Curve || tool == DrawingTool::BezierCurve ||
                               tool == DrawingTool::Spline);

    // Only show Fill controls for tools that actually apply fill on finalize.
    // (Arc + Line don't support fill in the current backend.)
    const bool toolSupportsFill = (tool == DrawingTool::Rectangle || tool == DrawingTool::Ellipse ||
                                   tool == DrawingTool::Circle || tool == DrawingTool::Polygon ||
                                   tool == DrawingTool::Spline);

    if (isStrokeTool && canvas) {
        addGroup("Stroke");
        addColorButton("tool.strokeColor", "Color", canvas->defaultDrawingColor(),
                       [canvas](const QColor& c){ canvas->setDefaultDrawingColor(c); });
        addFloatSpin("tool.strokeWidth", "Width", 0.5, 40.0, 0.5, canvas->defaultLineWidth(),
                     [canvas](double w){ canvas->setDefaultLineWidth(static_cast<float>(w)); });
        addLineStyleCombo("tool.strokeStyle", "Style", canvas->defaultLineStyle(),
                          [canvas](Qt::PenStyle st){ canvas->setDefaultLineStyle(st); });

        if (toolSupportsFill) {
            addGroup("Fill");
            addBool("tool.fillEnabled", "Enabled", canvas->defaultFillEnabled(),
                    [canvas](bool on){ canvas->setDefaultFillEnabled(on); });
            addColorButton("tool.fillColor", "Color", canvas->defaultFillColor(),
                           [canvas](const QColor& c){ canvas->setDefaultFillColor(c); });

            // Disable fill color when fill is off
            if (auto* fe = qobject_cast<QCheckBox*>(m_propertyEditors.value("tool.fillEnabled"))) {
                const bool on = fe->isChecked();
                if (m_propertyEditors.contains("tool.fillColor")) m_propertyEditors["tool.fillColor"]->setEnabled(on);
                connect(fe, &QCheckBox::toggled, this, [this](bool on2){
                    if (m_propertyEditors.contains("tool.fillColor")) m_propertyEditors["tool.fillColor"]->setEnabled(on2);
                });
            }
        }

    } else {
        // Keep non-drawing tools simple and working
        switch (tool) {
        case DrawingTool::Select: {
            addGroup("Options");
            if (canvas && canvas->selectionManager()) {
                SelectionManager* sm = canvas->selectionManager();
                addBool("select.pipetteMode", "Pipette mode", sm->isPipetteMode(),
                        [sm](bool on){ sm->setPipetteMode(on); });
                addIntSlider("select.pipetteTolerance", "Color tolerance", 0, 100,
                             sm->pipetteTolerance(),
                             [sm](int t){ sm->setPipetteTolerance(t); });
            }
            break;
        }
        case DrawingTool::Move: {
            addGroup("Options");
            if (canvas) {
                addBool("move.snap", "Snap to grid", canvas->isSnapEnabled(),
                        [canvas](bool on){ canvas->setSnapEnabled(on); });
                addBool("move.magnetic", "Magnetic connection", canvas->isMagneticConnectionEnabled(),
                        [canvas](bool on){ canvas->setMagneticConnectionEnabled(on); });
            }
            break;
        }
        case DrawingTool::Brush:
        case DrawingTool::Blur: {
            addGroup("Brush");
            if (canvas) {
                addIntSlider("brush.size", "Size", 1, 100,
                             static_cast<int>(canvas->brushSize()),
                             [canvas](int s){ canvas->setBrushSize(s); });
                addIntSlider("brush.hardness", tool == DrawingTool::Blur ? "Strength" : "Hardness",
                             0, 100,
                             static_cast<int>(canvas->brushHardness() * 100.0f),
                             [canvas](int h){ canvas->setBrushHardness(h / 100.0f); });
                addColorButton("brush.color", "Color", canvas->defaultDrawingColor(),
                               [canvas](const QColor& c){ canvas->setDefaultDrawingColor(c); });
            }
            break;
        }
        case DrawingTool::Eraser: {
            addGroup("Eraser");
            if (canvas) {
                addIntSlider("eraser.size", "Size", 1, 100,
                             static_cast<int>(canvas->eraserSize()),
                             [canvas](int s){ canvas->setEraserSize(static_cast<float>(s)); });
            }
            break;
        }
        case DrawingTool::Fill: {
            addGroup("Fill");
            if (canvas) {
                addColorButton("fill.color", "Color", canvas->defaultDrawingColor(),
                               [canvas](const QColor& c){ canvas->setDefaultDrawingColor(c); });
                addBool("fill.splash", "Splash mode",
                        canvas->fillMode() == DrawingCanvas::FillMode::Splash,
                        [canvas](bool on){
                            canvas->setFillMode(on ? DrawingCanvas::FillMode::Splash
                                                   : DrawingCanvas::FillMode::Normal);
                        });
            }
            break;
        }
        case DrawingTool::Image: {
            QLabel* info = new QLabel("Import an image, then select it to edit mask and transform properties.");
            info->setWordWrap(true);
            info->setStyleSheet("color: rgba(231,234,240,0.55);");
            m_contentLayout->addWidget(info);
            break;
        }
        case DrawingTool::Measure: {
            QLabel* info = new QLabel("Click-drag to measure distance on the canvas.");
            info->setWordWrap(true);
            info->setStyleSheet("color: rgba(231,234,240,0.55);");
            m_contentLayout->addWidget(info);
            break;
        }
        default: {
            QLabel* info = new QLabel("(No basic tool properties)");
            info->setStyleSheet("color: rgba(231,234,240,0.55);");
            m_contentLayout->addWidget(info);
            break;
        }
        }
    }

    m_contentLayout->addStretch();
}

