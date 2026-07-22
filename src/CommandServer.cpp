#include "CommandServer.h"
#include "AutomationCommandCatalog.h"
#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
#include "LayerManager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>
#include <QImage>
#include <QPixmap>
#include <QDebug>
#include <QFileInfo>
#include <QTimer>

CommandServer::CommandServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &CommandServer::onNewConnection);
}

CommandServer::~CommandServer()
{
    stop();
}

QJsonObject CommandServer::lastCommandResult() const
{
    if (m_resultProvider)
        return m_resultProvider();
    return QJsonObject();
}

bool CommandServer::start(quint16 port)
{
    if (m_server->isListening())
        return true;

    if (m_authToken.size() < kMinAuthTokenBytes) {
        qWarning() << "CommandServer: Refusing to start without an authentication token of at least"
                   << kMinAuthTokenBytes << "bytes";
        return false;
    }

    if (!m_server->listen(QHostAddress::LocalHost, port)) {
        qWarning() << "CommandServer: Failed to listen on port" << port
                    << m_server->errorString();
        return false;
    }

    qDebug() << "Command server listening on port" << port;
    return true;
}

void CommandServer::stop()
{
    if (m_server->isListening()) {
        m_server->close();
        qDebug() << "Command server stopped";
    }
}

bool CommandServer::isRunning() const
{
    return m_server->isListening();
}

void CommandServer::onNewConnection()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        if (m_buffers.size() >= kMaxConnections) {
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            sendErrorResponse(socket, 503, QStringLiteral("Too many connections"));
            continue;
        }
        connect(socket, &QTcpSocket::readyRead, this, &CommandServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &CommandServer::onDisconnected);
        m_buffers[socket] = QByteArray();

        auto *timer = new QTimer(socket);
        timer->setSingleShot(true);
        timer->setInterval(kConnectionTimeoutMs);
        connect(timer, &QTimer::timeout, socket, [socket]() {
            socket->disconnectFromHost();
        });
        timer->start();
    }
}

void CommandServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    QByteArray incoming = socket->readAll();
    QByteArray &buf = m_buffers[socket];

    if (buf.size() + incoming.size() > kMaxHeaderBytes + kMaxBodyBytes) {
        sendErrorResponse(socket, 413, QStringLiteral("Request too large"));
        m_buffers.remove(socket);
        return;
    }
    buf.append(incoming);

    // Check if we have the full HTTP request (headers + body)
    int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        if (buf.size() > kMaxHeaderBytes) {
            sendErrorResponse(socket, 431, QStringLiteral("Request headers too large"));
            m_buffers.remove(socket);
        }
        return; // Still waiting for headers
    }
    if (headerEnd > kMaxHeaderBytes) {
        sendErrorResponse(socket, 431, QStringLiteral("Request headers too large"));
        m_buffers.remove(socket);
        return;
    }

    // Parse Content-Length to know when body is complete
    QString headerStr = QString::fromUtf8(buf.left(headerEnd));
    int contentLength = 0;
    for (const QString &line : headerStr.split("\r\n")) {
        if (line.startsWith("Content-Length:", Qt::CaseInsensitive)) {
            bool ok = false;
            contentLength = line.mid(15).trimmed().toInt(&ok);
            if (!ok || contentLength < 0) {
                sendErrorResponse(socket, 400, QStringLiteral("Invalid Content-Length"));
                m_buffers.remove(socket);
                return;
            }
            break;
        }
    }

    if (contentLength > kMaxBodyBytes) {
        sendErrorResponse(socket, 413, QStringLiteral("Request body too large"));
        m_buffers.remove(socket);
        return;
    }

    int totalExpected = headerEnd + 4 + contentLength;
    if (buf.size() < totalExpected)
        return; // Still waiting for body

    QByteArray requestData = buf.left(totalExpected);
    buf.remove(0, totalExpected);

    handleRequest(socket, requestData);
}

void CommandServer::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        m_buffers.remove(socket);
        socket->deleteLater();
    }
}

void CommandServer::parseHttpRequest(const QByteArray &data, QString &method, QString &path,
                                     QByteArray &body, QMap<QString, QString> &headers)
{
    int headerEnd = data.indexOf("\r\n\r\n");
    QByteArray headerPart = data.left(headerEnd);
    body = data.mid(headerEnd + 4);

    QList<QByteArray> lines = headerPart.split('\n');
    if (lines.isEmpty()) return;

    // Parse request line: "GET /path HTTP/1.1"
    QList<QByteArray> requestLine = lines[0].trimmed().split(' ');
    if (requestLine.size() >= 2) {
        method = QString::fromUtf8(requestLine[0]);
        path = QString::fromUtf8(requestLine[1]);
    }

    // Parse headers
    for (int i = 1; i < lines.size(); ++i) {
        QString line = QString::fromUtf8(lines[i].trimmed());
        int colon = line.indexOf(':');
        if (colon > 0) {
            headers[line.left(colon).trimmed().toLower()] = line.mid(colon + 1).trimmed();
        }
    }
}

void CommandServer::handleRequest(QTcpSocket *socket, const QByteArray &requestData)
{
    QString method, path;
    QByteArray body;
    QMap<QString, QString> headers;
    parseHttpRequest(requestData, method, path, body, headers);

    if (!isAuthorized(headers)) {
        sendErrorResponse(socket, 401, QStringLiteral("Unauthorized"));
        return;
    }

    if (method == "OPTIONS") {
        sendErrorResponse(socket, 405, QStringLiteral("CORS preflight is not supported"));
        return;
    }

    if (path == "/api/command" && method == "POST") {
        handleCommandEndpoint(socket, body);
    } else if (path == "/api/batch" && method == "POST") {
        handleBatchEndpoint(socket, body);
    } else if ((path == "/api/commands" || path == "/api/help") && method == "GET") {
        handleCommandsCatalogEndpoint(socket);
    } else if (path == "/api/screenshot" && method == "GET") {
        handleScreenshotEndpoint(socket);
    } else if (path == "/api/window_screenshot" && method == "GET") {
        handleWindowScreenshotEndpoint(socket);
    } else if (path == "/api/mask_debug" && method == "GET") {
        handleMaskDebugEndpoint(socket);
    } else if (path == "/api/status" && method == "GET") {
        handleStatusEndpoint(socket);
    } else if (path == "/api/clear" && method == "POST") {
        handleClearEndpoint(socket);
    } else if (path == "/api/undo" && method == "POST") {
        handleUndoEndpoint(socket);
    } else if (path == "/api/redo" && method == "POST") {
        handleRedoEndpoint(socket);
    } else {
        sendErrorResponse(socket, 404, "Not found: " + path);
    }
}

void CommandServer::handleCommandEndpoint(QTcpSocket *socket, const QByteArray &body)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        sendErrorResponse(socket, 400, "Invalid JSON: " + parseError.errorString());
        return;
    }

    QJsonObject obj = doc.object();
    QString action = obj["action"].toString();
    if (obj.contains(QStringLiteral("params"))
        && !obj.value(QStringLiteral("params")).isObject()) {
        sendErrorResponse(socket, 400, QStringLiteral("'params' must be an object"));
        return;
    }
    QJsonObject params = obj["params"].toObject();

    if (action.isEmpty()) {
        sendErrorResponse(socket, 400, "Missing 'action' field");
        return;
    }

    if (!AutomationCommandCatalog::containsAction(action)) {
        sendErrorResponse(socket, 400,
                          QStringLiteral("Unknown action: %1").arg(action));
        return;
    }

    const QStringList missing =
        AutomationCommandCatalog::missingRequiredParams(action, params);
    if (!missing.isEmpty()) {
        sendErrorResponse(
            socket, 400,
            QStringLiteral("Missing required parameters for %1: %2")
                .arg(action, missing.join(QStringLiteral(", "))));
        return;
    }

    if (isFilesystemAction(action) && !m_allowFilesystemCommands) {
        sendErrorResponse(
            socket, 403,
            QStringLiteral("Filesystem commands require --automation-allow-filesystem"));
        return;
    }

    // Emit signal for MainWindow to handle (synchronous DirectConnection)
    emit commandReceived(action, params);

    QJsonObject response;
    response["action"] = action;

    QJsonObject result = lastCommandResult();
    if (!result.isEmpty()) {
        response["result"] = result;
    }

    // Explicit failure from handler (unknown / unimplemented / validation)
    if (result.contains("success") && !result.value("success").toBool()) {
        response["status"] = "error";
        if (result.contains("error")) {
            response["message"] = result.value("error").toString();
        }
        sendJsonResponse(socket, 400, response);
        return;
    }

    response["status"] = "ok";
    sendJsonResponse(socket, 200, response);
}

void CommandServer::handleBatchEndpoint(QTcpSocket *socket, const QByteArray &body)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        sendErrorResponse(socket, 400, "Invalid JSON: " + parseError.errorString());
        return;
    }

    QJsonObject obj = doc.object();
    QJsonArray commands = obj["commands"].toArray();

    if (commands.isEmpty()) {
        sendErrorResponse(socket, 400, "Missing or empty 'commands' array");
        return;
    }

    if (commands.size() > kMaxBatchCommands) {
        sendErrorResponse(socket, 400,
            QStringLiteral("Batch too large: %1 commands (max %2)")
                .arg(commands.size())
                .arg(kMaxBatchCommands));
        return;
    }

    // Optional: stop processing further commands after the first failure.
    const bool stopOnError = obj.value(QStringLiteral("stopOnError")).toBool(false);

    QJsonArray results;
    int okCount = 0;
    int errorCount = 0;
    int policyDeniedCount = 0;
    bool stoppedEarly = false;

    for (int i = 0; i < commands.size(); ++i) {
        QJsonObject cmd = commands[i].toObject();
        QString action = cmd["action"].toString();
        QJsonObject params = cmd["params"].toObject();

        if (action.isEmpty()) {
            QJsonObject r;
            r["index"] = i;
            r["status"] = "error";
            r["message"] = "Missing 'action' field";
            results.append(r);
            ++errorCount;
            if (stopOnError) {
                stoppedEarly = true;
                break;
            }
            continue;
        }

        QString validationError;
        if (cmd.contains(QStringLiteral("params"))
            && !cmd.value(QStringLiteral("params")).isObject()) {
            validationError = QStringLiteral("'params' must be an object");
        } else if (!AutomationCommandCatalog::containsAction(action)) {
            validationError = QStringLiteral("Unknown action: %1").arg(action);
        } else {
            const QStringList missing =
                AutomationCommandCatalog::missingRequiredParams(action, params);
            if (!missing.isEmpty()) {
                validationError =
                    QStringLiteral("Missing required parameters for %1: %2")
                        .arg(action, missing.join(QStringLiteral(", ")));
            }
        }
        if (!validationError.isEmpty()) {
            QJsonObject r;
            r["index"] = i;
            r["action"] = action;
            r["status"] = "error";
            r["message"] = validationError;
            results.append(r);
            ++errorCount;
            if (stopOnError) {
                stoppedEarly = true;
                break;
            }
            continue;
        }

        if (isFilesystemAction(action) && !m_allowFilesystemCommands) {
            QJsonObject r;
            r["index"] = i;
            r["action"] = action;
            r["status"] = "error";
            r["message"] =
                QStringLiteral("Filesystem commands require --automation-allow-filesystem");
            results.append(r);
            ++errorCount;
            ++policyDeniedCount;
            if (stopOnError) {
                stoppedEarly = true;
                break;
            }
            continue;
        }

        emit commandReceived(action, params);

        QJsonObject r;
        r["index"] = i;
        r["action"] = action;

        QJsonObject result = lastCommandResult();
        if (!result.isEmpty()) {
            r["result"] = result;
        }

        // Mirror single-command semantics: success:false → per-item error.
        const bool failed = result.contains(QStringLiteral("success"))
            && !result.value(QStringLiteral("success")).toBool();
        if (failed) {
            r["status"] = "error";
            if (result.contains(QStringLiteral("error"))) {
                r["message"] = result.value(QStringLiteral("error")).toString();
            }
            ++errorCount;
            results.append(r);
            if (stopOnError) {
                stoppedEarly = true;
                break;
            }
            continue;
        }

        r["status"] = "ok";
        ++okCount;
        results.append(r);
    }

    QJsonObject response;
    // overall "ok" if at least one command ran; "partial" / "error" by counts
    if (errorCount == 0) {
        response["status"] = "ok";
    } else if (okCount == 0) {
        response["status"] = "error";
    } else {
        response["status"] = "partial";
    }
    response["count"] = commands.size();
    response["okCount"] = okCount;
    response["errorCount"] = errorCount;
    response["processed"] = results.size();
    if (stoppedEarly)
        response["stoppedEarly"] = true;
    response["results"] = results;

    // HTTP 200 for partial (client inspects status field); 400 only when all fail.
    const int httpCode =
        (okCount == 0 && errorCount > 0)
            ? (policyDeniedCount == errorCount ? 403 : 400)
            : 200;
    sendJsonResponse(socket, httpCode, response);
}

bool CommandServer::constantTimeEquals(const QByteArray &left, const QByteArray &right)
{
    const qsizetype maxSize = qMax(left.size(), right.size());
    quint64 difference = static_cast<quint64>(left.size())
        ^ static_cast<quint64>(right.size());
    for (qsizetype i = 0; i < maxSize; ++i) {
        const unsigned char a = i < left.size() ? static_cast<unsigned char>(left.at(i)) : 0;
        const unsigned char b = i < right.size() ? static_cast<unsigned char>(right.at(i)) : 0;
        difference |= static_cast<quint64>(a ^ b);
    }
    return difference == 0;
}

bool CommandServer::isAuthorized(const QMap<QString, QString> &headers) const
{
    static const QByteArray prefix("Bearer ");
    const QByteArray value = headers.value(QStringLiteral("authorization")).trimmed().toUtf8();
    if (!value.startsWith(prefix))
        return false;
    return constantTimeEquals(value.mid(prefix.size()), m_authToken);
}

bool CommandServer::isFilesystemAction(const QString &action)
{
    return AutomationCommandCatalog::requiresFilesystemAccess(action);
}

void CommandServer::handleCommandsCatalogEndpoint(QTcpSocket *socket)
{
    sendJsonResponse(socket, 200, commandsCatalog());
}

QJsonObject CommandServer::commandsCatalog()
{
    auto endpoint = [](const char *method, const char *path, const char *desc) {
        QJsonObject o;
        o[QStringLiteral("method")] = QString::fromUtf8(method);
        o[QStringLiteral("path")] = QString::fromUtf8(path);
        o[QStringLiteral("description")] = QString::fromUtf8(desc);
        return o;
    };

    QJsonArray endpoints;
    endpoints.append(endpoint("GET", "/api/status", "Canvas status (size, objectCount, tool, zoom)"));
    endpoints.append(endpoint("GET", "/api/commands", "This catalog (also /api/help)"));
    endpoints.append(endpoint("GET", "/api/help", "Alias of /api/commands"));
    endpoints.append(endpoint("GET", "/api/screenshot", "PNG of canvas content"));
    endpoints.append(endpoint("GET", "/api/window_screenshot", "PNG of main window"));
    endpoints.append(endpoint("GET", "/api/mask_debug", "JSON debug info for image masks"));
    endpoints.append(endpoint("POST", "/api/command",
                              "Run one bot command: {action, params}"));
    endpoints.append(endpoint("POST", "/api/batch",
                              "Run many commands: {commands:[{action,params}], stopOnError?}"));
    endpoints.append(endpoint("POST", "/api/clear", "Clear canvas (with undo)"));
    endpoints.append(endpoint("POST", "/api/undo", "Undo last edit"));
    endpoints.append(endpoint("POST", "/api/redo", "Redo last undone edit"));

    const QJsonArray commands = AutomationCommandCatalog::commands();

    QJsonObject batch;
    batch[QStringLiteral("maxCommands")] = kMaxBatchCommands;
    batch[QStringLiteral("body")] = QStringLiteral(
        "{ \"commands\": [ {\"action\":\"...\",\"params\":{...}} ], \"stopOnError\": false }");
    batch[QStringLiteral("notes")] = QStringLiteral(
        "Per-item status ok|error; overall status ok|partial|error. "
        "HTTP 400 when every item fails or batch exceeds maxCommands.");

    QJsonObject response;
    response[QStringLiteral("status")] = QStringLiteral("ok");
    response[QStringLiteral("version")] = 2;
    response[QStringLiteral("endpoints")] = endpoints;
    response[QStringLiteral("commands")] = commands;
    response[QStringLiteral("batch")] = batch;
    response[QStringLiteral("commandCount")] = commands.size();
    return response;
}

void CommandServer::handleScreenshotEndpoint(QTcpSocket *socket)
{
    if (!m_canvas) {
        sendErrorResponse(socket, 500, "Canvas not available");
        return;
    }

    QImage image = m_canvas->renderToImage();
    if (image.isNull()) {
        sendErrorResponse(socket, 500, "Failed to render canvas");
        return;
    }

    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();

    sendImageResponse(socket, imageData);
}

void CommandServer::handleWindowScreenshotEndpoint(QTcpSocket *socket)
{
    if (!m_windowWidget) {
        sendErrorResponse(socket, 500, "Window widget not available");
        return;
    }

    QPixmap pixmap = m_windowWidget->grab();
    if (pixmap.isNull()) {
        sendErrorResponse(socket, 500, "Failed to grab window");
        return;
    }

    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");
    buffer.close();

    sendImageResponse(socket, imageData);
}

void CommandServer::handleMaskDebugEndpoint(QTcpSocket *socket)
{
    QJsonObject response;
    response["status"] = "ok";

    if (!m_canvas || !m_canvas->layerManager()) {
        response["images"] = QJsonArray();
        sendJsonResponse(socket, 200, response);
        return;
    }

    QJsonArray imagesArr;
    auto primitives = m_canvas->layerManager()->getAllPrimitives();
    int idx = 0;
    for (auto *prim : primitives) {
        if (auto *imgPrim = dynamic_cast<ImagePrimitive*>(prim)) {
            QJsonObject o;
            o["index"] = idx;
            o["position_x"] = static_cast<double>(imgPrim->position().x());
            o["position_y"] = static_cast<double>(imgPrim->position().y());
            o["width"] = static_cast<double>(imgPrim->size().x());
            o["height"] = static_cast<double>(imgPrim->size().y());
            o["selected"] = imgPrim->isSelected();
            o["maskCandidateCount"] = imgPrim->getMaskCandidateCount();
            o["selectedMaskIndex"] = imgPrim->getSelectedMaskIndex();
            imagesArr.append(o);
        }
        idx++;
    }
    response["images"] = imagesArr;

    sendJsonResponse(socket, 200, response);
}

void CommandServer::handleStatusEndpoint(QTcpSocket *socket)
{
    QJsonObject response;
    response["status"] = "ok";

    if (m_canvas) {
        response["width"] = m_canvas->width();
        response["height"] = m_canvas->height();
        int objectCount = 0;
        if (m_canvas->layerManager())
            objectCount = static_cast<int>(m_canvas->layerManager()->getAllPrimitives().size());
        response["objectCount"] = objectCount;
        response["zoomLevel"] = static_cast<double>(m_canvas->zoomLevel());
        response["gridVisible"] = m_canvas->isGridVisible();
        response["snapEnabled"] = m_canvas->isSnapEnabled();

        QString toolName;
        switch (m_canvas->currentTool()) {
            case DrawingTool::Select:    toolName = "select"; break;
            case DrawingTool::Line:      toolName = "line"; break;
            case DrawingTool::Rectangle: toolName = "rectangle"; break;
            case DrawingTool::Circle:    toolName = "circle"; break;
            case DrawingTool::Ellipse:   toolName = "ellipse"; break;
            case DrawingTool::Polygon:   toolName = "polygon"; break;
            case DrawingTool::Arc:       toolName = "arc"; break;
            case DrawingTool::Spline:    toolName = "spline"; break;
            case DrawingTool::BezierCurve: toolName = "bezier"; break;
            case DrawingTool::Text:      toolName = "text"; break;
            case DrawingTool::Brush:     toolName = "brush"; break;
            case DrawingTool::Eraser:    toolName = "eraser"; break;
            case DrawingTool::Image:     toolName = "image"; break;
            default:                     toolName = "other"; break;
        }
        response["currentTool"] = toolName;
    }

    sendJsonResponse(socket, 200, response);
}

void CommandServer::handleClearEndpoint(QTcpSocket *socket)
{
    if (!m_canvas) {
        sendErrorResponse(socket, 500, "Canvas not available");
        return;
    }

    // Emit as a command so MainWindow handles it with undo support
    QJsonObject params;
    emit commandReceived("clear_canvas", params);

    QJsonObject response;
    response["status"] = "ok";
    response["action"] = "clear_canvas";
    sendJsonResponse(socket, 200, response);
}

void CommandServer::handleUndoEndpoint(QTcpSocket *socket)
{
    QJsonObject params;
    emit commandReceived("undo", params);

    QJsonObject response;
    response["status"] = "ok";
    response["action"] = "undo";
    sendJsonResponse(socket, 200, response);
}

void CommandServer::handleRedoEndpoint(QTcpSocket *socket)
{
    QJsonObject params;
    emit commandReceived("redo", params);

    QJsonObject response;
    response["status"] = "ok";
    response["action"] = "redo";
    sendJsonResponse(socket, 200, response);
}

void CommandServer::sendJsonResponse(QTcpSocket *socket, int statusCode, const QJsonObject &json)
{
    QByteArray body = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QString statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 400: statusText = "Bad Request"; break;
        case 401: statusText = "Unauthorized"; break;
        case 403: statusText = "Forbidden"; break;
        case 404: statusText = "Not Found"; break;
        case 405: statusText = "Method Not Allowed"; break;
        case 413: statusText = "Payload Too Large"; break;
        case 431: statusText = "Request Header Fields Too Large"; break;
        case 500: statusText = "Internal Server Error"; break;
        case 503: statusText = "Service Unavailable"; break;
        default:  statusText = "Unknown"; break;
    }

    QByteArray response;
    response.append(QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
    response.append("Content-Type: application/json\r\n");
    response.append("Cache-Control: no-store\r\n");
    response.append("X-Content-Type-Options: nosniff\r\n");
    response.append("Connection: close\r\n");
    if (statusCode == 401)
        response.append("WWW-Authenticate: Bearer\r\n");
    response.append(QString("Content-Length: %1\r\n").arg(body.size()).toUtf8());
    response.append("\r\n");
    response.append(body);

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

void CommandServer::sendImageResponse(QTcpSocket *socket, const QByteArray &imageData)
{
    QByteArray response;
    response.append("HTTP/1.1 200 OK\r\n");
    response.append("Content-Type: image/png\r\n");
    response.append("Cache-Control: no-store\r\n");
    response.append("X-Content-Type-Options: nosniff\r\n");
    response.append("Connection: close\r\n");
    response.append(QString("Content-Length: %1\r\n").arg(imageData.size()).toUtf8());
    response.append("\r\n");
    response.append(imageData);

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

void CommandServer::sendErrorResponse(QTcpSocket *socket, int statusCode, const QString &message)
{
    QJsonObject json;
    json["status"] = "error";
    json["message"] = message;
    sendJsonResponse(socket, statusCode, json);
}
