#pragma once

#include <QWidget>
#include <QColor>
#include <QVector>
#include <QSignalMapper>
#include <QHBoxLayout>
#include <QPushButton>

class ColorPalette : public QWidget
{
    Q_OBJECT

public:
    explicit ColorPalette(QWidget *parent = nullptr);
    ~ColorPalette();

    QColor getCurrentColor() const { return m_currentColor; }
    void setCurrentColor(const QColor &color);

signals:
    void colorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void onColorButtonClicked(const QColor &color);

private:
    void setupPalette();
    void createColorButton(const QColor &color, int index);
    void updateButtonStyles();
    
    QHBoxLayout *m_layout;
    QVector<QColor> m_colors;
    QColor m_currentColor;
    QPushButton *m_currentButton;
    QVector<QPushButton*> m_colorButtons;
    
    int m_colorSize;
    int m_selectedIndex;
    
    // Default color palette
    static const QVector<QColor> s_defaultColors;
};
