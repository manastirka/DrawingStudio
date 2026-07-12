#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * Static catalog of image-generation capabilities per provider.
 * Used by the generate/edit dialog to populate models, sizes, and options.
 */
struct AIProviderOption {
    QString id;          // openai, stability, nanobanana, higgsfield, remotesd
    QString displayName;
    QStringList models;
    QStringList sizes;           // WxH labels where applicable
    QStringList aspectRatios;    // e.g. 1:1 (Google / some providers)
    QStringList qualities;       // optional quality tiers
    QStringList imageSizes;      // Google Nano Banana: 512, 1K, 2K, 4K
    QString notes;
    bool supportsEdit = true;
};

class AIProviderCatalog
{
public:
    static QVector<AIProviderOption> all();
    static AIProviderOption byId(const QString &id);
    static QStringList providerIds();
};
