#!/bin/bash
# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# Builds an fbcode WebGPU test binary in the CI sanitizer mode and runs one test directly, so that
# sanitizer reports reach the terminal (tpx keeps only the gtest output). Linux devserver only.
#
#   linux_repro.sh [gtest_filter] [Tests|TestsNull] [mode]
#
# Defaults: WebGPUResourcesBinderTest.SteadyStateCreatesNoBindGroups, Tests (lavapipe),
# @fbcode//mode/dev-asan-ubsan. Extra ASAN_OPTIONS/UBSAN_OPTIONS from the environment are kept.
set -euo pipefail

FILTER=${1:-WebGPUResourcesBinderTest.SteadyStateCreatesNoBindGroups}
FLAVOR=${2:-Tests}
MODE=${3:-@fbcode//mode/dev-asan-ubsan}

# Targets relative to the IGL root, six levels up from this script.
cd "$(dirname "$0")/../../../../../.."
build() {
  buck2 build "$MODE" "$1" --show-full-output | awk 'END { print $2 }'
}
BIN=$(build "public/src/igl:iglWebGPU${FLAVOR}Fbcode")
LVP=$(build fbsource//third-party/mesa/lavapipe:lavapipe_libs)
SUPP=$(build public/src/igl:webgpu_lsan_suppressions)
echo "binary: $BIN"

# Each copy of dawn_proc has its own proc table and only one gets dawnProcSetProcs(); more than one
# definition means wgpu*() calls can land on a null table.
echo "dawn_proc copies (dawnProcSetProcs definitions):"
for f in "$BIN" $(ldd "$BIN" | awk '$3 ~ /buck-out/ { print $3 }'); do
  n=$(nm --defined-only "$f" 2>/dev/null | grep -c ' T dawnProcSetProcs$' || true)
  if [ "$n" != "0" ]; then
    echo "  $n $f"
  fi
done

set -x
env \
  LD_LIBRARY_PATH="$LVP/lib" \
  VK_ICD_FILENAMES="$LVP/share/vulkan/icd.d/lvp_icd.x86_64.json" \
  VK_IMPLICIT_LAYER_PATH="$LVP/share/vulkan/implicit_layer.d" \
  VK_LAYER_PATH="$LVP/share/vulkan/explicit_layer.d" \
  VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1 \
  LSAN_OPTIONS="use_tls=0:suppressions=$SUPP" \
  ASAN_OPTIONS="${ASAN_OPTIONS:-symbolize=1}" \
  UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1}" \
  "$BIN" --gtest_filter="$FILTER"
