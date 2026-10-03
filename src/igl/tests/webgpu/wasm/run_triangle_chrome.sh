#!/bin/bash
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

set -euo pipefail
DIR=$1
REFERENCE=${2:-}
PORT=${3:-8766}
HERE=$(cd "$(dirname "$0")" && pwd)
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
trap 'kill $SERVER' EXIT
sleep 1
open -a "Google Chrome" "http://127.0.0.1:$PORT/triangle.html"
echo "Serving $SERVE on http://127.0.0.1:$PORT/triangle.html; the page title shows the result."
echo "Press Enter to stop the server."
read -r
