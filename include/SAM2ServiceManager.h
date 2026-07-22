#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QString>
#include <QNetworkAccessManager>
#include <QByteArray>

/**
 * Owns the optional local SAM2 Python service process and exposes health status.
 * Safe if the service is already running externally — then we only poll /health.
 */
class SAM2ServiceManager : public QObject
{
    Q_OBJECT

public:
    explicit SAM2ServiceManager(QObject *parent = nullptr);
    ~SAM2ServiceManager() override;

    void start();
    void stop();
    bool isHealthy() const { return m_healthy; }
    QString statusMessage() const { return m_statusMessage; }

signals:
    void statusChanged(bool healthy, const QString &message);

private slots:
    void pollHealth();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void applyHealthResult(bool ok, const QString &msg);
    void launchIfNeeded();
    QString serviceDir() const;
    QString pythonPath() const;
    QString scriptPath() const;

    QProcess *m_process = nullptr;
    QTimer *m_pollTimer = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    bool m_healthy = false;
    bool m_ownedProcess = false;
    bool m_launchAttempted = false;
    bool m_healthInFlight = false;
    QByteArray m_authToken;
    QString m_statusMessage;
};
