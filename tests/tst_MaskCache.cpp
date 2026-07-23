#include "MaskCache.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

class tst_MaskCache : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void roundTripPreservesMaskData();
    void corruptEntryIsNotReportedAsHit();
    void invalidDimensionsAreDiscarded();
    void invalidMaskIsNotSaved();

private:
    QImage sampleImage() const;
    MaskCache::CachedMask sampleMask() const;
    QString onlyCacheFile() const;
    void writeJson(const QString &path, const QJsonObject &root);

    std::unique_ptr<QTemporaryDir> m_tempDir;
    MaskCache *m_cache = nullptr;
};

void tst_MaskCache::init()
{
    m_tempDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_tempDir->isValid());
    m_cache = MaskCache::instance();
    QVERIFY(m_cache);
    m_cache->setEnabled(true);
    m_cache->setMaxCacheSize(500LL * 1024 * 1024);
    m_cache->setCacheDirectory(m_tempDir->path());
    m_cache->clearCache();
}

void tst_MaskCache::cleanup()
{
    if (m_cache) {
        m_cache->clearCache();
        m_cache->setMaxCacheSize(500LL * 1024 * 1024);
    }
    m_tempDir.reset();
}

QImage tst_MaskCache::sampleImage() const
{
    QImage image(4, 3, QImage::Format_ARGB32);
    image.fill(QColor(24, 80, 160));
    image.setPixelColor(0, 0, QColor(220, 40, 30));
    return image;
}

MaskCache::CachedMask tst_MaskCache::sampleMask() const
{
    MaskCache::CachedMask mask;
    mask.id = 7;
    mask.contour = {QPointF(0.0, 0.0), QPointF(3.0, 0.0),
                    QPointF(3.0, 2.0), QPointF(0.0, 2.0)};
    mask.score = 0.91f;
    mask.stability = 0.87f;
    mask.predicted_iou = 0.83f;
    mask.area_percent = 42.5f;
    mask.imageWidth = 4;
    mask.imageHeight = 3;
    mask.mask = QImage(4, 3, QImage::Format_Grayscale8);
    mask.mask.fill(255);
    mask.mask.setPixelColor(0, 0, QColor(0, 0, 0));
    return mask;
}

QString tst_MaskCache::onlyCacheFile() const
{
    const QFileInfoList files =
        QDir(m_tempDir->path()).entryInfoList({QStringLiteral("*.json")}, QDir::Files);
    return files.size() == 1 ? files.first().absoluteFilePath() : QString();
}

void tst_MaskCache::writeJson(const QString &path, const QJsonObject &root)
{
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    QSaveFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), bytes.size());
    QVERIFY(file.commit());
}

void tst_MaskCache::roundTripPreservesMaskData()
{
    const QImage image = sampleImage();
    const MaskCache::CachedMask expected = sampleMask();
    QSignalSpy addedSpy(m_cache, &MaskCache::cacheEntryAdded);

    m_cache->cacheMasks(image, {expected});

    QCOMPARE(addedSpy.count(), 1);
    QCOMPARE(m_cache->getCacheEntryCount(), 1);
    QVERIFY(m_cache->hasCachedMasks(image));
    const auto loaded = m_cache->getCachedMasks(image);
    QCOMPARE(loaded.size(), size_t(1));
    QCOMPARE(loaded[0].id, expected.id);
    QCOMPARE(loaded[0].contour.size(), expected.contour.size());
    QCOMPARE(loaded[0].imageWidth, image.width());
    QCOMPARE(loaded[0].imageHeight, image.height());
    QCOMPARE(loaded[0].mask.size(), image.size());
    QCOMPARE(loaded[0].mask.format(), QImage::Format_Grayscale8);
    QCOMPARE(loaded[0].mask.pixelColor(0, 0).red(), 0);
    QCOMPARE(loaded[0].mask.pixelColor(1, 1).red(), 255);
    QVERIFY(!onlyCacheFile().isEmpty());
}

void tst_MaskCache::corruptEntryIsNotReportedAsHit()
{
    const QImage image = sampleImage();
    m_cache->cacheMasks(image, {sampleMask()});
    const QString path = onlyCacheFile();
    QVERIFY(!path.isEmpty());

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write("{truncated"), qint64(10));
    file.close();

    QVERIFY(!m_cache->hasCachedMasks(image));
    QVERIFY(m_cache->getCachedMasks(image).empty());
    QVERIFY(!QFile::exists(path));
    QCOMPARE(m_cache->getCacheEntryCount(), 0);
}

void tst_MaskCache::invalidDimensionsAreDiscarded()
{
    const QImage image = sampleImage();
    m_cache->cacheMasks(image, {sampleMask()});
    const QString path = onlyCacheFile();
    QVERIFY(!path.isEmpty());

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    root[QStringLiteral("originalWidth")] = 0;
    writeJson(path, root);

    QVERIFY(m_cache->getCachedMasks(image).empty());
    QVERIFY(!QFile::exists(path));
}

void tst_MaskCache::invalidMaskIsNotSaved()
{
    MaskCache::CachedMask invalid = sampleMask();
    invalid.imageWidth = 99;
    QSignalSpy addedSpy(m_cache, &MaskCache::cacheEntryAdded);

    m_cache->cacheMasks(sampleImage(), {invalid});

    QCOMPARE(addedSpy.count(), 0);
    QCOMPARE(m_cache->getCacheEntryCount(), 0);
}

QTEST_MAIN(tst_MaskCache)
#include "tst_MaskCache.moc"
