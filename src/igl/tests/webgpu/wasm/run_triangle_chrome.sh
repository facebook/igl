#!/bin/bash
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# Serves the iglWebGPUTriangleWasm build with triangle.html and opens it in Chrome (JSPI and WebGPU
# are on by default since Chrome 137). The page checks the JSPI frame, an async readback on the
# page's imported device and 60 frames rendered to the WebGPU canvas; its title shows the result.
#
# usage: run_triangle_chrome.sh <dir with iglWebGPUTriangleWasm{,.wasm}> [reference.rgba] [port]
# reference.rgba: the native frame, written by WebGPUTriangleRenderTest to $IGL_RENDER_SNAPSHOT_DIR.

set -euo pipefail
DIR=$1
REFERENCE=${2:-}
PORT=${3:-8766}
HERE=$(cd "$(dirname "$0")" && pwd)
SERVE=
SERVER=
cleanup() {
  local rc=$?
  set +e
  if [ -n "$SERVER" ]; then
    kill "$SERVER" 2>/dev/null
  fi
  if [ -n "$SERVE" ]; then
    rm -rf "$SERVE"
  fi
  exit "$rc"
}
trap cleanup EXIT
for sig in 1 2 3 13 15; do
  # shellcheck disable=SC2064
  trap "exit $((sig + 128))" "$sig"
done
SERVE=$(mktemp -d)
cp "$DIR/iglWebGPUTriangleWasm" "$SERVE/iglWebGPUTriangleWasm.js"
cp "$DIR/iglWebGPUTriangleWasm.wasm" "$SERVE/"
cp "$HERE/triangle.html" "$SERVE/"
if [ -n "$REFERENCE" ]; then
  cp "$REFERENCE" "$SERVE/reference.rgba"
fi
cd "$SERVE"
python3 -m http.server "$PORT" --bind 127.0.0.1 >/dev/null 2>&1 &
SERVER=$!
sleep 1
open -a "Google Chrome" "http://127.0.0.1:$PORT/triangle.html"
echo "Serving $SERVE on http://127.0.0.1:$PORT/triangle.html; the page title shows the result."
echo "Press Enter to stop the server."
read -r
