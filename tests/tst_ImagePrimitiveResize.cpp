#include "ImagePrimitive.h"

#include <QBuffer>
#include <QJsonObject>
#include <QtTest>

#include <cmath>

class tst_ImagePrimitiveResize : public QObject {
  Q_OBJECT

private slots:
  void handlesRejectInvalidValues();
  void resizeCannotCrossOppositeEdge();
  void aspectLockedSetSizeUsesChangedDimension();
  void aspectLockedCornerKeepsOppositeCorner();
  void aspectLockRoundTrips();
  void extremeAspectRatioKeepsPositiveDimensions();
  void maskStateRoundTripsAndSettingsAreBounded();
  void missingMaskStateClearsPreviousValues();
  void malformedMaskSelectionIsFiltered();
  void malformedSerializedMaskPayloadIsRejected();
  void invalidSerializedImageIsRejected();
  void oversizedSerializedImageDimensionsAreRejected();
};

namespace {
QImage testImage(int width = 200, int height = 100)
{
  QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::blue);
  return image;
}

void compareFloat(float actual, float expected)
{
  QVERIFY2(std::abs(actual - expected) < 0.001f,
           qPrintable(QStringLiteral("actual=%1 expected=%2")
                          .arg(actual)
                          .arg(expected)));
}

QJsonArray contourJson(float offset)
{
  QJsonArray contour;
  for (const QPointF &point : {QPointF(offset, offset),
                               QPointF(offset + 10.0f, offset),
                               QPointF(offset + 5.0f, offset + 10.0f)}) {
    QJsonObject value;
    value["x"] = point.x();
    value["y"] = point.y();
    contour.append(value);
  }
  return contour;
}

QJsonObject candidateJson(int id, float offset)
{
  QJsonObject candidate;
  candidate["id"] = id;
  candidate["score"] = 0.9;
  candidate["stability"] = 0.8;
  candidate["predicted_iou"] = 0.7;
  candidate["area_percent"] = 25.0;
  candidate["contour"] = contourJson(offset);
  return candidate;
}

QJsonObject imageJsonWithMasks()
{
  ImagePrimitive source(testImage(), QVector2D(), QVector2D(200.0f, 100.0f));
  QJsonObject json = source.toJson();
  QJsonArray candidates;
  candidates.append(candidateJson(10, 0.0f));
  candidates.append(candidateJson(20, 20.0f));
  json["maskCandidates"] = candidates;
  json["maskContour"] = contourJson(20.0f);
  json["selectedMaskIndex"] = 1;
  QJsonArray selected;
  selected.append(0);
  selected.append(1);
  json["selectedMaskIndices"] = selected;
  json["maskInverted"] = true;
  json["contourSmoothness"] = 7;
  json["maskFeather"] = 12;
  json["maskBlur"] = 8;
  json["maskExpand"] = -4;
  json["maskOverlayVisible"] = false;
  return json;
}

void writeBigEndian32(QByteArray &data, qsizetype offset, quint32 value)
{
  data[offset] = static_cast<char>((value >> 24) & 0xff);
  data[offset + 1] = static_cast<char>((value >> 16) & 0xff);
  data[offset + 2] = static_cast<char>((value >> 8) & 0xff);
  data[offset + 3] = static_cast<char>(value & 0xff);
}

quint32 pngCrc32(const QByteArray &data)
{
  quint32 crc = 0xffffffffU;
  for (unsigned char byte : data) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return crc ^ 0xffffffffU;
}

QByteArray pngWithDeclaredSize(quint32 width, quint32 height)
{
  QByteArray png;
  QBuffer buffer(&png);
  buffer.open(QIODevice::WriteOnly);
  testImage(1, 1).save(&buffer, "PNG");
  Q_ASSERT(png.size() >= 33);
  Q_ASSERT(png.mid(12, 4) == QByteArray("IHDR"));

  writeBigEndian32(png, 16, width);
  writeBigEndian32(png, 20, height);
  writeBigEndian32(png, 29, pngCrc32(png.mid(12, 17)));
  return png;
}
}

void tst_ImagePrimitiveResize::handlesRejectInvalidValues()
{
  ImagePrimitive primitive(testImage(), QVector2D(10.0f, 20.0f),
                           QVector2D(100.0f, 50.0f));

  QCOMPARE(primitive.getControlPoints().size(),
           static_cast<size_t>(ImagePrimitive::ResizeHandleCount));
  QCOMPARE(primitive.getHandlePosition(ImagePrimitive::BottomRight),
           QVector2D(110.0f, 70.0f));
  QCOMPARE(primitive.getResizeHandleAt(QVector2D(110.0f, 70.0f), 0.1f),
           ImagePrimitive::BottomRight);
  QCOMPARE(primitive.getResizeHandleAt(QVector2D(500.0f, 500.0f), 0.1f),
           ImagePrimitive::None);
  QVERIFY(primitive.getResizeHandleRect(ImagePrimitive::None).isEmpty());

  const QVector2D originalPosition = primitive.position();
  const QVector2D originalSize = primitive.size();
  primitive.resizeFromHandle(ImagePrimitive::None, QVector2D(0.0f, 0.0f));
  QCOMPARE(primitive.position(), originalPosition);
  QCOMPARE(primitive.size(), originalSize);
}

void tst_ImagePrimitiveResize::resizeCannotCrossOppositeEdge()
{
  ImagePrimitive topLeft(testImage(), QVector2D(10.0f, 20.0f),
                         QVector2D(100.0f, 50.0f));
  topLeft.resizeFromHandle(ImagePrimitive::TopLeft,
                           QVector2D(200.0f, 200.0f));
  QCOMPARE(topLeft.position(), QVector2D(109.0f, 69.0f));
  QCOMPARE(topLeft.size(), QVector2D(1.0f, 1.0f));

  ImagePrimitive bottomRight(testImage(), QVector2D(10.0f, 20.0f),
                             QVector2D(100.0f, 50.0f));
  bottomRight.resizeFromHandle(ImagePrimitive::BottomRight,
                               QVector2D(-100.0f, -100.0f));
  QCOMPARE(bottomRight.position(), QVector2D(10.0f, 20.0f));
  QCOMPARE(bottomRight.size(), QVector2D(1.0f, 1.0f));
}

void tst_ImagePrimitiveResize::aspectLockedSetSizeUsesChangedDimension()
{
  ImagePrimitive primitive(testImage(), QVector2D(),
                           QVector2D(200.0f, 100.0f));
  primitive.setMaintainAspectRatio(true);

  primitive.setSize(QVector2D(300.0f, 100.0f));
  QCOMPARE(primitive.size(), QVector2D(300.0f, 150.0f));

  primitive.setSize(QVector2D(300.0f, 200.0f));
  QCOMPARE(primitive.size(), QVector2D(400.0f, 200.0f));

  primitive.setSize(QVector2D(-5.0f, -2.0f));
  QCOMPARE(primitive.size(), QVector2D(2.0f, 1.0f));
}

void tst_ImagePrimitiveResize::aspectLockedCornerKeepsOppositeCorner()
{
  ImagePrimitive primitive(testImage(), QVector2D(10.0f, 20.0f),
                           QVector2D(200.0f, 100.0f));
  primitive.setMaintainAspectRatio(true);
  primitive.resizeFromHandle(ImagePrimitive::TopLeft,
                             QVector2D(60.0f, 40.0f));

  QCOMPARE(primitive.position(), QVector2D(60.0f, 45.0f));
  QCOMPARE(primitive.size(), QVector2D(150.0f, 75.0f));
  compareFloat(primitive.position().x() + primitive.size().x(), 210.0f);
  compareFloat(primitive.position().y() + primitive.size().y(), 120.0f);
}

void tst_ImagePrimitiveResize::aspectLockRoundTrips()
{
  ImagePrimitive source(testImage(), QVector2D(4.0f, 8.0f),
                        QVector2D(300.0f, 150.0f));
  source.setMaintainAspectRatio(true);

  ImagePrimitive restored;
  restored.fromJson(source.toJson());
  QVERIFY(restored.maintainAspectRatio());
  QCOMPARE(restored.position(), source.position());
  QCOMPARE(restored.size(), source.size());

  QJsonObject invalid = source.toJson();
  invalid["sizeX"] = -10.0;
  invalid["sizeY"] = 0.0;
  restored.fromJson(invalid);
  QCOMPARE(restored.size(), QVector2D(2.0f, 1.0f));
}

void tst_ImagePrimitiveResize::extremeAspectRatioKeepsPositiveDimensions()
{
  ImagePrimitive primitive(testImage(1, 2000), QVector2D(),
                           QVector2D(1.0f, 2000.0f));
  primitive.setMaintainAspectRatio(true);
  primitive.setSize(QVector2D(1.0f, 1.0f));

  QVERIFY(primitive.size().x() >= 1.0f);
  QVERIFY(primitive.size().y() >= 1.0f);
  compareFloat(primitive.size().x() / primitive.size().y(), 1.0f / 2000.0f);
}

void tst_ImagePrimitiveResize::maskStateRoundTripsAndSettingsAreBounded()
{
  ImagePrimitive source;
  source.fromJson(imageJsonWithMasks());
  QCOMPARE(source.getMaskCandidateCount(), 2);
  QCOMPARE(source.getSelectedMaskIndex(), 1);
  QCOMPARE(source.selectedMaskIndices(), std::vector<int>({0, 1}));
  QVERIFY(source.isMaskInverted());
  QCOMPARE(source.getContourSmoothness(), 7);
  QCOMPARE(source.getMaskFeather(), 12);
  QCOMPARE(source.getMaskBlur(), 8);
  QCOMPARE(source.getMaskExpand(), -4);
  QVERIFY(!source.isMaskOverlayVisible());

  ImagePrimitive restored;
  restored.fromJson(source.toJson());
  QCOMPARE(restored.getMaskCandidateCount(), 2);
  QCOMPARE(restored.getSelectedMaskIndex(), 1);
  QCOMPARE(restored.selectedMaskIndices(), std::vector<int>({0, 1}));
  QVERIFY(restored.isMaskInverted());
  QCOMPARE(restored.getContourSmoothness(), 7);
  QCOMPARE(restored.getMaskFeather(), 12);
  QCOMPARE(restored.getMaskBlur(), 8);
  QCOMPARE(restored.getMaskExpand(), -4);
  QVERIFY(!restored.isMaskOverlayVisible());

  restored.setContourSmoothness(1000);
  restored.setMaskFeather(-10);
  restored.setMaskBlur(1000);
  restored.setMaskExpand(-1000);
  QCOMPARE(restored.getContourSmoothness(), 32);
  QCOMPARE(restored.getMaskFeather(), 0);
  QCOMPARE(restored.getMaskBlur(), 20);
  QCOMPARE(restored.getMaskExpand(), -20);
}

void tst_ImagePrimitiveResize::missingMaskStateClearsPreviousValues()
{
  ImagePrimitive primitive;
  primitive.fromJson(imageJsonWithMasks());
  QCOMPARE(primitive.getMaskCandidateCount(), 2);
  QVERIFY(!primitive.getEditableContour().empty());

  ImagePrimitive cleanSource(testImage(), QVector2D(),
                             QVector2D(200.0f, 100.0f));
  primitive.fromJson(cleanSource.toJson());

  QCOMPARE(primitive.getMaskCandidateCount(), 0);
  QCOMPARE(primitive.getSelectedMaskIndex(), -1);
  QVERIFY(primitive.selectedMaskIndices().empty());
  QVERIFY(primitive.getEditableContour().empty());
  QVERIFY(!primitive.isMaskInverted());
  QCOMPARE(primitive.getContourSmoothness(), 0);
  QCOMPARE(primitive.getMaskFeather(), 0);
  QCOMPARE(primitive.getMaskBlur(), 0);
  QCOMPARE(primitive.getMaskExpand(), 0);
  QVERIFY(primitive.isMaskOverlayVisible());
}

void tst_ImagePrimitiveResize::malformedMaskSelectionIsFiltered()
{
  QJsonObject json = imageJsonWithMasks();
  json.remove("maskContour");
  json["selectedMaskIndex"] = 99;
  QJsonArray selected;
  selected.append(1);
  selected.append(1);
  selected.append(-1);
  selected.append(99);
  selected.append(QStringLiteral("invalid"));
  json["selectedMaskIndices"] = selected;

  ImagePrimitive primitive;
  primitive.fromJson(json);
  QCOMPARE(primitive.getSelectedMaskIndex(), 0);
  QCOMPARE(primitive.selectedMaskIndices(), std::vector<int>({1}));
  QCOMPARE(primitive.getEditableContour().size(), static_cast<size_t>(3));
  QCOMPARE(primitive.getEditableContour().front(), QPointF(0.0f, 0.0f));
}

void tst_ImagePrimitiveResize::malformedSerializedMaskPayloadIsRejected()
{
  QJsonObject json = imageJsonWithMasks();
  QCOMPARE(DrawingPrimitive::serializedPointCount(json), qsizetype(9));
  QVERIFY(DrawingPrimitive::createFromJson(json));

  QJsonArray candidates = json["maskCandidates"].toArray();
  QJsonObject candidate = candidates[0].toObject();
  candidate["score"] = QStringLiteral("high");
  candidates[0] = candidate;
  json["maskCandidates"] = candidates;
  QVERIFY(!DrawingPrimitive::createFromJson(json));

  json = imageJsonWithMasks();
  QJsonArray contour = json["maskContour"].toArray();
  QJsonObject point = contour[0].toObject();
  point["x"] = QStringLiteral("left");
  contour[0] = point;
  json["maskContour"] = contour;
  QVERIFY(!DrawingPrimitive::createFromJson(json));

  json = imageJsonWithMasks();
  QJsonArray selected;
  selected.append(99);
  json["selectedMaskIndices"] = selected;
  QVERIFY(!DrawingPrimitive::createFromJson(json));

  QJsonObject validPoint;
  validPoint["x"] = 0.0;
  validPoint["y"] = 0.0;
  QJsonArray oversizedContour;
  for (qsizetype i = 0;
       i <= ImagePrimitive::kMaxSerializedMaskPoints; ++i) {
    oversizedContour.append(validPoint);
  }
  json = imageJsonWithMasks();
  json["maskContour"] = oversizedContour;
  QVERIFY(!DrawingPrimitive::createFromJson(json));
}

void tst_ImagePrimitiveResize::invalidSerializedImageIsRejected()
{
  ImagePrimitive source(testImage(), QVector2D(), QVector2D(200.0f, 100.0f));
  QJsonObject json = source.toJson();

  auto valid = DrawingPrimitive::createFromJson(json);
  auto *validImage = dynamic_cast<ImagePrimitive *>(valid.get());
  QVERIFY(validImage);
  QCOMPARE(validImage->image().size(), QSize(200, 100));

  json["imageData"] = QStringLiteral("not-valid-base64@@");

  source.fromJson(json);
  QVERIFY(source.image().isNull());
  QVERIFY(!DrawingPrimitive::createFromJson(json));
}

void tst_ImagePrimitiveResize::oversizedSerializedImageDimensionsAreRejected()
{
  ImagePrimitive source(testImage(), QVector2D(), QVector2D(200.0f, 100.0f));
  QJsonObject json = source.toJson();
  json["imageData"] =
      QString::fromLatin1(pngWithDeclaredSize(20000, 20000).toBase64());

  source.fromJson(json);
  QVERIFY(source.image().isNull());
  QVERIFY(!DrawingPrimitive::createFromJson(json));
}

QTEST_MAIN(tst_ImagePrimitiveResize)
#include "tst_ImagePrimitiveResize.moc"
