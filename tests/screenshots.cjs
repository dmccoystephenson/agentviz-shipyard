// Regenerates every README image in docs/media/ from the DEMO feed (never live):
//   npm run screenshots [-- name ...]
// Stills are drawn with ?drive=1 (the kit draws one frame on demand at a
// chosen scene time), so they repeat. The GIF steps scene time frame by frame
// and is encoded with Pillow (tests/make_gif.py: a palette per frame).
const {execFileSync} = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");
const h = require("../vendor/agentviz-kit/tests/harness.cjs");
const screens = require("../vendor/agentviz-kit/tests/screens.cjs");

const REPO = path.resolve(__dirname, "..");
const MEDIA = path.join(REPO, "docs/media");
const PORT = Number(process.env.SHIPYARD_TEST_PORT || 9125);
const READY = "window.__page && window.__page.polls > 0 && window.__yard.foremen.size > 0";
const only = new Set(process.argv.slice(2));
const want = (name) => only.size === 0 || only.has(name);
const DESKTOP = {viewport: {width: 1600, height: 900}};
const PHONE = {viewport: {width: 390, height: 844}, dpr: 3, mobile: true};

// Camera helpers, run in the page: aim at a dock, a foreman or a welder.
const HELPERS = `
window.__shots = {
  t: 6.3,
  cam(c) { window.__page.look(c); },
  dock(id, c) { const d = window.__yard.docks.get(id); this.cam(Object.assign({}, c, {tx: d.cur.x + (c.dx || 0)})); },
  session(name) { return window.__yard.findSession(name); },
  foreman(name, c) { const p = window.__yard.posOf(this.session(name)); this.cam(Object.assign({tx: p.x, tz: p.z}, c)); },
  welder(label, c) { const w = window.__yard.findFollower(label); window.__page.select(w.id);
    this.cam(Object.assign({tx: w.cur.x, ty: w.cur.y + 0.8, tz: w.cur.z}, c)); },
  draw() { window.__shots.t += 0.02; window.__page.kit.drive(window.__shots.t); },
};`;

async function openPage(browser, server, options) {
  const opened = await h.open(browser, `${server.base}/?fixture=1&drive=1`, Object.assign({ready: READY}, options));
  await h.stillAnimations(opened.page);
  await opened.page.evaluate(() => window.__page.stopPolling());
  await opened.page.evaluate(HELPERS);
  await opened.page.evaluate(() => { window.__page.kit.drive(0.5); window.__page.settle(); window.__page.kit.drive(1); });
  return opened;
}

async function shoot(page, name, setup) {
  if (!want(name)) return;
  await page.evaluate(() => { window.__page.select(null); window.__page.holdPulse(null); window.__shots.t = 6.3; });
  await page.evaluate(setup);
  await screens.still(page, path.join(MEDIA, name + ".png"), () => window.__shots.draw());
}

const DESKTOP_SHOTS = [
  ["hero", "window.__page.fit(true)"],
  ["angle-sea", "window.__page.fit(true); window.__shots.cam({yaw: 1.9, pitch: 0.42, tx: -10, tz: 2, dist: window.__page.cam.dist * 1.08})"],
  ["angle-top", "window.__page.fit(true); window.__shots.cam({yaw: -Math.PI / 2, pitch: 1.25, tx: 0, tz: 10})"],
  ["closeup-lead", "window.__shots.dock('plugin-site', {dx: -2, yaw: -1.0, pitch: 0.6, dist: 30, ty: 0, tz: -1})"],
  ["closeup-waiting", "window.__shots.dock('game', {dx: -6, yaw: 1.0, pitch: 0.3, dist: 34, ty: 2, tz: 3})"],
  ["closeup-question", "window.__shots.foreman('Juniper', {yaw: -1.0, pitch: 0.35, dist: 14, ty: 2.5})"],
  ["closeup-idle-offline", "window.__shots.foreman('Lantern', {yaw: -1.05, pitch: 0.45, dist: 24, ty: 2})"],
  ["closeup-stalled", "window.__shots.foreman('Patch', {yaw: 1.3, pitch: 0.3, dist: 26, ty: 5})"],
  ["closeup-stalled-subagent", "window.__shots.welder('cache warm-up', {yaw: -2.5, pitch: 0.12, dist: 15, ty: 4})"],
  ["closeup-resting", "window.__shots.welder('tests + docs', {yaw: 3.0, pitch: 0.5, dist: 13})"],
  ["closeup-tool-call", "const e = window.__shots.session('Anvil'); window.__page.holdPulse((id) => (id === e.id ? 0.7 : 0)); " +
    "window.__shots.cam({tx: e.dock.cur.x - 1, ty: 0.5, tz: e.cur.z, yaw: -0.5, pitch: 0.75, dist: 15})"],
  ["panel-desktop", "window.__page.select(window.__shots.session('Otto').id); " +
    "window.__shots.dock('plugin-site', {dx: -2, yaw: -0.9, pitch: 0.5, dist: 34, ty: 0, tz: -2})"],
];

async function desktopStills(browser, server) {
  const {ctx, page, errors} = await openPage(browser, server, DESKTOP);
  for (const [name, setup] of DESKTOP_SHOTS) await shoot(page, name, setup);
  h.assertNoErrors(errors);
  await ctx.close();
}

async function phone(browser, server) {
  if (!want("phone")) return;
  const {ctx, page, errors} = await openPage(browser, server, PHONE);
  await shoot(page, "phone", "window.__page.select(window.__shots.session('Wren').id); " +
    "window.__shots.foreman('Wren', {yaw: -0.85, pitch: 0.55, dist: 26, ty: -4})");
  h.assertNoErrors(errors);
  await ctx.close();
}

// The GIF: five seconds at the site-one dock at 12 frames a second, the camera drifting round.
async function motionGif(browser, server) {
  if (!want("motion")) return;
  const {ctx, page, errors} = await openPage(browser, server, {viewport: {width: 960, height: 540}});
  await page.evaluate(() => window.__shots.dock("site-one", {dx: -7, yaw: -0.8, pitch: 0.5, dist: 31, ty: 0, tz: -2}));
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "shipyard-gif-"));
  const FPS = 12;
  await screens.frames(page, tmp, FPS * 5, (i) => {
    window.__page.look({yaw: -0.8 + i * 0.004});
    window.__page.kit.drive(10 + i / 12);
  });
  execFileSync("python3", [path.join(__dirname, "make_gif.py"), tmp, path.join(MEDIA, "motion.gif"), String(FPS), "800"],
    {stdio: "inherit"});
  fs.rmSync(tmp, {recursive: true, force: true});
  h.assertNoErrors(errors);
  await ctx.close();
}

h.run("screenshots", {repo: REPO, port: PORT}, async ({server, browser}) => {
  fs.mkdirSync(MEDIA, {recursive: true});
  await desktopStills(browser, server);
  await phone(browser, server);
  await motionGif(browser, server);
});
