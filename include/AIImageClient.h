#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QVector>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QTemporaryDir>
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

    explicit AIImageClient(QObject *parent = nullptr);
    ~AIImageClient() override;

    bool isBusy() const { return m_busy; }
    void cancel();

    void generate(const Request &request);
    void edit(const Request &request);
    void testConnection(const QString &provider);

signals:
    void started(const QString &message);
    void progress(const QString &message);
    void finished(const QImage &image, const QString &prompt);
    void failed(const QString &error);
    void connectionTestFinished(bool ok, const QString &message);

private:
    void runRequest(Request request, bool isEdit);
    void runOpenAI(const Request &req, bool isEdit);
    void runStability(const Request &req, bool isEdit);
    void runNanoBananaGoogle(const Request &req, bool isEdit);
    void postNanoBananaGenerate(Request req, bool isEdit, int attempt);
    void runHiggsfieldCli(const Request &req, const QString &model);
    void runRemoteSd(const Request &req);
    void downloadImageUrl(const QUrl &url, const QString &prompt);
    void finishWithImage(const QImage &image, const QString &prompt);
    void failWith(const QString &error);

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
    void watchReply(QNetworkReply *reply, int timeoutMs = 180000);

    QNetworkAccessManager *m_nam = nullptr;
    QProcess *m_process = nullptr;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    bool m_busy = false;
    QString m_activePrompt;
};
