# CLAUDE.md

agentviz-shipyard: Claude Code sessions drawn as a shipyard along a harbour (Strata web
kit). Part of the agentviz family: https://github.com/dmccoystephenson/agentviz

- Data comes only from `/api/agents` (vendor/agentfeed; schema in the agentviz repo's docs/FEED.md).
- Shared pieces come from the agentviz kit (docs/KIT.md there): `vendor/agentviz-kit/` and the
  `web/agentviz-*` copies. Never edit `vendor/` or `web/agentviz-*`; re-vendor from an agentviz
  checkout with `tools/vendor-kit.sh <this repo>`. `tests/test_serve.py` checks the PINs.
- Page: `web/shipyard-model.js` lays out docks, foremen and welders and eases them;
  `web/shipyard-frame.js` packs each frame in the agentviz wire format; `web/page.js` boots the
  feed client, HUD, labels, picking and camera; `web/index.html` holds the theme and the legend.
- Scene: `scene/main.cpp` decodes the wire (`agentviz.h`, read by `yard.h`) and calls
  `harbour.h`, `dock.h`, `crew.h` (with `person.h`, `signals.h`; helpers in `draw_util.h`).
  Measurements shared with the page are marked "keep in step". Build:
  `EMSDK=/path/to/emsdk vendor/agentviz-kit/scene/build-scene.sh scene/kit.conf`
  (emsdk 6.0.11, Strata 0.10.0). The built `web/shipyard-engine.*` are committed.
- Run: `python3 -m vendor.agentfeed serve --web web --port 8848 [--fixture]`.
- Tests: `python3 -m unittest discover -s tests` (port 9126, `SHIPYARD_UNIT_PORT`); browser tests
  after `npm ci` and `npx playwright install chromium`: `npm run smoke`, `npm run screenshots`
  (Pillow from requirements-dev.txt for the GIF), on port 9125 (`SHIPYARD_TEST_PORT`).
- Screenshots come from the FIXTURE feed only, never live sessions.
