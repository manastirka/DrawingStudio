#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QVector>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QTemporaryDir>
#include <QtGlobal>
#include <memory>

/**
 * Unified async AI image generate / edit client.
 * Providers: openai, stability, higgsfield, nanobanana, remotesd.
 */
class AIImageClient : public QObject
{
    Q_OBJECT

public:
    struct Request {
        QString prompt;
        QImage sourceImage; // empty => text-to-image; set => image edit / img2img
        /** Extra reference images (cutouts) — Nano Banana / Gemini multi-image. */
        QVector<QImage> referenceImages;
        QString provider;   // empty => AI/provider from settings
        QString model;      // empty => provider default from settings
        QString size;       // e.g. 1024x1024
        QString aspectRatio;// e.g. 1:1
        QString imageSize;  // e.g. 1K (Google Nano Banana)
        QString quality;    // e.g. hd / high
        bool replaceSelected = false;
    };

    /** Draft credentials/endpoints used by a connection probe without persistence. */
    struct ConnectionTestConfig {
        QString apiKey;
        QString remoteSdUrl;
        QString higgsfieldCliPath;
    };

    explicit AIImageClient(QObject *parent = nullptr);
    ~AIImageClient() override;

    virtual bool isBusy() const { return m_busy; }
    virtual void cancel();

    virtual void generate(const Request &request);
    virtual void edit(const Request &request);
    void testConnection(const QString &provider);
    void testConnection(const QString &provider, const ConnectionTestConfig &config);

    static QString defaultRemoteSdUrl();

    static constexpr int kGenerationTimeoutMs = 180000;
    static constexpr int kDownloadTimeoutMs = 60000;
    static constexpr qsizetype kMaxNetworkResponseBytes = 128 * 1024 * 1024;

signals:
    void started(const QString &message);
    void progress(const QString &message);
    void finished(const QImage &image, const QString &prompt);
    void failed(const QString &error);
    void connectionTestFinished(bool ok, const QString &message);

protected:
    /** Subclasses / tests may complete a job without network I/O. */
    void finishWithImage(const QImage &image, const QString &prompt);
    void failWith(const QString &error);
    void setBusy(bool busy) { m_busy = busy; }

private:
    void runRequest(Request request, bool isEdit);
    void runOpenAI(const Request &req, bool isEdit);
    void runStability(const Request &req, bool isEdit);
    void runNanoBananaGoogle(const Request &req, bool isEdit);
    void postNanoBananaGenerate(Request req, bool isEdit, int attempt);
    void runHiggsfieldCli(const Request &req, const QString &model);
    void runRemoteSd(const Request &req);
    void downloadImageUrl(const QUrl &url, const QString &prompt);

    QString resolveProvider(const Request &req) const;
    QString resolveModel(const QString &provider, const Request &req) const;
    static QString normalizeNanoBananaModel(const QString &model);
    static QString sanitizeNanoBananaImageSize(const QString &model,
                                              const QString &size);
    static QString nextNanoBananaImageSize(const QString &size);
    static QString explainNanoBananaFinishReason(const QString &reason);
    QString sizeSetting(const Request &req) const;
    QByteArray imageToPngBytes(const QImage &image) const;
    QString saveTempPng(const QImage &image, QString *errorOut);
    static void applyRequestPolicy(QNetworkRequest &request,
                                   bool allowSafeRedirects = false);
    static QString networkReplyError(const QNetworkReply *reply);
    void watchReply(QNetworkReply *reply, int timeoutMs = kGenerationTimeoutMs,
                    qsizetype maxBytes = kMaxNetworkResponseBytes);

    QNetworkAccessManager *m_nam = nullptr;
    QProcess *m_process = nullptr;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    bool m_busy = false;
    QString m_activePrompt;
};
