/* page.js: boots the shipyard. The feed client, HUD, labels and wire come
   from the agentviz kit (agentviz-*.js); this file wires them to the Yard
   model (shipyard-model.js) and the Strata kit, and owns the camera. */
(function () {
  "use strict";
  const REASON = {question: "has a question", permission: "needs permission"};
  const VIEW = {yaw: -1.3, pitch: 0.5};  // from the land side, the harbour behind
  const VIEWS = {sea: {yaw: 1.9, pitch: 0.42}, top: {yaw: -Math.PI / 2, pitch: 1.25}};
  // A portrait phone looks along the yard instead, so every dock fits top to bottom.
  const PORTRAIT = {yaw: VIEW.yaw + 1.0, pitch: 0.8};
  const FOV_HALF_TAN = Math.tan(25 * Math.PI / 180);
  const WELDER_LABELS_WITHIN = 60;
  const canvas = document.getElementById("view");
  const yard = new window.Yard();
  const wire = AgentvizWire.create();
  const labels = AgentvizScreen.labels(document.getElementById("labels"));
  const query = new URLSearchParams(location.search);
  const cam = Object.assign({dist: 90, tx: 0, ty: 0, tz: 2}, VIEW);
  let kit = null, mod = null, feed = null, hud = null, pulseOverride = null;
  let selectedId = null, hoveredId = null, camAnim = null, fitted = false, sceneTime = 0, lastT = 0;

  /* ------------------------------------------------------------ camera */
  // Every dock in view from the land side: the yard is wide and shallow, so its
  // width decides. The near (+x) end looms larger, so the aim shifts that way,
  // and seaward, so the docks sit low with the harbour behind them.
  function fitView() {
    const aspect = Math.max(0.3, canvas.clientWidth / Math.max(1, canvas.clientHeight));
    const width = yard.world.maxX - yard.world.minX - 20;
    if (aspect < 0.8) {  // the yard's length now runs up the screen
      return Object.assign({tx: 0, ty: 0, tz: 2, dist: Math.min(Math.max(48, width / 2 / FOV_HALF_TAN * 0.86), 200)}, PORTRAIT);
    }
    const dist = Math.min(Math.max(48, width / 2 / (FOV_HALF_TAN * aspect) * 1.12), 200);
    return Object.assign({tx: width * 0.045, ty: 0, tz: 12, dist}, VIEW);
  }
  function flyTo(to, instant) {
    if (instant || !kit || kit.driven) { Object.assign(cam, to); camAnim = null; return; }
    camAnim = {from: Object.assign({}, cam), to, t0: performance.now(), ms: 900};
  }
  function fit(instant) { if (yard.ready) flyTo(fitView(), instant); }
  function stepCam() {
    if (!camAnim) return;
    const k = Math.min(1, (performance.now() - camAnim.t0) / camAnim.ms), e = AgentvizFeed.smoothstep(k);
    for (const key of Object.keys(camAnim.to)) cam[key] = AgentvizFeed.lerp(camAnim.from[key], camAnim.to[key], e);
    if (k >= 1) camAnim = null;
  }
  // The clear band, in canvas pixels: below the top bar and above the open
  // sheet (beside a desktop card the whole view is clear).
  function clearBand() {
    const p = hud.panelRect();
    return hud.freeRect(canvas, {panel: !!(p && p.sheet), margin: 0});
  }

  function subjectPoint(e) {
    const p = yard.posOf(e);
    return e.lead ? {x: e.cur.x, y: e.cur.y + 1.0, z: e.cur.z} : {x: p.x, y: 1.2, z: p.z};
  }

  function focusOn(e) {
    flyTo(AgentvizScreen.reveal(cam, subjectPoint(e), clearBand(), {set: {dist: 28}, force: true, dy: 0}));
  }

  // A tapped agent must stay in sight. On a phone the camera slides (same
  // angle and distance) until it sits in the clear band above the sheet; on
  // a desktop only when the card or the top bar covers it.
  function keepInSight(e) {
    const p = subjectPoint(e), q = kit.project(p.x, p.y, p.z);
    if (!q) return;
    const panel = hud.panelRect(), band = clearBand(), pad = 40;
    if (panel && panel.sheet) { flyTo(AgentvizScreen.reveal(cam, p, band, {force: true, dy: 0})); return; }
    const under = panel && q.x > panel.x0 - pad && q.x < panel.x1 + pad && q.y > panel.y0 - pad;
    if (!under && q.y > band.y0 + pad) return;
    flyTo(AgentvizScreen.aim(Object.assign({}, cam), p.x, p.y, p.z, q.x,
      Math.min(q.y, (panel ? panel.y0 : band.H) - 2 * pad), band.W, band.H));
  }

  /* ------------------------------------------------------------ labels */
  function edge(el, colour) { if (el.style.borderLeftColor !== colour) el.style.borderLeftColor = colour; }
  const selected = () => selectedId && yard.get(selectedId);

  function dockLabel(d) {
    const el = labels.set("p:" + d.id, {cls: "place" + (d.waiting ? " waiting" : ""), text: d.data.label || d.id});
    return {el, q: kit.project(d.cur.x, 1.0, -Yard.BASIN_HL - 0.8), rank: d.waiting ? 0 : 1};
  }

  // A foreman's label says what he is doing (the action's icon), and says so when he is stalled.
  function foremanLabel(e) {
    const s = e.data, waiting = s.state === "waiting";
    const text = (s.name || s.id.slice(0, 6)) + (waiting ? " " + (REASON[s.waitingReason] || "needs you") : s.stalled ? " · stalled" : "");
    const cls = "lead" + (waiting ? " waiting" : s.stalled ? " stalled" : s.state === "offline" ? " offline" : "");
    const action = s.state === "busy" && s.action && s.action !== "none" ? s.action : null;
    const el = labels.set("s:" + e.id, {cls, state: s.stalled ? "stalled" : s.state, action, text});
    edge(el, waiting ? "" : s.stalled ? "var(--av-stall)" : e.colour.css);
    const p = yard.posOf(e);
    return {el, q: kit.project(p.x, waiting ? 5.0 : 3.1, p.z), rank: waiting || e.id === selectedId ? 0 : s.stalled ? 1 : 2};
  }

  // Lane labels show close up, or for the selected agent's crew; a stalled lane's always shows.
  function welderLabel(w) {
    const sel = selected(), f = w.data;
    const crew = sel && (sel === w || sel === w.lead || sel.lead === w.lead);
    if (!crew && !f.stalled && cam.dist > WELDER_LABELS_WITHIN) return null;
    const resting = f.state !== "active";
    const el = labels.set("f:" + w.id, {cls: "sub" + (crew ? " crew" : "") + (f.stalled ? " stalled" : resting ? " resting" : ""),
      state: f.stalled ? "stalled" : null, action: resting ? "none" : f.action, text: f.label || "subagent"});
    edge(el, f.stalled ? "var(--av-stall)" : w.colour.css);
    return {el, q: kit.project(w.cur.x, w.cur.y + 2.3, w.cur.z), rank: crew ? 1 : f.stalled ? 2 : 4};
  }

  function placeLabels() {
    const items = [];
    labels.begin();
    for (const d of yard.docks.values()) items.push(dockLabel(d));
    for (const e of yard.foremen.values()) items.push(foremanLabel(e));
    for (const w of yard.welders.values()) {
      const item = welderLabel(w);
      if (item) items.push(item);
    }
    labels.end();
    // Labels never sit under the HUD: one under the top bar is dropped (a
    // waiting one is pinned just below it instead, so it still shows), and
    // one under the open panel or the legend is dropped.
    hud.placeLabels(items, {canvas});
  }

  /* ------------------------------------------------------------ picking + selection */
  function candidates() {
    const out = [];
    for (const e of yard.foremen.values()) {
      const p = yard.posOf(e);
      out.push({item: e, points: [[p.x, 1.2, p.z], [p.x, 0.5, p.z]]});
    }
    for (const w of yard.welders.values()) out.push({item: w, points: [[w.cur.x, w.cur.y + 1.0, w.cur.z]]});
    return out;
  }
  const pick = (x, y, radius) => AgentvizScreen.pick(kit.project, candidates(), x, y, radius * 1.5);
  const targetOf = (e) => (e ? {session: e.session, follower: e.follower} : null);
  const idOf = (target) => (target ? (target.follower ? AgentvizFeed.followerKey(target.session, target.follower) : target.session.id) : null);

  function onSelect(target, how) {
    selectedId = idOf(target);
    const e = selected();
    if (kit) kit.selected = e || null;
    if (e && (how === "waiting" || how === "list")) focusOn(e);
    else if (e && how === "tap") keepInSight(e);
  }

  function panelRows({session, follower}) {
    const place = ((feed && feed.doc && feed.doc.places) || []).find((p) => p.id === session.place);
    const rows = follower ? [] : [["tool calls", session.toolCount != null ? session.toolCount : "—"]];
    if (place) rows.push(["hull", place.toolCount + " tool calls at this dock"]);
    return rows;
  }

  /* ------------------------------------------------------------ frame */
  function look() {
    const sel = selected();
    return {
      camYaw: cam.yaw,
      selectedDock: sel ? sel.dock : null,
      highlight: (id) => (id === selectedId ? 1 : id === hoveredId ? 2 : 0),
      pulse: pulseOverride || feed.pulse,
      family: (e) => !!sel && sel !== e && !!e.lead && (sel === e.lead || sel.lead === e.lead),
    };
  }

  function frame(t) {
    const dt = Math.min(kit.driven ? 1 : 0.1, Math.max(0, t - lastT));
    lastT = sceneTime = t;
    stepCam();
    yard.step(dt, false);
    if (!yard.ready) return;
    window.ShipyardFrame.write(wire, yard, feed.doc, look()).send(mod);
  }

  function screenTargets() {
    const r = canvas.getBoundingClientRect();
    return candidates().map((c) => {
      const s = kit.project(c.points[0][0], c.points[0][1], c.points[0][2]);
      return s && {id: c.item.id, x: s.x + r.left, y: s.y + r.top};
    }).filter(Boolean);
  }

  function createKit() {
    return StrataKit.create({
      module: mod, canvas, camera: cam,
      limits: {minPitch: 0.08, maxPitch: 1.4, minDist: 6, maxDist: 240},
      frame,
      after: () => { if (yard.ready) placeLabels(); },
      pick,
      idOf: (e) => e.id,
      onTap: (e) => hud.select(targetOf(e), "tap"),
      onHover(e) { hoveredId = e ? e.id : null; canvas.style.cursor = e ? "pointer" : ""; },
      onSlow: () => StrataLoader.slow(),
      onFatal: (code, msg) => StrataLoader.fallback(code, msg),
      targets: screenTargets,
    });
  }

  function accept(doc) {
    yard.update(doc);
    hud.update(doc);
    if (fitted || !kit) return;
    fitted = true;
    fit(true);
    Object.assign(cam, VIEWS[query.get("view")] || {});
  }

  /* ------------------------------------------------------------ boot */
  async function boot() {
    try {
      mod = await StrataLoader.load({script: "shipyard-engine.js", factory: "createShipyard", wasm: "shipyard-engine.wasm",
        title: "the shipyard"});
    } catch (error) {
      console.error("shipyard: the engine did not load", error);
      return;  // StrataLoader has already shown its fallback
    }
    try {
      kit = createKit();
    } catch (error) {
      StrataLoader.fallback(error.code === "no-webgl2" ? "no-webgl2" : "engine");
      return;
    }
    hud = AgentvizHud.create({
      title: "⚓ agentviz shipyard",
      legend: document.getElementById("legend"),
      buttons: [{icon: "home", label: "Show the whole yard", onClick: () => fit(false)}],
      rows: panelRows,
      onSelect,
      oneRow: true,
    });
    // Under ?drive=1 tool-call pulses follow scene time, so captures repeat.
    const now = () => (kit.driven ? sceneTime : Date.now() / 1000);
    feed = AgentvizFeed.create({onUpdate: accept, onStatus: hud.setStatus, now});
    window.__yard = yard;
    window.__page = {kit, yard, hud, cam, fit, look: (o) => flyTo(Object.assign({}, cam, o), true),
      select: (id, focus) => hud.select(targetOf(yard.get(id)), focus ? "list" : "api"),
      get selectedId() { return selectedId; }, get doc() { return feed.doc; }, get polls() { return feed.polls; },
      settle: () => yard.step(0, true),
      stopPolling: () => feed.stop(),  // holdPulse: captures freeze a tool-call burst at a chosen strength
      holdPulse: (fn) => { pulseOverride = fn; },
      pollOnce: () => feed.pollOnce(), feed: (d) => feed.push(d)};
    // A fixed early-evening sun so the yard's lights read and captures repeat.
    kit.setSun(Date.UTC(2026, 5, 21, 0, 50, 0) / 1000, 40, -105);
    canvas.addEventListener("pointerdown", () => { camAnim = null; });
    feed.start();
    kit.start();
    if (kit.driven) kit.drive(0);
    StrataLoader.done();
    canvas.focus({preventScroll: true});
  }

  boot();
})();
