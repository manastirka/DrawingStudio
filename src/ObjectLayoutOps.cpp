#include "ObjectLayoutOps.h"

#include "DrawingPrimitive.h"

#include <QDebug>
#include <algorithm>

namespace ObjectLayoutOps {

int alignObjects(const std::vector<DrawingPrimitive *> &objects, Align type,
                 const QRectF &pageRect)
{
    if (objects.empty()) {
        return 0;
    }

    QVector2D targetPos;
    bool isHorizontal = false;
    bool isVertical = false;

    switch (type) {
    case Align::PageLeft:
        targetPos = QVector2D(static_cast<float>(pageRect.left()), 0);
        isHorizontal = true;
        break;
    case Align::PageRight:
        targetPos = QVector2D(static_cast<float>(pageRect.right()), 0);
        isHorizontal = true;
        break;
    case Align::PageCenterHorizontal:
        targetPos = QVector2D(static_cast<float>(pageRect.center().x()), 0);
        isHorizontal = true;
        break;
    case Align::PageTop:
        targetPos = QVector2D(0, static_cast<float>(pageRect.top()));
        isVertical = true;
        break;
    case Align::PageBottom:
        targetPos = QVector2D(0, static_cast<float>(pageRect.bottom()));
        isVertical = true;
        break;
    case Align::PageCenterVertical:
        targetPos = QVector2D(0, static_cast<float>(pageRect.center().y()));
        isVertical = true;
        break;
    default: {
        QRectF referenceBounds;
        bool found = false;

        switch (type) {
        case Align::Left:
            for (DrawingPrimitive *obj : objects) {
                if (!obj) {
                    continue;
                }
                const QRectF bounds = obj->boundingRect();
                if (!found || bounds.left() < referenceBounds.left()) {
                    referenceBounds = bounds;
                    found = true;
                }
            }
            if (found) {
                targetPos =
                    QVector2D(static_cast<float>(referenceBounds.left()), 0);
                isHorizontal = true;
            }
            break;

        case Align::Right:
            for (DrawingPrimitive *obj : objects) {
                if (!obj) {
                    continue;
                }
                const QRectF bounds = obj->boundingRect();
                if (!found || bounds.right() > referenceBounds.right()) {
                    referenceBounds = bounds;
                    found = true;
                }
            }
            if (found) {
                targetPos =
                    QVector2D(static_cast<float>(referenceBounds.right()), 0);
                isHorizontal = true;
            }
            break;

        case Align::CenterHorizontal: {
            QRectF combined;
            bool first = true;
            for (DrawingPrimitive *obj : objects) {
                if (!obj) {
                    continue;
                }
                const QRectF bounds = obj->boundingRect();
                if (first) {
                    combined = bounds;
                    first = false;
                } else {
                    combined = combined.united(bounds);
                }
            }
            if (!first) {
                targetPos =
                    QVector2D(static_cast<float>(combined.center().x()), 0);
                isHorizontal = true;
            }
            break;
        }

        case Align::Top:
            for (DrawingPrimitive *obj : objects) {
                if (!obj) {
                    continue;
                }
                const QRectF bounds = obj->boundingRect();
                if (!found || bounds.top() < referenceBounds.top()) {
                    referenceBounds = bounds;
                    found = true;
                }
            }
            if (found) {
                targetPos =
                    QVector2D(0, static_cast<float>(referenceBounds.top()));
                isVertical = true;
            }
            break;

        case Align::Bottom:
            for (DrawingPrimitive *obj : objects) {
                if (!obj) {
                    continue;
                }
                const QRectF bounds = obj->boundingRect();
                if (!found || bounds.bottom() > referenceBounds.bottom()) {
                    referenceBounds = bounds;
                    found = true;
                }
            }
            if (found) {
                targetPos =
                    QVector2D(0, static_cast<float>(referenceBounds.bottom()));
                isVertical = true;
            }
            break;

        case Align::CenterVertical: {
            QRectF combined;
            bool first = true;
            for (DrawingPrimitive *obj : objects) {
                if (!obj) {
                    continue;
                }
                const QRectF bounds = obj->boundingRect();
                if (first) {
                    combined = bounds;
                    first = false;
                } else {
                    combined = combined.united(bounds);
                }
            }
            if (!first) {
                targetPos =
                    QVector2D(0, static_cast<float>(combined.center().y()));
                isVertical = true;
            }
            break;
        }

        default:
            return 0;
        }
        break;
    }
    }

    if (!isHorizontal && !isVertical) {
        return 0;
    }

    int count = 0;
    for (DrawingPrimitive *obj : objects) {
        if (!obj) {
            continue;
        }
        const QRectF objBounds = obj->boundingRect();
        const QVector2D currentCenter(static_cast<float>(objBounds.center().x()),
                                      static_cast<float>(objBounds.center().y()));
        QVector2D offset(0, 0);

        if (isHorizontal) {
            const float targetX = targetPos.x();
            if (type == Align::Left || type == Align::PageLeft) {
                offset.setX(targetX - static_cast<float>(objBounds.left()));
            } else if (type == Align::Right || type == Align::PageRight) {
                offset.setX(targetX - static_cast<float>(objBounds.right()));
            } else if (type == Align::CenterHorizontal ||
                       type == Align::PageCenterHorizontal) {
                offset.setX(targetX - currentCenter.x());
            }
        }

        if (isVertical) {
            const float targetY = targetPos.y();
            if (type == Align::Top || type == Align::PageTop) {
                offset.setY(targetY - static_cast<float>(objBounds.top()));
            } else if (type == Align::Bottom || type == Align::PageBottom) {
                offset.setY(targetY - static_cast<float>(objBounds.bottom()));
            } else if (type == Align::CenterVertical ||
                       type == Align::PageCenterVertical) {
                offset.setY(targetY - currentCenter.y());
            }
        }

        obj->translate(offset);
        ++count;
    }

    qDebug() << "ObjectLayoutOps::alignObjects" << count << "type"
             << static_cast<int>(type);
    return count;
}

int distributeObjects(const std::vector<DrawingPrimitive *> &objects,
                      bool horizontal)
{
    struct Item {
        DrawingPrimitive *primitive = nullptr;
        QRectF bounds;
    };

    std::vector<Item> items;
    items.reserve(objects.size());
    for (auto *obj : objects) {
        if (obj) {
            items.push_back({obj, obj->boundingRect()});
        }
    }
    if (items.size() < 3) {
        return 0;
    }

    if (horizontal) {
        std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
            return a.bounds.left() < b.bounds.left();
        });
        const float minX = static_cast<float>(items.front().bounds.left());
        const float maxX = static_cast<float>(items.back().bounds.right());
        float totalWidth = 0.0f;
        for (const auto &item : items) {
            totalWidth += static_cast<float>(item.bounds.width());
        }
        const float spacing =
            (maxX - minX - totalWidth) /
            static_cast<float>(items.size() - 1);
        float cursor = minX;
        for (auto &item : items) {
            const float offsetX =
                cursor - static_cast<float>(item.bounds.left());
            item.primitive->translate(QVector2D(offsetX, 0.0f));
            cursor += static_cast<float>(item.bounds.width()) + spacing;
        }
    } else {
        std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
            return a.bounds.top() < b.bounds.top();
        });
        const float minY = static_cast<float>(items.front().bounds.top());
        const float maxY = static_cast<float>(items.back().bounds.bottom());
        float totalHeight = 0.0f;
        for (const auto &item : items) {
            totalHeight += static_cast<float>(item.bounds.height());
        }
        const float spacing =
            (maxY - minY - totalHeight) /
            static_cast<float>(items.size() - 1);
        float cursor = minY;
        for (auto &item : items) {
            const float offsetY =
                cursor - static_cast<float>(item.bounds.top());
            item.primitive->translate(QVector2D(0.0f, offsetY));
            cursor += static_cast<float>(item.bounds.height()) + spacing;
        }
    }

    return static_cast<int>(items.size());
}

} // namespace ObjectLayoutOps
