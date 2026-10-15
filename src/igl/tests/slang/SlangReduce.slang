/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Storage buffers 0..3, a groupshared tree reduction and an atomic counter.
#include "igl_abi.slangh"

struct ReduceParams {
  uint count;
  uint pad0;
  uint pad1;
  uint pad2;
};

IGL_STORAGE_BUFFER(float, inputValues, 0);
IGL_STORAGE_BUFFER(float, groupSums, 1);
IGL_STORAGE_BUFFER(Atomic<uint>, counters, 2);
IGL_STORAGE_BUFFER(ReduceParams, reduceParams, 3);

groupshared float partial[64];

[shader("compute")]
[numthreads(64, 1, 1)]
void reduceGroups(uint3 dispatchId: SV_DispatchThreadID,
                  uint3 threadId: SV_GroupThreadID,
                  uint3 groupId: SV_GroupID) {
  const uint i = dispatchId.x;
  partial[threadId.x] = i < reduceParams[0].count ? inputValues[i] : 0.0;
  GroupMemoryBarrierWithGroupSync();
  [ForceUnroll]
  for (uint stride = 32; stride > 0; stride >>= 1) {
    if (threadId.x < stride) {
      partial[threadId.x] += partial[threadId.x + stride];
    }
    GroupMemoryBarrierWithGroupSync();
  }
  if (threadId.x == 0) {
    groupSums[groupId.x] = partial[0];
    counters[0].add(1);
  }
}
