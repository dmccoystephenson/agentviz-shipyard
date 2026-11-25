// Regenerates every README image in docs/media/ from the FIXTURE feed:
//   NODE_PATH=/root/arcade-watch/node_modules node tests/screenshots.cjs [only-name ...]
// Frames are drawn with ?drive=1 so scene time is fixed per shot; the GIF needs Python with Pillow.
const fs = require("fs");
const os = require("os");
const path = require("path");
const {execFileSync} = require("child_process");
const {chromium} = require("playwright");
const {startServer, root} = require("./serve-helper.cjs");

const out = path.join(root, "docs/media");
const port = Number(process.env.SHIPYARD_TEST_PORT || 8948);
const only = process.argv.slice(2);
const want = (n) => !only.length || only.includes(n);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// Where things are in the fixture yard (docks sorted by place id, 24 apart):
// engine -48, game -24, plugin-site 0, site-one 24, home (~) 48. +x is screen-left from the land side.
const WIDE = {yaw: -1.25, pitch: 0.5, dist: 82, tx: 0, ty: 0, tz: 3};

async function open(browser, viewport, dpr, touch) {
  const ctx = await browser.newContext({viewport, deviceScaleFactor: dpr, hasTouch: !!touch, isMobile: !!touch});
  const page = await ctx.newPage();
  page.on("pageerror", (e) => console.error("pageerror:", e));
  await page.goto(`http://127.0.0.1:${port}/?fixture=1&drive=1`, {waitUntil: "commit"});
  await page.waitForFunction(() => window.__shipyard && window.__shipyard.figs().length > 0, null, {timeout: 90000});
  return {ctx, page};
}

// Draws scene time t a few times (labels fade in over real time) with the camera at cam.
async function frame(page, t, cam, sel) {
  await page.evaluate(([cam, sel]) => {
    const s = window.__shipyard;
    s.select(sel || null, false);
    s.view(cam);
  }, [cam, sel || null]);
  for (let i = 0; i < 6; i++) {
    await page.evaluate((t) => window.__shipyard.kit.drive(t), t + i * 0.02);
    await sleep(120);
  }
}

async function shot(page, name, t, cam, sel) {
  if (!want(name)) return;
  await frame(page, t, cam, sel);
  const t0 = Date.now();
  await page.screenshot({path: path.join(out, name + ".png"), timeout: 180000, animations: "disabled"});
  console.log(`  (${Date.now() - t0} ms)`);
  console.log("saved", name);
}

(async () => {
  fs.mkdirSync(out, {recursive: true});
  const server = await startServer(port);
  const browser = await chromium.launch({args: ["--use-angle=swiftshader", "--enable-unsafe-swiftshader"]});
  try {
    let {ctx, page} = await open(browser, {width: 1600, height: 900}, 1);
    // Let new figures ease into place.
    for (let i = 0; i < 30; i++) await page.evaluate((t) => window.__shipyard.kit.drive(t), i * 0.2);
    await shot(page, "hero", 6.3, WIDE);
    await shot(page, "angle-sea", 6.3, {yaw: 1.9, pitch: 0.42, dist: 90, tx: 0, ty: 0, tz: 4});
    await shot(page, "angle-top", 6.3, {yaw: -1.5707, pitch: 1.25, dist: 105, tx: 0, ty: 0, tz: 6});
    await shot(page, "closeup-lead", 6.35, {yaw: -0.75, pitch: 0.5, dist: 30, tx: -1, ty: 0, tz: -1});
    await shot(page, "closeup-waiting", 6.2, {yaw: 1.0, pitch: 0.3, dist: 34, tx: -30, ty: 2, tz: 3});
    await shot(page, "closeup-idle-offline", 6.3, {yaw: -1.0, pitch: 0.5, dist: 30, tx: 45, ty: 0, tz: -3});
    await shot(page, "closeup-stalled", 7.0, {yaw: -1.1, pitch: 0.4, dist: 36, tx: -50, ty: 2, tz: -3});
    await shot(page, "closeup-resting", 6.3, {yaw: -2.2, pitch: 0.45, dist: 22, tx: 24, ty: -1, tz: -2});
    await shot(page, "panel-desktop", 6.3, {yaw: -0.9, pitch: 0.5, dist: 34, tx: -2, ty: 0, tz: -2}, "demo-lead-b");
    if (want("motion")) {
      const dir = fs.mkdtempSync(path.join(os.tmpdir(), "shipyard-gif-"));
      await page.setViewportSize({width: 960, height: 540});
      const cam = {yaw: -0.8, pitch: 0.48, dist: 34, tx: 18, ty: 0, tz: -1};
      await frame(page, 10, cam);
      const n = 60;
      for (let i = 0; i < n; i++) {
        await page.evaluate(([t, yaw]) => { window.__shipyard.view({yaw}); window.__shipyard.kit.drive(t); },
          [10 + i / 12, cam.yaw + i * 0.004]);
        await page.screenshot({path: path.join(dir, `f${String(i).padStart(3, "0")}.png`)});
      }
      const gif = path.join(out, "motion.gif");
      execFileSync("python3", [path.join(__dirname, "make_gif.py"), dir, gif, "12", "800"]);
      fs.rmSync(dir, {recursive: true, force: true});
      console.log("saved motion", (fs.statSync(gif).size / 1e6).toFixed(1), "MB");
    }
    await ctx.close();
    if (want("phone")) {
      ({ctx, page} = await open(browser, {width: 390, height: 844}, 3, true));
      for (let i = 0; i < 30; i++) await page.evaluate((t) => window.__shipyard.kit.drive(t), i * 0.2);
      await shot(page, "phone", 6.3, {yaw: -0.85, pitch: 0.55, dist: 44, tx: 20, ty: 0, tz: 2}, "demo-lead-a");
      await ctx.close();
    }
  } finally {
    await browser.close();
    server.kill();
  }
})().catch((e) => { console.error(e); process.exit(1); });
