#pragma once

#include <QVector2D>
#include <vector>

/**
 * Pure airbrush particle simulation (refactor D1).
 * Timer ownership stays on DrawingCanvas; this module advances spray + drips.
 */
namespace AirbrushEngine {

struct Drip {
    QVector2D pos;
    QVector2D vel;
    float life = 0.f;
};

struct State {
    QVector2D lastPos;
    QVector2D sprayLastPos;
    float stationarySeconds = 0.f;
    std::vector<Drip> drips;
};

struct Params {
    float brushSize = 10.f;
    float hardness = 0.5f;
    float dt = 0.016f; // ~60 FPS
};

/** Reset motion tracking when a stroke begins. */
void beginStroke(State &state, const QVector2D &cursorPos);

/**
 * One timer tick: appends mist + drip particles into stroke buffers.
 * @return true if any particles were added (caller may request repaint).
 */
bool tick(State &state, const QVector2D &cursorPos, const Params &params,
          std::vector<QVector2D> &strokeOut,
          std::vector<float> &particleScaleOut,
          std::vector<float> &particleAlphaOut);

void clearDrips(State &state);

} // namespace AirbrushEngine
