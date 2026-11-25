// Headless smoke test: loads /?fixture=1, waits for the kit to draw, checks
// there are no console errors and the frame is not one flat colour, taps a
// foreman and checks the panel opens, and saves docs/media/screenshot.png.
//   NODE_PATH=/root/arcade-watch/node_modules node tests/smoke.cjs [url]
const path = require("path");
const {chromium} = require("playwright");
const {startServer, root} = require("./serve-helper.cjs");

(async () => {
  const port = Number(process.env.SHIPYARD_TEST_PORT || 8948);
  const server = process.argv[2] ? null : await startServer(port);
  const url = process.argv[2] || `http://127.0.0.1:${port}/?fixture=1`;
  const browser = await chromium.launch({args: ["--use-angle=swiftshader", "--enable-unsafe-swiftshader"]});
  const errors = [];
  let failed = false;
  try {
    const page = await browser.newPage({viewport: {width: 1280, height: 720}});
    page.on("console", (m) => { if (m.type() === "error") errors.push(m.text()); });
    page.on("pageerror", (e) => errors.push(String(e)));
    await page.goto(url, {waitUntil: "commit"});
    await page.waitForFunction(() => window.__strataKit && window.__shipyard && window.__shipyard.figs().length > 0,
      null, {timeout: 90000});
    await page.waitForTimeout(4000);
    const flat = await page.evaluate(() => {
      const c = document.getElementById("view");
      const g = document.createElement("canvas");
      g.width = 64; g.height = 36;
      const x = g.getContext("2d");
      x.drawImage(c, 0, 0, 64, 36);
      const d = x.getImageData(0, 0, 64, 36).data;
      const first = d.slice(0, 3).join(",");
      for (let i = 0; i < d.length; i += 4) if (d.slice(i, i + 3).join(",") !== first) return false;
      return true;
    });
    if (flat) throw new Error("the frame is one flat colour");
    const figs = await page.evaluate(() => window.__shipyard.figs().map((f) => ({id: f.id, kind: f.kind})));
    const leads = figs.filter((f) => f.kind === 0).length, welders = figs.filter((f) => f.kind === 1).length;
    if (leads !== 8 || welders !== 11) throw new Error(`expected 8 foremen and 11 welders, got ${leads} and ${welders}`);
    const banner = await page.$eval("#needs", (e) => !e.hidden && e.textContent);
    if (!banner || !banner.includes("Thistle")) throw new Error("the waiting banner does not name Thistle");
    // The banner selects the waiting session and opens the panel.
    await page.click("#needs button");
    await page.waitForTimeout(800);
    const sheet = await page.$eval("#sheet", (e) => !e.hidden && e.textContent);
    if (!sheet || !sheet.includes("NEEDS YOU")) throw new Error("the panel did not open on the waiting session");
    await page.click("#close");
    await page.evaluate(() => window.__shipyard.view({yaw: -1.2, pitch: 0.55, dist: 95, tx: 0, ty: 0, tz: 0}));
    await page.waitForTimeout(1500);
    await page.screenshot({path: path.join(root, "docs/media/screenshot.png")});
    if (errors.length) throw new Error("console errors:\n" + errors.join("\n"));
    console.log(`smoke OK: ${leads} foremen, ${welders} welders, banner "${banner}"`);
  } catch (e) {
    failed = true;
    console.error("smoke FAILED:", e.message);
  } finally {
    await browser.close();
    if (server) server.kill();
  }
  process.exit(failed ? 1 : 0);
})();
