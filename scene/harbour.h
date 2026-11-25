// harbour.h: the land, the quays between the dry docks, the sea and the ship offshore.
#pragma once

#include <vector>

#include "yard.h"

namespace shipyard {

// One dock's basin: floor, gate at the seaward end, yellow kerbs along both quay edges.
inline void drawBasin(Host& h, float x) {
    span(h, x - kBasinHW, kFloor - 0.8f, -kBasinHL, x + kBasinHW, kFloor, kBasinHL, kFloorC);
    span(h, x - kBasinHW - 0.2f, kFloor, kBasinHL - 0.2f, x + kBasinHW + 0.2f, 0.15f, kBasinHL + 0.8f, kConcreteDark);
    for (int s = -1; s <= 1; s += 2)
        span(h, x + s * kBasinHW - 0.15f, 0.0f, -kBasinHL, x + s * kBasinHW + 0.15f, 0.12f, kBasinHL, kKerb);
}

inline void drawGround(Host& h, const World& w, const std::vector<Dock>& docks) {
    const float minX = w.minX - 30.0f, maxX = w.maxX + 30.0f;
    span(h, minX, -3.4f, -60.0f, maxX, 0.0f, -kBasinHL, kConcrete);  // land behind the docks
    float from = minX;
    for (const Dock& d : docks) {
        span(h, from, -3.4f, -kBasinHL, d.x - kBasinHW, 0.0f, kBasinHL, kConcrete);  // the quay before it
        drawBasin(h, d.x);
        from = d.x + kBasinHW;
    }
    span(h, from, -3.4f, -kBasinHL, maxX, 0.0f, kBasinHL, kConcrete);
    span(h, minX - 170.0f, kSeaY - 3.0f, kBasinHL, maxX + 170.0f, kSeaY, 260.0f, kSea);
}

// Glints on the water, and the ship whose lamp answers while anyone signals (web).
inline void drawSea(Host& h, const World& w, float t, bool anyoneSignalling) {
    const float shipX = w.shipX();
    for (int i = 0; i < 60; ++i) {
        const float x = shipX + (hash(3.0f, i) - 0.5f) * 140.0f, z = kBasinHL + 2.0f + hash(5.0f, i) * 70.0f;
        const float twinkle = 0.5f + 0.5f * std::sin(t * (0.8f + hash(7.0f, i)) + i);
        h.point(x, kSeaY + 0.02f, z, 3.0f, 0.12f, {0.6f, 0.75f, 0.9f}, 0.25f * twinkle);
    }
    const float y = kSeaY + 0.15f * std::sin(t * 0.7f);
    span(h, shipX - 2.0f, y - 0.8f, kShipZ - 6.0f, shipX + 2.0f, y + 1.0f, kShipZ + 5.0f, {0.12f, 0.12f, 0.14f});
    span(h, shipX - 1.4f, y + 1.0f, kShipZ - 4.0f, shipX + 1.4f, y + 2.8f, kShipZ - 1.0f, {0.88f, 0.88f, 0.84f});
    span(h, shipX - 0.1f, y + 2.8f, kShipZ - 2.6f, shipX + 0.1f, y + 4.2f, kShipZ - 2.4f, kPole);
    if (anyoneSignalling && cycle(t, 1.7f) < 0.5f) h.point(shipX, y + 4.3f, kShipZ - 2.5f, 20.0f, 0.5f, kLamp, 1.0f);
}

}  // namespace shipyard
