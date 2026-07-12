#include "MaskCache.h"
#include <QStandardPaths>
#include <QFile>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QDirIterator>

MaskCache* MaskCache::s_instance = nullptr;

namespace {
constexpr int kMaskCacheVersion = 3;
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
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    
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
    
    QString hash = computeImageHash(image);
    QString filePath = getCacheFilePath(hash);
    
    bool exists = QFile::exists(filePath);
    if (exists) {
        qDebug() << "MaskCache: Found cached masks for image hash" << hash.left(16) << "...";
    }
    
    return exists;
}

std::vector<MaskCache::CachedMask> MaskCache::getCachedMasks(const QImage& image) const
{
    std::vector<CachedMask> masks;
    
    if (!m_enabled || image.isNull()) {
        return masks;
    }
    
    QString hash = computeImageHash(image);
    CacheEntry entry;
    
    if (loadCacheEntry(hash, entry)) {
        qDebug() << "MaskCache: Loaded" << entry.masks.size() << "cached masks for image" 
                 << image.width() << "x" << image.height();
        
        // Scale masks if image dimensions changed
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
    QString filePath = getCacheFilePath(hash);
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "MaskCache: Invalid cache file format" << filePath;
        return false;
    }
    
    QJsonObject root = doc.object();

    const int version = root["version"].toInt(0);
    if (version != kMaskCacheVersion) {
        qWarning() << "MaskCache: Outdated cache entry version" << version
                   << "(expected" << kMaskCacheVersion << ") for" << filePath;
        return false;
    }
    
    entry.imageHash = root["imageHash"].toString();
    entry.timestamp = QDateTime::fromString(root["timestamp"].toString(), Qt::ISODate);
    entry.originalWidth = root["originalWidth"].toInt();
    entry.originalHeight = root["originalHeight"].toInt();
    
    QJsonArray masksArray = root["masks"].toArray();
    entry.masks.clear();
    
    for (const QJsonValue& maskValue : masksArray) {
        QJsonObject maskObj = maskValue.toObject();
        
        CachedMask mask;
        mask.id = maskObj["id"].toInt();
        mask.score = maskObj["score"].toDouble();
        mask.stability = maskObj["stability"].toDouble();
        mask.predicted_iou = maskObj["predicted_iou"].toDouble();
        mask.area_percent = maskObj["area_percent"].toDouble();
        mask.imageWidth = maskObj["imageWidth"].toInt();
        mask.imageHeight = maskObj["imageHeight"].toInt();
        
        QJsonArray contourArray = maskObj["contour"].toArray();
        for (const QJsonValue& pointValue : contourArray) {
            QJsonObject pointObj = pointValue.toObject();
            mask.contour.push_back(QPointF(pointObj["x"].toDouble(), pointObj["y"].toDouble()));
        }

        const QString maskB64 = maskObj["maskPng"].toString();
        if (!maskB64.isEmpty()) {
            QByteArray bytes = QByteArray::fromBase64(maskB64.toLatin1());
            QImage decoded;
            if (decoded.loadFromData(bytes, "PNG")) {
                mask.mask = decoded.convertToFormat(QImage::Format_Grayscale8);
            }
        }
        
        entry.masks.push_back(mask);
    }
    
    return true;
}

bool MaskCache::saveCacheEntry(const QString& hash, const CacheEntry& entry)
{
    QJsonObject root;
    root["version"] = kMaskCacheVersion;
    root["imageHash"] = entry.imageHash;
    root["timestamp"] = entry.timestamp.toString(Qt::ISODate);
    root["originalWidth"] = entry.originalWidth;
    root["originalHeight"] = entry.originalHeight;
    
    QJsonArray masksArray;
    for (const auto& mask : entry.masks) {
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
            QJsonObject pointObj;
            pointObj["x"] = point.x();
            pointObj["y"] = point.y();
            contourArray.append(pointObj);
        }
        maskObj["contour"] = contourArray;

        if (!mask.mask.isNull()) {
            QByteArray pngBytes;
            QBuffer buffer(&pngBytes);
            buffer.open(QIODevice::WriteOnly);
            mask.mask.save(&buffer, "PNG");
            maskObj["maskPng"] = QString::fromLatin1(pngBytes.toBase64());
        }
        
        masksArray.append(maskObj);
    }
    root["masks"] = masksArray;
    
    QJsonDocument doc(root);
    
    QString filePath = getCacheFilePath(hash);
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "MaskCache: Failed to open cache file for writing" << filePath;
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Compact));
    file.close();
    
    return true;
}

void MaskCache::clearCache()
{
    QStringList filters;
    filters << "*.json";
    
    QFileInfoList files = m_cacheDir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo& fileInfo : files) {
        QFile::remove(fileInfo.absoluteFilePath());
        emit cacheEntryRemoved(fileInfo.baseName());
    }
    
    qDebug() << "MaskCache: Cleared" << files.size() << "cache entries";
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
                QFile::remove(fileInfo.absoluteFilePath());
                emit cacheEntryRemoved(fileInfo.baseName());
                removedCount++;
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
    m_cacheDir = QDir(dir);
    
    if (!m_cacheDir.exists()) {
        m_cacheDir.mkpath(".");
    }
    
    qDebug() << "MaskCache: Cache directory set to" << m_cacheDir.absolutePath();
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
        
        qint64 fileSize = fileInfo.size();
        QFile::remove(fileInfo.absoluteFilePath());
        emit cacheEntryRemoved(fileInfo.baseName());
        
        currentSize -= fileSize;
        qDebug() << "MaskCache: Removed old cache entry" << fileInfo.fileName() << "(" << fileSize << "bytes)";
    }
    
    qDebug() << "MaskCache: Cache size after cleanup:" << currentSize << "bytes";
}
