#include "LineExtractor.h"

#include <QDebug>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// LineExtractor geometry post-process (refactor E25).

// --- extractAxisAlignedRuns ---
std::vector<LineExtractor::ExtractedLine>
LineExtractor::extractAxisAlignedRuns(const QImage &wallMask, double minLength,
                                      double maxGap)
{
    std::vector<ExtractedLine> lines;
    const int w = wallMask.width();
    const int h = wallMask.height();
    const double minLen = std::max(8.0, minLength);

    // Horizontal runs (per row), with gap bridging
    for (int y = 0; y < h; ++y) {
        const uchar *row = wallMask.constScanLine(y);
        int runStart = -1;
        int lastOn = -1;
        for (int x = 0; x <= w; ++x) {
            const bool on = (x < w) && row[x] > 0;
            if (on) {
                if (runStart < 0)
                    runStart = x;
                lastOn = x;
            } else if (runStart >= 0) {
                // Allow small gaps inside a wall
                bool bridge = false;
                if (x < w) {
                    for (int g = 1; g <= static_cast<int>(maxGap) && x + g < w; ++g) {
                        if (row[x + g] > 0) {
                            bridge = true;
                            break;
                        }
                    }
                }
                if (bridge)
                    continue;

                const double len = lastOn - runStart + 1;
                if (len >= minLen) {
                    // Use middle of wall thickness: scan nearby rows for center
                    lines.push_back(makeLine(QPointF(runStart, y), QPointF(lastOn, y),
                                             static_cast<int>(len)));
                }
                runStart = -1;
                lastOn = -1;
            }
        }
    }

    // Vertical runs (per column)
    for (int x = 0; x < w; ++x) {
        int runStart = -1;
        int lastOn = -1;
        for (int y = 0; y <= h; ++y) {
            const bool on = (y < h) && wallMask.constScanLine(y)[x] > 0;
            if (on) {
                if (runStart < 0)
                    runStart = y;
                lastOn = y;
            } else if (runStart >= 0) {
                bool bridge = false;
                if (y < h) {
                    for (int g = 1; g <= static_cast<int>(maxGap) && y + g < h; ++g) {
                        if (wallMask.constScanLine(y + g)[x] > 0) {
                            bridge = true;
                            break;
                        }
                    }
                }
                if (bridge)
                    continue;

                const double len = lastOn - runStart + 1;
                if (len >= minLen) {
                    lines.push_back(makeLine(QPointF(x, runStart), QPointF(x, lastOn),
                                             static_cast<int>(len)));
                }
                runStart = -1;
                lastOn = -1;
            }
        }
    }

    return lines;
}


// --- snapJunctions ---
void LineExtractor::snapJunctions(std::vector<ExtractedLine> &lines, double snapDist)
{
    if (lines.empty() || snapDist <= 0)
        return;

    struct Pt {
        double x, y;
        int count;
    };
    std::vector<Pt> clusters;

    auto addPoint = [&](QPointF p) {
        for (auto &c : clusters) {
            if (std::hypot(c.x - p.x(), c.y - p.y()) <= snapDist) {
                c.x = (c.x * c.count + p.x()) / (c.count + 1);
                c.y = (c.y * c.count + p.y()) / (c.count + 1);
                ++c.count;
                return;
            }
        }
        clusters.push_back({p.x(), p.y(), 1});
    };

    for (const auto &l : lines) {
        addPoint(l.start);
        addPoint(l.end);
    }

    auto snapPt = [&](QPointF p) -> QPointF {
        double best = snapDist + 1;
        QPointF out = p;
        for (const auto &c : clusters) {
            const double d = std::hypot(c.x - p.x(), c.y - p.y());
            if (d < best) {
                best = d;
                out = QPointF(c.x, c.y);
            }
        }
        return out;
    };

    for (auto &l : lines) {
        l.start = snapPt(l.start);
        l.end = snapPt(l.end);
        const double dx = l.end.x() - l.start.x();
        const double dy = l.end.y() - l.start.y();
        l.length = static_cast<float>(std::hypot(dx, dy));
        l.angle = static_cast<float>(std::atan2(dy, dx) * 180.0 / M_PI);
    }
}


// --- quantizeAxes ---
void LineExtractor::quantizeAxes(std::vector<ExtractedLine> &lines, double band)
{
    if (lines.empty() || band <= 0)
        return;

    std::vector<double> hYs, vXs;
    for (const auto &l : lines) {
        const float a = std::abs(l.angle);
        if (a < 20.f || a > 160.f)
            hYs.push_back(0.5 * (l.start.y() + l.end.y()));
        else if (std::abs(a - 90.f) < 20.f)
            vXs.push_back(0.5 * (l.start.x() + l.end.x()));
    }

    auto cluster = [&](std::vector<double> vals) {
        std::vector<double> centers;
        std::sort(vals.begin(), vals.end());
        for (double v : vals) {
            if (centers.empty() || std::abs(v - centers.back()) > band)
                centers.push_back(v);
            else
                centers.back() = 0.5 * (centers.back() + v);
        }
        for (double &c : centers)
            c = std::round(c);
        return centers;
    };

    const auto yGrid = cluster(hYs);
    const auto xGrid = cluster(vXs);

    auto nearest = [](double v, const std::vector<double> &grid, double maxDist) {
        double best = v;
        double bestD = maxDist + 1;
        for (double g : grid) {
            const double d = std::abs(g - v);
            if (d < bestD) {
                bestD = d;
                best = g;
            }
        }
        return bestD <= maxDist ? best : v;
    };

    for (auto &l : lines) {
        const float a = std::abs(l.angle);
        if (a < 20.f || a > 160.f) {
            const double y = nearest(0.5 * (l.start.y() + l.end.y()), yGrid, band);
            l.start.setY(y);
            l.end.setY(y);
            l.angle = (l.end.x() >= l.start.x()) ? 0.f : 180.f;
        } else if (std::abs(a - 90.f) < 20.f) {
            const double x = nearest(0.5 * (l.start.x() + l.end.x()), xGrid, band);
            l.start.setX(x);
            l.end.setX(x);
            l.angle = (l.end.y() >= l.start.y()) ? 90.f : -90.f;
        }
        l.length = static_cast<float>(
            std::hypot(l.end.x() - l.start.x(), l.end.y() - l.start.y()));
    }
}


// --- extendToMeetJunctions ---
void LineExtractor::extendToMeetJunctions(std::vector<ExtractedLine> &lines,
                                          double maxExtend)
{
    if (lines.size() < 2 || maxExtend <= 0)
        return;

    auto isH = [](const ExtractedLine &l) {
        const float a = std::abs(l.angle);
        return a < 20.f || a > 160.f;
    };
    auto isV = [](const ExtractedLine &l) {
        return std::abs(std::abs(l.angle) - 90.f) < 20.f;
    };

    for (auto &h : lines) {
        if (!isH(h))
            continue;
        const double y = 0.5 * (h.start.y() + h.end.y());
        double x0 = std::min(h.start.x(), h.end.x());
        double x1 = std::max(h.start.x(), h.end.x());

        for (const auto &v : lines) {
            if (!isV(v))
                continue;
            const double x = 0.5 * (v.start.x() + v.end.x());
            const double y0 = std::min(v.start.y(), v.end.y());
            const double y1 = std::max(v.start.y(), v.end.y());
            if (x < x0 && (x0 - x) <= maxExtend && y >= y0 - 1 && y <= y1 + 1)
                x0 = x;
            if (x > x1 && (x - x1) <= maxExtend && y >= y0 - 1 && y <= y1 + 1)
                x1 = x;
        }
        h = makeLine(QPointF(x0, y), QPointF(x1, y), h.votes);
    }

    for (auto &v : lines) {
        if (!isV(v))
            continue;
        const double x = 0.5 * (v.start.x() + v.end.x());
        double y0 = std::min(v.start.y(), v.end.y());
        double y1 = std::max(v.start.y(), v.end.y());

        for (const auto &h : lines) {
            if (!isH(h))
                continue;
            const double y = 0.5 * (h.start.y() + h.end.y());
            const double x0 = std::min(h.start.x(), h.end.x());
            const double x1 = std::max(h.start.x(), h.end.x());
            if (y < y0 && (y0 - y) <= maxExtend && x >= x0 - 1 && x <= x1 + 1)
                y0 = y;
            if (y > y1 && (y - y1) <= maxExtend && x >= x0 - 1 && x <= x1 + 1)
                y1 = y;
        }
        v = makeLine(QPointF(x, y0), QPointF(x, y1), v.votes);
    }
}


// --- recoverPartitionWalls ---
void LineExtractor::recoverPartitionWalls(const QImage &ink,
                                          std::vector<ExtractedLine> &lines,
                                          double minLength)
{
    const auto candidates =
        extractAxisAlignedRuns(ink, std::max(12.0, minLength * 0.5), 4.0);

    auto covers = [&](const ExtractedLine &cand) {
        const float a = std::abs(cand.angle);
        const bool hor = (a < 20.f || a > 160.f);
        const double c = hor ? 0.5 * (cand.start.y() + cand.end.y())
                             : 0.5 * (cand.start.x() + cand.end.x());
        const double a0 = hor ? std::min(cand.start.x(), cand.end.x())
                              : std::min(cand.start.y(), cand.end.y());
        const double a1 = hor ? std::max(cand.start.x(), cand.end.x())
                              : std::max(cand.start.y(), cand.end.y());

        for (const auto &l : lines) {
            const float la = std::abs(l.angle);
            const bool lHor = (la < 20.f || la > 160.f);
            if (lHor != hor)
                continue;
            const double lc = hor ? 0.5 * (l.start.y() + l.end.y())
                                  : 0.5 * (l.start.x() + l.end.x());
            if (std::abs(lc - c) > 3.5)
                continue;
            const double b0 = hor ? std::min(l.start.x(), l.end.x())
                                  : std::min(l.start.y(), l.end.y());
            const double b1 = hor ? std::max(l.start.x(), l.end.x())
                                  : std::max(l.start.y(), l.end.y());
            const double overlap = std::min(a1, b1) - std::max(a0, b0);
            if (overlap > (a1 - a0) * 0.5)
                return true;
        }
        return false;
    };

    for (const auto &c : candidates) {
        const float a = std::abs(c.angle);
        const bool hor = (a < 20.f || a > 160.f);
        // Skip short horizontals (stair treads ~80px); keep long partitions
        if (hor && c.length < 100)
            continue;
        if (inkCoverage(ink, c, 1) < 0.72)
            continue;
        if (!covers(c))
            lines.push_back(c);
    }
}


// --- inkCoverage ---
double LineExtractor::inkCoverage(const QImage &ink, const ExtractedLine &line,
                                  int halfBand)
{
    if (ink.isNull() || line.length < 1.f)
        return 0.0;

    const int w = ink.width();
    const int h = ink.height();
    const int samples = std::max(3, static_cast<int>(line.length / 3.0));
    int hits = 0;
    for (int i = 0; i <= samples; ++i) {
        const double t = static_cast<double>(i) / samples;
        const int cx =
            static_cast<int>(std::round(line.start.x() + t * (line.end.x() - line.start.x())));
        const int cy =
            static_cast<int>(std::round(line.start.y() + t * (line.end.y() - line.start.y())));
        bool found = false;
        for (int dy = -halfBand; dy <= halfBand && !found; ++dy) {
            for (int dx = -halfBand; dx <= halfBand; ++dx) {
                const int x = cx + dx;
                const int y = cy + dy;
                if (x >= 0 && x < w && y >= 0 && y < h &&
                    ink.constScanLine(y)[x] > 0) {
                    found = true;
                    break;
                }
            }
        }
        if (found)
            ++hits;
    }
    return static_cast<double>(hits) / (samples + 1);
}


// --- trimToInk ---
LineExtractor::ExtractedLine LineExtractor::trimToInk(const QImage &ink,
                                                      const ExtractedLine &line,
                                                      int halfBand)
{
    if (ink.isNull() || line.length < 1.f)
        return line;

    const int w = ink.width();
    const int h = ink.height();
    const int samples = std::max(3, static_cast<int>(line.length));
    int first = -1;
    int last = -1;
    for (int i = 0; i <= samples; ++i) {
        const double t = static_cast<double>(i) / samples;
        const int cx =
            static_cast<int>(std::round(line.start.x() + t * (line.end.x() - line.start.x())));
        const int cy =
            static_cast<int>(std::round(line.start.y() + t * (line.end.y() - line.start.y())));
        bool found = false;
        for (int dy = -halfBand; dy <= halfBand && !found; ++dy) {
            for (int dx = -halfBand; dx <= halfBand; ++dx) {
                const int x = cx + dx;
                const int y = cy + dy;
                if (x >= 0 && x < w && y >= 0 && y < h &&
                    ink.constScanLine(y)[x] > 0) {
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            if (first < 0)
                first = i;
            last = i;
        }
    }
    if (first < 0 || last < 0 || first == last)
        return line;

    const double t0 = static_cast<double>(first) / samples;
    const double t1 = static_cast<double>(last) / samples;
    return makeLine(
        QPointF(line.start.x() + t0 * (line.end.x() - line.start.x()),
                line.start.y() + t0 * (line.end.y() - line.start.y())),
        QPointF(line.start.x() + t1 * (line.end.x() - line.start.x()),
                line.start.y() + t1 * (line.end.y() - line.start.y())),
        line.votes);
}


// --- filterByInkSupport ---
void LineExtractor::filterByInkSupport(std::vector<ExtractedLine> &lines,
                                       const QImage &ink, double minCoverage)
{
    lines.erase(std::remove_if(lines.begin(), lines.end(),
                             [&](const ExtractedLine &l) {
                                 return inkCoverage(ink, l, 1) < minCoverage;
                             }),
                lines.end());
}


// --- trimAllToInk ---
void LineExtractor::trimAllToInk(std::vector<ExtractedLine> &lines,
                                   const QImage &ink, double minKeepRatio)
{
    for (auto &l : lines) {
        const double before = l.length;
        ExtractedLine trimmed = trimToInk(ink, l, 1);
        if (before > 0 && trimmed.length >= before * minKeepRatio)
            l = trimmed;
    }
}


// --- removeParallelOffsetDuplicates ---
void LineExtractor::removeParallelOffsetDuplicates(std::vector<ExtractedLine> &lines,
                                                   double offsetBand)
{
    if (lines.size() < 2 || offsetBand <= 0)
        return;

    std::vector<bool> drop(lines.size(), false);
    for (size_t i = 0; i < lines.size(); ++i) {
        if (drop[i])
            continue;
        const float ai = std::abs(lines[i].angle);
        const bool horI = (ai < 20.f || ai > 160.f);
        const bool verI = (std::abs(ai - 90.f) < 20.f);
        if (!horI && !verI)
            continue;

        for (size_t j = i + 1; j < lines.size(); ++j) {
            if (drop[j])
                continue;
            const float aj = std::abs(lines[j].angle);
            const bool horJ = (aj < 20.f || aj > 160.f);
            const bool verJ = (std::abs(aj - 90.f) < 20.f);
            if (horI != horJ)
                continue;

            const double ci = horI ? 0.5 * (lines[i].start.y() + lines[i].end.y())
                                   : 0.5 * (lines[i].start.x() + lines[i].end.x());
            const double cj = horJ ? 0.5 * (lines[j].start.y() + lines[j].end.y())
                                   : 0.5 * (lines[j].start.x() + lines[j].end.x());
            if (std::abs(ci - cj) > offsetBand)
                continue;

            const double a0 = horI ? std::min(lines[i].start.x(), lines[i].end.x())
                                   : std::min(lines[i].start.y(), lines[i].end.y());
            const double a1 = horI ? std::max(lines[i].start.x(), lines[i].end.x())
                                   : std::max(lines[i].start.y(), lines[i].end.y());
            const double b0 = horJ ? std::min(lines[j].start.x(), lines[j].end.x())
                                   : std::min(lines[j].start.y(), lines[j].end.y());
            const double b1 = horJ ? std::max(lines[j].start.x(), lines[j].end.x())
                                   : std::max(lines[j].start.y(), lines[j].end.y());
            const double overlap = std::min(a1, b1) - std::max(a0, b0);
            const double minLen = std::min(a1 - a0, b1 - b0);
            if (overlap < minLen * 0.55)
                continue;

            // Drop the weaker duplicate (window symbols, double strokes)
            const double scoreI = lines[i].length * lines[i].votes;
            const double scoreJ = lines[j].length * lines[j].votes;
            if (scoreJ > scoreI * 1.05 ||
                (scoreJ >= scoreI * 0.95 && lines[j].length > lines[i].length)) {
                drop[i] = true;
                break;
            }
            drop[j] = true;
        }
    }

    std::vector<ExtractedLine> kept;
    kept.reserve(lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        if (!drop[i])
            kept.push_back(lines[i]);
    }
    lines = std::move(kept);
}


// --- roundToPixels ---
void LineExtractor::roundToPixels(std::vector<ExtractedLine> &lines)
{
    for (auto &l : lines) {
        l.start = QPointF(std::round(l.start.x()), std::round(l.start.y()));
        l.end = QPointF(std::round(l.end.x()), std::round(l.end.y()));
        l.length = static_cast<float>(
            std::hypot(l.end.x() - l.start.x(), l.end.y() - l.start.y()));
        l.angle = static_cast<float>(
            std::atan2(l.end.y() - l.start.y(), l.end.x() - l.start.x()) * 180.0 /
            M_PI);
    }
}


// --- collapseParallelWalls ---
void LineExtractor::collapseParallelWalls(std::vector<ExtractedLine> &lines,
                                          double band)
{
    if (lines.size() < 2 || band <= 0)
        return;

    // Max gap only for anti-alias touch when collapsing thickness duplicates.
    // Door openings must NOT be bridged here — that is mergeCollinear's job
    // with an explicit small gap budget.
    const double bridge = 2.0;

    struct Seg {
        double a, b; // along-axis range [a,b]
        double c;    // cross-axis coordinate
        int votes;
        bool horizontal;
    };
    std::vector<Seg> segs;
    segs.reserve(lines.size());
    for (const auto &l : lines) {
        const float ang = std::abs(l.angle);
        const bool hor = (ang < 20.f || ang > 160.f);
        const bool ver = (std::abs(ang - 90.f) < 20.f);
        if (!hor && !ver)
            continue;
        Seg s;
        s.horizontal = hor;
        s.votes = l.votes;
        if (hor) {
            s.a = std::min(l.start.x(), l.end.x());
            s.b = std::max(l.start.x(), l.end.x());
            s.c = 0.5 * (l.start.y() + l.end.y());
        } else {
            s.a = std::min(l.start.y(), l.end.y());
            s.b = std::max(l.start.y(), l.end.y());
            s.c = 0.5 * (l.start.x() + l.end.x());
        }
        if (s.b - s.a >= 1.0)
            segs.push_back(s);
    }

    // Keep non H/V as-is
    std::vector<ExtractedLine> out;
    for (const auto &l : lines) {
        const float ang = std::abs(l.angle);
        const bool hor = (ang < 20.f || ang > 160.f);
        const bool ver = (std::abs(ang - 90.f) < 20.f);
        if (!hor && !ver)
            out.push_back(l);
    }

    auto process = [&](bool horizontal) {
        std::vector<int> idx;
        for (int i = 0; i < static_cast<int>(segs.size()); ++i) {
            if (segs[i].horizontal == horizontal)
                idx.push_back(i);
        }
        std::sort(idx.begin(), idx.end(), [&](int i, int j) {
            if (std::abs(segs[i].c - segs[j].c) > 1e-6)
                return segs[i].c < segs[j].c;
            return segs[i].a < segs[j].a;
        });

        std::vector<bool> used(idx.size(), false);
        for (size_t ii = 0; ii < idx.size(); ++ii) {
            if (used[ii])
                continue;
            used[ii] = true;
            double cSum = segs[idx[ii]].c;
            int n = 1;
            double a = segs[idx[ii]].a;
            double b = segs[idx[ii]].b;
            int votes = segs[idx[ii]].votes;

            // Grow by absorbing thickness-siblings on same centerline strip
            bool grew = true;
            while (grew) {
                grew = false;
                for (size_t jj = 0; jj < idx.size(); ++jj) {
                    if (used[jj])
                        continue;
                    const Seg &s = segs[idx[jj]];
                    if (std::abs(s.c - cSum / n) > band)
                        continue;
                    // Require real overlap (or 2px touch) — do not span door gaps
                    if (s.b < a - bridge || s.a > b + bridge)
                        continue;
                    // Also require meaningful overlap length when both are long
                    const double overlap = std::min(b, s.b) - std::max(a, s.a);
                    const double minLen = std::min(b - a, s.b - s.a);
                    if (overlap < -bridge)
                        continue;
                    if (minLen > 40 && overlap < minLen * 0.15 && overlap < 10)
                        continue;
                    used[jj] = true;
                    cSum += s.c;
                    ++n;
                    a = std::min(a, s.a);
                    b = std::max(b, s.b);
                    votes = std::max(votes, s.votes);
                    grew = true;
                }
            }
            const double c = cSum / n;
            if (horizontal)
                out.push_back(makeLine(QPointF(a, c), QPointF(b, c), votes));
            else
                out.push_back(makeLine(QPointF(c, a), QPointF(c, b), votes));
        }
    };

    process(true);
    process(false);
    lines = std::move(out);
}


// --- filterStairTreads ---
void filterStairTreads(std::vector<LineExtractor::ExtractedLine> &lines)
{
    if (lines.size() < 4)
        return;
    std::vector<int> hor;
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const float a = std::abs(lines[i].angle);
        if (a < 15.f || a > 165.f)
            hor.push_back(i);
    }
    if (hor.size() < 4)
        return;

    std::sort(hor.begin(), hor.end(), [&](int i, int j) {
        return 0.5 * (lines[i].start.y() + lines[i].end.y()) <
               0.5 * (lines[j].start.y() + lines[j].end.y());
    });

    std::vector<bool> drop(lines.size(), false);
    // Find runs of many short H lines with similar length & regular spacing
    for (size_t i = 0; i < hor.size();) {
        size_t j = i + 1;
        const double y0 =
            0.5 * (lines[hor[i]].start.y() + lines[hor[i]].end.y());
        const double len0 = lines[hor[i]].length;
        const double x0 =
            0.5 * (lines[hor[i]].start.x() + lines[hor[i]].end.x());
        while (j < hor.size()) {
            const double y =
                0.5 * (lines[hor[j]].start.y() + lines[hor[j]].end.y());
            const double len = lines[hor[j]].length;
            const double x =
                0.5 * (lines[hor[j]].start.x() + lines[hor[j]].end.x());
            if (y - y0 > 200)
                break;
            if (std::abs(len - len0) > len0 * 0.35)
                break;
            if (std::abs(x - x0) > len0 * 0.35)
                break;
            ++j;
        }
        const int count = static_cast<int>(j - i);
        if (count >= 5 && len0 < 200) {
            // Stair pack — drop all
            for (size_t k = i; k < j; ++k)
                drop[hor[k]] = true;
        }
        i = std::max(j, i + 1);
    }

    std::vector<LineExtractor::ExtractedLine> kept;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (!drop[i])
            kept.push_back(lines[i]);
    }
    lines = std::move(kept);
}


// --- mergeCollinearSegments ---
void LineExtractor::mergeCollinearSegments(std::vector<ExtractedLine> &lines,
                                          double maxGap, double angleTol)
{
    if (lines.size() < 2)
        return;

    bool merged = true;
    int guard = 0;
    while (merged && guard++ < 50) {
        merged = false;
        for (size_t i = 0; i < lines.size() && !merged; ++i) {
            for (size_t j = i + 1; j < lines.size() && !merged; ++j) {
                float angleDiff = std::abs(lines[i].angle - lines[j].angle);
                if (angleDiff > 180.0f)
                    angleDiff = 360.0f - angleDiff;
                const bool collinear =
                    (angleDiff < angleTol) || (std::abs(angleDiff - 180.0f) < angleTol);
                if (!collinear)
                    continue;

                auto dist = [](QPointF a, QPointF b) {
                    return std::hypot(a.x() - b.x(), a.y() - b.y());
                };

                QPointF d(lines[i].end.x() - lines[i].start.x(),
                          lines[i].end.y() - lines[i].start.y());
                const double len = std::hypot(d.x(), d.y());
                if (len < 1e-6)
                    continue;
                QPointF n(-d.y() / len, d.x() / len);

                auto distToLine = [&](QPointF p) {
                    return std::abs((p.x() - lines[i].start.x()) * n.x() +
                                    (p.y() - lines[i].start.y()) * n.y());
                };

                // Lateral tolerance must stay tight — maxGap is for along-axis
                // door bridges only. Using maxGap here welded parallel walls
                // 8–12px apart (e.g. stair side at x=510 into wall at x=500).
                const double lateralTol = std::min(2.5, maxGap * 0.35);
                if (distToLine(lines[j].start) > lateralTol ||
                    distToLine(lines[j].end) > lateralTol)
                    continue;

                QPointF dir(d.x() / len, d.y() / len);
                QPointF origin = lines[i].start;
                auto project = [&](QPointF p) {
                    return (p.x() - origin.x()) * dir.x() +
                           (p.y() - origin.y()) * dir.y();
                };
                const double a0 = std::min(project(lines[i].start), project(lines[i].end));
                const double a1 = std::max(project(lines[i].start), project(lines[i].end));
                const double b0 = std::min(project(lines[j].start), project(lines[j].end));
                const double b1 = std::max(project(lines[j].start), project(lines[j].end));
                const double alongGap = std::max(0.0, std::max(b0 - a1, a0 - b1));
                if (alongGap > maxGap * 2.0)
                    continue;

                // Reject merges that would bridge a large clear gap relative to
                // either segment (door openings).
                const double minSeg = std::min(a1 - a0, b1 - b0);
                if (alongGap > 16.0 && alongGap > minSeg * 0.25)
                    continue;

                const double tMin = std::min(a0, b0);
                const double tMax = std::max(a1, b1);

                lines[i] = makeLine(
                    QPointF(origin.x() + tMin * dir.x(), origin.y() + tMin * dir.y()),
                    QPointF(origin.x() + tMax * dir.x(), origin.y() + tMax * dir.y()),
                    std::max(lines[i].votes, lines[j].votes));
                lines.erase(lines.begin() + static_cast<long>(j));
                merged = true;
            }
        }
    }
}


// --- snapToAxisAligned ---
void LineExtractor::snapToAxisAligned(std::vector<ExtractedLine> &lines,
                                      double angleTolerance)
{
    for (auto &line : lines) {
        const double angle =
            std::atan2(line.end.y() - line.start.y(), line.end.x() - line.start.x()) *
            180.0 / M_PI;
        if (std::abs(angle) < angleTolerance ||
            std::abs(angle - 180.0) < angleTolerance ||
            std::abs(angle + 180.0) < angleTolerance) {
            const double midY = (line.start.y() + line.end.y()) * 0.5;
            line.start.setY(midY);
            line.end.setY(midY);
            line.angle = (line.end.x() >= line.start.x()) ? 0.0f : 180.0f;
        } else if (std::abs(angle - 90.0) < angleTolerance ||
                   std::abs(angle + 90.0) < angleTolerance) {
            const double midX = (line.start.x() + line.end.x()) * 0.5;
            line.start.setX(midX);
            line.end.setX(midX);
            line.angle = (line.end.y() >= line.start.y()) ? 90.0f : -90.0f;
        }
        const double dx = line.end.x() - line.start.x();
        const double dy = line.end.y() - line.start.y();
        line.length = static_cast<float>(std::hypot(dx, dy));
    }
}


// --- removeDuplicates ---
void LineExtractor::removeDuplicates(std::vector<ExtractedLine> &lines,
                                     double distThreshold)
{
    auto dist = [](QPointF a, QPointF b) {
        return std::hypot(a.x() - b.x(), a.y() - b.y());
    };
    std::vector<bool> toRemove(lines.size(), false);
    for (size_t i = 0; i < lines.size(); ++i) {
        if (toRemove[i])
            continue;
        for (size_t j = i + 1; j < lines.size(); ++j) {
            if (toRemove[j])
                continue;
            const bool sameForward =
                dist(lines[i].start, lines[j].start) < distThreshold &&
                dist(lines[i].end, lines[j].end) < distThreshold;
            const bool sameReversed =
                dist(lines[i].start, lines[j].end) < distThreshold &&
                dist(lines[i].end, lines[j].start) < distThreshold;

            // Also collapse nearly-overlapping parallel walls (thick double-stroke)
            float angleDiff = std::abs(lines[i].angle - lines[j].angle);
            if (angleDiff > 180.f)
                angleDiff = 360.f - angleDiff;
            const bool parallel =
                angleDiff < 8.f || std::abs(angleDiff - 180.f) < 8.f;
            bool overlapParallel = false;
            if (parallel && !sameForward && !sameReversed) {
                // Midpoint distance alone is wrong for staggered collinear
                // segments; require near-identical axis + overlapping range.
                const float ai = std::abs(lines[i].angle);
                const bool hor = (ai < 20.f || ai > 160.f);
                const double ci = hor ? 0.5 * (lines[i].start.y() + lines[i].end.y())
                                     : 0.5 * (lines[i].start.x() + lines[i].end.x());
                const double cj = hor ? 0.5 * (lines[j].start.y() + lines[j].end.y())
                                     : 0.5 * (lines[j].start.x() + lines[j].end.x());
                if (std::abs(ci - cj) < distThreshold * 0.75) {
                    const double a0 = hor ? std::min(lines[i].start.x(), lines[i].end.x())
                                          : std::min(lines[i].start.y(), lines[i].end.y());
                    const double a1 = hor ? std::max(lines[i].start.x(), lines[i].end.x())
                                          : std::max(lines[i].start.y(), lines[i].end.y());
                    const double b0 = hor ? std::min(lines[j].start.x(), lines[j].end.x())
                                          : std::min(lines[j].start.y(), lines[j].end.y());
                    const double b1 = hor ? std::max(lines[j].start.x(), lines[j].end.x())
                                          : std::max(lines[j].start.y(), lines[j].end.y());
                    const double overlap = std::min(a1, b1) - std::max(a0, b0);
                    const double minLen = std::min(a1 - a0, b1 - b0);
                    if (overlap > minLen * 0.6)
                        overlapParallel = true;
                }
            }

            if (sameForward || sameReversed || overlapParallel) {
                if (lines[j].length > lines[i].length ||
                    (lines[j].votes > lines[i].votes &&
                     lines[j].length >= lines[i].length * 0.9f)) {
                    toRemove[i] = true;
                    break;
                }
                toRemove[j] = true;
            }
        }
    }
    std::vector<ExtractedLine> filtered;
    filtered.reserve(lines.size());
    for (size_t i = 0; i < lines.size(); ++i) {
        if (!toRemove[i])
            filtered.push_back(lines[i]);
    }
    lines = std::move(filtered);
}


// --- removeShortRemnants ---
void LineExtractor::removeShortRemnants(std::vector<ExtractedLine> &lines,
                                        double minLength)
{
    lines.erase(std::remove_if(lines.begin(), lines.end(),
                               [minLength](const ExtractedLine &l) {
                                   return l.length < minLength;
                               }),
                lines.end());
}


