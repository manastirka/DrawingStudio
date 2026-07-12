#ifndef COLORFULSPINNER_H
#define COLORFULSPINNER_H

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QColor>

class ColorfulSpinner : public QWidget {
    Q_OBJECT
    
public:
    explicit ColorfulSpinner(QWidget* parent = nullptr);
    
    void start();
    void stop();
    void setProgress(int percentage);  // 0-100, controls color transition
    
    QSize sizeHint() const override { return QSize(64, 64); }
    QSize minimumSizeHint() const override { return QSize(32, 32); }
    
protected:
    void paintEvent(QPaintEvent* event) override;
    
private slots:
    void rotate();
    
private:
    QTimer* m_timer;
    int m_angle;
    int m_progress;  // 0-100, controls B&W to color transition
    bool m_isAnimating;
    
    QColor interpolateColor(int segmentIndex, float colorProgress) const;
};

#endif // COLORFULSPINNER_H
