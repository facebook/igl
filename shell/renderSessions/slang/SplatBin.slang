/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Lists, for every 8x8 tile of the training image, the splats that reach it, in splat order.
#include "igl_abi.slangh"
import SplatCommon;

IGL_STORAGE_BUFFER(SplatRecord, splats, 0);
IGL_STORAGE_BUFFER(uint, tiles, 1);
IGL_STORAGE_BUFFER(Atomic<uint>, overflowCount, 2);

[shader("compute")]
[numthreads(64, 1, 1)]
void binTiles(uint3 dispatchId: SV_DispatchThreadID) {
  const uint tile = dispatchId.x;
  if (tile >= kTilesPerRow * kTilesPerRow) {
    return;
  }
  const float2 tileMin = float2(float(tile % kTilesPerRow), float(tile / kTilesPerRow)) * kTileSize;
  const float2 tileMax = tileMin + float(kTileSize);
  uint count = 0;
  for (uint i = 0; i < kSplatCount; ++i) {
    const Splat splat = Splat.load(splats[i]);
    const float2 radius = boundingRadius(splat);
    if (all(splat.center + radius >= tileMin) && all(splat.center - radius <= tileMax)) {
      if (count < kTileCapacity) {
        tiles[tile * kTileStride + 1 + count] = i;
      }
      ++count;
    }
  }
  tiles[tile * kTileStride] = min(count, kTileCapacity);
  if (count > kTileCapacity) {
    overflowCount[0].add(1);
  }
}
