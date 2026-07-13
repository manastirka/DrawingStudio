#pragma once

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <functional>

class DrawingCanvas;
class ImagePrimitive;

/**
 * Image-to-drawing engines used by the HTTP command API:
 * render_mosaic, auto_trace, render_photo_copy.
 * Extracted from MainWindow::executeDrawingCommand (refactor A1).
 */
class ImageToDrawingEngine {
public:
    struct Context {
        DrawingCanvas *canvas = nullptr;
        std::function<void(const QString &)> saveUndo;
        std::function<ImagePrimitive *(int)> findImage;
    };

    ImageToDrawingEngine();
    explicit ImageToDrawingEngine(Context ctx);

    void setContext(Context ctx) { m_ctx = std::move(ctx); }
    const Context &context() const { return m_ctx; }

    static bool handlesAction(const QString &action);

    QJsonObject execute(const QString &action, const QJsonObject &params);
    QJsonObject renderMosaic(const QJsonObject &params);
    QJsonObject autoTrace(const QJsonObject &params);
    QJsonObject renderPhotoCopy(const QJsonObject &params);

    const QJsonArray &adaptiveLeafCells() const { return m_adaptiveLeafCells; }
    void setAdaptiveLeafCells(QJsonArray cells) { m_adaptiveLeafCells = std::move(cells); }

private:
    ImagePrimitive *findImageByIndex(int index) const;

    static QColor parseColor(const QJsonObject &p, const QString &key,
                             const QColor &defaultColor = Qt::black);
    static double getDouble(const QJsonObject &p, const QString &key, double def = 0.0);
    static bool getBool(const QJsonObject &p, const QString &key, bool def = false);

    Context m_ctx;
    QJsonArray m_adaptiveLeafCells;
};
