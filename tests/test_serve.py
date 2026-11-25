"""Serves the fixture feed next to web/ and checks the page, its assets and the feed."""
import json
import os
import sys
import threading
import unittest
from http.server import ThreadingHTTPServer
from urllib.request import urlopen

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from vendor.agentfeed.serve import make_handler  # noqa: E402

ASSETS = ["/", "/index.html", "/shipyard.js", "/strata-kit.js", "/strata-loader.js",
          "/shipyard-engine.js", "/shipyard-engine.wasm"]


class ServeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.httpd = ThreadingHTTPServer(("127.0.0.1", 0), make_handler(os.path.join(ROOT, "web"), True))
        cls.base = "http://127.0.0.1:%d" % cls.httpd.server_address[1]
        threading.Thread(target=cls.httpd.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.httpd.shutdown()
        cls.httpd.server_close()

    def get(self, path):
        with urlopen(self.base + path, timeout=10) as r:
            return r.status, r.headers.get("Content-Type", ""), r.read()

    def test_page_and_assets(self):
        for path in ASSETS:
            with self.subTest(path=path):
                status, _, body = self.get(path)
                self.assertEqual(status, 200)
                self.assertGreater(len(body), 100)

    def test_wasm_mime(self):
        _, ctype, body = self.get("/shipyard-engine.wasm")
        self.assertEqual(ctype, "application/wasm")
        self.assertEqual(body[:4], b"\0asm")

    def test_page_references(self):
        _, _, body = self.get("/")
        html = body.decode()
        for name in ("strata-loader.js", "strata-kit.js", "shipyard.js"):
            self.assertIn(name, html)
        _, _, js = self.get("/shipyard.js")
        for name in ("shipyard-engine.js", "shipyard-engine.wasm", "createShipyard", "_sy_set_docks",
                     "_sy_set_figures", "/api/agents"):
            self.assertIn(name, js.decode())

    def test_feed_parses(self):
        for path in ("/api/agents", "/api/agents?fixture=1"):
            status, ctype, body = self.get(path)
            self.assertEqual(status, 200)
            self.assertIn("json", ctype)
            doc = json.loads(body)
            self.assertEqual(doc["version"], 1)
            self.assertTrue(doc["fixture"])
            states = {s["state"] for s in doc["sessions"]}
            self.assertTrue({"busy", "idle", "waiting", "offline"} <= states)
            followers = [f for s in doc["sessions"] for f in s["followers"]]
            self.assertEqual(len(followers), 11)
            for s in doc["sessions"]:
                self.assertIn(s["place"], {p["id"] for p in doc["places"]})

    def test_no_escape_from_web(self):
        from urllib.error import HTTPError
        with self.assertRaises(HTTPError):
            urlopen(self.base + "/../vendor/agentfeed/feed.py", timeout=10)


if __name__ == "__main__":
    unittest.main()
