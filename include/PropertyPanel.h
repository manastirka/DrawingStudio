#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QSlider>
#include <QPushButton>
#include <QColorDialog>
#include <QGroupBox>
#include <QScrollArea>
#include <QFormLayout>
#include <QTextEdit>
#include <memory>

#include "DrawingCanvas.h" // for DrawingTool

class DrawingPrimitive;
class SimpleTextPanel;
class ClassicTextTool;

class ColorButton : public QPushButton
{
    Q_OBJECT
    
public:
    explicit ColorButton(const QColor& color = Qt::white, QWidget* parent = nullptr);
    
    QColor color() const { return m_color; }
    void setColor(const QColor& color);
    
signals:
    void colorChanged(const QColor& color);
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    
private:
    QColor m_color;
    void updateStyle();
};

class PropertyPanel : public QWidget
{
    Q_OBJECT
    
public:
    explicit PropertyPanel(QWidget* parent = nullptr);
    
    void showPrimitiveProperties(DrawingPrimitive* primitive, class DrawingCanvas* canvas = nullptr);
    void showMultiplePrimitiveProperties(const std::vector<DrawingPrimitive*>& primitives);
    void showCanvasProperties(class DrawingCanvas* canvas);
    void showToolProperties(DrawingTool tool, class DrawingCanvas* canvas);
    void clearProperties();
    void refreshCurrentProperties();
    
    // Tool wiring
    void setTextTool(ClassicTextTool* textTool);
    
    // Real-time text controls access
    SimpleTextPanel* getSimpleTextPanel() const { return m_simpleTextPanel; }
    void showTextControls();
    
signals:
    void propertyChanged(const QString& objectName, const QString& propertyName, const QVariant& value);
    
private slots:
    void onPropertyValueChanged();
    
private:
    void setupUI();
    void addPropertyEditor(const QString& propertyName, const QString& displayName, 
                          QMetaType::Type type, const QVariant& value);
    void addSeparator();
    void addGroup(const QString& title);
    std::vector<QVector2D> subdivideBezierCurve(const std::vector<QVector2D>& points, float t);
    
    QScrollArea* m_scrollArea;
    QWidget* m_contentWidget;
    QVBoxLayout* m_contentLayout;
    QFormLayout* m_formLayout;
    
    // Current object being edited
    DrawingPrimitive* m_currentPrimitive;
    std::vector<DrawingPrimitive*> m_currentPrimitives;
    class DrawingCanvas* m_currentCanvas;
    
    // Text tool UI
    SimpleTextPanel* m_simpleTextPanel;
    ClassicTextTool* m_textTool;
    QString m_currentObjectName;
    
    // Property editors
    QMap<QString, QWidget*> m_propertyEditors;
    
    void clearLayout();
    QWidget* createEditor(const QString& propertyName, QMetaType::Type type, const QVariant& value);
};
