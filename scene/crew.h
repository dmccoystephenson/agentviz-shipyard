// crew.h: the people. A foreman (session) on the quay with a crane whose jib
// reaches over the hull; a welder (subagent) on a scaffold bay round the hull,
// in the foreman's hard-hat colour; delegation lines from foreman to crew.
#pragma once

#include <vector>

#include "person.h"
#include "signals.h"

namespace shipyard {

inline void drawCraneFrame(Host& h, const Foreman& f) {
    const float cx = f.x - 1.6f, cz = f.z;
    box(h, cx, 0.3f, cz, 2.2f, 0.6f, 2.2f, mul(kCrane, 0.7f));
    for (int a = -1; a <= 1; a += 2)
        for (int b = -1; b <= 1; b += 2) box(h, cx + a * 0.55f, kCraneTop * 0.5f, cz + b * 0.55f, 0.14f, kCraneTop, 0.14f, kCrane);
    for (int k = 1; k < 9; ++k) box(h, cx, k * 1.1f, cz, 1.25f, 0.08f, 1.25f, kCrane);
    span(h, cx - 3.0f, kCraneTop, cz - 0.35f, f.hookX + 1.5f, kCraneTop + 0.6f, cz + 0.35f, kCrane);  // jib
    box(h, cx - 2.6f, kCraneTop - 0.3f, cz, 1.2f, 1.0f, 1.0f, {0.3f, 0.3f, 0.32f});                  // counterweight
    box(h, cx + 0.9f, kCraneTop - 0.6f, cz, 1.0f, 1.0f, 1.2f, {0.25f, 0.35f, 0.45f});                // cab
}

// The trolley and hook. While the session runs things (run) it carries a plate
// from the quay out over the hull and back.
inline void drawCraneHook(Host& h, const Foreman& f, float t) {
    const float cx = f.x - 1.6f, cz = f.z;
    float trolley = f.hookX - 1.0f, hookY = f.hullTop + 1.8f;
    const bool carrying = f.busy() && f.action == Action::Run;
    if (carrying) {
        const float ph = 0.5f - 0.5f * std::cos(t * 0.9f + f.seed * 6.0f);
        trolley = cx + 2.0f + (f.hookX - cx - 2.0f) * ph;
        hookY = std::max(1.2f, 1.2f + (f.hullTop - 0.4f) * std::sin(ph * kPi) + ph * 0.6f);
    }
    box(h, trolley, kCraneTop - 0.15f, cz, 0.6f, 0.3f, 0.8f, {0.3f, 0.3f, 0.3f});
    h.line(trolley, kCraneTop - 0.3f, cz, trolley, hookY + 0.2f, cz, kInk, 1.0f);
    box(h, trolley, hookY, cz, 0.25f, 0.35f, 0.25f, {0.85f, 0.2f, 0.1f});
    if (carrying) box(h, trolley, hookY - 0.4f, cz, 2.0f, 0.12f, 1.4f, kGreyPlate);
}

// Offline: the crane cab under a tarp and a covered heap where the foreman stood.
inline void drawTarpedCrane(Host& h, const Foreman& f) {
    const float cx = f.x - 1.6f, cz = f.z;
    span(h, cx + 0.3f, kCraneTop - 1.2f, cz - 0.8f, cx + 1.5f, kCraneTop + 0.8f, cz + 0.8f, kTarp);
    h.add(STRATA_PRIMITIVE_SPHERE, f.x, 0.4f, f.z, 1.6f, 0.9f, 1.4f, kTarp);
    box(h, cx + 0.6f, kCraneTop - 1.6f, cz, 0.1f, 1.6f, 0.1f, kPole);
}

inline Pose foremanPose(const Foreman& f) {
    if (f.state == State::Waiting || (f.busy() && f.action == Action::Delegate)) return Pose::Wave;
    if (f.busy() && (f.action == Action::Web || f.action == Action::Other)) return Pose::Reach;
    return Pose::Stand;
}

// What the busy foreman does with his hands (run is the crane, delegate the lines).
inline void drawForemanWork(Host& h, const Foreman& f, float x, float z, float shipX, float t) {
    switch (f.action) {
        case Action::Edit: drawSparks(h, t, f.hookX - 0.3f, f.hullTop + 0.1f, f.hookZ, f.seed, -1.0f, 6, 1.4f); break;
        case Action::Read: drawBlueprint(h, x, 1.15f, z, 1.0f, 1.4f); break;
        case Action::Web: drawSignalLamp(h, t, x + 0.75f, 1.5f, z + 0.33f, f.seed, shipX, 0.9f); break;
        case Action::Think: drawThought(h, t, x, 2.45f, z, f.seed); break;
        case Action::Other: drawHammer(h, t, x, 1.4f, z, 1.0f, f.seed); break;
        default: break;
    }
}

inline void drawForeman(Host& h, const Foreman& f, const World& w, float t) {
    drawCraneFrame(h, f);
    if (f.state == State::Offline) {
        drawTarpedCrane(h, f);
        if (f.selected) drawSelectionRing(h, f.x, 0.06f, f.z, 1.6f, false);
        return;
    }
    drawCraneHook(h, f, t);
    const float x = f.x;
    float z = f.figureZ();
    if (f.busy() && f.action == Action::Think) z += 1.4f * std::sin(t * 0.9f + f.seed * 6.0f);  // pacing
    const bool dim = f.state == State::Dim;
    drawFigure(h, {x, 0.0f, z, 1.0f, foremanPose(f), dim ? mul(f.hat, 0.6f) : f.hat, dim ? mul(kShirt, 0.5f) : kShirt,
                   f.seed, true}, t);
    box(h, x, 1.25f, z, 0.38f, 0.5f, 0.56f, mul(f.hat, 0.85f));  // the vest in the session's colour
    if (f.busy()) drawForemanWork(h, f, x, z, w.shipX(), t);
    if (f.state == State::Waiting) drawWaitingMark(h, x, z, f.reason, w.camYaw, t);
    if (f.stalled) drawStall(h, f.x - 0.7f, kCraneTop - 0.1f, f.z, f.seed, t, 1.0f);
    drawRivet(h, f.hookX - 0.3f, f.hullTop + 0.2f, f.hookZ, f.pulse, 1.2f);
    if (f.selected || f.family) drawSelectionRing(h, x, 0.06f, z, 1.2f, !f.selected);
}

// The scaffold bay: plank, toe-board in the lead's colour, standards to the basin floor, guard rail.
inline void drawScaffold(Host& h, const Welder& w) {
    const float s = w.side;
    span(h, w.x - 0.75f, w.y - 0.08f, w.z - 0.8f, w.x + 0.75f, w.y, w.z + 0.8f, kWood);
    span(h, w.x - s * 0.78f, w.y, w.z - 0.8f, w.x - s * 0.7f, w.y + 0.22f, w.z + 0.8f, w.hat);
    for (int a = -1; a <= 1; a += 2)
        for (int b = -1; b <= 1; b += 2)
            span(h, w.x + a * 0.7f - 0.04f, kFloor, w.z + b * 0.75f - 0.04f, w.x + a * 0.7f + 0.04f, w.y + 1.1f,
                 w.z + b * 0.75f + 0.04f, kPole);
    span(h, w.x - s * 0.7f - 0.03f, w.y + 1.0f, w.z - 0.8f, w.x - s * 0.7f + 0.03f, w.y + 1.06f, w.z + 0.8f, kPole);
}

// run, for a welder: a chain hoist lifting a plate up beside the bay.
inline void drawChainHoist(Host& h, const Welder& w, float z, float t) {
    const float ph = 0.5f - 0.5f * std::cos(t * 1.4f + w.seed * 6.0f), py = w.y + 0.3f + ph * 2.0f;
    const float hx = w.x + w.side * 0.5f, hz = z - 0.6f;
    h.line(hx, w.y + 3.0f, hz, hx, py + 0.1f, hz, kInk, 1.0f);
    box(h, hx, w.y + 3.0f, hz, 0.3f, 0.2f, 0.3f, kCrane);
    box(h, hx, py, hz, 0.9f, 0.08f, 0.7f, kGreyPlate);
}

inline Pose welderPose(const Welder& w) {
    if (!w.active) return Pose::Sit;
    if (w.action == Action::Delegate) return Pose::Wave;
    if (w.action == Action::Edit || w.action == Action::Web || w.action == Action::Other) return Pose::Reach;
    return Pose::Stand;
}

inline void drawWelderWork(Host& h, const Welder& w, float z, float shipX, float t) {
    const float s = w.side;
    switch (w.action) {
        case Action::Edit:
            box(h, w.x + s * 0.2f, w.y + 1.75f, z, 0.08f, 0.3f, 0.36f, {0.08f, 0.08f, 0.1f});  // visor
            drawSparks(h, t, w.torchX(), w.y + 1.45f, w.z, w.seed, -s, 3, 0.8f);
            break;
        case Action::Run: drawChainHoist(h, w, z, t); break;
        case Action::Read: drawBlueprint(h, w.x, w.y + 1.15f, z, s, 1.0f); break;
        case Action::Web: drawSignalLamp(h, t, w.x + s * 0.75f, w.y + 1.5f, z + 0.33f, w.seed, shipX, 0.45f); break;
        case Action::Think: drawThought(h, t, w.x, w.y + 2.4f, z, w.seed); break;
        case Action::Other: drawHammer(h, t, w.x, w.y + 1.4f, z, s, w.seed); break;
        default: break;
    }
}

inline void drawWelder(Host& h, const Welder& w, const World& world, float t) {
    drawScaffold(h, w);
    float z = w.z;
    if (w.active && w.action == Action::Think) z += 0.45f * std::sin(t * 1.1f + w.seed * 6.0f);
    const float x = w.active ? w.x : w.x - w.side * 0.2f;
    drawFigure(h, {x, w.y, z, w.active ? w.side : -w.side, welderPose(w), w.active ? w.hat : mul(w.hat, 0.75f),
                   kOveralls, w.seed, false}, t);
    if (w.active) drawWelderWork(h, w, z, world.shipX(), t);
    if (w.stalled) drawStall(h, w.x, w.y + 2.0f, w.z, w.seed, t, 0.8f);
    drawRivet(h, w.torchX(), w.y + 1.45f, w.z, w.pulse, 0.7f);
    if (w.selected || w.family) drawSelectionRing(h, w.x, w.y + 0.04f, w.z, 1.1f, !w.selected);
}

// delegate: a line in the foreman's colour to each working welder, a dot running along it.
inline void drawDelegation(Host& h, const Delegation& d, float t) {
    thick(h, d.x0, d.y0, d.z0, d.x1, d.y1, d.z1, d.colour, 2.0f, 0.45f);
    const float ph = cycle(t, 0.8f, d.seed);
    h.point(d.x0 + (d.x1 - d.x0) * ph, d.y0 + (d.y1 - d.y0) * ph, d.z0 + (d.z1 - d.z0) * ph, 9.0f, 0.25f, d.colour, 1.0f);
}

}  // namespace shipyard
