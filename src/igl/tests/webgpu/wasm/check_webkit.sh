#!/bin/bash
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# WebKit check for the WebGPU wasm sample (optional, macOS 26+): builds iglWebGPUTriangleWasmAsync
# (WebKit has no JSPI) and the native reference frame (WebGPUTriangleRenderTest on Dawn-Metal), then
# runs triangle.html?flavor=async in a WKWebView through run_triangle_webkit.swift and prints the
# per-check log. Unlike check_chrome.sh it needs no browser and exits non-zero on failure.
#
# usage: check_webkit.sh [out dir] [port]

set -euo pipefail
OUT=${1:-$(mktemp -d)}
PORT=${2:-8767}
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$(sl root)"
# @fb-only
mkdir -p "$OUT/serve"
JS=$(buck2 build "$T:iglWebGPUTriangleWasmAsync" --show-full-output 2>/dev/null | awk '{print $2}')
WASM=$(buck2 build "$T:iglWebGPUTriangleWasmAsync[emscripten-wasm]" --show-full-output 2>/dev/null |
  awk '{print $2}')
cp "$JS" "$OUT/serve/iglWebGPUTriangleWasmAsync.js"
cp "$WASM" "$OUT/serve/iglWebGPUTriangleWasmAsync.wasm"
cp "$HERE/triangle.html" "$OUT/serve/"
TESTS=$(buck2 build "$T:iglWebGPUTestsAppleMac" --show-full-output 2>/dev/null | awk '{print $2}')
IGL_RENDER_SNAPSHOT_DIR="$OUT" xcrun xctest -XCTest All "$TESTS" >"$OUT/native.log" 2>&1 || true
if [ ! -f "$OUT/TriangleWebGPU.rgba" ]; then
  echo "No native reference frame; see $OUT/native.log" >&2
  exit 1
fi
cp "$OUT/TriangleWebGPU.rgba" "$OUT/serve/reference.rgba"
SERVER=
cleanup() {
  local rc=$?
  set +e
  if [ -n "$SERVER" ]; then
    kill "$SERVER" 2>/dev/null
  fi
  exit "$rc"
}
trap cleanup EXIT
for sig in 1 2 3 13 15; do
  # shellcheck disable=SC2064
  trap "exit $((sig + 128))" "$sig"
done
python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$OUT/serve" >/dev/null 2>&1 &
SERVER=$!
sleep 1
xcrun swift "$HERE/run_triangle_webkit.swift" "http://127.0.0.1:$PORT/triangle.html?flavor=async"
