#!/bin/bash
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# Renders the shell sessions offscreen on WebGPU (Dawn on Metal), Metal and OpenGL (macOS), and
# compares every WebGPU frame with the Metal and OpenGL ones. Known differences: Metal renders
# CheckerboardMipmapSession's mips white and does not render the Uniform{Packed,Array}TestSessions.
#
# usage: render_compare.sh [out dir]   (run from anywhere inside fbsource)

set -euo pipefail
OUT=${1:-$(mktemp -d)}
ROOT=$(sl root)
cd "$ROOT"
# @fb-only
for BACKEND in WebGPU Metal OpenGL; do
  TARGET="$PKG:offscreen${BACKEND}TestAppleMac"
  BUNDLE=$(buck2 build "$TARGET" --show-full-output 2>/dev/null | awk '{print $2}')
  mkdir -p "$OUT/$BACKEND"
  IGL_RENDER_SNAPSHOT_DIR="$OUT/$BACKEND" xcrun xctest -XCTest All "$BUNDLE" >"$OUT/$BACKEND.log" 2>&1 || true
  echo "$BACKEND: $(grep Executed "$OUT/$BACKEND.log" | tail -1 | sed 's/^[[:space:]]*//')"
done
PNGDIFF="$(dirname "$0")/pngdiff.py"
for REF in Metal OpenGL; do
  echo
  echo "WebGPU vs $REF:"
  python3 "$PNGDIFF" "$OUT/WebGPU" "$OUT/$REF"
done
echo
echo "PNGs and logs: $OUT"
