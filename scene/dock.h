// dock.h: a dry dock: the hull on its keel blocks, floodlights, the foghorn
// tower that calls the owner, and the tarp over a dock nobody works any more.
#pragma once

#include "yard.h"

namespace shipyard {

inline void drawKeelBlocks(Host& h, const Dock& d) {
    const float stern = -d.len * 0.5f;
    for (float kz = stern + 0.6f; kz < stern + d.len - 0.4f; kz += 1.6f)
        span(h, d.x - 0.5f, kFloor, kz - 0.3f, d.x + 0.5f, kKeelTop, kz + 0.3f, kWood);
}

// One plated layer: a box for the body and three narrowing steps for the bow (+z).
inline void drawPlatedLayer(Host& h, const Dock& d, float y0, float y1, float z0, float w, float l, Rgb c) {
    const float body = l * 0.7f, seg = (l - body) / 3.0f;
    span(h, d.x - w * 0.5f, y0, z0, d.x + w * 0.5f, y1, z0 + body, c);
    for (int b = 0; b < 3; ++b) {
        const float bw = w * (0.78f - 0.26f * b);
        span(h, d.x - bw * 0.5f, y0, z0 + body + b * seg, d.x + bw * 0.5f, y1, z0 + body + (b + 1) * seg, c);
    }
}

// A layer still waiting for its plates: bare ribs up each side and a stringer.
inline void drawBareLayer(Host& h, const Dock& d, float y0, float y1, float z0, float w, float l) {
    for (float rz = z0 + 0.3f; rz < z0 + l * 0.85f; rz += 1.2f)
        for (int s = -1; s <= 1; s += 2)
            span(h, d.x + s * w * 0.5f - 0.1f, y0, rz - 0.08f, d.x + s * w * 0.5f + 0.1f, y1, rz + 0.08f, kRib);
    span(h, d.x - w * 0.5f, y1 - 0.1f, z0, d.x + w * 0.5f, y1, z0 + 0.15f, kRib);
}

// The hull grows with the place's tool calls: longer, and plated higher.
inline void drawHull(Host& h, const Dock& d) {
    drawKeelBlocks(h, d);
    const float lh = d.height / kHullLayers, stern = -d.len * 0.5f;
    for (int k = 0; k < kHullLayers; ++k) {
        const float f = static_cast<float>(k) / (kHullLayers - 1);
        const float w = d.width * (0.35f + 0.65f * std::sqrt(f)), l = d.len * (0.78f + 0.22f * f);
        const float y0 = kKeelTop + k * lh, z0 = stern + (d.len - l) * 0.15f;
        if (k >= d.plated) {
            drawBareLayer(h, d, y0, y0 + lh, z0, w, l);
            continue;
        }
        const Rgb steel = k < 2 ? kRust : mix(kRust, kGreyPlate, (f - 0.3f) * 1.4f);
        drawPlatedLayer(h, d, y0, y0 + lh, z0, w, l, mul(steel, 0.9f + 0.1f * (k & 1)));
    }
}

// Floodlights at the landward corners: bright while anyone works, low when idle, off when gone.
inline void drawFloodlights(Host& h, const Dock& d) {
    const float live = d.offline ? 0.0f : (d.busy ? 1.0f : 0.25f);
    for (int s = -1; s <= 1; s += 2) {
        const float x = d.x + s * (kBasinHW + 1.6f), z = -kBasinHL + 1.0f;
        box(h, x, 3.5f, z, 0.25f, 7.0f, 0.25f, kPole);
        box(h, x - s * 0.3f, 7.1f, z, 0.9f, 0.4f, 0.6f, kDarkSteel);
        if (live <= 0.0f) continue;
        h.point(x - s * 0.4f, 7.0f, z + 0.2f, 14.0f * live, 0.6f, kLamp, 0.4f + 0.6f * live);
        h.light(x - s * 2.0f, 6.0f, z + 3.0f, 18.0f, {1.0f, 0.86f, 0.62f}, 2.4f * live, 1);
    }
}

// The foghorn tower at the seaward end of the foremen's quay, and its flagpole.
struct Tower {
    float x, z;
    float flagX() const { return x - 1.2f; }
};
inline Tower towerOf(const Dock& d) { return {d.x - kBasinHW - 2.2f, kBasinHL - 1.2f}; }

inline void drawTowerBody(Host& h, const Tower& tw) {
    for (int k = 0; k < 6; ++k)
        box(h, tw.x, 0.5f + k, tw.z, 1.0f, 1.0f, 1.0f, (k & 1) ? Rgb{0.85f, 0.85f, 0.82f} : Rgb{0.70f, 0.12f, 0.10f});
    box(h, tw.x, 6.15f, tw.z, 1.3f, 0.3f, 1.3f, kDarkSteel);
    box(h, tw.flagX(), 4.0f, tw.z, 0.1f, 8.0f, 0.1f, kPole);
}

// Someone at this dock needs the owner: the lamp flashes red, the flag flies,
// red rings spread on the water and a red beam climbs into the sky.
inline void drawFoghornCalling(Host& h, const Tower& tw, float t) {
    const bool on = cycle(t, 1.0f) < 0.55f;
    ball(h, tw.x, 6.8f, tw.z, 1.1f, on ? Rgb{1.0f, 0.15f, 0.1f} : Rgb{0.4f, 0.05f, 0.04f});
    if (on) {
        h.point(tw.x, 6.8f, tw.z, 90.0f, 5.0f, kRed, 0.95f);
        h.light(tw.x, 6.5f, tw.z - 1.5f, 26.0f, {1.0f, 0.1f, 0.06f}, 7.0f, 20);
    }
    for (int k = 0; k < 4; ++k) {
        const float ripple = 0.12f * std::sin(t * 6.0f - k * 1.2f);
        box(h, tw.flagX() - 0.35f - k * 0.5f, 7.45f + ripple, tw.z, 0.5f, 1.0f, 0.06f,
            (k & 1) ? Rgb{1.0f, 0.9f, 0.1f} : Rgb{0.95f, 0.1f, 0.05f});
    }
    for (int k = 0; k < 3; ++k) {
        const float ph = cycle(t, 0.5f, k / 3.0f);
        ring(h, tw.x, kSeaY + 0.05f, kBasinHL + 2.0f, 1.5f + ph * 9.0f, kRed, 0.9f * (1.0f - ph));
    }
    thick(h, tw.x, 7.0f, tw.z, tw.x, 30.0f, tw.z, kRed, 6.0f, 0.35f + 0.35f * agentviz::pulse(t, 1.0f));
}

inline void drawFoghornQuiet(Host& h, const Tower& tw) {
    ball(h, tw.x, 6.8f, tw.z, 0.8f, {0.25f, 0.22f, 0.2f});
    box(h, tw.flagX() - 0.2f, 0.7f, tw.z, 0.3f, 0.5f, 0.12f, {0.6f, 0.1f, 0.06f});  // furled at the foot
}

// Every session here is gone: a tarp, battened down, over the whole dock.
inline void drawDockTarp(Host& h, const Dock& d) {
    span(h, d.x - kBasinHW - 0.4f, 0.0f, -kBasinHL - 0.4f, d.x + kBasinHW + 0.4f, 0.35f, kBasinHL + 0.4f, kTarp);
    for (int k = -4; k <= 4; ++k)
        span(h, d.x - kBasinHW - 0.4f, 0.35f, k * 2.4f - 0.1f, d.x + kBasinHW + 0.4f, 0.42f, k * 2.4f + 0.1f,
             mul(kTarp, 0.7f));
}

inline void drawDock(Host& h, const Dock& d, float t) {
    drawHull(h, d);
    drawFloodlights(h, d);
    const Tower tw = towerOf(d);
    drawTowerBody(h, tw);
    if (d.waiting) drawFoghornCalling(h, tw, t);
    else drawFoghornQuiet(h, tw);
    if (d.selected) ring(h, d.x, 0.2f, 0.0f, kBasinHL + 1.0f, kWhite, 0.6f, 48);
    if (d.offline) drawDockTarp(h, d);
}

}  // namespace shipyard
