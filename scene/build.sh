#!/usr/bin/env bash
# Builds scene/main.cpp into web/shipyard-engine.{js,wasm} on the Strata 0.10.0
# web kit, and copies the kit's page scripts into web/. Downloads into .strata/.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
VERSION=0.10.0
SHA=bb86b974c7e0309db91625ba5ee157273704c77d93f4338c8a63ee625e3d56de
TAR=".strata/strata-$VERSION-wasm32-emscripten.tar.gz"
mkdir -p .strata
if [ ! -f "$TAR" ]; then
  gh release download "v$VERSION" -R Preponderous-Software/strata \
    -p "strata-$VERSION-wasm32-emscripten.tar.gz" -D .strata
fi
echo "$SHA  $TAR" | sha256sum -c --quiet -
[ -f .strata/kit/build-kit.sh ] || tar -xzf "$TAR" -C .strata
# emcc 6.0.11 (the agentviz helper handles the EMSDK_PYTHON trap).
if ! command -v emcc >/dev/null || ! emcc --version | head -1 | grep -q 6.0.11; then
  # shellcheck disable=SC1091
  . "${AGENTVIZ_EMSDK_ENV:-/root/agentviz/tools/emsdk-env.sh}"
fi
STRATA_TARBALL="$TAR" STRATA_SHA256="$SHA" .strata/kit/build-kit.sh scene/kit.conf
