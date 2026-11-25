// agentviz-shipyard: Claude Code sessions as a shipyard along a harbour.
//
// Each place (repo) is a dry dock with a hull under construction; each
// session is a foreman on the quay beside it, with a crane; each subagent is
// a welder on the scaffolding round the hull, in its lead's hard-hat colour.
// The page (web/shipyard.js) lays everything out from /api/agents and hands
// it here as float rows every frame (sy_set_docks, sy_set_figures); this file
// only draws, and every animation is a function of scene time t.
#define STRATA_KIT_MAIN
#include "strata_kit.h"

#include <algorithm>
#include <cmath>
#include <vector>

using strata_kit::Host;
using strata_kit::Rgb;

namespace {

// Rows from the page. Keep in step with web/shipyard.js (DOCK_STRIDE, FIG_STRIDE).
struct Dock {
    float x, z, hullLen, waiting, busy, offline, stalled, seed, selected, progress;
};
struct Fig {
    float kind;     // 0 foreman (session), 1 welder (subagent)
    float x, y, z;  // feet
    float state;    // foreman: 0 offline 1 dim 2 idle 3 busy 4 waiting; welder: 0 resting 1 active
    float action;   // 0 none 1 edit 2 run 3 read 4 web 5 delegate 6 think 7 other
    float seed, selected;
    float r, g, b;  // the lead's colour (hard hat)
    float ax, az;   // foreman: crane hook point on the hull; welder: torch point on the hull
    float side;     // welder: +1 when the hull is at +x, -1 at -x
    float hullTop;  // y of the hull's top at this dock
    float parent;   // welder: row index of its foreman; foreman: -1
};
static_assert(sizeof(Dock) == 10 * sizeof(float), "dock stride");
static_assert(sizeof(Fig) == 16 * sizeof(float), "figure stride");

std::vector<Dock> docks;
std::vector<Fig> figs;
float shipX = 0.0f;

constexpr float kBasinHW = 5.5f, kBasinHL = 11.0f, kFloor = -2.6f, kSeaY = -0.7f, kShipZ = 48.0f;
constexpr float kPi = 3.14159265f;

const Rgb kConcrete{0.42f, 0.41f, 0.39f}, kConcreteDark{0.30f, 0.30f, 0.29f}, kSea{0.05f, 0.16f, 0.24f},
    kFloorC{0.24f, 0.22f, 0.20f}, kRust{0.50f, 0.17f, 0.10f}, kGreyPlate{0.40f, 0.43f, 0.46f},
    kRib{0.20f, 0.20f, 0.22f}, kWood{0.55f, 0.40f, 0.22f}, kPole{0.55f, 0.56f, 0.58f},
    kCrane{0.92f, 0.68f, 0.08f}, kSkin{0.86f, 0.66f, 0.50f}, kOveralls{0.18f, 0.24f, 0.36f},
    kShirt{0.85f, 0.85f, 0.80f}, kTarp{0.16f, 0.30f, 0.20f}, kBlueprint{0.15f, 0.35f, 0.85f},
    kSpark{1.0f, 0.85f, 0.45f}, kWeld{0.7f, 0.85f, 1.0f}, kRed{1.0f, 0.08f, 0.05f}, kLamp{1.0f, 0.88f, 0.35f};

float hash(float s, float k) {
    const float v = std::sin(s * 127.1f + k * 311.7f) * 43758.5453f;
    return v - std::floor(v);
}

Rgb mul(Rgb c, float k) { return {c.r * k, c.g * k, c.b * k}; }
Rgb mix(Rgb a, Rgb b, float k) { return {a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k}; }

void box(Host& h, float x, float y, float z, float sx, float sy, float sz, Rgb c) {
    h.add(STRATA_PRIMITIVE_CUBE, x, y, z, sx, sy, sz, c);
}
// A box given by its two corners.
void span(Host& h, float x0, float y0, float z0, float x1, float y1, float z1, Rgb c) {
    box(h, (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f, std::fabs(x1 - x0), std::fabs(y1 - y0),
        std::fabs(z1 - z0), c);
}

void ring(Host& h, float x, float y, float z, float r, Rgb c, float a, int n = 28) {
    for (int k = 0; k < n; ++k) {
        const float a0 = k * 2 * kPi / n, a1 = (k + 1) * 2 * kPi / n;
#if STRATA_ABI_MINOR >= 13
        h.wideLine(x + std::cos(a0) * r, y, z + std::sin(a0) * r, x + std::cos(a1) * r, y, z + std::sin(a1) * r, c,
                   3.0f, 0.0f, a);
#else
        h.line(x + std::cos(a0) * r, y, z + std::sin(a0) * r, x + std::cos(a1) * r, y, z + std::sin(a1) * r, c, a);
#endif
    }
}

void thick(Host& h, float x0, float y0, float z0, float x1, float y1, float z1, Rgb c, float px, float a) {
#if STRATA_ABI_MINOR >= 13
    h.wideLine(x0, y0, z0, x1, y1, z1, c, px, 0.0f, a);
#else
    (void)px;
    h.line(x0, y0, z0, x1, y1, z1, c, a);
#endif
}

// Welding: a spray of sparks falling from (x, y, z) and a flickering light.
void sparks(Host& h, double t, float x, float y, float z, float seed, float dir, int prio, float scale = 1.0f) {
    const int n = 14;
    for (int i = 0; i < n; ++i) {
        const float ph = static_cast<float>(std::fmod(t * 1.6 + hash(seed, i), 1.0));
        const float vx = (hash(seed, i + 40) - 0.5f) * 2.2f + dir * 0.8f, vz = (hash(seed, i + 80) - 0.5f) * 2.2f;
        const float vy = 1.2f + hash(seed, i + 120) * 1.6f;
        const float tt = ph * 0.9f;
        h.point(x + vx * tt * scale, y + (vy * tt - 4.9f * tt * tt) * scale, z + vz * tt * scale, 4.0f, 0.06f,
                kSpark, 1.0f - ph);
    }
    const float flick = 0.5f + 0.5f * std::sin(static_cast<float>(t) * 37.0f + seed * 20.0f) *
                                   std::sin(static_cast<float>(t) * 23.0f);
    h.point(x, y, z, 18.0f * scale, 0.35f * scale, kWeld, 0.6f + 0.4f * flick);
    h.light(x - dir * 0.4f, y + 0.2f, z, 6.0f, kWeld, 1.2f + 2.0f * flick, prio);
}

// A person. Facing +x when face > 0. Poses: 0 stand, 1 both arms up (waving),
// 2 sit (legs out over an edge), 3 one arm out (torch / lamp).
void person(Host& h, double t, float x, float y, float z, float face, int pose, Rgb hat, Rgb body, float seed,
            bool foreman) {
    const float sitDrop = pose == 2 ? 0.55f : 0.0f;
    if (pose == 2) {
        // Thighs out over the plank edge, shins hanging.
        span(h, x - face * 0.05f, y + 0.0f, z - 0.17f, x - face * 0.55f, y + 0.2f, z + 0.17f, body);
        span(h, x - face * 0.45f, y - 0.55f, z - 0.17f, x - face * 0.62f, y + 0.05f, z + 0.17f, body);
    } else {
        box(h, x, y + 0.42f, z, 0.32f, 0.84f, 0.3f, body);
    }
    const float ty = y + 1.2f - sitDrop;
    box(h, x, ty, z, 0.34f, 0.72f, 0.52f, body);
    if (foreman) box(h, x + face * 0.01f, ty + 0.06f, z, 0.36f, 0.5f, 0.54f, body);
    h.add(STRATA_PRIMITIVE_SPHERE, x, ty + 0.56f, z, 0.34f, 0.34f, 0.34f, kSkin);
    h.add(STRATA_PRIMITIVE_SPHERE, x, ty + 0.68f, z, 0.42f, 0.24f, 0.42f, hat);
    box(h, x + face * 0.14f, ty + 0.64f, z, 0.24f, 0.04f, 0.4f, hat);  // brim
    const float t1 = static_cast<float>(t);
    for (int s = -1; s <= 1; s += 2) {
        const float az = z + s * 0.33f;
        if (pose == 1) {
            const float wave = 0.15f * std::sin(t1 * 7.0f + s * 1.6f + seed * 6.0f);
            box(h, x + wave, ty + 0.7f, az, 0.13f, 0.62f, 0.13f, body);
        } else if (pose == 3 && s == 1) {
            span(h, x, ty + 0.18f, az - 0.06f, x + face * 0.7f, ty + 0.3f, az + 0.06f, body);
        } else {
            box(h, x, ty - 0.05f, az, 0.13f, 0.62f, 0.13f, body);
        }
    }
}

void blueprint(Host& h, float x, float y, float z, float face, float size) {
    box(h, x + face * 0.42f * size, y, z, 0.5f * size, 0.03f, 0.7f * size, kBlueprint);
    for (int k = 0; k < 3; ++k) {
        const float lz = z - 0.22f * size + k * 0.22f * size;
        h.line(x + face * 0.25f * size, y + 0.03f, lz, x + face * 0.6f * size, y + 0.03f, lz, {0.9f, 0.95f, 1.0f},
               0.9f);
    }
}

// Signal lamp: blinking Morse at (x, y, z) toward the ship offshore.
void signal(Host& h, double t, float x, float y, float z, float seed, float alpha) {
    const float ph = static_cast<float>(std::fmod(t * 2.2 + seed * 5.0, 3.0));
    const bool on = ph < 0.35f || (ph > 0.6f && ph < 0.8f) || (ph > 1.1f && ph < 1.9f);
    box(h, x, y, z, 0.3f, 0.3f, 0.3f, {0.15f, 0.15f, 0.15f});
    if (!on) return;
    h.point(x, y, z, 22.0f, 0.5f, kLamp, 1.0f);
    const float sx = shipX, sy = kSeaY + 4.2f, sz = kShipZ;
    const int dots = 10;
    for (int k = 0; k < dots; ++k) {
        const float f = static_cast<float>(std::fmod(k / static_cast<float>(dots) + t * 0.6, 1.0));
        h.point(x + (sx - x) * f, y + (sy - y) * f, z + (sz - z) * f, 6.0f, 0.2f, kLamp, alpha * (1.0f - f * 0.5f));
    }
}

void hammer(Host& h, double t, float x, float y, float z, float face, float seed) {
    const float up = 0.18f * std::fabs(std::sin(static_cast<float>(t) * 5.0f + seed * 4.0f));
    box(h, x + face * 0.55f, y + up, z + 0.33f, 0.08f, 0.35f, 0.08f, kWood);
    box(h, x + face * 0.55f, y + up + 0.2f, z + 0.33f, 0.14f, 0.1f, 0.26f, kPole);
}

void thought(Host& h, double t, float x, float y, float z, float seed) {
    for (int k = 0; k < 3; ++k) {
        const float a = static_cast<float>(t) * 1.8f + k * 2.094f + seed * 6.0f;
        h.point(x + std::cos(a) * 0.45f, y + 0.12f * k, z + std::sin(a) * 0.45f, 7.0f - k, 0.14f,
                {0.95f, 0.95f, 1.0f}, 0.85f);
    }
}

void drawGround(Host& h) {
    float minX = -20.0f, maxX = 20.0f;
    if (!docks.empty()) {
        minX = docks.front().x - kBasinHW - 18.0f;
        maxX = docks.back().x + kBasinHW + 18.0f;
    }
    // Land behind the docks, and the quays between them.
    span(h, minX - 30.0f, -3.4f, -60.0f, maxX + 30.0f, 0.0f, -kBasinHL, kConcrete);
    float from = minX - 30.0f;
    for (const Dock& d : docks) {
        span(h, from, -3.4f, -kBasinHL, d.x - kBasinHW, 0.0f, kBasinHL, kConcrete);
        // Basin floor, keel blocks, gate.
        span(h, d.x - kBasinHW, kFloor - 0.8f, -kBasinHL, d.x + kBasinHW, kFloor, kBasinHL, kFloorC);
        span(h, d.x - kBasinHW - 0.2f, kFloor, kBasinHL - 0.2f, d.x + kBasinHW + 0.2f, 0.15f, kBasinHL + 0.8f,
             kConcreteDark);
        // Quay edge kerbs.
        for (int s = -1; s <= 1; s += 2)
            span(h, d.x + s * kBasinHW - 0.15f, 0.0f, -kBasinHL, d.x + s * kBasinHW + 0.15f, 0.12f, kBasinHL,
                 {0.75f, 0.62f, 0.15f});
        from = d.x + kBasinHW;
    }
    span(h, from, -3.4f, -kBasinHL, maxX + 30.0f, 0.0f, kBasinHL, kConcrete);
    // The harbour.
    span(h, minX - 200.0f, kSeaY - 3.0f, kBasinHL, maxX + 200.0f, kSeaY, 260.0f, kSea);
}

void drawSea(Host& h, double t) {
    for (int i = 0; i < 60; ++i) {
        const float x = shipX + (hash(3.0f, i) - 0.5f) * 140.0f, z = kBasinHL + 2.0f + hash(5.0f, i) * 70.0f;
        const float tw = 0.5f + 0.5f * std::sin(static_cast<float>(t) * (0.8f + hash(7.0f, i)) + i);
        h.point(x, kSeaY + 0.02f, z, 3.0f, 0.12f, {0.6f, 0.75f, 0.9f}, 0.25f * tw);
    }
    // The ship offshore that signal lamps talk to.
    const float bob = 0.15f * std::sin(static_cast<float>(t) * 0.7f);
    const float y = kSeaY + bob;
    span(h, shipX - 2.0f, y - 0.8f, kShipZ - 6.0f, shipX + 2.0f, y + 1.0f, kShipZ + 5.0f, {0.12f, 0.12f, 0.14f});
    span(h, shipX - 1.4f, y + 1.0f, kShipZ - 4.0f, shipX + 1.4f, y + 2.8f, kShipZ - 1.0f, {0.88f, 0.88f, 0.84f});
    span(h, shipX - 0.1f, y + 2.8f, kShipZ - 2.6f, shipX + 0.1f, y + 4.2f, kShipZ - 2.4f, kPole);
    bool anyWeb = false;
    for (const Fig& f : figs)
        if (static_cast<int>(f.action) == 4 && (f.kind > 0.5f ? f.state > 0.5f : f.state >= 2.5f)) anyWeb = true;
    if (anyWeb && std::fmod(t * 1.7, 1.0) < 0.5)
        h.point(shipX, y + 4.3f, kShipZ - 2.5f, 20.0f, 0.5f, kLamp, 1.0f);
}

void drawHull(Host& h, double t, const Dock& d) {
    const float L = d.hullLen, W = std::max(3.2f, L * 0.36f), H = 0.28f * L + 1.0f;
    const float base = kFloor + 0.6f, stern = d.z - L * 0.5f;
    for (float kz = stern + 0.6f; kz < stern + L - 0.4f; kz += 1.6f)
        span(h, d.x - 0.5f, kFloor, kz - 0.3f, d.x + 0.5f, base, kz + 0.3f, kWood);
    const int layers = 5;
    const float p = std::clamp(d.progress, 0.0f, 1.0f);
    const int plated = std::max(1, static_cast<int>(std::round(p * layers)));
    const float lh = H / layers;
    for (int k = 0; k < layers; ++k) {
        const float f = static_cast<float>(k) / (layers - 1);
        const float w = W * (0.35f + 0.65f * std::sqrt(f)), l = L * (0.78f + 0.22f * f);
        const float y0 = base + k * lh, y1 = y0 + lh;
        const float z0 = stern + (L - l) * 0.15f, body = l * 0.7f;
        if (k < plated) {
            const Rgb c = k < 2 ? kRust : mix(kRust, kGreyPlate, (f - 0.3f) * 1.4f);
            const Rgb cc = mul(c, 0.9f + 0.1f * (k & 1));
            span(h, d.x - w * 0.5f, y0, z0, d.x + w * 0.5f, y1, z0 + body, cc);
            const float seg = (l - body) / 3.0f;
            for (int b = 0; b < 3; ++b) {
                const float bw = w * (0.78f - 0.26f * b);
                span(h, d.x - bw * 0.5f, y0, z0 + body + b * seg, d.x + bw * 0.5f, y1, z0 + body + (b + 1) * seg, cc);
            }
        } else {
            // Bare frames: ribs up each side, still waiting for plates.
            for (float rz = z0 + 0.3f; rz < z0 + l * 0.85f; rz += 1.2f)
                for (int s = -1; s <= 1; s += 2)
                    span(h, d.x + s * w * 0.5f - 0.1f, y0, rz - 0.08f, d.x + s * w * 0.5f + 0.1f, y1, rz + 0.08f,
                         kRib);
            span(h, d.x - w * 0.5f, y1 - 0.1f, z0, d.x + w * 0.5f, y1, z0 + 0.15f, kRib);
        }
    }
    if (d.stalled > 0.5f) {
        const float top = base + plated * lh;
        for (int i = 0; i < 30; ++i) {
            const float ph = static_cast<float>(std::fmod(t * 0.2 + i / 30.0, 1.0));
            const float sx = d.x + std::sin(ph * 5.0f + i) * (0.5f + ph * 2.0f);
            const float sz = d.z + (hash(d.seed, i) - 0.5f) * L * 0.4f + ph * 3.0f;
            const float g = 0.62f - 0.25f * ph;
            h.point(sx, top + 0.3f + ph * 12.0f, sz, 16.0f + ph * 50.0f, 1.2f + ph * 4.5f, {g, g * 0.97f, g * 0.94f},
                    0.9f * (1.0f - ph * ph), STRATA_POINT_SOFT);
        }
    }
}

void drawDockProps(Host& h, double t, const Dock& d) {
    const float live = d.offline > 0.5f ? 0.0f : (d.busy > 0.5f ? 1.0f : 0.25f);
    // Floodlights at the landward corners: bright while anyone works, low when idle.
    for (int s = -1; s <= 1; s += 2) {
        const float x = d.x + s * (kBasinHW + 1.6f), z = -kBasinHL + 1.0f;
        box(h, x, 3.5f, z, 0.25f, 7.0f, 0.25f, kPole);
        box(h, x - s * 0.3f, 7.1f, z, 0.9f, 0.4f, 0.6f, {0.2f, 0.2f, 0.2f});
        if (live > 0.0f) {
            h.point(x - s * 0.4f, 7.0f, z + 0.2f, 14.0f * live, 0.6f, kLamp, 0.4f + 0.6f * live);
            h.light(x - s * 2.0f, 6.0f, z + 3.0f, 18.0f, {1.0f, 0.86f, 0.62f}, 2.4f * live, 1);
        }
    }
    // Foghorn tower at the seaward end of the foremen's quay, with its lamp and flag.
    const float tx = d.x - kBasinHW - 2.2f, tz = kBasinHL - 1.2f;
    for (int k = 0; k < 6; ++k)
        box(h, tx, 0.5f + k, tz, 1.0f, 1.0f, 1.0f, (k & 1) ? Rgb{0.85f, 0.85f, 0.82f} : Rgb{0.70f, 0.12f, 0.10f});
    box(h, tx, 6.15f, tz, 1.3f, 0.3f, 1.3f, {0.2f, 0.2f, 0.2f});
    const float fx = tx - 1.2f;
    box(h, fx, 4.0f, tz, 0.1f, 8.0f, 0.1f, kPole);
    const bool waiting = d.waiting > 0.5f;
    const float t1 = static_cast<float>(t);
    if (waiting) {
        const bool on = std::fmod(t, 1.0) < 0.55;
        const float pulse = 0.5f + 0.5f * std::sin(t1 * 6.2832f);
        h.add(STRATA_PRIMITIVE_SPHERE, tx, 6.8f, tz, 1.1f, 1.1f, 1.1f, on ? Rgb{1.0f, 0.15f, 0.1f} : Rgb{0.4f, 0.05f, 0.04f});
        if (on) {
            h.point(tx, 6.8f, tz, 90.0f, 5.0f, kRed, 0.95f);
            h.light(tx, 6.5f, tz - 1.5f, 26.0f, {1.0f, 0.1f, 0.06f}, 7.0f, 20);
        }
        // Flag at the masthead, flapping.
        for (int k = 0; k < 4; ++k) {
            const float ripple = 0.12f * std::sin(t1 * 6.0f - k * 1.2f);
            box(h, fx - 0.35f - k * 0.5f, 7.45f + ripple, tz, 0.5f, 1.0f, 0.06f,
                (k & 1) ? Rgb{1.0f, 0.9f, 0.1f} : Rgb{0.95f, 0.1f, 0.05f});
        }
        // Ripples spreading on the water and a beam up into the sky.
        for (int k = 0; k < 3; ++k) {
            const float ph = static_cast<float>(std::fmod(t * 0.5 + k / 3.0, 1.0));
            ring(h, tx, kSeaY + 0.05f, kBasinHL + 2.0f, 1.5f + ph * 9.0f, kRed, 0.9f * (1.0f - ph));
        }
        thick(h, tx, 7.0f, tz, tx, 30.0f, tz, kRed, 6.0f, 0.35f + 0.35f * pulse);
    } else {
        h.add(STRATA_PRIMITIVE_SPHERE, tx, 6.8f, tz, 0.8f, 0.8f, 0.8f, {0.25f, 0.22f, 0.2f});
        box(h, fx - 0.2f, 0.7f, tz, 0.3f, 0.5f, 0.12f, {0.6f, 0.1f, 0.06f});  // furled at the foot
    }
    if (d.selected > 0.5f) ring(h, d.x, 0.2f, d.z, kBasinHL + 1.0f, {1, 1, 1}, 0.6f, 48);
    if (d.offline > 0.5f) {
        // Every session here is gone: a tarp over the whole dock.
        span(h, d.x - kBasinHW - 0.4f, 0.0f, -kBasinHL - 0.4f, d.x + kBasinHW + 0.4f, 0.35f, kBasinHL + 0.4f, kTarp);
        for (int k = -4; k <= 4; ++k)
            span(h, d.x - kBasinHW - 0.4f, 0.35f, k * 2.4f - 0.1f, d.x + kBasinHW + 0.4f, 0.42f, k * 2.4f + 0.1f,
                 mul(kTarp, 0.7f));
    }
}

void drawForeman(Host& h, double t, const Fig& f, int index) {
    const int state = static_cast<int>(f.state + 0.5f), action = static_cast<int>(f.action + 0.5f);
    const Rgb hat{f.r, f.g, f.b};
    const float t1 = static_cast<float>(t);
    const bool busy = state >= 3;
    // Crane on rails beside the foreman, jib over the hull.
    const float cx = f.x - 1.6f, cz = f.z, top = 10.0f;
    box(h, cx, 0.3f, cz, 2.2f, 0.6f, 2.2f, mul(kCrane, 0.7f));
    for (int a = -1; a <= 1; a += 2)
        for (int b = -1; b <= 1; b += 2) box(h, cx + a * 0.55f, top * 0.5f, cz + b * 0.55f, 0.14f, top, 0.14f, kCrane);
    for (int k = 1; k < 9; ++k) box(h, cx, k * 1.1f, cz, 1.25f, 0.08f, 1.25f, kCrane);
    const float jibEnd = f.ax + 1.5f;
    span(h, cx - 3.0f, top, cz - 0.35f, jibEnd, top + 0.6f, cz + 0.35f, kCrane);
    box(h, cx - 2.6f, top - 0.3f, cz, 1.2f, 1.0f, 1.0f, {0.3f, 0.3f, 0.32f});  // counterweight
    box(h, cx + 0.9f, top - 0.6f, cz, 1.0f, 1.0f, 1.2f, {0.25f, 0.35f, 0.45f});  // cab
    if (state == 0) {
        // Offline: crane under a tarp, foreman gone, a covered heap on the quay.
        span(h, cx + 0.3f, top - 1.2f, cz - 0.8f, cx + 1.5f, top + 0.8f, cz + 0.8f, kTarp);
        h.add(STRATA_PRIMITIVE_SPHERE, f.x, 0.4f, f.z, 1.6f, 0.9f, 1.4f, kTarp);
        box(h, cx + 0.6f, top - 1.6f, cz, 0.1f, 1.6f, 0.1f, kPole);
        return;
    }
    // Trolley and hook: runs between quay and hull while the session runs things.
    float trolley = f.ax - 1.0f, hookY = f.hullTop + 1.8f;
    if (action == 2 && busy) {
        const float ph = 0.5f - 0.5f * std::cos(t1 * 0.9f + f.seed * 6.0f);
        trolley = cx + 2.0f + (f.ax - cx - 2.0f) * ph;
        hookY = 1.2f + (f.hullTop + 0.8f - 1.2f) * std::sin(ph * kPi) + ph * 0.6f;
        hookY = std::max(hookY, 1.2f);
    }
    box(h, trolley, top - 0.15f, cz, 0.6f, 0.3f, 0.8f, {0.3f, 0.3f, 0.3f});
    h.line(trolley, top - 0.3f, cz, trolley, hookY + 0.2f, cz, {0.1f, 0.1f, 0.1f}, 1.0f);
    box(h, trolley, hookY, cz, 0.25f, 0.35f, 0.25f, {0.85f, 0.2f, 0.1f});
    if (action == 2 && busy) box(h, trolley, hookY - 0.4f, cz, 2.0f, 0.12f, 1.4f, kGreyPlate);
    // The foreman, pacing while thinking.
    float x = f.x, z = f.z + 1.6f;
    if (action == 6 && busy) z += 1.4f * std::sin(t1 * 0.9f + f.seed * 6.0f);
    const float face = 1.0f;
    int pose = 0;
    if (state == 4 || (action == 5 && busy)) pose = 1;
    else if ((action == 4 || action == 7) && busy) pose = 3;
    const Rgb shirt = state <= 1 ? mul(kShirt, 0.5f) : kShirt;
    person(h, t, x, 0.0f, z, face, pose, state <= 1 ? mul(hat, 0.6f) : hat, shirt, f.seed, true);
    // Lead-colour vest.
    box(h, x, 1.25f, z, 0.38f, 0.5f, 0.56f, mul(hat, 0.85f));
    if (state == 4) {
        const float pulse = 0.5f + 0.5f * std::sin(t1 * 6.2832f);
        h.point(x, 2.9f + 0.2f * pulse, z, 26.0f + 10.0f * pulse, 0.7f, kRed, 0.95f);
        h.add(STRATA_PRIMITIVE_SPHERE, x, 2.9f + 0.2f * pulse, z, 0.32f, 0.32f, 0.32f, kRed);
        ring(h, x, 0.08f, z, 1.0f + 0.5f * pulse, kRed, 0.9f);
    }
    if (busy) {
        switch (action) {
            case 1: sparks(h, t, f.ax - 0.3f, f.hullTop + 0.1f, f.az, f.seed, -1.0f, 6, 1.4f); break;
            case 3: blueprint(h, x, 1.15f, z, face, 1.4f); break;
            case 4: signal(h, t, x + 0.75f, 1.5f, z + 0.33f, f.seed, 0.9f); break;
            case 5: {
                for (int i = 0; i < static_cast<int>(figs.size()); ++i) {
                    const Fig& w = figs[i];
                    if (w.kind < 0.5f || static_cast<int>(w.parent + 0.5f) != index || w.state < 0.5f) continue;
                    const float ph = static_cast<float>(std::fmod(t * 0.8 + i * 0.13, 1.0));
                    thick(h, x, 2.3f, z, w.x, w.y + 1.6f, w.z, hat, 2.0f, 0.45f);
                    h.point(x + (w.x - x) * ph, 2.3f + (w.y + 1.6f - 2.3f) * ph, z + (w.z - z) * ph, 9.0f, 0.25f, hat,
                            1.0f);
                }
                break;
            }
            case 6: thought(h, t, x, 2.45f, z, f.seed); break;
            case 7: hammer(h, t, x, 1.4f, z, face, f.seed); break;
            default: break;
        }
    }
    if (f.selected > 0.5f) ring(h, x, 0.06f, z, 1.2f, {1, 1, 1}, 0.95f);
}

void drawWelder(Host& h, double t, const Fig& f) {
    const int action = static_cast<int>(f.action + 0.5f);
    const bool active = f.state > 0.5f;
    const Rgb hat{f.r, f.g, f.b};
    const float s = f.side;
    // Scaffold bay: plank, toe-board in the lead's colour, standards down to the basin floor.
    span(h, f.x - 0.75f, f.y - 0.08f, f.z - 0.8f, f.x + 0.75f, f.y, f.z + 0.8f, kWood);
    span(h, f.x - s * 0.78f, f.y, f.z - 0.8f, f.x - s * 0.7f, f.y + 0.22f, f.z + 0.8f, hat);
    for (int a = -1; a <= 1; a += 2)
        for (int b = -1; b <= 1; b += 2)
            span(h, f.x + a * 0.7f - 0.04f, kFloor, f.z + b * 0.75f - 0.04f, f.x + a * 0.7f + 0.04f, f.y + 1.1f,
                 f.z + b * 0.75f + 0.04f, kPole);
    span(h, f.x - s * 0.7f - 0.03f, f.y + 1.0f, f.z - 0.8f, f.x - s * 0.7f + 0.03f, f.y + 1.06f, f.z + 0.8f, kPole);
    float x = f.x, z = f.z;
    if (!active) {
        person(h, t, x - s * 0.2f, f.y, z, -s, 2, mul(hat, 0.75f), kOveralls, f.seed, false);
    } else {
        if (action == 6) z += 0.45f * std::sin(static_cast<float>(t) * 1.1f + f.seed * 6.0f);
        int pose = 0;
        if (action == 1 || action == 4 || action == 7) pose = 3;
        if (action == 5) pose = 1;
        person(h, t, x, f.y, z, s, pose, hat, kOveralls, f.seed, false);
        switch (action) {
            case 1:
                box(h, x + s * 0.2f, f.y + 1.75f, z, 0.08f, 0.3f, 0.36f, {0.08f, 0.08f, 0.1f});  // visor
                sparks(h, t, f.ax, f.y + 1.45f, f.az, f.seed, -s, 3, 0.8f);
                break;
            case 2: {
                const float ph = 0.5f - 0.5f * std::cos(static_cast<float>(t) * 1.4f + f.seed * 6.0f);
                const float py = f.y + 0.3f + ph * 2.0f;
                h.line(x + s * 0.5f, f.y + 3.0f, z - 0.6f, x + s * 0.5f, py + 0.1f, z - 0.6f, {0.1f, 0.1f, 0.1f});
                box(h, x + s * 0.5f, f.y + 3.0f, z - 0.6f, 0.3f, 0.2f, 0.3f, kCrane);
                box(h, x + s * 0.5f, py, z - 0.6f, 0.9f, 0.08f, 0.7f, kGreyPlate);
                break;
            }
            case 3: blueprint(h, x, f.y + 1.15f, z, s, 1.0f); break;
            case 4: signal(h, t, x + s * 0.75f, f.y + 1.5f, z + 0.33f, f.seed, 0.45f); break;
            case 6: thought(h, t, x, f.y + 2.4f, z, f.seed); break;
            case 7: hammer(h, t, x, f.y + 1.4f, z, s, f.seed); break;
            default: break;
        }
    }
    if (f.selected > 0.5f) ring(h, f.x, f.y + 0.04f, f.z, 1.1f, {1, 1, 1}, 0.95f);
}

}  // namespace

void strata_kit_configure(strata_kit::Options& o) { o.name = "shipyard"; }

void strata_kit_engine_ready(Host&) {}

void strata_kit_build(Host& h, double t) {
    if (h.reducedMotion) t = 0.0;
#if STRATA_ABI_MINOR >= 16
    h.ambientLift = {0.05f, 0.05f, 0.07f};
#endif
    drawGround(h);
    drawSea(h, t);
    for (const Dock& d : docks) {
        drawHull(h, t, d);
        drawDockProps(h, t, d);
    }
    for (int i = 0; i < static_cast<int>(figs.size()); ++i) {
        if (figs[i].kind < 0.5f) drawForeman(h, t, figs[i], i);
        else drawWelder(h, t, figs[i]);
    }
}

// The docks, one row of 10 floats each (see Dock).
STRATA_KIT_EXPORT void sy_set_docks(const float* rows, int count) {
    docks.assign(reinterpret_cast<const Dock*>(rows), reinterpret_cast<const Dock*>(rows) + std::max(count, 0));
    shipX = docks.empty() ? 0.0f : (docks.front().x + docks.back().x) * 0.5f;
}

// Foremen and welders, one row of 16 floats each (see Fig).
STRATA_KIT_EXPORT void sy_set_figures(const float* rows, int count) {
    figs.assign(reinterpret_cast<const Fig*>(rows), reinterpret_cast<const Fig*>(rows) + std::max(count, 0));
}
