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

// PropertyPanel core (refactor E17).

PropertyPanel::PropertyPanel(QWidget* parent)
    : QWidget(parent)
    , m_currentPrimitive(nullptr)
    , m_currentCanvas(nullptr)
    , m_simpleTextPanel(nullptr)
    , m_textTool(nullptr)
{
    setupUI();

    // Create simplified text panel but keep hidden until needed
    m_simpleTextPanel = new SimpleTextPanel(this);
    m_simpleTextPanel->hide();
}

void PropertyPanel::setTextTool(ClassicTextTool* textTool)
{
    m_textTool = textTool;
    if (m_simpleTextPanel) {
        m_simpleTextPanel->setTextTool(textTool);
    }
}

