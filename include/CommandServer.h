#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QMap>
#include <QWidget>

#include <functional>

class DrawingCanvas;

class CommandServer : public QObject
{
    Q_OBJECT

public:
    explicit CommandServer(QObject *parent = nullptr);
    ~CommandServer();

    bool start(quint16 port = 19100);
    void stop();
    bool isRunning() const;

    void setCanvas(DrawingCanvas *canvas) { m_canvas = canvas; }

    /** Window used for full-window screenshots (typically MainWindow). */
    void setWindowWidget(QWidget *window) { m_windowWidget = window; }

    /** Provider for last bot-command JSON result (typically dispatcher / MainWindow). */
    void setCommandResultProvider(std::function<QJsonObject()> provider)
    {
        m_resultProvider = std::move(provider);
    }

signals:
    void commandReceived(const QString &action, const QJsonObject &params);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void handleRequest(QTcpSocket *socket, const QByteArray &requestData);
    void parseHttpRequest(const QByteArray &data, QString &method, QString &path,
                          QByteArray &body, QMap<QString, QString> &headers);

    void handleCommandEndpoint(QTcpSocket *socket, const QByteArray &body);
    void handleBatchEndpoint(QTcpSocket *socket, const QByteArray &body);
    void handleScreenshotEndpoint(QTcpSocket *socket);
    void handleWindowScreenshotEndpoint(QTcpSocket *socket);
    void handleMaskDebugEndpoint(QTcpSocket *socket);
    void handleStatusEndpoint(QTcpSocket *socket);
    void handleClearEndpoint(QTcpSocket *socket);
    void handleUndoEndpoint(QTcpSocket *socket);
    void handleRedoEndpoint(QTcpSocket *socket);

    void sendJsonResponse(QTcpSocket *socket, int statusCode, const QJsonObject &json);
    void sendImageResponse(QTcpSocket *socket, const QByteArray &imageData);
    void sendErrorResponse(QTcpSocket *socket, int statusCode, const QString &message);

    QJsonObject lastCommandResult() const;

    QTcpServer *m_server = nullptr;
    DrawingCanvas *m_canvas = nullptr;
    QWidget *m_windowWidget = nullptr;
    std::function<QJsonObject()> m_resultProvider;
    QMap<QTcpSocket*, QByteArray> m_buffers;
};
