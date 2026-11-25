// yard.h: the shipyard's reading of one wire frame (web/shipyard-frame.js packs it).
//   scene floats  world min x, max x, the camera's yaw (glyphs face it)
//   zones         dry docks, one per place; extras: hull length, width, height,
//                 plated layers, waiting, busy, offline, selected
//   agents        sessions (foremen) at their quay spot; extras: hat rgb, crane hook x, z,
//                 hull top, in the selected family
//                 followers (welders) on their scaffold bay, facing = side of the hull;
//                 extras: hat rgb, in the selected family
//   links         kind 0: delegation, foreman's hand to an active welder
#pragma once

#include <vector>

#include "../vendor/agentviz-kit/scene/agentviz.h"
#include "draw_util.h"

namespace shipyard {

using agentviz::Action;
using agentviz::State;
using agentviz::WaitingReason;

struct World {
    float minX, maxX, camYaw;
    float shipX() const { return (minX + maxX) * 0.5f; }
};

struct Dock {
    float x, seed, len, width, height;
    int plated;  // of kHullLayers
    bool waiting, busy, offline, selected;
};
constexpr int kHullLayers = 5;

// A session: its crane column at (x, z) on the quay; the figure stands 1.6 seaward.
struct Foreman {
    uint32_t id;
    float x, z, hookX, hookZ, hullTop, seed, pulse;
    Rgb hat;
    State state;
    Action action;
    WaitingReason reason;
    bool stalled, selected, family;
    float figureZ() const { return z + 1.6f; }
    bool busy() const { return state == State::Busy; }
};

// A subagent on a scaffold bay at (x, y, z); `side` is +1 when the hull is at +x.
struct Welder {
    float x, y, z, side, seed, pulse;
    Rgb hat;
    Action action;
    bool active, stalled, selected, family;
    float torchX() const { return x + side * 0.95f; }
};

struct Delegation {
    float x0, y0, z0, x1, y1, z1, seed;
    Rgb colour;
};

inline World worldOf(const agentviz::Frame& f) { return {f.scene[0], f.scene[1], f.scene[2]}; }

inline Dock dockOf(const agentviz::Zone& z) {
    const float* e = z.extra;
    return {(z.x0 + z.x1) * 0.5f, z.seed, e[0], e[1], e[2], agentviz::toInt(e[3]),
            e[4] > 0.5f, e[5] > 0.5f, e[6] > 0.5f, e[7] > 0.5f};
}

inline Rgb hatOf(const agentviz::Agent& a) { return {a.extra[0], a.extra[1], a.extra[2]}; }
inline bool selected(const agentviz::Agent& a) { return a.highlight == agentviz::Highlight::Selected; }

inline Foreman foremanOf(const agentviz::Agent& a) {
    return {a.id, a.x, a.z, a.extra[3], a.extra[4], a.extra[5], a.seed, a.pulse, hatOf(a), a.state, a.action,
            a.waitingReason, a.stalled, selected(a), a.extra[6] > 0.5f};
}

inline Welder welderOf(const agentviz::Agent& a) {
    return {a.x, a.y, a.z, a.facing < 0.0f ? -1.0f : 1.0f, a.seed, a.pulse, hatOf(a), a.action,
            a.state == State::Busy, a.stalled, selected(a), a.extra[3] > 0.5f};
}

// The line's colour is its foreman's hat.
inline Delegation delegationOf(const agentviz::Link& l, const std::vector<Foreman>& foremen) {
    Rgb colour = kWhite;
    for (const Foreman& f : foremen)
        if (f.id == l.from) colour = f.hat;
    return {l.x0, l.y0, l.z0, l.x1, l.y1, l.z1, l.seed, colour};
}

}  // namespace shipyard
