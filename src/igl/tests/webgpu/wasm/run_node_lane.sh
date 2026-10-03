#!/bin/bash
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# Test driver for iglWebGPUNodeTest: runs run_triangle_node.mjs on the two TriangleWebGPU wasm
# flavors under Node, with `navigator.gpu` from Dawn's Node binding (the `webgpu` npm package).
# The binding is not vendored: point WEBGPU_NODE_MODULE at its index.js, e.g.
#   buck2 test <igl>:iglWebGPUNodeTest -- --env WEBGPU_NODE_MODULE=<dir>/node_modules/webgpu/index.js
# NODE picks the Node binary (default: `node` on PATH; JSPI needs Node >= 24). IGL_WEBGPU_REFERENCE
# optionally names the native frame (TriangleWebGPU.rgba, written by WebGPUTriangleRenderTest to
# $IGL_RENDER_SNAPSHOT_DIR); without it the frames are checked against each other.
#
# usage: run_node_lane.sh <jspi js> <jspi wasm> <async js> <async wasm> [run_triangle_node.mjs]

set -euo pipefail
if [ $# -lt 4 ]; then
  echo "usage: $0 <jspi js> <jspi wasm> <async js> <async wasm> [run_triangle_node.mjs]" >&2
  exit 2
fi
# CI schedules this target without the binding, so an unset module is a skip; a wrong path fails.
if [ -z "${WEBGPU_NODE_MODULE:-}" ]; then
  echo "SKIP: WEBGPU_NODE_MODULE is not set; set it to the webgpu npm package's index.js (see $0)"
  exit 0
fi
if [ ! -f "$WEBGPU_NODE_MODULE" ]; then
  echo "FAIL: WEBGPU_NODE_MODULE=$WEBGPU_NODE_MODULE does not exist" >&2
  exit 1
fi
NODE=${NODE:-node}
SCRIPT=${5:-$(cd "$(dirname "$0")" && pwd)/run_triangle_node.mjs}
WORK=
cleanup() {
  local rc=$?
  set +e
  if [ -n "$WORK" ]; then
    rm -rf "$WORK"
  fi
  exit "$rc"
}
trap cleanup EXIT
for sig in 1 2 3 13 15; do
  # shellcheck disable=SC2064
  trap "exit $((sig + 128))" "$sig"
done
WORK=$(mktemp -d)
mkdir -p "$WORK/jspi" "$WORK/async"
cp "$1" "$WORK/jspi/iglWebGPUTriangleWasm"
cp "$2" "$WORK/jspi/iglWebGPUTriangleWasm.wasm"
cp "$3" "$WORK/async/iglWebGPUTriangleWasmAsync"
cp "$4" "$WORK/async/iglWebGPUTriangleWasmAsync.wasm"
"$NODE" "$SCRIPT" "$WORK/jspi" "$WORK/async" \
  "${IGL_WEBGPU_REFERENCE:--}"
