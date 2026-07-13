#include "TextRenderer.h"

#include "DrawingPrimitive.h"
#include "Layer.h"
#include "LayerManager.h"

#include <QBrush>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStringList>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace TextRenderer {

void drawFormatted(QPainter *painter, const TextPrimitive *textPrim,
                    const QPoint &pos, const QFont &font, float zoomLevel,
                    bool showBox) {
  if (!textPrim || !painter)
    return;

  // Always draw the real text color so Format / ATE edits are visible
  // while selected. Selection is indicated by handles, not recoloring.
  if (!textPrim->gradientEnabled()) {
    painter->setPen(textPrim->color());
  }

  QString text = textPrim->text();

  QFont effectiveFont(font);
  effectiveFont.setUnderline(textPrim->isUnderline());

  constexpr qreal kSubSuperscriptScale = 0.7;
  const auto baselineShift = textPrim->baselineShift();
  if (baselineShift != TextPrimitive::BaselineShift::Normal) {
    if (effectiveFont.pointSizeF() > 0) {
      effectiveFont.setPointSizeF(
          std::max(1.0, effectiveFont.pointSizeF() * kSubSuperscriptScale));
    } else if (effectiveFont.pixelSize() > 0) {
      int scaled = std::max(1, static_cast<int>(effectiveFont.pixelSize() *
                                                kSubSuperscriptScale));
      effectiveFont.setPixelSize(scaled);
    }
  }

  QFontMetrics fm(effectiveFont);
  QFont previousFont = painter->font();
  painter->setFont(effectiveFont);

  int baselineOffset = 0;
  if (baselineShift == TextPrimitive::BaselineShift::Subscript) {
    baselineOffset = static_cast<int>(fm.height() * 0.25f);
  } else if (baselineShift == TextPrimitive::BaselineShift::Superscript) {
    baselineOffset = static_cast<int>(-fm.height() * 0.35f);
  }

  float letterSpacing = textPrim->letterSpacing() * zoomLevel;
  float lineSpacing = textPrim->lineSpacing();
  int lineHeight = fm.height() * lineSpacing;

  // Get text box dimensions in screen space
  float boxWidth = textPrim->textBoxWidth() * zoomLevel;
  float boxHeight = textPrim->textBoxHeight() * zoomLevel;

  // If no box dimensions, calculate actual text bounds for gradient
  // Check world-space dimensions (not screen-space) to avoid zoom-related
  // issues
  bool hasBox = (textPrim->textBoxWidth() > 0 && textPrim->textBoxHeight() > 0);
  float actualTextWidth = 0;
  float actualTextHeight = 0;

  if (!hasBox) {
    // Calculate actual text dimensions for gradient purposes
    QStringList lines = text.split('\n');
    for (const QString &line : lines) {
      actualTextWidth =
          qMax(actualTextWidth, (float)fm.horizontalAdvance(line));
    }
    actualTextHeight = fm.height() * lines.size() * textPrim->lineSpacing() +
                       std::abs(baselineOffset);

    // Use large values for wrapping, but keep actual dimensions for gradient
    boxWidth = 10000; // Very large for no wrapping
    boxHeight = 10000;
  } else {
    actualTextWidth = boxWidth;
    actualTextHeight = boxHeight;

    // IMPORTANT: Set clipping region to prevent text from rendering outside box
    painter->save();
    painter->setClipRect(QRectF(pos.x(), pos.y(), boxWidth, boxHeight));
  }

  // Word wrap text to fit within box width
  QStringList wrappedLines;
  QStringList paragraphs = text.split('\n');

  for (const QString &paragraph : paragraphs) {
    if (paragraph.isEmpty()) {
      wrappedLines.append("");
      continue;
    }

    QStringList words = paragraph.split(' ', Qt::SkipEmptyParts);
    QString currentLine;

    for (const QString &word : words) {
      QString testLine =
          currentLine.isEmpty() ? word : currentLine + " " + word;

      // Calculate width with letter spacing
      int testWidth = 0;
      for (int i = 0; i < testLine.length(); ++i) {
        testWidth += fm.horizontalAdvance(testLine[i]);
        if (i < testLine.length() - 1) {
          testWidth += letterSpacing;
        }
      }

      if (testWidth <= boxWidth) {
        currentLine = testLine;
      } else {
        if (!currentLine.isEmpty()) {
          wrappedLines.append(currentLine);
        }
        currentLine = word;
      }
    }

    if (!currentLine.isEmpty()) {
      wrappedLines.append(currentLine);
    }
  }

  // Alignment must be relative to the text area:
  // - explicit text box → use box width
  // - auto-sized text → use the widest content line (not the wrap sentinel)
  float maxContentWidth = 0.0f;
  for (const QString &line : wrappedLines) {
    if (line.isEmpty()) {
      continue;
    }
    float lineWidth = 0.0f;
    for (int i = 0; i < line.length(); ++i) {
      lineWidth += fm.horizontalAdvance(line[i]);
      if (i < line.length() - 1) {
        lineWidth += letterSpacing;
      }
    }
    maxContentWidth = qMax(maxContentWidth, lineWidth);
  }
  const float alignWidth = hasBox ? boxWidth : maxContentWidth;

  // Draw wrapped lines
  int y = pos.y() + fm.ascent() + baselineOffset;

  for (int lineIdx = 0; lineIdx < wrappedLines.size(); ++lineIdx) {
    const QString &line = wrappedLines[lineIdx];

    // Check if we're exceeding box height
    if (y - pos.y() > boxHeight)
      break;

    if (line.isEmpty()) {
      y += lineHeight;
      continue;
    }

    // Calculate line width with letter spacing
    int lineWidth = 0;
    for (int i = 0; i < line.length(); ++i) {
      lineWidth += fm.horizontalAdvance(line[i]);
      if (i < line.length() - 1) {
        lineWidth += letterSpacing;
      }
    }

    // Calculate x position based on alignment within the text area
    int x = pos.x();
    switch (textPrim->alignment()) {
    case TextPrimitive::TextAlignment::Left:
      break;
    case TextPrimitive::TextAlignment::Center:
      x = pos.x() + static_cast<int>((alignWidth - lineWidth) / 2);
      break;
    case TextPrimitive::TextAlignment::Right:
      x = pos.x() + static_cast<int>(alignWidth - lineWidth);
      break;
    case TextPrimitive::TextAlignment::Justify:
      // Justify only if not the last line
      if (lineIdx < wrappedLines.size() - 1 && line.contains(' ')) {
        int totalCharWidth = 0;
        for (QChar ch : line) {
          totalCharWidth += fm.horizontalAdvance(ch);
        }

        int spaceCount = line.count(' ');
        if (spaceCount > 0) {
          float extraSpacing =
              (alignWidth - totalCharWidth) / (float)(line.length() - 1);

          int currentX = x;
          for (QChar ch : line) {
            QString charStr(ch);

            // Draw shadow only when explicitly enabled
            if (textPrim->shadowEnabled()) {
              QPoint shadowOffset(textPrim->shadowOffsetX() * zoomLevel,
                                  textPrim->shadowOffsetY() * zoomLevel);
              float blur = textPrim->shadowBlur() * zoomLevel;
              QColor shadowColor = textPrim->shadowColor();

              painter->save();

              // Ensure shadow color has proper alpha
              if (shadowColor.alpha() == 0) {
                shadowColor.setAlpha(
                    180); // Default to ~70% opacity if fully transparent
              }

              // Apply blur effect by drawing multiple passes with reduced
              // opacity
              if (blur > 0.5f) {
                int blurPasses = qMin(15, qMax(3, (int)(blur / 1.5f)));
                float baseAlpha = shadowColor.alphaF();

                for (int i = 0; i < blurPasses; ++i) {
                  // Gaussian-like distribution for better blur
                  float t = (float)i / (float)(blurPasses - 1);
                  float gaussianWeight = expf(-2.5f * t * t);
                  float alpha =
                      baseAlpha * gaussianWeight / (float)blurPasses * 3.0f;

                  QColor blurColor = shadowColor;
                  blurColor.setAlphaF(qMax(0.05f, qMin(1.0f, alpha)));
                  painter->setPen(blurColor);

                  float spread = t * blur;
                  QPoint blurOffset = shadowOffset + QPoint(spread, spread);
                  painter->drawText(currentX + blurOffset.x(),
                                    y + blurOffset.y(), charStr);
                }
              } else {
                // No blur - simple shadow with proper alpha
                painter->setPen(shadowColor);
                painter->drawText(currentX + shadowOffset.x(),
                                  y + shadowOffset.y(), charStr);
              }

              painter->restore();
            }

            // Draw stroke if enabled
            if (textPrim->strokeEnabled()) {
              QPainterPath charPath;
              charPath.addText(currentX, y, effectiveFont, charStr);
              painter->save();
              QPen strokePen(textPrim->strokeColor(),
                             textPrim->strokeWidth() * zoomLevel);
              strokePen.setJoinStyle(Qt::RoundJoin);
              painter->setPen(strokePen);
              painter->setBrush(Qt::NoBrush);
              painter->drawPath(charPath);
              painter->restore();
            }

            // Draw character with gradient or normal color
            if (textPrim->gradientEnabled()) {
              // Create gradient across the actual text dimensions
              float angleRad = textPrim->gradientAngle() * M_PI / 180.0f;

              // Calculate gradient start and end points using actual text size
              QPointF gradStart(pos.x(), pos.y());
              QPointF gradEnd;

              if (textPrim->gradientAngle() == 0) {
                // Horizontal gradient
                gradEnd = QPointF(pos.x() + actualTextWidth, pos.y());
              } else if (textPrim->gradientAngle() == 90) {
                // Vertical gradient
                gradEnd = QPointF(pos.x(), pos.y() + actualTextHeight);
              } else {
                // Angled gradient - use actual text dimensions
                gradEnd = QPointF(pos.x() + cos(angleRad) * actualTextWidth,
                                  pos.y() + sin(angleRad) * actualTextHeight);
              }

              QLinearGradient gradient(gradStart, gradEnd);
              gradient.setColorAt(0, textPrim->gradientStartColor());
              gradient.setColorAt(1, textPrim->gradientEndColor());

              // Use QPainterPath for reliable gradient rendering
              QPainterPath textPath;
              textPath.addText(currentX, y, effectiveFont, charStr);

              painter->save();
              painter->setPen(Qt::NoPen);
              painter->setBrush(QBrush(gradient));
              painter->drawPath(textPath);
              painter->restore();
            } else {
              // Use current painter pen (which has the text color set)
              painter->drawText(currentX, y, charStr);
            }

            currentX += fm.horizontalAdvance(ch) + extraSpacing;
          }
          y += lineHeight;
          continue;
        }
      }
      break;
    }

    // Draw characters with letter spacing, shadow, and stroke
    int currentX = x;
    for (int i = 0; i < line.length(); ++i) {
      QChar ch = line[i];
      QString charStr(ch);

      // Draw shadow only when explicitly enabled
      if (textPrim->shadowEnabled()) {
        QPoint shadowOffset(textPrim->shadowOffsetX() * zoomLevel,
                            textPrim->shadowOffsetY() * zoomLevel);
        float blur = textPrim->shadowBlur() * zoomLevel;
        QColor shadowColor = textPrim->shadowColor();

        painter->save();

        // Ensure shadow color has proper alpha
        if (shadowColor.alpha() == 0) {
          shadowColor.setAlpha(
              180); // Default to ~70% opacity if fully transparent
        }

        // Apply blur effect by drawing multiple passes with reduced opacity
        if (blur > 0.5f) {
          int blurPasses = qMin(15, qMax(3, (int)(blur / 1.5f)));
          float baseAlpha = shadowColor.alphaF();

          for (int i = 0; i < blurPasses; ++i) {
            // Gaussian-like distribution for better blur
            float t = (float)i / (float)(blurPasses - 1);
            float gaussianWeight = expf(-2.5f * t * t);
            float alpha = baseAlpha * gaussianWeight / (float)blurPasses * 3.0f;

            QColor blurColor = shadowColor;
            blurColor.setAlphaF(qMax(0.05f, qMin(1.0f, alpha)));
            painter->setPen(blurColor);

            float spread = t * blur;
            QPoint blurOffset = shadowOffset + QPoint(spread, spread);
            painter->drawText(currentX + blurOffset.x(), y + blurOffset.y(),
                              charStr);
          }
        } else {
          // No blur - simple shadow with proper alpha
          painter->setPen(shadowColor);
          painter->drawText(currentX + shadowOffset.x(), y + shadowOffset.y(),
                            charStr);
        }

        painter->restore();
      }

      // Draw stroke if enabled
      if (textPrim->strokeEnabled()) {
        QPainterPath charPath;
        charPath.addText(currentX, y, effectiveFont, charStr);
        painter->save();
        QPen strokePen(textPrim->strokeColor(),
                       textPrim->strokeWidth() * zoomLevel);
        strokePen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(strokePen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(charPath);
        painter->restore();
      }

      // Draw the character with gradient or normal color
      if (textPrim->gradientEnabled()) {
        // Create gradient across the actual text dimensions
        float angleRad = textPrim->gradientAngle() * M_PI / 180.0f;

        // Calculate gradient start and end points using actual text size
        QPointF gradStart(pos.x(), pos.y());
        QPointF gradEnd;

        if (textPrim->gradientAngle() == 0) {
          // Horizontal gradient
          gradEnd = QPointF(pos.x() + actualTextWidth, pos.y());
        } else if (textPrim->gradientAngle() == 90) {
          // Vertical gradient
          gradEnd = QPointF(pos.x(), pos.y() + actualTextHeight);
        } else {
          // Angled gradient - use actual text dimensions
          gradEnd = QPointF(pos.x() + cos(angleRad) * actualTextWidth,
                            pos.y() + sin(angleRad) * actualTextHeight);
        }

        QLinearGradient gradient(gradStart, gradEnd);
        gradient.setColorAt(0, textPrim->gradientStartColor());
        gradient.setColorAt(1, textPrim->gradientEndColor());

        // Use QPainterPath for reliable gradient rendering
        QPainterPath textPath;
        textPath.addText(currentX, y, effectiveFont, charStr);

        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(gradient));
        painter->drawPath(textPath);
        painter->restore();
      } else {
        // Use current painter pen (which has the text color set)
        painter->drawText(currentX, y, charStr);
      }

      currentX += fm.horizontalAdvance(ch);
      if (i < line.length() - 1) {
        currentX += letterSpacing;
      }
    }

    y += lineHeight;
  }

  // Calculate actual text dimensions for bounding box if no explicit box size
  if (!hasBox) {
    // Update box dimensions to actual text size with small padding
    float padding = 4.0f; // Small padding around text
    boxWidth = maxContentWidth + padding * 2;
    boxHeight = wrappedLines.size() * lineHeight + padding * 2 +
                std::abs(baselineOffset);
  }

  // Draw corner handles for resize/rotate ONLY if showBox is true (when
  // selected)
  if (showBox) {
    painter->save();

    // Draw bounding box outline with constant dash pattern
    QPen boxPen(QColor(100, 149, 237), 1.5f, Qt::CustomDashLine);
    // Set dash pattern with constant pixel sizes
    QVector<qreal> dashPattern;
    dashPattern << 5.0f << 3.0f; // 5 pixel dash, 3 pixel gap
    boxPen.setDashPattern(dashPattern);
    painter->setPen(boxPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(QRectF(pos.x(), pos.y(), boxWidth, boxHeight));

    // Draw all 8 resize handles (4 corners + 4 edges)
    float handleSize = 8.0f; // Constant 8-pixel visual size at all zoom levels
    QColor handleColor(100, 149, 237);
    painter->setPen(QPen(Qt::white, 1.5f)); // Constant pen width
    painter->setBrush(handleColor);

    // All 8 control points: 4 corners + 4 edge midpoints
    QPointF controlPoints[8] = {
        QPointF(pos.x(), pos.y()),                // 0: Top-left corner
        QPointF(pos.x() + boxWidth / 2, pos.y()), // 1: Top edge
        QPointF(pos.x() + boxWidth, pos.y()),     // 2: Top-right corner
        QPointF(pos.x() + boxWidth, pos.y() + boxHeight / 2), // 3: Right edge
        QPointF(pos.x() + boxWidth,
                pos.y() + boxHeight), // 4: Bottom-right corner
        QPointF(pos.x() + boxWidth / 2, pos.y() + boxHeight), // 5: Bottom edge
        QPointF(pos.x(), pos.y() + boxHeight),    // 6: Bottom-left corner
        QPointF(pos.x(), pos.y() + boxHeight / 2) // 7: Left edge
    };

    for (const QPointF &point : controlPoints) {
      // Draw square handles centered on control points
      QRectF handleRect(point.x() - handleSize / 2, point.y() - handleSize / 2,
                        handleSize, handleSize);
      painter->drawRect(handleRect);
    }

    // Draw rotation handle (green circle at top center)
    float rotateHandleY = pos.y() - 20.0f;           // 20 pixels above top edge
    float rotateHandleX = pos.x() + boxWidth / 2.0f; // Center horizontally
    QPointF rotateHandlePos(rotateHandleX, rotateHandleY);

    // Draw line connecting rotation handle to top edge
    painter->setPen(QPen(QColor(100, 149, 237), 1.5f));
    painter->drawLine(QPointF(rotateHandleX, pos.y()), rotateHandlePos);

    // Draw rotation handle as green circle
    painter->setPen(QPen(Qt::white, 1.5f));
    painter->setBrush(QColor(0, 255, 0)); // Green
    QRectF rotateHandleRect(rotateHandlePos.x() - handleSize / 2,
                            rotateHandlePos.y() - handleSize / 2, handleSize,
                            handleSize);
    painter->drawEllipse(rotateHandleRect);

    painter->restore();
  }

  // Restore painter state if clipping was applied
  if (hasBox) {
    painter->restore();
  }

  painter->setFont(previousFont);
}


void drawOnSpline(QPainter &painter, const TextPrimitive *textPrim,
                   const SplinePrimitive *spline, float zoomLevel,
                   const std::function<QPoint(const QVector2D &)> &worldToScreen)
{
  if (!textPrim || !textPrim->followsSpline() || !spline || !worldToScreen)
    return;

  // Get spline points
  const auto &splinePoints = spline->points();
  if (splinePoints.size() < 2)
    return;
  // Generate smooth curve points
  std::vector<QVector2D> curvePoints;
  int segments =
      (splinePoints.size() - 1) * 20; // 20 segments per spline section

  for (int i = 0; i <= segments; ++i) {
    float t = (float)i / (float)segments;
    int segmentIndex = qMin((int)(t * (splinePoints.size() - 1)),
                            (int)splinePoints.size() - 2);
    float localT = t * (splinePoints.size() - 1) - segmentIndex;

    // Catmull-Rom spline interpolation
    QVector2D p0 = splinePoints[qMax(0, segmentIndex - 1)];
    QVector2D p1 = splinePoints[segmentIndex];
    QVector2D p2 =
        splinePoints[qMin(segmentIndex + 1, (int)splinePoints.size() - 1)];
    QVector2D p3 =
        splinePoints[qMin(segmentIndex + 2, (int)splinePoints.size() - 1)];

    float t2 = localT * localT;
    float t3 = t2 * localT;

    QVector2D point = 0.5f * ((2.0f * p1) + (-p0 + p2) * localT +
                              (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                              (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);

    curvePoints.push_back(point);
  }

  if (curvePoints.empty())
    return;

  // Set up font with zoom for rendering
  int scaledFontSize =
      static_cast<int>(textPrim->fontSize() * zoomLevel * textPrim->scale());
  QFont font(textPrim->fontFamily(), scaledFontSize);
  font.setBold(textPrim->isBold());
  font.setItalic(textPrim->isItalic());
  painter.setFont(font);

  QFontMetrics fm(font);
  QString text = textPrim->text();

  // Also need font metrics in world space for spacing calculations
  QFont worldFont(textPrim->fontFamily(),
                  static_cast<int>(textPrim->fontSize() * textPrim->scale()));
  worldFont.setBold(textPrim->isBold());
  worldFont.setItalic(textPrim->isItalic());
  QFontMetrics worldFm(worldFont);

  // Calculate total path length
  float totalLength = 0.0f;
  std::vector<float> segmentLengths;
  for (size_t i = 1; i < curvePoints.size(); ++i) {
    float len = (curvePoints[i] - curvePoints[i - 1]).length();
    segmentLengths.push_back(len);
    totalLength += len;
  }

  // Apply path offset
  float startOffset = textPrim->pathOffset() * totalLength;

  // Draw each character along the path
  float currentDistance = startOffset;

  for (int i = 0; i < text.length(); ++i) {
    QChar ch = text[i];
    int charWidthScreen = fm.horizontalAdvance(ch);
    int charWidthWorld = worldFm.horizontalAdvance(ch);

    // Find position on curve for this character (use world space width)
    float targetDist = currentDistance + charWidthWorld * 0.5f;
    float accumulatedDist = 0.0f;

    QVector2D charPos;
    QVector2D tangent;

    for (size_t j = 0; j < segmentLengths.size(); ++j) {
      if (accumulatedDist + segmentLengths[j] >= targetDist) {
        float t = (targetDist - accumulatedDist) / segmentLengths[j];
        charPos = curvePoints[j] * (1.0f - t) + curvePoints[j + 1] * t;
        tangent = (curvePoints[j + 1] - curvePoints[j]).normalized();
        break;
      }
      accumulatedDist += segmentLengths[j];
    }

    if (tangent.length() > 0.0f) {
      // Calculate angle from tangent
      float angle = atan2f(tangent.y(), tangent.x()) * 180.0f / M_PI;

      // Convert to screen coordinates
      QPoint screenPos = worldToScreen(charPos);

      // Draw character rotated along path
      painter.save();
      painter.translate(screenPos);
      painter.rotate(angle);
      painter.setPen(textPrim->color());
      painter.drawText(QPoint(-charWidthScreen / 2, fm.ascent() / 2),
                       QString(ch));
      painter.restore();
    }

    currentDistance += charWidthWorld;

    // Stop if we've gone past the end of the path
    if (currentDistance > totalLength)
      break;
  }
}

void drawDimensionLabel(QPainter *painter, DimensionPrimitive *dimension,
                        const std::function<QPoint(const QVector2D &)> &worldToScreen) {
  if (!dimension || !dimension->isVisible() || !worldToScreen)
    return;

  // Get dimension line endpoints in world coordinates
  QVector2D start = dimension->getStart();
  QVector2D end = dimension->getEnd();

  // Calculate text position (midpoint of dimension line with offset)
  QVector2D direction = (end - start).normalized();
  QVector2D perpendicular(-direction.y(), direction.x());
  float offset = 25.0f; // Text offset from measured line

  // Convert world coordinates to screen coordinates
  QPoint screenStart = worldToScreen(start);
  QPoint screenEnd = worldToScreen(end);
  QVector2D screenMidpoint((screenStart + screenEnd) / 2.0);
  QVector2D screenPerp(perpendicular * offset);

  QPoint textPos = (screenMidpoint + screenPerp).toPoint();

  // Format measurement text from live geometry (correct units)
  QString text = dimension->getDisplayText();

  // Draw text with background
  QFontMetrics fm(painter->font());
  QRect textRect = fm.boundingRect(text);
  textRect.moveCenter(textPos);

  // Draw background rectangle
  painter->fillRect(textRect.adjusted(-2, -2, 2, 2), QColor(0, 0, 0, 128));

  // Draw text
  painter->setPen(QColor(255, 255, 255));
  painter->drawText(textRect, Qt::AlignCenter, text);
}

void drawTextPrimitive(QPainter &painter, TextPrimitive *textPrim,
                       float zoomLevel, const WorldToScreenFn &worldToScreen,
                       const FindSplineFn &findSpline)
{
    if (!textPrim || !textPrim->isVisible() || !worldToScreen) {
        return;
    }

    if (textPrim->followsSpline()) {
        SplinePrimitive *spline = nullptr;
        if (findSpline) {
            spline = findSpline(textPrim->splineId());
        }
        drawOnSpline(painter, textPrim, spline, zoomLevel, worldToScreen);
        return;
    }

    const QPoint screenPos = worldToScreen(textPrim->position());
    const int scaledFontSize = std::max(
        1, static_cast<int>(textPrim->fontSize() * zoomLevel * textPrim->scale()));
    QFont font(textPrim->fontFamily(), scaledFontSize);
    font.setBold(textPrim->isBold());
    font.setItalic(textPrim->isItalic());

    // Never draw selection box here — renderSelection owns handles
    if (textPrim->rotation() != 0.0f) {
        painter.save();
        painter.translate(screenPos);
        painter.rotate(textPrim->rotation() * 180.0f / M_PI);
        drawFormatted(&painter, textPrim, QPoint(0, 0), font, zoomLevel, false);
        painter.restore();
    } else {
        drawFormatted(&painter, textPrim, screenPos, font, zoomLevel, false);
    }
}

void renderAllTexts(QPainter &painter, LayerManager *layers,
                    const PrimitiveList &legacy, float zoomLevel,
                    const WorldToScreenFn &worldToScreen,
                    const FindSplineFn &findSpline)
{
    auto renderList =
        [&](const std::vector<std::unique_ptr<DrawingPrimitive>> &list) {
            for (const auto &primitive : list) {
                drawTextPrimitive(painter,
                                  dynamic_cast<TextPrimitive *>(primitive.get()),
                                  zoomLevel, worldToScreen, findSpline);
            }
        };

    if (layers) {
        for (const auto &layer : layers->layers()) {
            if (!layer || !layer->isVisible()) {
                continue;
            }
            renderList(layer->primitives());
        }
    } else {
        renderList(legacy);
    }
}

void renderAllDimensions(QPainter &painter, LayerManager *layers,
                         const PrimitiveList &legacy,
                         DrawingPrimitive *currentPrimitive,
                         const WorldToScreenFn &worldToScreen)
{
    auto renderList =
        [&](const std::vector<std::unique_ptr<DrawingPrimitive>> &list) {
            for (const auto &primitive : list) {
                if (auto *dimension =
                        dynamic_cast<DimensionPrimitive *>(primitive.get())) {
                    drawDimensionLabel(&painter, dimension, worldToScreen);
                }
            }
        };

    if (layers) {
        for (const auto &layer : layers->layers()) {
            if (!layer || !layer->isVisible()) {
                continue;
            }
            renderList(layer->primitives());
        }
    } else {
        renderList(legacy);
    }

    if (auto *dimension =
            dynamic_cast<DimensionPrimitive *>(currentPrimitive)) {
        drawDimensionLabel(&painter, dimension, worldToScreen);
    }
}

} // namespace TextRenderer
