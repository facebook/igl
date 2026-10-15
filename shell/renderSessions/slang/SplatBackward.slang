/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// One workgroup per splat gathers the splat's gradient over the pixels it covers with reverse-mode
// autodiff, then sums the threads' parts in a fixed order: no atomics, and the same result on
// every run.
#include "igl_abi.slangh"
import SplatCommon;

typealias Kernel = GaussianKernel;

IGL_STORAGE_BUFFER(SplatRecord, splats, 0);
IGL_STORAGE_BUFFER(PixelState, pixels, 1);
IGL_STORAGE_BUFFER(SplatRecord, gradients, 2);

groupshared SplatRecord partialSums[64];

[shader("compute")]
[numthreads(64, 1, 1)]
void backwardSplats(uint3 groupId: SV_GroupID, uint3 threadId: SV_GroupThreadID) {
  const uint index = groupId.x;
  const Splat splat = Splat.load(splats[index]);
  const float2 radius = boundingRadius(splat);
  const int2 low = max(int2(floor(splat.center - radius)), int2(0));
  const int2 high = min(int2(ceil(splat.center + radius)), int2(int(kImageSize) - 1));
  const uint2 size = uint2(max(high - low + 1, int2(0)));
  Splat.Differential gradient = Splat.dzero();
  for (uint i = threadId.x; i < size.x * size.y; i += 64) {
    const uint2 p = uint2(low) + uint2(i % size.x, i / size.x);
    const PixelState state = pixels[p.y * kImageSize + p.x];
    var term = diffPair(splat);
    bwd_diff(blendTerm<Kernel>)(
        term, float2(p) + 0.5, state.blended.rgb, state.blended.a, state.gradient.rgb);
    gradient = Splat.dadd(gradient, term.d);
  }
  partialSums[threadId.x] = toRecord(gradient);
  GroupMemoryBarrierWithGroupSync();
  [ForceUnroll]
  for (uint stride = 32; stride > 0; stride >>= 1) {
    if (threadId.x < stride) {
      partialSums[threadId.x] = addRecords(partialSums[threadId.x], partialSums[threadId.x + stride]);
    }
    GroupMemoryBarrierWithGroupSync();
  }
  if (threadId.x == 0) {
    gradients[index] = partialSums[0];
  }
}
