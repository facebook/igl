#!/bin/bash
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# Chrome check for the WebGPU wasm sample: builds iglWebGPUTriangleWasm and the native reference
# frame (WebGPUTriangleRenderTest on Dawn-Metal), then serves triangle.html in Chrome through
# run_triangle_chrome.sh. Run on a Mac from a normal Terminal; the page title shows the result.
#
# usage: check_chrome.sh [out dir]

set -euo pipefail
OUT=${1:-$(mktemp -d)}
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$(sl root)"
# @fb-only
mkdir -p "$OUT/build"
JS=$(buck2 build "$T:iglWebGPUTriangleWasm" --show-full-output 2>/dev/null | awk '{print $2}')
WASM=$(buck2 build "$T:iglWebGPUTriangleWasm[emscripten-wasm]" --show-full-output 2>/dev/null |
  awk '{print $2}')
cp "$JS" "$OUT/build/iglWebGPUTriangleWasm"
cp "$WASM" "$OUT/build/iglWebGPUTriangleWasm.wasm"
TESTS=$(buck2 build "$T:iglWebGPUTestsAppleMac" --show-full-output 2>/dev/null | awk '{print $2}')
IGL_RENDER_SNAPSHOT_DIR="$OUT" xcrun xctest -XCTest All "$TESTS" >"$OUT/native.log" 2>&1 || true
if [ ! -f "$OUT/TriangleWebGPU.rgba" ]; then
  echo "No native reference frame; see $OUT/native.log" >&2
  exit 1
fi
exec "$HERE/run_triangle_chrome.sh" "$OUT/build" "$OUT/TriangleWebGPU.rgba"
