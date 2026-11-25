// draw_util.h: the yard's measurements, colours and primitive shapes.
#pragma once

#include <algorithm>
#include <cmath>

#include "strata_kit.h"

namespace shipyard {

using strata_kit::Host;
using strata_kit::Rgb;

constexpr float kPi = 3.14159265f;

// Measurements shared with web/shipyard-model.js: keep in step.
constexpr float kBasinHW = 5.5f;   // half the dry dock's width (x)
constexpr float kBasinHL = 11.0f;  // half its length (z); the gate faces the sea at +z
constexpr float kFloor = -2.6f;    // the basin floor
constexpr float kKeelTop = kFloor + 0.6f;  // the hull sits on keel blocks this high
constexpr float kSeaY = -0.7f;
constexpr float kShipZ = 48.0f;    // the ship offshore that signal lamps talk to
constexpr float kCraneTop = 10.0f;

const Rgb kConcrete{0.42f, 0.41f, 0.39f}, kConcreteDark{0.30f, 0.30f, 0.29f}, kSea{0.05f, 0.16f, 0.24f},
    kFloorC{0.24f, 0.22f, 0.20f}, kKerb{0.75f, 0.62f, 0.15f}, kRust{0.50f, 0.17f, 0.10f},
    kGreyPlate{0.40f, 0.43f, 0.46f}, kRib{0.20f, 0.20f, 0.22f}, kWood{0.55f, 0.40f, 0.22f},
    kPole{0.55f, 0.56f, 0.58f}, kCrane{0.92f, 0.68f, 0.08f}, kSkin{0.86f, 0.66f, 0.50f},
    kOveralls{0.18f, 0.24f, 0.36f}, kShirt{0.85f, 0.85f, 0.80f}, kTarp{0.16f, 0.30f, 0.20f},
    kBlueprint{0.15f, 0.35f, 0.85f}, kSpark{1.0f, 0.85f, 0.45f}, kWeld{0.7f, 0.85f, 1.0f},
    kRed{1.0f, 0.08f, 0.05f}, kLamp{1.0f, 0.88f, 0.35f}, kAmber{1.0f, 0.62f, 0.10f}, kWhite{1.0f, 1.0f, 1.0f},
    kInk{0.1f, 0.1f, 0.1f}, kDarkSteel{0.2f, 0.2f, 0.2f};

inline float hash(float s, float k) {
    const float v = std::sin(s * 127.1f + k * 311.7f) * 43758.5453f;
    return v - std::floor(v);
}
// 0..1 sawtooth at `rate` cycles a second, offset by `phase` cycles.
inline float cycle(float t, float rate, float phase = 0.0f) {
    const float v = t * rate + phase;
    return v - std::floor(v);
}

inline Rgb mul(Rgb c, float k) { return {c.r * k, c.g * k, c.b * k}; }
inline Rgb mix(Rgb a, Rgb b, float k) { return {a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k}; }

inline void box(Host& h, float x, float y, float z, float sx, float sy, float sz, Rgb c) {
    h.add(STRATA_PRIMITIVE_CUBE, x, y, z, sx, sy, sz, c);
}
inline void ball(Host& h, float x, float y, float z, float d, Rgb c) {
    h.add(STRATA_PRIMITIVE_SPHERE, x, y, z, d, d, d, c);
}
// A box given by two opposite corners.
inline void span(Host& h, float x0, float y0, float z0, float x1, float y1, float z1, Rgb c) {
    box(h, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f, std::fabs(x1 - x0), std::fabs(y1 - y0),
        std::fabs(z1 - z0), c);
}
inline void thick(Host& h, float x0, float y0, float z0, float x1, float y1, float z1, Rgb c, float px, float a) {
    h.wideLine(x0, y0, z0, x1, y1, z1, c, px, 0.0f, a);
}
// A flat ring on the ground (or water) at height y.
inline void ring(Host& h, float x, float y, float z, float r, Rgb c, float a, int n = 28) {
    for (int k = 0; k < n; ++k) {
        const float a0 = k * 2 * kPi / n, a1 = (k + 1) * 2 * kPi / n;
        thick(h, x + std::cos(a0) * r, y, z + std::sin(a0) * r, x + std::cos(a1) * r, y, z + std::sin(a1) * r, c, 3.0f,
              a);
    }
}

// A plane facing the camera, for glyphs that must read the right way round:
// u runs screen-right, v straight up. `yaw` is the page's camera yaw.
struct Billboard {
    float x, y, z, rx, rz, size;
    void at(float u, float v, float& wx, float& wy, float& wz) const {
        wx = x + rx * u * size;
        wy = y + v * size;
        wz = z + rz * u * size;
    }
    void stroke(Host& h, float u0, float v0, float u1, float v1, Rgb c, float px) const {
        float x0, y0, z0, x1, y1, z1;
        at(u0, v0, x0, y0, z0);
        at(u1, v1, x1, y1, z1);
        thick(h, x0, y0, z0, x1, y1, z1, c, px, 1.0f);
    }
    // An arc of radius r round (cu, cv), angles in radians, counter-clockwise from screen-right.
    void arc(Host& h, float cu, float cv, float r, float from, float to, Rgb c, float px, int n = 8) const {
        for (int k = 0; k < n; ++k) {
            const float a0 = from + (to - from) * k / n, a1 = from + (to - from) * (k + 1) / n;
            stroke(h, cu + std::cos(a0) * r, cv + std::sin(a0) * r, cu + std::cos(a1) * r, cv + std::sin(a1) * r, c, px);
        }
    }
};
// Screen-right in world space for the kit's orbit camera. strata-kit.js pans
// along (-sin yaw, cos yaw), but the rendered image has +x on screen-left
// from the land side (agentviz-hive's cell.h notes the same), so glyphs use
// the opposite vector or they read mirrored.
inline Billboard billboard(float x, float y, float z, float yaw, float size) {
    return {x, y, z, std::sin(yaw), -std::cos(yaw), size};
}

}  // namespace shipyard
