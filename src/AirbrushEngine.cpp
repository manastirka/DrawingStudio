#include "AirbrushEngine.h"

#include <QtGlobal>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>

namespace AirbrushEngine {
namespace {
constexpr float kPi = 3.14159265358979323846f;
}

void beginStroke(State &state, const QVector2D &cursorPos)
{
    state.lastPos = cursorPos;
    state.sprayLastPos = cursorPos;
    state.stationarySeconds = 0.f;
    state.drips.clear();
}

void clearDrips(State &state)
{
    state.drips.clear();
}

bool tick(State &state, const QVector2D &cursorPos, const Params &params,
          std::vector<QVector2D> &strokeOut,
          std::vector<float> &particleScaleOut,
          std::vector<float> &particleAlphaOut)
{
    const float dt = params.dt > 0.f ? params.dt : 0.016f;
    const float radius = qMax(2.0f, params.brushSize * 0.65f);
    const int particlesPerTick =
        qBound(10, static_cast<int>(params.brushSize * 1.8f), 70);
    constexpr float stationaryEps = 0.8f;

    const float moved = (cursorPos - state.lastPos).length();
    const bool stationary = (moved < stationaryEps);
    if (stationary) {
        state.stationarySeconds += dt;
    } else {
        state.stationarySeconds = 0.f;
        state.lastPos = cursorPos;
    }

    const int tickParticles =
        stationary ? qMin(140, particlesPerTick * 2) : particlesPerTick;

    const float segmentLen = (cursorPos - state.sprayLastPos).length();
    const float step = qMax(0.8f, radius * 0.22f);
    const int steps = qMax(1, static_cast<int>(std::ceil(segmentLen / step)));
    const int perStep = qMax(1, tickParticles / steps);

    const size_t before = strokeOut.size();
    auto *rng = QRandomGenerator::global();

    for (int s = 1; s <= steps; ++s) {
        const float t = static_cast<float>(s) / static_cast<float>(steps);
        const QVector2D center =
            state.sprayLastPos + (cursorPos - state.sprayLastPos) * t;

        for (int i = 0; i < perStep; ++i) {
            const float u = static_cast<float>(rng->generateDouble());
            const float v = static_cast<float>(rng->generateDouble());
            const float angle = u * 2.0f * kPi;
            const float exp =
                1.15f + (2.2f * qBound(0.0f, params.hardness, 1.0f));
            const float r = radius * std::pow(v, exp);

            const QVector2D p =
                center + QVector2D(std::cos(angle) * r, std::sin(angle) * r);
            strokeOut.push_back(p);

            const float nr = qBound(0.0f, r / radius, 1.0f);
            constexpr float sigma = 0.55f;
            const float radial =
                std::exp(-(nr * nr) / (2.0f * sigma * sigma));
            const float scaleJitter =
                (0.80f + 0.45f * static_cast<float>(rng->generateDouble())) *
                (0.85f + 0.25f * radial);
            const float alphaJitter =
                (0.55f + 0.45f * static_cast<float>(rng->generateDouble())) *
                radial;
            particleScaleOut.push_back(scaleJitter);
            particleAlphaOut.push_back(alphaJitter);
        }
    }

    state.sprayLastPos = cursorPos;

    constexpr float leakStartSec = 1.3f;
    if (state.stationarySeconds > leakStartSec) {
        if (rng->bounded(100) < 12) {
            Drip drip;
            drip.pos =
                cursorPos +
                QVector2D(static_cast<float>(rng->bounded(-2, 3)), 0.0f);
            // World Y grows up → drip uses negative Y velocity.
            drip.vel = QVector2D(
                0.0f, -(6.0f + static_cast<float>(rng->bounded(0, 9))));
            drip.life =
                1.8f + static_cast<float>(rng->bounded(0, 13)) / 10.0f;
            state.drips.push_back(drip);
        }

        for (auto &drip : state.drips) {
            drip.pos += drip.vel * dt;
            drip.vel.setY(drip.vel.y() * 1.02f);
            drip.life -= dt;
            strokeOut.push_back(drip.pos);
            particleScaleOut.push_back(0.55f);
            particleAlphaOut.push_back(0.85f);
        }

        state.drips.erase(
            std::remove_if(state.drips.begin(), state.drips.end(),
                           [](const Drip &d) { return d.life <= 0.0f; }),
            state.drips.end());
    } else {
        state.drips.clear();
    }

    return strokeOut.size() > before;
}

} // namespace AirbrushEngine
