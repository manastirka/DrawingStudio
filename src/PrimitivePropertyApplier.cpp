#include "PrimitivePropertyApplier.h"

#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"

#include <QColor>
#include <QVector2D>
#include <QtMath>

#include <cmath>

void PrimitivePropertyApplier::apply(DrawingPrimitive* primitive, const QString& propertyName, const QVariant& value)
{
    // Handle readonly/calculated properties that shouldn't be modified
    if (propertyName == "area" || propertyName == "circumference" || propertyName == "perimeter" || 
        propertyName == "actualLength" || propertyName == "arcLength" || propertyName == "aspectRatio" ||
        propertyName == "controlPointCount" || propertyName == "pointCount" || propertyName == "vertices" ||
        propertyName == "type") {
        return; // Readonly property
    }
    
    // Handle basic properties
    if (propertyName == "visible") {
        primitive->setVisible(value.toBool());
    }
    else if (propertyName == "lineWidth") {
        primitive->setLineWidth(value.toFloat());
    }
    else if (propertyName == "color") {
        primitive->setColor(value.value<QColor>());
    }
    else if (propertyName == "lineStyle") {
        primitive->setLineStyle(static_cast<Qt::PenStyle>(value.toInt()));
    }
    else if (propertyName == "fillColor") {
        primitive->setFillColor(value.value<QColor>());
        if (auto *rect = dynamic_cast<RectanglePrimitive *>(primitive)) {
            rect->setFilled(true);
        } else if (auto *ellipse = dynamic_cast<EllipsePrimitive *>(primitive)) {
            ellipse->setFilled(true);
        } else if (auto *circle = dynamic_cast<CirclePrimitive *>(primitive)) {
            circle->setFilled(true);
        } else if (auto *polygon = dynamic_cast<PolygonPrimitive *>(primitive)) {
            polygon->setFilled(true);
        } else if (auto *spline = dynamic_cast<SplinePrimitive *>(primitive)) {
            spline->setFilled(true);
        }
    }
    else if (propertyName == "shadowEnabled") {
        primitive->setShadowEnabled(value.toBool());
    }
    else if (propertyName == "shadowColor") {
        primitive->setShadowColor(value.value<QColor>());
    }
    else if (propertyName == "shadowBlur") {
        primitive->setShadowBlur(value.toFloat());
    }
    else if (propertyName == "shadowOffsetX") {
        primitive->setShadowOffset(value.toFloat(), primitive->shadowOffsetY());
    }
    else if (propertyName == "shadowOffsetY") {
        primitive->setShadowOffset(primitive->shadowOffsetX(), value.toFloat());
    }
    // Handle line properties
    else if (auto line = dynamic_cast<LinePrimitive*>(primitive)) {
        if (propertyName == "length") {
            // Adjust line length while maintaining angle
            QVector2D start = line->startPoint();
            QVector2D end = line->endPoint();
            QVector2D direction = (end - start).normalized();
            float newLength = value.toFloat();
            line->setEndPoint(start + direction * newLength);
        }
        else if (propertyName == "angle") {
            // Adjust line angle while maintaining length
            QVector2D start = line->startPoint();
            QVector2D end = line->endPoint();
            float length = (end - start).length();
            float angleRad = value.toFloat() * M_PI / 180.0f;
            QVector2D newEnd = start + QVector2D(cos(angleRad), sin(angleRad)) * length;
            line->setEndPoint(newEnd);
        }
        // Line tools (stored as metadata for future implementation)
        else if (propertyName == "snapToGrid" || propertyName == "constrainAngle" || propertyName == "showDimensions") {
            // These would be implemented with additional line properties in the future
        }
    }
    // Handle rectangle properties
    else if (auto rect = dynamic_cast<RectanglePrimitive*>(primitive)) {
        if (propertyName == "width") {
            QVector2D tl = rect->topLeft();
            QVector2D br = rect->bottomRight();
            QVector2D center = (tl + br) * 0.5f;
            float newWidth = value.toFloat();
            float height = abs(br.y() - tl.y());
            rect->setTopLeft(QVector2D(center.x() - newWidth/2, center.y() - height/2));
            rect->setBottomRight(QVector2D(center.x() + newWidth/2, center.y() + height/2));
        }
        else if (propertyName == "height") {
            QVector2D tl = rect->topLeft();
            QVector2D br = rect->bottomRight();
            QVector2D center = (tl + br) * 0.5f;
            float width = abs(br.x() - tl.x());
            float newHeight = value.toFloat();
            rect->setTopLeft(QVector2D(center.x() - width/2, center.y() - newHeight/2));
            rect->setBottomRight(QVector2D(center.x() + width/2, center.y() + newHeight/2));
        }
        else if (propertyName == "filled") {
            bool filled = value.toBool();
            rect->setFilled(filled);
            if (filled) {
                if (!rect->hasFillColor()) {
                    rect->setFillColor(rect->color());
                }
            } else {
                rect->clearFillColor();
            }
        }
        // Rectangle tools - corner radius now implemented!
        else if (propertyName == "cornerRadius") {
            float radius = value.toFloat();
            qDebug() << "Setting corner radius to:" << radius;
            rect->setCornerRadius(radius);
        }
        // Other rectangle tools - some now implemented!
        else if (propertyName == "maintainAspectRatio") {
            rect->setMaintainAspectRatio(value.toBool());
        }
        else if (propertyName == "centerOnResize") {
            rect->setCenterOnResize(value.toBool());
        }
        else if (propertyName == "convertToRoundedRect") {
            if (value.toBool()) {
                // Convert to rounded rectangle by setting a default corner radius
                float cornerRadius = std::min(abs(rect->bottomRight().x() - rect->topLeft().x()),
                                               abs(rect->bottomRight().y() - rect->topLeft().y())) * 0.1f;
                qDebug() << "Converting to rounded rect with radius:" << cornerRadius;
                rect->setCornerRadius(cornerRadius);
            } else {
                // Remove rounding
                qDebug() << "Removing rounded corners";
                rect->setCornerRadius(0.0f);
            }
        }
    }
    // Handle ellipse properties
    else if (auto ellipse = dynamic_cast<EllipsePrimitive*>(primitive)) {
        if (propertyName == "radiusX") {
            ellipse->setRadiusX(value.toFloat());
        }
        else if (propertyName == "radiusY") {
            ellipse->setRadiusY(value.toFloat());
        }
        else if (propertyName == "filled") {
            bool filled = value.toBool();
            ellipse->setFilled(filled);
            if (filled) {
                if (!ellipse->hasFillColor()) {
                    ellipse->setFillColor(ellipse->color());
                }
            } else {
                ellipse->clearFillColor();
            }
        }
        else if (propertyName == "makeCircle") {
            if (value.toBool()) {
                float avgRadius = (ellipse->radiusX() + ellipse->radiusY()) / 2.0f;
                ellipse->setRadiusX(avgRadius);
                ellipse->setRadiusY(avgRadius);
            }
        }
        // Ellipse tools - now implemented!
        else if (propertyName == "subdivisions") {
            ellipse->setSubdivisions(value.toInt());
        }
        else if (propertyName == "showAxes") {
            ellipse->setShowAxes(value.toBool());
        }
        else if (propertyName == "lockAspectRatio") {
            ellipse->setLockAspectRatio(value.toBool());
        }
    }
    // Handle bezier curve properties
    else if (auto bezier = dynamic_cast<BezierCurvePrimitive*>(primitive)) {
        if (propertyName == "directDistance") {
            // Modify the distance between start and end points
            const auto& controlPoints = bezier->controlPoints();
            if (controlPoints.size() >= 2) {
                QVector2D start = controlPoints.front();
                QVector2D end = controlPoints.back();
                QVector2D direction = (end - start).normalized();
                float newDistance = value.toFloat();
                
                // Create new control points with updated distance
                std::vector<QVector2D> newPoints = controlPoints;
                newPoints.back() = start + direction * newDistance;
                
                // Update intermediate control points proportionally
                if (controlPoints.size() == 4) {
                    float ratio = newDistance / (end - start).length();
                    newPoints[1] = start + (controlPoints[1] - start) * ratio;
                    newPoints[2] = start + (controlPoints[2] - start) * ratio;
                }
                
                bezier->setControlPoints(newPoints);
            }
        }
        // Bezier visual tools - now implemented!
        else if (propertyName == "showControlLines") {
            bezier->setShowControlLines(value.toBool());
        }
        else if (propertyName == "subdivisionLevel") {
            bezier->setSubdivisionLevel(value.toInt());
        }
        else if (propertyName == "autoTangents") {
            bezier->setAutoTangents(value.toBool());
        }
        else if (propertyName == "symmetricHandles") {
            bezier->setSymmetricHandles(value.toBool());
        }
        // curveSmoothing is not yet implemented - would require rendering changes
        else if (propertyName == "curveSmoothing") {
            qDebug() << "Bézier curve smoothing not yet implemented in renderer";
        }
    }
    // Handle spline properties
    else if (auto spline = dynamic_cast<SplinePrimitive*>(primitive)) {
        if (propertyName == "closed") {
            spline->setClosed(value.toBool());
        }
        else if (propertyName == "filled") {
            bool filled = value.toBool();
            spline->setFilled(filled);
            if (filled) {
                if (!spline->hasFillColor()) {
                    spline->setFillColor(spline->color());
                }
            } else {
                spline->clearFillColor();
            }
        }
        else if (propertyName == "smoothness") {
            spline->setSmoothness(value.toFloat());
        }
        // Spline tools - interpolation type now implemented!
        else if (propertyName == "interpolationType") {
            spline->setInterpolationType(value.toInt());
        }
        // Other spline tools - now implemented!
        else if (propertyName == "tension") {
            spline->setTension(value.toFloat());
        }
        else if (propertyName == "autoSmooth") {
            spline->setAutoSmooth(value.toBool());
        }
        else if (propertyName == "showPoints") {
            spline->setShowPoints(value.toBool());
        }
    }
    // Handle arc properties
    else if (auto arc = dynamic_cast<ArcPrimitive*>(primitive)) {
        if (propertyName == "radius") {
            arc->setRadius(value.toFloat());
        }
        else if (propertyName == "startAngle") {
            arc->setStartAngle(value.toFloat());
        }
        else if (propertyName == "endAngle") {
            arc->setEndAngle(value.toFloat());
        }
        else if (propertyName == "makeFullCircle") {
            if (value.toBool()) {
                arc->setStartAngle(0.0f);
                arc->setEndAngle(360.0f);
            }
        }
        else if (propertyName == "reverseDirection") {
            if (value.toBool()) {
                float temp = arc->startAngle();
                arc->setStartAngle(arc->endAngle());
                arc->setEndAngle(temp);
                qDebug() << "Arc direction reversed: start=" << arc->startAngle() << "end=" << arc->endAngle();
            }
        }
        // Arc tools (future implementation)
        else if (propertyName == "snapAngles" || propertyName == "showCenterlines" || propertyName == "arcQuality") {
            qDebug() << "Arc tool property changed:" << propertyName << "=" << value;
        }
    }
    // Handle circle properties
    else if (auto circle = dynamic_cast<CirclePrimitive*>(primitive)) {
        if (propertyName == "radius") {
            circle->setRadius(value.toFloat());
        }
        else if (propertyName == "diameter") {
            circle->setRadius(value.toFloat() / 2.0f);
        }
        else if (propertyName == "filled") {
            bool filled = value.toBool();
            circle->setFilled(filled);
            if (filled) {
                if (!circle->hasFillColor()) {
                    circle->setFillColor(circle->color());
                }
            } else {
                circle->clearFillColor();
            }
        }
        // Circle visual tools - now implemented!
        else if (propertyName == "showCenterPoint") {
            circle->setShowCenterPoint(value.toBool());
        }
        else if (propertyName == "showQuadrants") {
            circle->setShowQuadrants(value.toBool());
        }
        // Other circle tools (future implementation)
        else if (propertyName == "subdivideToPolygon" || propertyName == "circleQuality") {
            qDebug() << "Circle tool property changed:" << propertyName << "=" << value << "(not yet implemented)";
        }
    }
    // Handle polygon properties
    else if (auto polygon = dynamic_cast<PolygonPrimitive*>(primitive)) {
        if (propertyName == "vertices") {
            // Changing vertex count is complex - log for now
            qDebug() << "Changing polygon vertex count to" << value.toInt() << "not yet implemented";
        }
        else if (propertyName == "filled") {
            bool filled = value.toBool();
            polygon->setFilled(filled);
            if (filled) {
                if (!polygon->hasFillColor()) {
                    polygon->setFillColor(polygon->color());
                }
            } else {
                polygon->clearFillColor();
            }
        }
        else if (propertyName == "closed") {
            polygon->setClosed(value.toBool());
        }
        // Polygon tools (future implementation - methods don't exist yet)
        else if (propertyName == "regular" || propertyName == "rotation" || 
                 propertyName == "snapVertices" || propertyName == "showAngles" || propertyName == "cornerRadius") {
            qDebug() << "Polygon tool property changed:" << propertyName << "=" << value;
        }
    }
    // Handle curve properties (similar to spline but with different characteristics)
    else if (auto curve = dynamic_cast<CurvePrimitive*>(primitive)) {
        if (propertyName == "closed") {
            curve->setClosed(value.toBool());
        }
        else if (propertyName == "filled") {
            bool filled = value.toBool();
            curve->setFilled(filled);
            if (filled) {
                if (!curve->hasFillColor()) {
                    curve->setFillColor(curve->color());
                }
            } else {
                curve->clearFillColor();
            }
        }
        // Curve type selection - now implemented!
        else if (propertyName == "curveType") {
            curve->setCurveType(value.toInt());
        }
        // Show control polygon - now implemented!
        else if (propertyName == "showControlPolygon") {
            curve->setShowControlPolygon(value.toBool());
        }
        // Other curve properties that still need implementation
        else if (propertyName == "simplifyTolerance" || propertyName == "smoothPasses") {
            qDebug() << "Curve property" << propertyName << "not yet implemented";
        }
    }
    // Handle image properties (from redesigned Property panel)
    else if (auto imagePrim = dynamic_cast<ImagePrimitive*>(primitive)) {
        if (propertyName == "imagePositionX") {
            QVector2D pos = imagePrim->position();
            imagePrim->setPosition(QVector2D(value.toFloat(), pos.y()));
        }
        else if (propertyName == "imagePositionY") {
            QVector2D pos = imagePrim->position();
            imagePrim->setPosition(QVector2D(pos.x(), value.toFloat()));
        }
        else if (propertyName == "imageWidth") {
            QVector2D sz = imagePrim->size();
            imagePrim->setSize(QVector2D(value.toFloat(), sz.y()));
        }
        else if (propertyName == "imageHeight") {
            QVector2D sz = imagePrim->size();
            imagePrim->setSize(QVector2D(sz.x(), value.toFloat()));
        }
        else if (propertyName == "imageRotation") {
            imagePrim->setRotation(value.toFloat());
        }
        else if (propertyName == "maintainAspectRatio") {
            imagePrim->setMaintainAspectRatio(value.toBool());
        }
        else if (propertyName == "opacity") {
            imagePrim->setOpacityMultiplier(value.toFloat());
        }
        else if (propertyName == "maskOverlayVisible") {
            imagePrim->setMaskOverlayVisible(value.toBool());
        }
        else if (propertyName == "maskInverted") {
            if (value.toBool() != imagePrim->isMaskInverted()) {
                imagePrim->invertMask();
            }
        }
    }
    // Handle dimension properties
    else if (auto dimension = dynamic_cast<DimensionPrimitive*>(primitive)) {
        if (propertyName == "unitsString") {
            dimension->setUnitsString(value.toString());
        }
        else if (propertyName == "measurementValue") {
            dimension->setMeasurementValue(value.toFloat());
        }
        // DimensionPrimitive doesn't have these methods yet
        else if (propertyName == "precision" || propertyName == "units" || 
                 propertyName == "showArrows" || propertyName == "offset") {
            qDebug() << "Dimension property" << propertyName << "not yet implemented";
        }
        // Dimension tools (future implementation)
        else if (propertyName == "dimensionStyle" || propertyName == "textSize" || 
                 propertyName == "autoPosition" || propertyName == "displayText" ||
                 propertyName == "extensionLineOffset" || propertyName == "arrowSize" ||
                 propertyName == "textPosition" || propertyName == "showUnits") {
            qDebug() << "Dimension tool property changed:" << propertyName << "=" << value;
        }
    }
    // Handle text properties
    else if (auto textPrim = dynamic_cast<TextPrimitive*>(primitive)) {
        if (propertyName == "text") {
            textPrim->setText(value.toString());
        }
        else if (propertyName == "fontFamily") {
            textPrim->setFontFamily(value.toString());
        }
        else if (propertyName == "fontSize") {
            textPrim->setFontSize(value.toInt());
        }
        else if (propertyName == "bold") {
            textPrim->setBold(value.toBool());
        }
        else if (propertyName == "italic") {
            textPrim->setItalic(value.toBool());
        }
        else if (propertyName == "rotation") {
            // Convert degrees to radians
            float radians = value.toFloat() * M_PI / 180.0f;
            textPrim->setRotation(radians);
        }
        else if (propertyName == "scale") {
            // Convert percentage to scale factor
            float scaleFactor = value.toFloat() / 100.0f;
            textPrim->setScale(scaleFactor);
        }
        else if (propertyName == "posX") {
            QVector2D pos = textPrim->position();
            textPrim->setPosition(QVector2D(value.toFloat(), pos.y()));
        }
        else if (propertyName == "posY") {
            QVector2D pos = textPrim->position();
            textPrim->setPosition(QVector2D(pos.x(), value.toFloat()));
        }
    }
}
