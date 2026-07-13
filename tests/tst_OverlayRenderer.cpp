#include "OverlayRenderer.h"

#include <QImage>
#include <QPainter>
#include <QtTest>

class tst_OverlayRenderer : public QObject {
    Q_OBJECT

private slots:
    void drawControlPoint_doesNotCrash();
    void drawSnapIndicator_doesNotCrash();
    void drawLasso_emptySafe();
    void drawAlignmentGuides_verticalAndHorizontal();
};

void tst_OverlayRenderer::drawControlPoint_doesNotCrash()
{
    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    OverlayRenderer::drawControlPoint(p, QVector2D(32, 32), 1.0f, false);
    OverlayRenderer::drawControlPoint(p, QVector2D(40, 40), 2.0f, true);
    p.end();
    QVERIFY(!img.isNull());
}

void tst_OverlayRenderer::drawSnapIndicator_doesNotCrash()
{
    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    OverlayRenderer::drawSnapIndicator(p, QPoint(32, 32));
    OverlayRenderer::drawCursorPreviewRing(p, QPoint(16, 16), 12.0f);
    p.end();
    QVERIFY(!img.isNull());
}

void tst_OverlayRenderer::drawLasso_emptySafe()
{
    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    OverlayRenderer::drawLassoPolygon(p, QPolygonF());
    QPolygonF poly;
    poly << QPointF(10, 10) << QPointF(40, 10) << QPointF(25, 40);
    OverlayRenderer::drawLassoPolygon(p, poly);
    p.end();
    QVERIFY(!img.isNull());
}

void tst_OverlayRenderer::drawAlignmentGuides_verticalAndHorizontal()
{
    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    std::vector<OverlayRenderer::GuideLine> guides = {
        {OverlayRenderer::GuideLine::Axis::Vertical, 20.f, QColor(0, 200, 255, 200)},
        {OverlayRenderer::GuideLine::Axis::Horizontal, 30.f, QColor(255, 128, 0, 200)},
    };
    OverlayRenderer::drawAlignmentGuides(p, guides);
    p.end();
    QVERIFY(!img.isNull());
}

QTEST_MAIN(tst_OverlayRenderer)
#include "tst_OverlayRenderer.moc"
