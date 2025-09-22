#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSlider>
#include <QPushButton>
#include <QColorDialog>
#include <QGroupBox>
#include <QScrollArea>
#include <QFormLayout>
#include <memory>

class GuitarComponent;
class DrawingPrimitive;

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
    
    void showComponentProperties(GuitarComponent* component);
    void showPrimitiveProperties(DrawingPrimitive* primitive);
    void showPromotedComponentProperties(DrawingPrimitive* primitive, const QString& componentType, const QString& componentName);
    void showMultiplePrimitiveProperties(const std::vector<DrawingPrimitive*>& primitives);
    void clearProperties();
    void refreshCurrentProperties();
    
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
    GuitarComponent* m_currentComponent;
    DrawingPrimitive* m_currentPrimitive;
    std::vector<DrawingPrimitive*> m_currentPrimitives;
    QString m_currentObjectName;
    
    // Property editors
    QMap<QString, QWidget*> m_propertyEditors;
    
    void clearLayout();
    QWidget* createEditor(const QString& propertyName, QMetaType::Type type, const QVariant& value);
};

