/* shipyard-frame.js: packs the yard for the scene, once a frame, in the
   agentviz wire format (vendor/agentviz-kit, docs/KIT.md in agentviz).
   scene/yard.h reads what is written here.

     scene floats  world min x, max x, the camera's yaw (glyphs face it)
     zones         kind 0 dry dock (one per place) over its basin; extras: hull length,
                     width, height, plated layers, waiting, busy, offline, selected
     agents        foremen at their crane column; extras: hat rgb, hook x, hook z, hull top,
                     in the selected family
                   welders on their scaffold bay, facing = side; extras: hat rgb, in family
     links         kind 0 delegation: a delegating foreman's hand to each active welder

   Classic script: window.ShipyardFrame. Needs Yard. */
(function () {
  "use strict";
  const ZONE = {dock: 0};
  const LINK = {delegation: 0};
  const flag = (v) => (v ? 1 : 0);

  function dock(wire, d, selectedDock) {
    const len = d.cur.len, hw = window.Yard.BASIN_HW, hl = window.Yard.BASIN_HL;
    wire.zone({id: "place:" + d.id, kind: ZONE.dock, seed: d.data.seed, x0: d.cur.x - hw, z0: -hl, x1: d.cur.x + hw, z1: hl,
      extra: [len, Math.max(3.2, len * 0.36), 0.28 * len + 1.0, d.hull.plated, flag(d.waiting), flag(d.busy),
        flag(d.offline), flag(d === selectedDock)]});
  }

  function foreman(wire, e, look) {
    const c = e.colour.lin;
    wire.agent({session: e.session, x: e.cur.x, y: 0, z: e.cur.z, facing: 1, highlight: look.highlight(e.id),
      pulse: look.pulse(e.id), extra: [c[0], c[1], c[2], e.dock.cur.x, e.cur.z, e.dock.hull.top, flag(look.family(e))]});
  }

  function welder(wire, w, look) {
    const c = w.colour.lin;
    wire.agent({session: w.session, follower: w.follower, x: w.cur.x, y: w.cur.y, z: w.cur.z, facing: w.side,
      highlight: look.highlight(w.id), pulse: look.pulse(w.id), extra: [c[0], c[1], c[2], flag(look.family(w))]});
  }

  // A delegating foreman's lines run from his raised hands to each working welder.
  function delegations(wire, yard) {
    let n = 0;
    for (const w of yard.welders.values()) {
      const s = w.lead.session;
      if (s.state !== "busy" || s.action !== "delegate" || w.follower.state !== "active") continue;
      const p = yard.posOf(w.lead);
      wire.link({kind: LINK.delegation, from: s.id, to: w.id, x0: p.x, y0: 2.3, z0: p.z,
        x1: w.cur.x, y1: w.cur.y + 1.6, z1: w.cur.z, seed: (n++ * 0.13) % 1});
    }
  }

  // look: {highlight(id) -> 0 none / 1 selected / 2 hovered, pulse(id) -> 0..1,
  //        family(entity) -> true when its foreman or crew mate is selected, selectedDock, camYaw}
  function write(wire, yard, doc, look) {
    wire.begin({doc, scene: [yard.world.minX, yard.world.maxX, look.camYaw]});
    for (const d of yard.docks.values()) dock(wire, d, look.selectedDock);
    for (const e of yard.foremen.values()) foreman(wire, e, look);
    for (const w of yard.welders.values()) welder(wire, w, look);
    delegations(wire, yard);
    return wire;
  }

  window.ShipyardFrame = {write, ZONE, LINK};
})();
