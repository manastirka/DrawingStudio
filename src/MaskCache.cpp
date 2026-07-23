#include "MaskCache.h"
#include <QStandardPaths>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QDirIterator>

#include <algorithm>
#include <cmath>

MaskCache* MaskCache::s_instance = nullptr;

namespace {
constexpr int kMaskCacheVersion = 3;
constexpr qint64 kMaxCacheEntryBytes = 128LL * 1024 * 1024;
constexpr qsizetype kMaxMasksPerEntry = 4096;
constexpr qsizetype kMaxContourPointsPerMask = 1000000;
constexpr int kMaxImageDimension = 100000;

bool isValidHash(const QString &hash)
{
    return hash.size() == 64
        && std::all_of(hash.cbegin(), hash.cend(), [](QChar ch) {
               const ushort value = ch.unicode();
               return (value >= '0' && value <= '9')
                   || (value >= 'a' && value <= 'f');
           });
}

bool isFiniteNumber(const QJsonValue &value)
{
    return value.isDouble() && std::isfinite(value.toDouble());
}

bool discardInvalidEntry(const QString &filePath, const QString &reason)
{
    qWarning() << "MaskCache: Discarding invalid cache entry" << filePath
               << "-" << reason;
    if (!QFile::remove(filePath) && QFile::exists(filePath))
        qWarning() << "MaskCache: Could not remove invalid entry" << filePath;
    return false;
}
}

MaskCache::MaskCache(QObject* parent)
    : QObject(parent)
    , m_maxCacheSize(500 * 1024 * 1024)  // 500 MB default
    , m_enabled(true)
{
    // Set default cache directory
    QString cacheLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    m_cacheDir = QDir(cacheLocation + "/DrawingStudio/MaskCache");
    
    // Create cache directory if it doesn't exist
    if (!m_cacheDir.exists()) {
        m_cacheDir.mkpath(".");
        qDebug() << "MaskCache: Created cache directory at" << m_cacheDir.absolutePath();
    }
    
    qDebug() << "MaskCache: Initialized with directory" << m_cacheDir.absolutePath();
    qDebug() << "MaskCache: Current cache size:" << getCacheSize() << "bytes";
}

MaskCache::~MaskCache()
{
}

MaskCache* MaskCache::instance()
{
    if (!s_instance) {
        s_instance = new MaskCache();
    }
    return s_instance;
}

QString MaskCache::computeImageHash(const QImage& image) const
{
    if (image.isNull()) {
        return QString();
    }
    
    // Compute SHA-256 hash of image data
    QByteArray imageData;
    QBuffer buffer(&imageData);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
        return QString();
    
    QByteArray hash = QCryptographicHash::hash(imageData, QCryptographicHash::Sha256);
    return QString(hash.toHex());
}

QString MaskCache::getCacheFilePath(const QString& hash) const
{
    return m_cacheDir.absoluteFilePath(hash + ".json");
}

bool MaskCache::hasCachedMasks(const QImage& image) const
{
    if (!m_enabled || image.isNull()) {
        return false;
    }
    
    const QString hash = computeImageHash(image);
    if (hash.isEmpty())
        return false;

    CacheEntry entry;
    const bool valid = loadCacheEntry(hash, entry);
    if (valid) {
        qDebug() << "MaskCache: Found cached masks for image hash" << hash.left(16) << "...";
    }

    return valid;
}

std::vector<MaskCache::CachedMask> MaskCache::getCachedMasks(const QImage& image) const
{
    std::vector<CachedMask> masks;
    
    if (!m_enabled || image.isNull()) {
        return masks;
    }
    
    QString hash = computeImageHash(image);
    if (hash.isEmpty())
        return masks;
    CacheEntry entry;
    
    if (loadCacheEntry(hash, entry)) {
        qDebug() << "MaskCache: Loaded" << entry.masks.size() << "cached masks for image" 
                 << image.width() << "x" << image.height();
        
        // loadCacheEntry guarantees positive dimensions before division.
        float scaleX = static_cast<float>(image.width()) / entry.originalWidth;
        float scaleY = static_cast<float>(image.height()) / entry.originalHeight;
        
        if (std::abs(scaleX - 1.0f) > 0.01f || std::abs(scaleY - 1.0f) > 0.01f) {
            qDebug() << "MaskCache: Scaling masks from" << entry.originalWidth << "x" << entry.originalHeight
                     << "to" << image.width() << "x" << image.height();
            
            for (auto& mask : entry.masks) {
                for (auto& point : mask.contour) {
                    point.setX(point.x() * scaleX);
                    point.setY(point.y() * scaleY);
                }
                if (!mask.mask.isNull()) {
                    mask.mask = mask.mask.scaled(image.size(), Qt::IgnoreAspectRatio,
                                                 Qt::SmoothTransformation);
                    for (int y = 0; y < mask.mask.height(); ++y) {
                        uchar *line = mask.mask.scanLine(y);
                        for (int x = 0; x < mask.mask.width(); ++x)
                            line[x] = line[x] >= 128 ? 255 : 0;
                    }
                }
                mask.imageWidth = image.width();
                mask.imageHeight = image.height();
            }
        }
        
        masks = entry.masks;
    }
    
    return masks;
}

void MaskCache::cacheMasks(const QImage& image, const std::vector<CachedMask>& masks)
{
    if (!m_enabled || image.isNull() || masks.empty()) {
        return;
    }
    
    QString hash = computeImageHash(image);
    if (hash.isEmpty())
        return;
    
    CacheEntry entry;
    entry.imageHash = hash;
    entry.timestamp = QDateTime::currentDateTime();
    entry.masks = masks;
    entry.originalWidth = image.width();
    entry.originalHeight = image.height();
    
    if (saveCacheEntry(hash, entry)) {
        qDebug() << "MaskCache: Cached" << masks.size() << "masks for image hash" << hash.left(16) << "...";
        emit cacheEntryAdded(hash);
        
        // Enforce cache size limit
        enforceCacheSizeLimit();
    }
}

bool MaskCache::loadCacheEntry(const QString& hash, CacheEntry& entry) const
{
    const QString filePath = getCacheFilePath(hash);
    if (!isValidHash(hash))
        return discardInvalidEntry(filePath, QStringLiteral("invalid cache key"));

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists())
        return false;
    if (fileInfo.size() <= 0 || fileInfo.size() > kMaxCacheEntryBytes) {
        return discardInvalidEntry(
            filePath, QStringLiteral("entry size is outside the allowed range"));
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray data = file.readAll();
    const QFileDevice::FileError readError = file.error();
    file.close();
    if (readError != QFileDevice::NoError)
        return discardInvalidEntry(filePath, QStringLiteral("file read failed"));

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return discardInvalidEntry(
            filePath, QStringLiteral("invalid JSON: %1").arg(parseError.errorString()));
    }

    const QJsonObject root = doc.object();

    const int version = root["version"].toInt(0);
    if (version != kMaskCacheVersion) {
        return discardInvalidEntry(
            filePath, QStringLiteral("unsupported version %1").arg(version));
    }

    CacheEntry candidate;
    candidate.imageHash = root["imageHash"].toString();
    if (candidate.imageHash != hash)
        return discardInvalidEntry(filePath, QStringLiteral("image hash mismatch"));

    candidate.timestamp =
        QDateTime::fromString(root["timestamp"].toString(), Qt::ISODate);
    if (!candidate.timestamp.isValid())
        return discardInvalidEntry(filePath, QStringLiteral("invalid timestamp"));

    candidate.originalWidth = root["originalWidth"].toInt();
    candidate.originalHeight = root["originalHeight"].toInt();
    if (candidate.originalWidth <= 0 || candidate.originalHeight <= 0
        || candidate.originalWidth > kMaxImageDimension
        || candidate.originalHeight > kMaxImageDimension) {
        return discardInvalidEntry(filePath, QStringLiteral("invalid image dimensions"));
    }

    const QJsonValue masksValue = root["masks"];
    if (!masksValue.isArray())
        return discardInvalidEntry(filePath, QStringLiteral("masks is not an array"));
    const QJsonArray masksArray = masksValue.toArray();
    if (masksArray.isEmpty() || masksArray.size() > kMaxMasksPerEntry) {
        return discardInvalidEntry(filePath, QStringLiteral("invalid mask count"));
    }
    candidate.masks.reserve(static_cast<size_t>(masksArray.size()));

    for (const QJsonValue& maskValue : masksArray) {
        if (!maskValue.isObject())
            return discardInvalidEntry(filePath, QStringLiteral("mask is not an object"));
        const QJsonObject maskObj = maskValue.toObject();
        if (!isFiniteNumber(maskObj["id"])
            || !isFiniteNumber(maskObj["score"])
            || !isFiniteNumber(maskObj["stability"])
            || !isFiniteNumber(maskObj["predicted_iou"])
            || !isFiniteNumber(maskObj["area_percent"])) {
            return discardInvalidEntry(filePath, QStringLiteral("invalid mask metrics"));
        }

        CachedMask mask;
        mask.id = maskObj["id"].toInt();
        mask.score = static_cast<float>(maskObj["score"].toDouble());
        mask.stability = static_cast<float>(maskObj["stability"].toDouble());
        mask.predicted_iou = static_cast<float>(maskObj["predicted_iou"].toDouble());
        mask.area_percent = static_cast<float>(maskObj["area_percent"].toDouble());
        mask.imageWidth = maskObj["imageWidth"].toInt();
        mask.imageHeight = maskObj["imageHeight"].toInt();
        if (mask.imageWidth <= 0 || mask.imageHeight <= 0
            || mask.imageWidth > kMaxImageDimension
            || mask.imageHeight > kMaxImageDimension) {
            return discardInvalidEntry(filePath, QStringLiteral("invalid mask dimensions"));
        }

        const QJsonValue contourValue = maskObj["contour"];
        if (!contourValue.isArray())
            return discardInvalidEntry(filePath, QStringLiteral("contour is not an array"));
        const QJsonArray contourArray = contourValue.toArray();
        if (contourArray.size() > kMaxContourPointsPerMask) {
            return discardInvalidEntry(filePath, QStringLiteral("contour is too large"));
        }
        mask.contour.reserve(static_cast<size_t>(contourArray.size()));
        for (const QJsonValue& pointValue : contourArray) {
            if (!pointValue.isObject())
                return discardInvalidEntry(filePath, QStringLiteral("invalid contour point"));
            const QJsonObject pointObj = pointValue.toObject();
            if (!isFiniteNumber(pointObj["x"]) || !isFiniteNumber(pointObj["y"]))
                return discardInvalidEntry(filePath, QStringLiteral("invalid contour coordinate"));
            mask.contour.push_back(QPointF(pointObj["x"].toDouble(), pointObj["y"].toDouble()));
        }

        if (maskObj.contains("maskPng") && !maskObj["maskPng"].isString())
            return discardInvalidEntry(filePath, QStringLiteral("mask PNG is not a string"));
        const QString maskB64 = maskObj["maskPng"].toString();
        if (!maskB64.isEmpty()) {
            const auto decodedResult = QByteArray::fromBase64Encoding(
                maskB64.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
            if (!decodedResult)
                return discardInvalidEntry(filePath, QStringLiteral("invalid mask PNG encoding"));
            QImage decoded;
            if (!decoded.loadFromData(decodedResult.decoded, "PNG")
                || decoded.width() != mask.imageWidth
                || decoded.height() != mask.imageHeight) {
                return discardInvalidEntry(filePath, QStringLiteral("invalid mask PNG"));
            }
            mask.mask = decoded.convertToFormat(QImage::Format_Grayscale8);
        }

        candidate.masks.push_back(std::move(mask));
    }

    entry = std::move(candidate);
    return true;
}

bool MaskCache::saveCacheEntry(const QString& hash, const CacheEntry& entry)
{
    if (!isValidHash(hash) || entry.imageHash != hash
        || entry.originalWidth <= 0 || entry.originalHeight <= 0
        || entry.originalWidth > kMaxImageDimension
        || entry.originalHeight > kMaxImageDimension
        || entry.masks.empty()
        || entry.masks.size() > static_cast<size_t>(kMaxMasksPerEntry)) {
        qWarning() << "MaskCache: Refusing to save invalid cache metadata";
        return false;
    }

    QJsonObject root;
    root["version"] = kMaskCacheVersion;
    root["imageHash"] = entry.imageHash;
    root["timestamp"] = entry.timestamp.toString(Qt::ISODate);
    root["originalWidth"] = entry.originalWidth;
    root["originalHeight"] = entry.originalHeight;
    
    QJsonArray masksArray;
    for (const auto& mask : entry.masks) {
        if (mask.imageWidth <= 0 || mask.imageHeight <= 0
            || mask.imageWidth > kMaxImageDimension
            || mask.imageHeight > kMaxImageDimension
            || mask.contour.size()
                > static_cast<size_t>(kMaxContourPointsPerMask)
            || !std::isfinite(mask.score) || !std::isfinite(mask.stability)
            || !std::isfinite(mask.predicted_iou)
            || !std::isfinite(mask.area_percent)) {
            qWarning() << "MaskCache: Refusing to save invalid mask metadata";
            return false;
        }
        QJsonObject maskObj;
        maskObj["id"] = mask.id;
        maskObj["score"] = mask.score;
        maskObj["stability"] = mask.stability;
        maskObj["predicted_iou"] = mask.predicted_iou;
        maskObj["area_percent"] = mask.area_percent;
        maskObj["imageWidth"] = mask.imageWidth;
        maskObj["imageHeight"] = mask.imageHeight;
        
        QJsonArray contourArray;
        for (const auto& point : mask.contour) {
            if (!std::isfinite(point.x()) || !std::isfinite(point.y())) {
                qWarning() << "MaskCache: Refusing to save invalid contour point";
                return false;
            }
            QJsonObject pointObj;
            pointObj["x"] = point.x();
            pointObj["y"] = point.y();
            contourArray.append(pointObj);
        }
        maskObj["contour"] = contourArray;

        if (!mask.mask.isNull()) {
            if (mask.mask.width() != mask.imageWidth
                || mask.mask.height() != mask.imageHeight) {
                qWarning() << "MaskCache: Refusing to save mismatched mask dimensions";
                return false;
            }
            QByteArray pngBytes;
            QBuffer buffer(&pngBytes);
            if (!buffer.open(QIODevice::WriteOnly)
                || !mask.mask.save(&buffer, "PNG")) {
                qWarning() << "MaskCache: Failed to encode mask PNG";
                return false;
            }
            maskObj["maskPng"] = QString::fromLatin1(pngBytes.toBase64());
        }
        
        masksArray.append(maskObj);
    }
    root["masks"] = masksArray;
    
    const QByteArray data = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (data.isEmpty() || data.size() > kMaxCacheEntryBytes) {
        qWarning() << "MaskCache: Serialized entry exceeds the per-file limit";
        return false;
    }

    const QString filePath = getCacheFilePath(hash);
    QSaveFile file(filePath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "MaskCache: Failed to open cache file for writing" << filePath;
        return false;
    }

    if (file.write(data) != data.size()) {
        qWarning() << "MaskCache: Failed to write complete cache entry" << filePath;
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        qWarning() << "MaskCache: Failed to atomically commit cache entry" << filePath;
        return false;
    }

    return true;
}

void MaskCache::clearCache()
{
    QStringList filters;
    filters << "*.json";
    
    QFileInfoList files = m_cacheDir.entryInfoList(filters, QDir::Files);
    
    int removedCount = 0;
    for (const QFileInfo& fileInfo : files) {
        if (QFile::remove(fileInfo.absoluteFilePath())) {
            emit cacheEntryRemoved(fileInfo.baseName());
            ++removedCount;
        } else {
            qWarning() << "MaskCache: Failed to remove" << fileInfo.absoluteFilePath();
        }
    }

    qDebug() << "MaskCache: Cleared" << removedCount << "cache entries";
    emit cacheCleared();
}

void MaskCache::clearOldEntries(int daysOld)
{
    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-daysOld);
    
    QStringList filters;
    filters << "*.json";
    
    QFileInfoList files = m_cacheDir.entryInfoList(filters, QDir::Files);
    int removedCount = 0;
    
    for (const QFileInfo& fileInfo : files) {
        CacheEntry entry;
        if (loadCacheEntry(fileInfo.baseName(), entry)) {
            if (entry.timestamp < cutoffDate) {
                if (QFile::remove(fileInfo.absoluteFilePath())) {
                    emit cacheEntryRemoved(fileInfo.baseName());
                    removedCount++;
                } else {
                    qWarning() << "MaskCache: Failed to remove old entry"
                               << fileInfo.absoluteFilePath();
                }
            }
        }
    }
    
    qDebug() << "MaskCache: Removed" << removedCount << "entries older than" << daysOld << "days";
}

qint64 MaskCache::getCacheSize() const
{
    qint64 totalSize = 0;
    
    QStringList filters;
    filters << "*.json";
    
    QFileInfoList files = m_cacheDir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo& fileInfo : files) {
        totalSize += fileInfo.size();
    }
    
    return totalSize;
}

int MaskCache::getCacheEntryCount() const
{
    QStringList filters;
    filters << "*.json";
    
    return m_cacheDir.entryInfoList(filters, QDir::Files).size();
}

void MaskCache::setCacheDirectory(const QString& dir)
{
    if (dir.trimmed().isEmpty()) {
        qWarning() << "MaskCache: Refusing to use an empty cache directory";
        return;
    }

    QDir target(QDir::cleanPath(dir));
    if (!target.exists() && !target.mkpath(".")) {
        qWarning() << "MaskCache: Could not create cache directory" << dir;
        return;
    }
    m_cacheDir = target;

    qDebug() << "MaskCache: Cache directory set to" << m_cacheDir.absolutePath();
}

void MaskCache::setMaxCacheSize(qint64 bytes)
{
    m_maxCacheSize = qMax<qint64>(0, bytes);
    enforceCacheSizeLimit();
}

void MaskCache::enforceCacheSizeLimit()
{
    qint64 currentSize = getCacheSize();
    
    if (currentSize <= m_maxCacheSize) {
        return;
    }
    
    qDebug() << "MaskCache: Cache size" << currentSize << "exceeds limit" << m_maxCacheSize << "- removing old entries";
    
    // Get all cache files sorted by modification time (oldest first)
    QStringList filters;
    filters << "*.json";
    
    QFileInfoList files = m_cacheDir.entryInfoList(filters, QDir::Files, QDir::Time | QDir::Reversed);
    
    // Remove oldest files until we're under the limit
    for (const QFileInfo& fileInfo : files) {
        if (currentSize <= m_maxCacheSize) {
            break;
        }
        
        const qint64 fileSize = fileInfo.size();
        if (QFile::remove(fileInfo.absoluteFilePath())) {
            emit cacheEntryRemoved(fileInfo.baseName());
            currentSize -= fileSize;
            qDebug() << "MaskCache: Removed old cache entry" << fileInfo.fileName()
                     << "(" << fileSize << "bytes)";
        } else {
            qWarning() << "MaskCache: Failed to evict" << fileInfo.absoluteFilePath();
        }
    }
    
    qDebug() << "MaskCache: Cache size after cleanup:" << currentSize << "bytes";
}
