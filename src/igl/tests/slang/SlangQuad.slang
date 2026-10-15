/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Vertex buffer 0, uniform buffer 1 (a matrix and a vector), texture unit 0.
#include "igl_abi.slangh"

struct QuadVertex {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float2 texcoord : TEXCOORD0;
};

struct QuadVarying {
  float4 position : SV_Position;
  [[vk::location(0)]] float2 uv : TEXCOORD0;
};

struct QuadUniforms {
  float4x4 transform;
  float4 tint;
};

IGL_UNIFORM_BUFFER(QuadUniforms, quadUniforms, 1);
IGL_TEXTURE2D(albedo, 0);

[shader("vertex")]
QuadVarying quadVertex(QuadVertex input) {
  QuadVarying output;
  output.position = mul(float4(input.position, 1.0), quadUniforms.transform);
  output.uv = input.texcoord;
  return output;
}

[shader("fragment")]
float4 quadFragment(QuadVarying input) : SV_Target0 {
  return IGL_SAMPLE2D(albedo, input.uv) * quadUniforms.tint;
}
