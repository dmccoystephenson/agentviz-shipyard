// signals.h: the marks that say something about an agent's state, drawn over
// the figure: why a foreman is waiting (a "?" or a padlock), the smoke and
// amber lamp of a stall, the rivet flash of a new tool call, the selection ring.
#pragma once

#include "yard.h"

namespace shipyard {

// A red "?" facing the camera: the session asked the owner a question.
inline void drawQuestionMark(Host& h, const Billboard& b) {
    b.arc(h, 0.0f, 0.45f, 0.35f, kPi * 0.95f, -kPi * 0.35f, kRed, 7.0f, 10);
    b.stroke(h, 0.16f, 0.13f, 0.0f, -0.05f, kRed, 7.0f);
    b.stroke(h, 0.0f, -0.05f, 0.0f, -0.3f, kRed, 7.0f);
    float x, y, z;
    b.at(0.0f, -0.62f, x, y, z);
    h.point(x, y, z, 12.0f, 0.22f * b.size, kRed, 1.0f, 0);
}

// A red padlock facing the camera: the session is held at a permission prompt.
inline void drawPadlock(Host& h, const Billboard& b) {
    b.arc(h, 0.0f, 0.05f, 0.3f, 0.0f, kPi, kRed, 6.0f, 8);
    b.stroke(h, -0.3f, 0.05f, -0.3f, -0.15f, kRed, 6.0f);
    b.stroke(h, 0.3f, 0.05f, 0.3f, -0.15f, kRed, 6.0f);
    float x, y, z;
    b.at(0.0f, -0.45f, x, y, z);
    box(h, x, y, z, 0.62f * b.size, 0.55f * b.size, 0.62f * b.size, kRed);
    box(h, x, y + 0.02f * b.size, z, 0.66f * b.size, 0.12f * b.size, 0.18f * b.size, kInk);  // keyhole band
}

// Over a waiting foreman: a pulsing red light, the reason glyph, a red ring at his feet.
inline void drawWaitingMark(Host& h, float x, float z, WaitingReason reason, float camYaw, float t) {
    const float beat = agentviz::pulse(t, 1.0f), y = 3.4f + 0.2f * beat;
    h.point(x, y, z, 26.0f + 10.0f * beat, 0.7f, kRed, 0.55f);
    const Billboard b = billboard(x, y, z, camYaw, 1.3f);
    if (reason == WaitingReason::Question) drawQuestionMark(h, b);
    else drawPadlock(h, b);
    ring(h, x, 0.08f, z, 1.0f + 0.5f * beat, kRed, 0.9f);
}

// A stall: grey smoke rising from (x, y, z) and a blinking amber lamp beside it.
inline void drawStall(Host& h, float x, float y, float z, float seed, float t, float scale) {
    const int n = scale > 0.9f ? 24 : 16;
    for (int i = 0; i < n; ++i) {
        const float ph = cycle(t, 0.2f, static_cast<float>(i) / n);
        const float sx = x + std::sin(ph * 5.0f + i) * (0.3f + ph * 1.6f) * scale;
        const float sz = z + (hash(seed, i) - 0.5f) * scale + ph * 2.5f * scale;
        const float g = 0.5f - 0.2f * ph;  // sooty enough to read against a hull as well as the sky
        h.point(sx, y + ph * 10.0f * scale, sz, (14.0f + ph * 44.0f) * scale, (1.0f + ph * 4.0f) * scale,
                {g, g * 0.97f, g * 0.94f}, 0.9f * (1.0f - ph * ph));
    }
    if (cycle(t, 1.2f, seed) < 0.5f) {
        h.point(x, y + 0.2f, z - 0.5f * scale, 14.0f * scale, 0.35f * scale, kAmber, 1.0f, 0);
        h.light(x, y + 0.6f, z - 0.5f * scale, 5.0f, kAmber, 2.0f * scale, 4);
    }
}

// A new tool call drives a rivet home at the work point: a white flash and a
// ring of sparks flying out, fading over the feed's pulse (about a second).
inline void drawRivet(Host& h, float x, float y, float z, float pulse, float scale) {
    if (pulse <= 0.01f) return;
    const float out = 1.0f - pulse;
    h.point(x, y, z, 40.0f * pulse * scale, 0.8f * scale, kWhite, pulse);
    h.light(x, y + 0.3f, z, 8.0f, kWhite, 5.0f * pulse, 18);
    for (int i = 0; i < 10; ++i) {
        const float a = i * 2.0f * kPi / 10.0f, r = (0.3f + out * 1.4f) * scale;
        h.point(x + std::cos(a) * r, y + std::sin(a) * r * 0.6f + out * 0.3f, z + std::sin(a) * r, 6.0f, 0.1f, kSpark,
                pulse);
    }
}

inline void drawSelectionRing(Host& h, float x, float y, float z, float r, bool family) {
    ring(h, x, y, z, r, kWhite, family ? 0.55f : 0.95f);
}

}  // namespace shipyard
