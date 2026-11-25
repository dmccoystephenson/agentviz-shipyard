// Headless smoke test on the demo feed (vendor/agentviz-kit/tests/harness.cjs):
// every fixture agent is in the yard; the "needs you" chip cycles every
// waiting session and the panel says why it waits; a tap on a waiting foreman
// opens its panel; a stalled subagent is marked stalled (scene, label, panel);
// a subagent's own branch shows in its panel; feed strings never become
// HTML; the empty state and the unreachable banner show; the frame is not
// flat; no console errors. Expected counts come from the feed document.
//   npm ci && npx playwright install chromium && npm run smoke
const path = require("path");
const h = require("../vendor/agentviz-kit/tests/harness.cjs");

const REPO = path.resolve(__dirname, "..");
const PORT = Number(process.env.SHIPYARD_TEST_PORT || 9125);
const READY = "window.__page && window.__page.polls > 0 && window.__yard.foremen.size > 0";
const IMG = "<img src=x id=pwned>";

function check(cond, message) { if (!cond) throw new Error(message); }

const panelTitle = (page) => page.evaluate(() => {
  const panel = document.querySelector(".av-panel");
  return panel.hidden ? null : panel.querySelector("h2").textContent.trim();
});
const panelValues = (page) => page.evaluate(() => [...document.querySelectorAll(".av-panel dd")].map((d) => d.textContent));

async function checkScene(page) {
  const info = await page.evaluate(() => {
    const y = window.__yard, doc = window.__page.doc;
    return {have: {sessions: y.foremen.size, followers: y.welders.size, places: y.docks.size},
      want: {sessions: doc.sessions.length, followers: doc.sessions.reduce((n, s) => n + s.followers.length, 0),
        places: new Set(doc.sessions.map((s) => s.place)).size},
      waiting: doc.sessions.filter((s) => s.state === "waiting").map((s) => ({id: s.id, name: s.name, reason: s.waitingReason}))};
  });
  console.log("scene:", JSON.stringify(info.have), "want", JSON.stringify(info.want));
  check(JSON.stringify(info.have) === JSON.stringify(info.want), "the yard does not hold every fixture agent");
  check(info.waiting.length >= 2, "the fixture should have at least two sessions waiting");
  return info;
}

async function checkWaitingChip(page, waiting) {
  const chip = page.locator(".av-wait");
  check(await chip.isVisible(), "the needs-you chip is hidden");
  check((await chip.locator("b").textContent()) === String(waiting.length), `the needs-you chip does not count ${waiting.length}`);
  const seen = [];
  for (let i = 0; i < waiting.length; i++) {
    await chip.click();
    const reason = await page.locator(".av-panel .av-needs").textContent();
    seen.push((await panelTitle(page)) + " (" + reason + ")");
  }
  console.log("needs-you chip cycles:", seen.join(" -> "));
  const want = waiting.map((w) => w.name + " (" + (w.reason === "question" ? "has a question" : "needs permission") + ")");
  check(seen.slice().sort().join() === want.sort().join(), "the chip did not cycle every waiting session with its reason");
}

async function checkTapOnWaitingForeman(page, waiting) {
  const target = waiting[0];
  await page.evaluate((id) => {
    window.__page.select(null);
    const p = window.__yard.posOf(window.__yard.get(id));
    window.__page.look({tx: p.x, ty: 0.5, tz: p.z, dist: 18});
  }, target.id);
  await page.waitForTimeout(800);
  const hit = await page.evaluate((id) => {
    const k = window.__page.kit, p = window.__yard.posOf(window.__yard.get(id));
    const q = k.project(p.x, 1.0, p.z), r = k.canvas.getBoundingClientRect();
    return q && {x: q.x + r.left, y: q.y + r.top};
  }, target.id);
  check(hit, "the waiting foreman is off screen");
  await page.mouse.click(hit.x, hit.y);
  await page.waitForTimeout(400);
  const title = await panelTitle(page);
  console.log("tap on the waiting foreman:", title);
  check(title === target.name, `the tap did not open ${target.name}'s panel`);
}

async function checkStalledSubagent(page) {
  const found = await page.evaluate(() => {
    const doc = window.__page.doc, keys = {stalled: [], fine: []};
    for (const s of doc.sessions) for (const f of s.followers) (f.stalled ? keys.stalled : keys.fine).push(s.id + "/" + f.id);
    // What the scene is told: pack a frame and read each agent's stalled flag off the wire.
    const W = window.AgentvizWire, buf = window.ShipyardFrame.write(W.create(), window.__yard, doc,
      {camYaw: 0, selectedDock: null, highlight: () => 0, pulse: () => 0, family: () => false}).finish();
    const stalledById = new Map();
    for (let i = 0, o = W.HEADER + buf[2] * W.ZONE_STRIDE; i < buf[3]; i++, o += W.AGENT_STRIDE) stalledById.set(buf[o], buf[o + 6]);
    const marked = (k) => stalledById.get(W.id24(k)) === 1;
    return {keys, ok: keys.stalled.every(marked) && !keys.fine.some(marked)};
  });
  check(found.keys.stalled.length > 0, "the fixture should have a stalled subagent");
  check(found.ok, "only stalled subagents should be marked stalled in the scene");
  await page.evaluate((key) => window.__page.select(key, true), found.keys.stalled[0]);
  await page.waitForTimeout(1500);
  const marks = await page.evaluate(() => ({
    panel: [...document.querySelectorAll(".av-panel .av-chip b")].some((b) => b.textContent === "stalled"),
    label: !!document.querySelector("#labels .av-label.sub .av-s-stalled")}));
  console.log("stalled subagent:", found.keys.stalled[0], JSON.stringify(marks));
  check(marks.panel && marks.label, "the stalled subagent's panel or label does not say stalled");
}

async function checkSubagentBranch(page) {
  const lane = await page.evaluate(() => {
    for (const s of window.__page.doc.sessions)
      for (const f of s.followers) if (f.branch && f.branch !== s.branch) return {key: s.id + "/" + f.id, branch: f.branch};
    return null;
  });
  check(lane, "the fixture should have a subagent on its own branch");
  await page.evaluate((key) => window.__page.select(key, false), lane.key);
  check((await panelValues(page)).includes(lane.branch), "the subagent panel does not show its own branch");
  console.log("subagent branch:", lane.branch);
}

async function checkTextOnly(page) {
  const result = await page.evaluate((img) => {
    const doc = JSON.parse(JSON.stringify(window.__page.doc));
    const lead = doc.sessions.find((s) => s.followers.length > 0);
    lead.followers[0].label = img;
    window.__page.feed(doc);
    window.__page.select(lead.id, true);
    return {img: !!document.getElementById("pwned"),
      lane: [...document.querySelectorAll(".av-panel .av-lanes button")].some((b) => b.textContent === img)};
  }, IMG);
  await page.waitForTimeout(1500);
  const label = await page.evaluate((img) => [...document.querySelectorAll("#labels .av-label")].some((l) => l.textContent === img), IMG);
  check(!result.img && result.lane && label && !(await page.evaluate(() => !!document.getElementById("pwned"))),
    "a feed label was parsed as HTML (or is missing as text)");
  console.log("injected <img> label stayed text in the panel and the world label");
}

async function checkEmptyState(page) {
  const shown = await page.evaluate(() => {
    const doc = JSON.parse(JSON.stringify(window.__page.doc));
    const sessions = doc.sessions;
    window.__page.feed(Object.assign(doc, {sessions: []}));
    const visible = !document.querySelector(".av-empty").hidden;
    window.__page.feed(Object.assign(doc, {sessions}));
    return visible && document.querySelector(".av-empty").hidden;
  });
  check(shown, "no empty-state hint with zero sessions");
  console.log("empty state: shown with zero sessions");
}

async function checkUnreachable(page) {
  await page.route("**/api/agents*", (route) => route.fulfill({status: 503, body: "down"}));
  await page.evaluate(async () => { await window.__page.pollOnce(); await window.__page.pollOnce(); });
  const banner = await page.evaluate(() => !document.querySelector(".av-banner").hidden && document.querySelector(".av-banner").textContent);
  console.log("banner:", banner);
  check(banner && banner.includes("HTTP 503"), "no unreachable banner after failed polls");
}

h.run("smoke", {repo: REPO, port: PORT}, async ({server, browser}) => {
  const {page, errors} = await h.open(browser, server.base + "/?fixture=1&quality=1", {ready: READY});
  await h.stillAnimations(page);
  await page.waitForFunction(() => window.__page.kit.stats().frames > 3, null, {timeout: 400000});
  await page.evaluate(() => window.__page.stopPolling());
  const info = await checkScene(page);
  await checkWaitingChip(page, info.waiting);
  await checkTapOnWaitingForeman(page, info.waiting);
  await checkStalledSubagent(page);
  await checkSubagentBranch(page);
  await checkTextOnly(page);
  await checkEmptyState(page);
  await page.evaluate(() => { window.__page.select(null); window.__page.fit(true); });
  await page.mouse.move(2, 400);  // no hover in the saved frame
  await page.waitForTimeout(1500);
  const png = await h.shot(page, path.join(REPO, "docs/media/screenshot.png"));
  console.log("distinct colours:", await h.assertNotFlat(page, png));
  h.assertNoErrors(errors);
  await checkUnreachable(page);  // last: the failed fetches log console errors of their own
});
