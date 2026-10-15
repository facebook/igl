/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Instanced quads built from the vertex ID, per-instance attributes, two render targets.
#include "igl_abi.slangh"

struct QuadInstance {
  [[vk::location(0)]] float4 position : POSITION; // xy center, zw half extent
  [[vk::location(1)]] float4 color : COLOR0;
};

struct QuadVarying {
  float4 position : SV_Position;
  [[vk::location(0)]] float4 color : COLOR0;
};

struct QuadTargets {
  float4 color : SV_Target0;
  float4 coverage : SV_Target1;
};

[shader("vertex")]
QuadVarying instanceVertex(QuadInstance instance, uint vertexId: IGL_SV_VERTEX_ID) {
  const float2 corner =
      float2((vertexId & 1u) != 0u ? 1.0 : -1.0, (vertexId & 2u) != 0u ? 1.0 : -1.0);
  QuadVarying output;
  output.position = float4(instance.position.xy + corner * instance.position.zw, 0.0, 1.0);
  output.color = instance.color;
  return output;
}

[shader("fragment")]
QuadTargets instanceFragment(QuadVarying input) {
  QuadTargets targets;
  targets.color = input.color;
  targets.coverage = float4(0.0, 0.0, 0.25, 0.25);
  return targets;
}
