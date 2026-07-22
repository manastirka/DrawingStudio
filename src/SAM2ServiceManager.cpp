#include "SAM2ServiceManager.h"
#include "SAM2Client.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QDebug>
#include <QUrl>
#include <QUuid>

namespace {
constexpr const char *kSam2TokenEnvironment = "DRAWINGSTUDIO_SAM2_TOKEN";
constexpr const char *kProtocolHeader = "X-DrawingStudio-SAM2-Protocol";
constexpr const char *kProtocolVersion = "2";
}

SAM2ServiceManager::SAM2ServiceManager(QObject *parent)
    : QObject(parent)
{
    m_authToken = qEnvironmentVariable(kSam2TokenEnvironment).trimmed().toUtf8();
    if (!m_authToken.isEmpty()
        && m_authToken.size() < SAM2Client::kMinAuthTokenBytes) {
        qWarning() << "SAM2ServiceManager: Ignoring weak DRAWINGSTUDIO_SAM2_TOKEN";
        m_authToken.clear();
    }
    if (m_authToken.isEmpty()) {
        m_authToken = QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8();
    }
    SAM2Client::setDefaultAuthToken(m_authToken);

    m_nam = new QNetworkAccessManager(this);
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(5000);
    connect(m_pollTimer, &QTimer::timeout, this, &SAM2ServiceManager::pollHealth);
}

SAM2ServiceManager::~SAM2ServiceManager()
{
    stop();
}

QString SAM2ServiceManager::serviceDir() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;
    QDir walk(appDir);
    for (int i = 0; i < 8; ++i) {
        candidates << walk.absoluteFilePath(QStringLiteral("sam2_service"));
        if (!walk.cdUp())
            break;
    }
    candidates << appDir + QStringLiteral("/../sam2_service");
    candidates << QDir(appDir).absoluteFilePath(QStringLiteral("../../sam2_service"));
    candidates << QDir(appDir).absoluteFilePath(QStringLiteral("../../../../sam2_service"));

    for (const QString &c : candidates) {
        const QString abs = QDir(c).absolutePath();
        if (QFileInfo::exists(abs + QStringLiteral("/sam2_service.py")))
            return abs;
    }
    return QDir(appDir + QStringLiteral("/../sam2_service")).absolutePath();
}

QString SAM2ServiceManager::pythonPath() const
{
    const QString venv = serviceDir() + QStringLiteral("/venv/bin/python3");
    if (QFileInfo::exists(venv))
        return venv;
    return QStringLiteral("python3");
}

QString SAM2ServiceManager::scriptPath() const
{
    return serviceDir() + QStringLiteral("/sam2_service.py");
}

void SAM2ServiceManager::applyHealthResult(bool ok, const QString &msg)
{
    if (ok != m_healthy || msg != m_statusMessage) {
        m_healthy = ok;
        m_statusMessage = msg;
        emit statusChanged(m_healthy, m_statusMessage);
    }
}

void SAM2ServiceManager::launchIfNeeded()
{
    if (m_healthy || m_launchAttempted)
        return;
    m_launchAttempted = true;

    const QString script = scriptPath();
    const QString python = pythonPath();
    if (!QFileInfo::exists(script)) {
        applyHealthResult(false, QStringLiteral("SAM2 service not found"));
        return;
    }

    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &SAM2ServiceManager::onProcessFinished);
    }

    if (m_process->state() != QProcess::NotRunning)
        return;

    m_process->setWorkingDirectory(serviceDir());
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QString::fromLatin1(kSam2TokenEnvironment),
                       QString::fromUtf8(m_authToken));
    m_process->setProcessEnvironment(environment);
    qDebug() << "SAM2ServiceManager: launching" << python << script
             << "cwd" << serviceDir();
    m_process->start(python, {script});
    m_ownedProcess = true;
    if (!m_process->waitForStarted(4000)) {
        applyHealthResult(false, QStringLiteral("SAM2 failed to start"));
        m_ownedProcess = false;
        qWarning() << "SAM2ServiceManager: start failed:"
                   << m_process->errorString();
    } else {
        applyHealthResult(false, QStringLiteral("SAM2 starting…"));
        QTimer::singleShot(8000, this, [this]() {
            if (m_healthy || !m_process)
                return;
            const QByteArray out = m_process->readAllStandardOutput();
            if (!out.isEmpty())
                qWarning() << "SAM2ServiceManager output:"
                           << QString::fromUtf8(out.right(800));
        });
    }
}

void SAM2ServiceManager::start()
{
    m_launchAttempted = false;
    applyHealthResult(false, QStringLiteral("Checking SAM2…"));
    m_pollTimer->start();
    pollHealth();
    // If not already healthy, try launching after first probe settles
    QTimer::singleShot(600, this, [this]() {
        if (!m_healthy)
            launchIfNeeded();
    });
    QTimer::singleShot(1500, this, &SAM2ServiceManager::pollHealth);
    QTimer::singleShot(4000, this, &SAM2ServiceManager::pollHealth);
    QTimer::singleShot(10000, this, &SAM2ServiceManager::pollHealth);
}

void SAM2ServiceManager::stop()
{
    if (m_pollTimer)
        m_pollTimer->stop();
    if (m_ownedProcess && m_process &&
        m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000))
            m_process->kill();
    }
    m_ownedProcess = false;
}

void SAM2ServiceManager::pollHealth()
{
    // Async — never nest QEventLoop (breaks native file dialogs on macOS).
    if (m_healthInFlight)
        return;
    m_healthInFlight = true;

    QNetworkRequest req(QUrl(QStringLiteral("http://127.0.0.1:5001/health")));
    req.setRawHeader("Authorization", "Bearer " + m_authToken);
    req.setRawHeader("Accept", "application/json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::ManualRedirectPolicy);
    req.setTransferTimeout(1500);
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_healthInFlight = false;

        bool ok = false;
        QString msg = QStringLiteral("SAM2 offline");
        if (reply->error() == QNetworkReply::NoError
            && reply->rawHeader(kProtocolHeader) == kProtocolVersion) {
            const QJsonObject obj =
                QJsonDocument::fromJson(reply->readAll()).object();
            const bool authenticated =
                obj.value(QStringLiteral("auth_required")).toBool(false);
            ok = authenticated
                && (obj.value(QStringLiteral("sam2_loaded")).toBool(false)
                    || obj.value(QStringLiteral("status")).toString()
                        == QStringLiteral("ok"));
            msg = ok ? QStringLiteral("SAM2 ready")
                     : QStringLiteral("SAM2 starting…");
        } else if (reply->error() == QNetworkReply::NoError) {
            msg = QStringLiteral("Insecure or outdated SAM2 service detected");
        }
        applyHealthResult(ok, msg);
        if (!ok)
            launchIfNeeded();
    });
}

void SAM2ServiceManager::onProcessFinished(int exitCode,
                                           QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    m_ownedProcess = false;
    m_launchAttempted = false; // allow relaunch on next unhealthy poll
    applyHealthResult(false, QStringLiteral("SAM2 exited (%1)").arg(exitCode));
}
