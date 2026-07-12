#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>

class DrawingCanvas;
class MainWindow;

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
    void setMainWindow(MainWindow *mw) { m_mainWindow = mw; }

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

    QTcpServer *m_server = nullptr;
    DrawingCanvas *m_canvas = nullptr;
    MainWindow *m_mainWindow = nullptr;
    QMap<QTcpSocket*, QByteArray> m_buffers;
};
