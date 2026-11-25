/* shipyard-model.js: lays the yard out from a feed document and eases it.

   One dry dock per place (sorted by id, side by side along the harbour), a
   foreman per session on the quay to the dock's -x side, a welder per
   subagent on a scaffold bay round the hull. The hull grows with the place's
   tool calls (its toolCount). Everything is keyed by id and eased towards the
   latest snapshot, so a poll never rebuilds the scene; a new welder climbs out
   from its foreman.

   Classic script: window.Yard. Needs AgentvizFeed. */
(function () {
  "use strict";
  const {followerKey} = window.AgentvizFeed;

  // Measurements the scene uses too (scene/draw_util.h): keep in step.
  const BASIN_HW = 5.5, BASIN_HL = 11, KEEL_TOP = -2.0, HULL_LAYERS = 5;
  const DOCK_SPACING = 24, QUAY_OFFSET = BASIN_HW + 3.0, QUAY_MARGIN = 18;
  const EASE_RATE = 2.5;

  // A saturated hue per session: linear RGB for the scene, CSS for the page.
  function hatColour(seed) {
    const hue = ((seed || 0) * 360 + 25) % 360, s = 0.85, l = 0.55;
    const k = (n) => (n + hue / 30) % 12, a = s * Math.min(l, 1 - l);
    const f = (n) => l - a * Math.max(-1, Math.min(k(n) - 3, Math.min(9 - k(n), 1)));
    const rgb = [f(0), f(8), f(4)];
    return {lin: rgb.map((c) => Math.pow(c, 2.2)), css: `rgb(${rgb.map((c) => Math.round(c * 255)).join(",")})`};
  }

  // Busier places have bigger hulls: 7 long with one layer of plates at no tool
  // calls, 20 long and fully plated from a few hundred.
  function hullOf(toolCount) {
    const grown = Math.log2(1 + (toolCount || 0) / 10);
    const len = Math.min(20, 7 + 2.6 * grown);
    const width = Math.max(3.2, len * 0.36), height = 0.28 * len + 1.0;
    const plated = Math.max(1, Math.round(Math.min(1, 0.2 + grown / 6) * HULL_LAYERS));
    return {len, width, height, plated, top: KEEL_TOP + plated * height / HULL_LAYERS};
  }

  function groupByPlace(doc) {
    const known = new Map((doc.places || []).map((p) => [p.id, p]));
    const byPlace = new Map();
    for (const s of doc.sessions || []) {
      if (!byPlace.has(s.place)) byPlace.set(s.place, []);
      byPlace.get(s.place).push(s);
    }
    return [...byPlace.keys()].sort().map((id) => ({place: known.get(id) || {id, label: id, toolCount: 0}, sessions: byPlace.get(id)}));
  }

  // Foremen spread along the quay; the first stands nearest the land.
  function foremanZ(j, count) { return count === 1 ? -3 : -8 + j * (14 / (count - 1)); }

  // Welders alternate sides of the hull, four bays a side, then a tier higher.
  function scaffoldBay(k, x, hull) {
    const side = k % 2 === 0 ? 1 : -1;  // +1: on the hull's -x side, facing +x
    const column = Math.floor(k / 2) % 4, tier = Math.floor(k / 8);
    return {side, x: x - side * (hull.width / 2 + 1.0), y: KEEL_TOP + 1.8 + tier * 1.9,
      z: -hull.len * 0.32 + column * (hull.len * 0.64 / 3)};
  }

  class Yard {
    constructor() {
      this.docks = new Map();     // place id -> dock
      this.foremen = new Map();   // session id -> foreman
      this.welders = new Map();   // "session/follower" -> welder
      this.world = {minX: -20, maxX: 20};
      this.ready = false;
    }

    update(doc) {
      const groups = groupByPlace(doc);
      const seen = new Set();
      groups.forEach((g, i) => this._placeDock(g, (i - (groups.length - 1) / 2) * DOCK_SPACING, seen));
      const half = Math.max(1, groups.length) * DOCK_SPACING / 2;
      this.world = {minX: -half - QUAY_MARGIN, maxX: half + QUAY_MARGIN};
      for (const map of [this.docks, this.foremen, this.welders]) for (const id of map.keys()) if (!seen.has(id)) map.delete(id);
      this.ready = true;
    }

    _placeDock({place, sessions}, x, seen) {
      const dock = this._keep(this.docks, place.id, seen, {x, len: hullOf(place.toolCount).len});
      Object.assign(dock, {data: place, hull: hullOf(place.toolCount), sessions,
        waiting: sessions.some((s) => s.state === "waiting"), busy: sessions.some((s) => s.state === "busy"),
        offline: sessions.every((s) => s.state === "offline")});
      const crew = [];
      sessions.forEach((s, j) => {
        const lead = this._keep(this.foremen, s.id, seen, {x: x - QUAY_OFFSET, y: 0, z: foremanZ(j, sessions.length)});
        Object.assign(lead, {data: s, session: s, follower: null, dock, colour: hatColour(s.seed)});
        for (const f of [...(s.followers || [])].sort((a, b) => (a.id < b.id ? -1 : 1))) crew.push({f, lead});
      });
      crew.forEach(({f, lead}, k) => {
        const bay = scaffoldBay(k, x, dock.hull);
        const key = followerKey(lead.session, f);
        const welder = this._keep(this.welders, key, seen, {x: bay.x, y: bay.y, z: bay.z}, lead.cur);
        Object.assign(welder, {data: f, session: lead.session, follower: f, lead, dock, side: bay.side, colour: lead.colour});
      });
    }

    // The entity for id, with its new target; a new one starts at `from` (or at its target).
    _keep(map, id, seen, tgt, from) {
      let e = map.get(id);
      if (!e) { e = {id, cur: Object.assign({}, from || tgt)}; map.set(id, e); }
      seen.add(id);
      e.tgt = tgt;
      return e;
    }

    step(dt, instant) {
      const k = instant ? 1 : 1 - Math.exp(-dt * EASE_RATE);
      for (const map of [this.docks, this.foremen, this.welders]) {
        for (const e of map.values()) for (const key of Object.keys(e.tgt)) e.cur[key] += (e.tgt[key] - e.cur[key]) * k;
      }
    }

    // Where a figure stands (a foreman stands 1.6 seaward of his crane column).
    posOf(e) { return e.lead ? e.cur : {x: e.cur.x, y: 0, z: e.cur.z + 1.6}; }
    get(id) { return this.foremen.get(id) || this.welders.get(id) || null; }
    findSession(name) { return [...this.foremen.values()].find((e) => e.data.name === name) || null; }
    findFollower(label) { return [...this.welders.values()].find((w) => w.data.label === label) || null; }
  }

  Yard.BASIN_HW = BASIN_HW;
  Yard.BASIN_HL = BASIN_HL;
  window.Yard = Yard;
})();
