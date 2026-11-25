// agentviz-shipyard: Claude Code sessions as a shipyard along a harbour.
//
// Each place (repo) is a dry dock with a hull under construction; each
// session is a foreman on the quay beside it, with a crane; each subagent is
// a welder on the scaffolding round the hull, in its lead's hard-hat colour.
// The page (web/shipyard-*.js, web/page.js) owns the data and the layout: it
// reads the agent feed, places docks and people, eases them between snapshots
// and, every frame, packs the scene in the agentviz wire format
// (vendor/agentviz-kit, docs/KIT.md in the agentviz repo) for agentviz_frame().
// This file only draws what the frame says (yard.h reads it), with
// harbour.h, dock.h and crew.h. Every animation is a function of scene time.
#define STRATA_KIT_MAIN
#include "strata_kit.h"

#include "crew.h"
#include "dock.h"
#include "harbour.h"

namespace {

using namespace shipyard;

agentviz::Frame frame;

bool anyoneSignalling(const std::vector<Foreman>& foremen, const std::vector<Welder>& welders) {
    for (const Foreman& f : foremen)
        if (f.busy() && f.action == Action::Web) return true;
    for (const Welder& w : welders)
        if (w.active && w.action == Action::Web) return true;
    return false;
}

}  // namespace

AGENTVIZ_FRAME_EXPORT(frame)

void strata_kit_configure(strata_kit::Options& o) {
    o.name = "agentviz-shipyard";
    o.farPlane = 600.0f;
}

void strata_kit_engine_ready(strata_kit::Host&) {}

void strata_kit_build(strata_kit::Host& h, double seconds) {
    if (!frame.ok) return;
    const float t = h.reducedMotion ? 0.0f : static_cast<float>(seconds);
    h.ambientLift = {0.05f, 0.05f, 0.07f};
    const World world = worldOf(frame);
    std::vector<Dock> docks;
    std::vector<Foreman> foremen;
    std::vector<Welder> welders;
    for (const agentviz::Zone& z : frame.zones) docks.push_back(dockOf(z));
    for (const agentviz::Agent& a : frame.agents) {
        if (a.lead()) foremen.push_back(foremanOf(a));
        else welders.push_back(welderOf(a));
    }
    drawGround(h, world, docks);
    drawSea(h, world, t, anyoneSignalling(foremen, welders));
    for (const Dock& d : docks) drawDock(h, d, t);
    for (const Foreman& f : foremen) drawForeman(h, f, world, t);
    for (const Welder& w : welders) drawWelder(h, w, world, t);
    for (const agentviz::Link& l : frame.links) drawDelegation(h, delegationOf(l, foremen), t);
}
