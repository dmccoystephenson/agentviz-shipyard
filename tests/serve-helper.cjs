// Starts the fixture server for the browser scripts (port 8948 by default) and stops it by PID.
const {spawn} = require("child_process");
const path = require("path");
const http = require("http");

const root = path.resolve(__dirname, "..");

function get(url) {
  return new Promise((res) => {
    http.get(url, (r) => { r.resume(); res(r.statusCode); }).on("error", () => res(0));
  });
}

async function startServer(port) {
  const proc = spawn("python3", ["-m", "vendor.agentfeed", "serve", "--web", "web", "--port", String(port), "--fixture"],
    {cwd: root, stdio: "ignore"});
  for (let i = 0; i < 100; i++) {
    if (await get(`http://127.0.0.1:${port}/api/agents`) === 200) return proc;
    await new Promise((r) => setTimeout(r, 100));
  }
  proc.kill();
  throw new Error("fixture server did not start");
}

module.exports = {startServer, root};
