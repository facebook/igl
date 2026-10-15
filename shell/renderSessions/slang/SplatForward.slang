/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Blends the splats at every training pixel and differentiates the loss against the target.
#include "igl_abi.slangh"
import SplatCommon;

typealias Kernel = GaussianKernel;
typealias Loss = L2Loss;

IGL_STORAGE_BUFFER(SplatRecord, splats, 0);
IGL_STORAGE_BUFFER(uint, tiles, 1);
IGL_STORAGE_BUFFER(float4, targetPixels, 2);
IGL_STORAGE_BUFFER(PixelState, pixels, 3);

[shader("compute")]
[numthreads(8, 8, 1)]
void forwardPixels(uint3 dispatchId: SV_DispatchThreadID) {
  const uint2 p = dispatchId.xy;
  const float2 pixel = float2(p) + 0.5;
  const uint tile = (p.y / kTileSize) * kTilesPerRow + p.x / kTileSize;
  const uint count = tiles[tile * kTileStride];
  float3 weightedColor = float3(0.0);
  float totalWeight = kWeightEpsilon;
  for (uint k = 0; k < count; ++k) {
    const Splat splat = Splat.load(splats[tiles[tile * kTileStride + 1 + k]]);
    const float w = splatWeight<Kernel>(splat, pixel);
    weightedColor += w * splat.color;
    totalWeight += w;
  }
  const float3 blended = weightedColor / totalWeight;
  const uint index = p.y * kImageSize + p.x;
  const float3 target = targetPixels[index].rgb;
  var color = diffPair(blended);
  bwd_diff(Loss.pixelLoss)(color, target, 1.0 / float(kImageSize * kImageSize));
  PixelState state;
  state.blended = float4(blended, totalWeight);
  state.gradient = float4(color.d, Loss.pixelLoss(blended, target));
  pixels[index] = state;
}
