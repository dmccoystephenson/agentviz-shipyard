# CLAUDE.md

agentviz-shipyard: Claude Code sessions drawn as a shipyard on Strata's web kit.
One of the agentviz family (https://github.com/dmccoystephenson/agentviz).

- Data comes only from `/api/agents` (vendor/agentfeed, schema in its docs). Never edit
  `vendor/agentfeed/`; re-vendor with agentviz's `tools/vendor-feed.sh`.
- Scene: `scene/main.cpp` -> `scene/build.sh` -> `web/shipyard-engine.js|.wasm` (committed).
- Page: `web/index.html` + `web/shipyard.js` poll every 2 s, key by id, ease.
- Run: `python3 -m vendor.agentfeed serve --web web --port 8848 [--fixture]`.
- Tests: `python3 -m unittest discover -s tests`; smoke: `node tests/smoke.cjs`.
- Private repo; hosted Actions are refused, run localci on PRs. Screenshots from the fixture only.
