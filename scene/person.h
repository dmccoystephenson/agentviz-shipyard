// person.h: a yard worker and the things in their hands: welding torch
// sparks, a blueprint, a signal lamp, a hammer, thought dots.
#pragma once

#include "draw_util.h"

namespace shipyard {

// stand: arms down; wave: both arms up; sit: legs out over an edge; reach: one arm out.
enum class Pose { Stand, Wave, Sit, Reach };

struct Figure {
    float x, y, z, face;  // feet; facing +x when face > 0
    Pose pose;
    Rgb hat, body;
    float seed;
    bool foreman;  // a broader chest
};

inline void drawLegs(Host& h, const Figure& p) {
    if (p.pose != Pose::Sit) {
        box(h, p.x, p.y + 0.42f, p.z, 0.32f, 0.84f, 0.3f, p.body);
        return;
    }
    // Thighs out over the plank edge, shins hanging.
    span(h, p.x - p.face * 0.05f, p.y, p.z - 0.17f, p.x - p.face * 0.55f, p.y + 0.2f, p.z + 0.17f, p.body);
    span(h, p.x - p.face * 0.45f, p.y - 0.55f, p.z - 0.17f, p.x - p.face * 0.62f, p.y + 0.05f, p.z + 0.17f, p.body);
}

inline void drawArms(Host& h, const Figure& p, float chest, float t) {
    for (int s = -1; s <= 1; s += 2) {
        const float az = p.z + s * 0.33f;
        if (p.pose == Pose::Wave) {
            const float wave = 0.15f * std::sin(t * 7.0f + s * 1.6f + p.seed * 6.0f);
            box(h, p.x + wave, chest + 0.7f, az, 0.13f, 0.62f, 0.13f, p.body);
        } else if (p.pose == Pose::Reach && s == 1) {
            span(h, p.x, chest + 0.18f, az - 0.06f, p.x + p.face * 0.7f, chest + 0.3f, az + 0.06f, p.body);
        } else {
            box(h, p.x, chest - 0.05f, az, 0.13f, 0.62f, 0.13f, p.body);
        }
    }
}

inline void drawFigure(Host& h, const Figure& p, float t) {
    drawLegs(h, p);
    const float chest = p.y + 1.2f - (p.pose == Pose::Sit ? 0.55f : 0.0f);
    box(h, p.x, chest, p.z, 0.34f, 0.72f, 0.52f, p.body);
    if (p.foreman) box(h, p.x + p.face * 0.01f, chest + 0.06f, p.z, 0.36f, 0.5f, 0.54f, p.body);
    ball(h, p.x, chest + 0.56f, p.z, 0.34f, kSkin);
    h.add(STRATA_PRIMITIVE_SPHERE, p.x, chest + 0.68f, p.z, 0.42f, 0.24f, 0.42f, p.hat);  // hard hat
    box(h, p.x + p.face * 0.14f, chest + 0.64f, p.z, 0.24f, 0.04f, 0.4f, p.hat);          // its brim
    drawArms(h, p, chest, t);
}

// Welding: sparks falling from (x, y, z) and a flickering blue-white light.
inline void drawSparks(Host& h, float t, float x, float y, float z, float seed, float dir, int prio, float scale) {
    for (int i = 0; i < 14; ++i) {
        const float ph = cycle(t, 1.6f, hash(seed, i));
        const float vx = (hash(seed, i + 40) - 0.5f) * 2.2f + dir * 0.8f, vz = (hash(seed, i + 80) - 0.5f) * 2.2f;
        const float vy = 1.2f + hash(seed, i + 120) * 1.6f, tt = ph * 0.9f;
        h.point(x + vx * tt * scale, y + (vy * tt - 4.9f * tt * tt) * scale, z + vz * tt * scale, 4.0f, 0.06f, kSpark,
                1.0f - ph);
    }
    const float flicker = 0.5f + 0.5f * std::sin(t * 37.0f + seed * 20.0f) * std::sin(t * 23.0f);
    h.point(x, y, z, 18.0f * scale, 0.35f * scale, kWeld, 0.6f + 0.4f * flicker);
    h.light(x - dir * 0.4f, y + 0.2f, z, 6.0f, kWeld, 1.2f + 2.0f * flicker, prio);
}

inline void drawBlueprint(Host& h, float x, float y, float z, float face, float size) {
    box(h, x + face * 0.42f * size, y, z, 0.5f * size, 0.03f, 0.7f * size, kBlueprint);
    for (int k = 0; k < 3; ++k) {
        const float lz = z - 0.22f * size + k * 0.22f * size;
        h.line(x + face * 0.25f * size, y + 0.03f, lz, x + face * 0.6f * size, y + 0.03f, lz, {0.9f, 0.95f, 1.0f}, 0.9f);
    }
}

// A signal lamp blinking Morse at the ship offshore (shipX), dots streaming out to it.
inline void drawSignalLamp(Host& h, float t, float x, float y, float z, float seed, float shipX, float alpha) {
    const float ph = std::fmod(t * 2.2f + seed * 5.0f, 3.0f);
    const bool on = ph < 0.35f || (ph > 0.6f && ph < 0.8f) || (ph > 1.1f && ph < 1.9f);
    box(h, x, y, z, 0.3f, 0.3f, 0.3f, {0.15f, 0.15f, 0.15f});
    if (!on) return;
    h.point(x, y, z, 22.0f, 0.5f, kLamp, 1.0f);
    const float sy = kSeaY + 4.2f;
    for (int k = 0; k < 10; ++k) {
        const float f = cycle(t, 0.6f, k / 10.0f);
        h.point(x + (shipX - x) * f, y + (sy - y) * f, z + (kShipZ - z) * f, 6.0f, 0.2f, kLamp, alpha * (1.0f - f * 0.5f));
    }
}

inline void drawHammer(Host& h, float t, float x, float y, float z, float face, float seed) {
    const float up = 0.18f * std::fabs(std::sin(t * 5.0f + seed * 4.0f));
    box(h, x + face * 0.55f, y + up, z + 0.33f, 0.08f, 0.35f, 0.08f, kWood);
    box(h, x + face * 0.55f, y + up + 0.2f, z + 0.33f, 0.14f, 0.1f, 0.26f, kPole);
}

inline void drawThought(Host& h, float t, float x, float y, float z, float seed) {
    for (int k = 0; k < 3; ++k) {
        const float a = t * 1.8f + k * 2.094f + seed * 6.0f;
        h.point(x + std::cos(a) * 0.45f, y + 0.12f * k, z + std::sin(a) * 0.45f, 7.0f - k, 0.14f, {0.95f, 0.95f, 1.0f},
                0.85f);
    }
}

}  // namespace shipyard
