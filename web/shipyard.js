"use strict";
/* The shipyard page: polls /api/agents every 2 s, lays the yard out (one dry
   dock per place, a foreman per session, welders per subagent), eases every
   figure toward its new spot, and hands rows to shipyard-engine.wasm each
   frame. Rows must match scene/main.cpp (Dock, Fig). */
(function () {
  const DOCK_STRIDE = 10, FIG_STRIDE = 16;
  const DOCK_SPACING = 24, BASIN_HW = 5.5, FLOOR = -2.6;
  const STATE = {offline: 0, dim: 1, idle: 2, busy: 3, waiting: 4};
  const ACTION = {none: 0, edit: 1, run: 2, read: 3, web: 4, delegate: 5, think: 6, other: 7};
  const ACTION_TEXT = {
    edit: "welding (edit)", run: "crane moving a plate (run)", read: "studying blueprints (read)",
    web: "signalling a ship offshore (web)", delegate: "waving crews to sections (delegate)",
    think: "pacing the quay (think)", other: "hammering (other tool)", none: "standing by",
  };
  const ACTION_ICON = {edit: "✷", run: "⇅", read: "▤", web: "☀", delegate: "☛",
    think: "…", other: "⚒", none: "·"};
  const STATE_TEXT = {busy: "working", idle: "idle", waiting: "NEEDS YOU", offline: "offline (tarped)",
    dim: "dim", active: "working", resting: "resting on the scaffold"};

  const params = new URLSearchParams(location.search);
  const useFixture = params.get("fixture") === "1";
  const api = "/api/agents" + (useFixture ? "?fixture=1" : "");

  const $ = (id) => document.getElementById(id);
  let mod = null, kit = null;
  let doc = null;
  let docks = [];      // [{id, label, x, z, hullLen, ...}]
  let figs = [];       // [{id, kind, data, lead, pos:{x,y,z}, target:{x,y,z}, ...}]
  const figById = new Map();
  const labelEls = new Map();
  let selectedId = null;
  let dockPtr = 0, dockCap = 0, figPtr = 0, figCap = 0;
  let camGoal = null;

  function leadColour(seed) {
    // A saturated hue per session (linear-ish RGB for the engine, CSS for the page).
    const h = (seed * 360 + 25) % 360, s = 0.85, l = 0.55;
    const k = (n) => (n + h / 30) % 12, a = s * Math.min(l, 1 - l);
    const f = (n) => l - a * Math.max(-1, Math.min(k(n) - 3, Math.min(9 - k(n), 1)));
    const rgb = [f(0), f(8), f(4)];
    return {lin: rgb.map((c) => Math.pow(c, 2.2)), css: `rgb(${rgb.map((c) => Math.round(c * 255)).join(",")})`};
  }

  function hullGeom(total) {
    const L = Math.min(18, 7 + Math.sqrt(total) * 1.3);
    const W = Math.max(3.2, L * 0.36), H = 0.28 * L + 1.0;
    const progress = Math.min(1, 0.25 + total / 160);
    const plated = Math.max(1, Math.round(progress * 5));
    const base = FLOOR + 0.6;
    return {L, W, H, progress, top: base + plated * H / 5, base};
  }

  /* -------------------------------------------------------- layout */
  function layout(d) {
    doc = d;
    const sessions = d.sessions || [];
    const placeIds = [...new Set(sessions.map((s) => s.place))].sort();
    const placeById = new Map((d.places || []).map((p) => [p.id, p]));
    docks = placeIds.map((id, i) => {
      const ss = sessions.filter((s) => s.place === id);
      const total = ss.reduce((a, s) => a + (s.followers || []).reduce((b, f) => b + (f.toolCount || 0), 0), 0);
      const g = hullGeom(total);
      const p = placeById.get(id) || {};
      return {id, label: p.label || id, seed: p.seed || 0, x: (i - (placeIds.length - 1) / 2) * DOCK_SPACING, z: 0,
        g, total, sessions: ss,
        waiting: ss.some((s) => s.state === "waiting"), busy: ss.some((s) => s.state === "busy"),
        offline: ss.every((s) => s.state === "offline"), stalled: ss.some((s) => s.stalled)};
    });
    const next = [];
    for (const dk of docks) {
      const ss = dk.sessions, m = ss.length;
      const welders = [];
      ss.forEach((s, j) => {
        const z = m === 1 ? -3 : -8 + j * (14 / (m - 1));
        const col = leadColour(s.seed || 0);
        const fg = {id: s.id, kind: 0, data: s, dock: dk, col, lead: null,
          target: {x: dk.x - BASIN_HW - 3.0, y: 0, z}, ax: dk.x, az: z};
        next.push(fg);
        const fs = [...(s.followers || [])].sort((a, b) => (a.id < b.id ? -1 : 1));
        for (const f of fs) welders.push({f, lead: fg});
      });
      welders.forEach(({f, lead}, k) => {
        const side = k % 2 === 0 ? 1 : -1;  // +1: on the hull's -x side, facing +x
        const col = Math.floor(k / 2) % 4, tier = Math.floor(k / 8);
        const g = dk.g;
        const x = dk.x - side * (g.W / 2 + 1.0);
        const z = dk.z - g.L * 0.32 + col * (g.L * 0.64 / 3);
        const y = g.base + 1.8 + tier * 1.9;
        next.push({id: lead.id + "/" + f.id, kind: 1, data: f, dock: dk, col: lead.col, lead,
          target: {x, y, z}, side, ax: x + side * 0.95, az: z});
      });
    }
    for (const f of next) {
      const prev = figById.get(f.id);
      if (prev) f.pos = prev.pos;
      else if (f.lead && f.lead.pos) f.pos = Object.assign({}, f.lead.pos);  // a new welder climbs out from its foreman
      else f.pos = Object.assign({}, f.target);
    }
    figs = next;
    figById.clear();
    for (const f of figs) figById.set(f.id, f);
    syncLabels();
    renderBanner();
    if (selectedId && !figById.has(selectedId)) select(null);
    else if (selectedId) renderSheet();
  }

  /* --------------------------------------------------------- rows */
  function writeRows(dt) {
    const ease = 1 - Math.exp(-dt * 2.5);
    for (const f of figs) {
      for (const k of ["x", "y", "z"]) f.pos[k] += (f.target[k] - f.pos[k]) * (dt > 0 ? ease : 1);
    }
    const nd = docks.length, nf = figs.length;
    if (nd * DOCK_STRIDE > dockCap) {
      if (dockPtr) mod._free(dockPtr);
      dockCap = (nd + 8) * DOCK_STRIDE; dockPtr = mod._malloc(dockCap * 4);
    }
    if (nf * FIG_STRIDE > figCap) {
      if (figPtr) mod._free(figPtr);
      figCap = (nf + 32) * FIG_STRIDE; figPtr = mod._malloc(figCap * 4);
    }
    const heap = mod.HEAPF32;
    let b = dockPtr >> 2;
    const sel = selectedId && figById.get(selectedId);
    for (const d of docks) {
      heap.set([d.x, d.z, d.g.L, d.waiting ? 1 : 0, d.busy ? 1 : 0, d.offline ? 1 : 0, d.stalled ? 1 : 0, d.seed,
        sel && sel.dock === d ? 1 : 0, d.g.progress], b);
      b += DOCK_STRIDE;
    }
    mod._sy_set_docks(dockPtr, nd);
    b = figPtr >> 2;
    const index = new Map(figs.map((f, i) => [f, i]));
    for (const f of figs) {
      const d = f.data;
      const state = f.kind === 0 ? (STATE[d.state] ?? 1) : (d.state === "active" ? 1 : 0);
      const action = ACTION[d.action] || 0;
      const isSel = selectedId === f.id || (sel && sel.kind === 0 && f.lead === sel);
      heap.set([f.kind, f.pos.x, f.pos.y, f.pos.z, state, action, d.seed || 0, isSel ? 1 : 0,
        f.col.lin[0], f.col.lin[1], f.col.lin[2],
        f.kind === 0 ? f.ax : f.pos.x + f.side * 0.95, f.kind === 0 ? f.az : f.pos.z, f.side || 0,
        f.dock.g.top, f.lead ? index.get(f.lead) : -1], b);
      b += FIG_STRIDE;
    }
    mod._sy_set_figures(figPtr, nf);
  }

  /* ------------------------------------------------------- labels */
  function syncLabels() {
    const root = $("labels");
    const want = new Map();
    for (const d of docks) want.set("dock:" + d.id, {text: d.label, cls: "label place" + (d.waiting ? " alert" : "")});
    for (const f of figs) {
      if (f.kind === 0) {
        const s = f.data;
        want.set(f.id, {text: s.name || s.id.slice(0, 6), cls: "label lead" + (s.state === "waiting" ? " alert" : "") +
          (s.state === "offline" ? " off" : ""), colour: f.col.css});
      } else {
        want.set(f.id, {text: f.data.label || "subagent", cls: "label lane" + (f.data.state === "resting" ? " off" : ""),
          colour: f.col.css});
      }
    }
    for (const [id, el] of labelEls) if (!want.has(id)) { el.remove(); labelEls.delete(id); }
    for (const [id, w] of want) {
      let el = labelEls.get(id);
      if (!el) { el = document.createElement("div"); root.appendChild(el); labelEls.set(id, el); }
      el.className = w.cls;
      if (el.textContent !== w.text) el.textContent = w.text;
      if (w.colour) el.style.setProperty("--c", w.colour);
    }
  }

  function placeLabels() {
    const items = [];
    for (const d of docks) {
      const el = labelEls.get("dock:" + d.id);
      if (el) items.push({el, q: kit.project(d.x, 1.0, -11.8), rank: d.waiting ? 0 : 1});
    }
    for (const f of figs) {
      const el = labelEls.get(f.id);
      if (!el) continue;
      const sel = f.id === selectedId;
      if (f.kind === 0) {
        items.push({el, q: kit.project(f.pos.x, 3.1, f.pos.z + 1.6), rank: sel || f.data.state === "waiting" ? 0 : 2});
      } else {
        items.push({el, q: kit.project(f.pos.x, f.pos.y + 2.3, f.pos.z), rank: sel ? 0 : 4});
      }
    }
    StrataKit.placeLabels(items);
  }

  /* ------------------------------------------------- pick + panel */
  function anchors(f) {
    if (f.kind === 0) return [[f.pos.x, 1.2, f.pos.z + 1.6], [f.pos.x, 0.5, f.pos.z + 1.6]];
    return [[f.pos.x, f.pos.y + 1.0, f.pos.z]];
  }
  function pick(x, y, radius) {
    let best = null, bestD = radius * radius;
    for (const f of figs) {
      for (const a of anchors(f)) {
        const s = kit.project(a[0], a[1], a[2]);
        if (!s) continue;
        const d = (s.x - x) ** 2 + (s.y - y) ** 2;
        if (d < bestD) { bestD = d; best = f; }
      }
    }
    return best;
  }

  function select(f, focus) {
    selectedId = f ? f.id : null;
    if (kit) kit.selected = f || null;
    if (f && focus) camGoal = {tx: f.pos.x + (f.kind === 0 ? 3 : 0), ty: 0.5, tz: f.pos.z, dist: 26};
    renderSheet();
  }

  function esc(s) { return String(s ?? "").replace(/[&<>"]/g, (c) => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;"}[c])); }

  function renderSheet() {
    const sheet = $("sheet");
    const f = selectedId && figById.get(selectedId);
    if (!f) { sheet.hidden = true; return; }
    sheet.hidden = false;
    const d = f.data;
    const act = d.action || "none";
    const st = d.state;
    let html = `<button id="close" aria-label="Close">×</button>`;
    html += `<div class="who"><span class="hat" style="--c:${f.col.css}"></span><div><b>${esc(f.kind === 0 ? d.name : d.label)}</b>` +
      `<small>${f.kind === 0 ? "foreman (session) at " + esc(f.dock.label) : "welder (subagent) of " + esc(f.lead.data.name)}</small></div></div>`;
    html += `<div class="chips"><span class="chip st-${esc(st)}">${esc(STATE_TEXT[st] || st)}${d.stalled ? " · stalled (smoke)" : ""}</span>` +
      `<span class="chip"><i>${ACTION_ICON[act] || ""}</i> ${esc(ACTION_TEXT[act] || act)}</span></div>`;
    const rows = [];
    if (f.kind === 0) {
      if (d.activity) rows.push(["tool", `${esc(d.activity.tool)}${d.activity.label ? " · " + esc(d.activity.label) : ""}`]);
      rows.push(["branch", esc(d.branch || "—")]);
    } else {
      rows.push(["tool", `${esc(d.tool || "—")}${d.toolLabel ? " · " + esc(d.toolLabel) : ""}`]);
      rows.push(["tool calls", esc(d.toolCount ?? 0)]);
      rows.push(["branch", esc(f.lead.data.branch || "—")]);
    }
    html += `<dl>${rows.map(([k, v]) => `<dt>${k}</dt><dd>${v}</dd>`).join("")}</dl>`;
    if (f.kind === 0 && (d.followers || []).length) {
      html += `<div class="crew">` + d.followers.map((w) =>
        `<button class="crewman${w.state === "resting" ? " off" : ""}" data-id="${esc(d.id + "/" + w.id)}" style="--c:${f.col.css}">` +
        `<i>${ACTION_ICON[w.action] || ""}</i>${esc(w.label)}</button>`).join("") + `</div>`;
    } else if (f.kind === 1) {
      html += `<div class="crew"><button class="crewman" data-id="${esc(f.lead.id)}" style="--c:${f.col.css}"><i>↑</i>${esc(f.lead.data.name)}</button></div>`;
    }
    sheet.innerHTML = html;
    $("close").onclick = () => select(null);
    for (const b of sheet.querySelectorAll(".crewman")) b.onclick = () => select(figById.get(b.dataset.id), true);
  }

  function renderBanner() {
    const el = $("needs");
    const waiting = (doc.sessions || []).filter((s) => s.state === "waiting");
    el.hidden = !waiting.length;
    el.innerHTML = waiting.map((s) => `<button data-id="${esc(s.id)}"><span class="flag">⚑</span>${esc(s.name)}</button>`).join("");
    for (const b of el.querySelectorAll("button")) b.onclick = () => select(figById.get(b.dataset.id), true);
  }

  /* --------------------------------------------------------- poll */
  async function poll() {
    try {
      const r = await fetch(api, {cache: "no-store"});
      if (r.ok) {
        layout(await r.json());
        $("conn").hidden = true;
      }
    } catch (e) {
      $("conn").hidden = false;
    }
  }

  function fitCamera(cam) {
    const span = Math.max(1, docks.length) * DOCK_SPACING;
    cam.dist = Math.min(110, Math.max(38, span * 0.95));
  }

  (async () => {
    try {
      mod = await StrataLoader.load({script: "shipyard-engine.js", factory: "createShipyard", wasm: "shipyard-engine.wasm",
        title: "the shipyard"});
    } catch (e) { return; }
    await poll();
    const canvas = $("view");
    const camera = {yaw: -1.2, pitch: 0.55, dist: 60, tx: 0, ty: 0, tz: 0};
    fitCamera(camera);
    if (params.get("view") === "sea") Object.assign(camera, {yaw: 1.85, pitch: 0.42});
    if (params.get("view") === "top") Object.assign(camera, {yaw: -1.5707, pitch: 1.3});
    try {
      kit = StrataKit.create({
        module: mod, canvas, camera, limits: {maxDist: 160, minDist: 6},
        pick, idOf: (f) => f.id,
        onTap(f) { select(f, false); },
        frame(t, now, dt) {
          if (camGoal) {
            const k = 1 - Math.exp(-(dt || 0.016) * 3);
            for (const key of Object.keys(camGoal)) camera[key] += (camGoal[key] - camera[key]) * k;
            if (Math.abs(camera.tx - camGoal.tx) + Math.abs(camera.tz - camGoal.tz) < 0.05) camGoal = null;
          }
          writeRows(dt || 0);
        },
        after: placeLabels,
        targets: () => figs.map((f) => {
          const a = anchors(f)[0], s = kit.project(a[0], a[1], a[2]);
          const r = canvas.getBoundingClientRect();
          return s && {id: f.id, x: s.x + r.left, y: s.y + r.top};
        }).filter(Boolean),
        onSlow: () => StrataLoader.slow(),
        onFatal: (code, msg) => StrataLoader.fallback(code, msg),
      });
    } catch (e) {
      return StrataLoader.fallback(e.code === "no-webgl2" ? "no-webgl2" : "engine");
    }
    // A fixed early-evening sun so the yard's lights read and captures repeat.
    kit.setSun(Date.UTC(2026, 5, 21, 0, 50, 0) / 1000, 40, -105);
    canvas.addEventListener("pointerdown", () => { camGoal = null; });
    window.__shipyard = {select: (id, focus) => select(figById.get(id) || null, focus), figs: () => figs, docks: () => docks,
      kit, camera, view(o) { camGoal = null; Object.assign(camera, o); }};
    kit.start();
    if (kit.driven) kit.drive(0);
    StrataLoader.done();
    setInterval(poll, 2000);
    document.addEventListener("keydown", (e) => { if (e.key === "Escape") select(null); });
  })();
})();
