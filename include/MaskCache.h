#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointF>
#include <QDir>
#include <QCryptographicHash>
#include <vector>

/**
 * @brief Caches SAM2 mask detection results to avoid re-processing
 * 
 * Stores mask candidates with their contours, scores, and metadata
 * Uses image hash as key for fast lookup
 */
class MaskCache : public QObject
{
    Q_OBJECT
    
public:
    struct CachedMask {
        int id = 0;
        std::vector<QPointF> contour;
        QImage mask; // Full-resolution binary mask when available
        float score = 0.0f;
        float stability = 0.0f;
        float predicted_iou = 0.0f;
        float area_percent = 0.0f;
        int imageWidth = 0;   // Original image dimensions for scaling
        int imageHeight = 0;
    };
    
    struct CacheEntry {
        QString imageHash;
        QDateTime timestamp;
        std::vector<CachedMask> masks;
        int originalWidth = 0;
        int originalHeight = 0;
    };
    
    static MaskCache* instance();
    
    // Cache operations
    bool hasCachedMasks(const QImage& image) const;
    std::vector<CachedMask> getCachedMasks(const QImage& image) const;
    void cacheMasks(const QImage& image, const std::vector<CachedMask>& masks);
    
    // Cache management
    void clearCache();
    void clearOldEntries(int daysOld = 30);
    qint64 getCacheSize() const;
    int getCacheEntryCount() const;
    
    // Settings
    void setCacheDirectory(const QString& dir);
    QString cacheDirectory() const { return m_cacheDir.absolutePath(); }
    
    void setMaxCacheSize(qint64 bytes);
    qint64 maxCacheSize() const { return m_maxCacheSize; }
    
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    
signals:
    void cacheCleared();
    void cacheEntryAdded(const QString& hash);
    void cacheEntryRemoved(const QString& hash);
    
private:
    explicit MaskCache(QObject* parent = nullptr);
    ~MaskCache() override;
    
    QString computeImageHash(const QImage& image) const;
    QString getCacheFilePath(const QString& hash) const;
    
    bool loadCacheEntry(const QString& hash, CacheEntry& entry) const;
    bool saveCacheEntry(const QString& hash, const CacheEntry& entry);
    
    void enforceCacheSizeLimit();
    
    QDir m_cacheDir;
    qint64 m_maxCacheSize;  // Maximum cache size in bytes (default: 500MB)
    bool m_enabled;
    
    static MaskCache* s_instance;
};
