/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Draws the splats into two float targets, then composes four panels: target, fit, error and
// how the fit changes as every splat moves right (forward-mode autodiff).
#include "igl_abi.slangh"
import SplatCommon;

typealias Kernel = GaussianKernel;

struct RenderParams {
  float4 flip; // x: sign of NDC y for the splat pass, y: 1 to flip v when reading its targets
  float4 gains; // x: error, y: sensitivity, z: 1 to linearize for an sRGB surface
};

IGL_UNIFORM_BUFFER(RenderParams, renderParams, 1);
IGL_TEXTURE2D(accumulationMap, 0);
IGL_TEXTURE2D(sensitivityMap, 1);
IGL_TEXTURE2D(targetMap, 2);

struct SplatInstance {
  [[vk::location(0)]] float4 shape : POSITION;
  [[vk::location(1)]] float4 extra : TEXCOORD0;
  [[vk::location(2)]] float4 color : COLOR0;
};

struct SplatVarying {
  float4 position : SV_Position;
  [[vk::location(0)]] float2 pixel : TEXCOORD0;
  [[vk::location(1)]] nointerpolation float4 shape : TEXCOORD1;
  [[vk::location(2)]] nointerpolation float4 extra : TEXCOORD2;
  [[vk::location(3)]] nointerpolation float4 color : COLOR0;
};

struct SplatTargets {
  float4 accumulation : SV_Target0; // weight * color, weight
  float4 sensitivity : SV_Target1; // its derivative with respect to center.x
};

[shader("vertex")]
SplatVarying splatVertex(SplatInstance instance, uint vertexId: IGL_SV_VERTEX_ID) {
  const SplatRecord record = {instance.shape, instance.extra, instance.color};
  const Splat splat = Splat.load(record);
  const float2 corner = float2((vertexId & 1u) != 0u ? kExtent : -kExtent,
                               (vertexId & 2u) != 0u ? kExtent : -kExtent);
  const float2 scaled = corner * exp(splat.logScale);
  const float c = cos(splat.rotation);
  const float s = sin(splat.rotation);
  const float2 pixel = splat.center + float2(c * scaled.x - s * scaled.y, s * scaled.x + c * scaled.y);
  const float2 ndc = pixel / float(kImageSize) * 2.0 - 1.0;
  SplatVarying output;
  output.position = float4(ndc.x, -ndc.y * renderParams.flip.x, 0.0, 1.0);
  output.pixel = pixel;
  output.shape = instance.shape;
  output.extra = instance.extra;
  output.color = instance.color;
  return output;
}

[shader("fragment")]
SplatTargets splatFragment(SplatVarying input) {
  const SplatRecord record = {input.shape, input.extra, input.color};
  const Splat splat = Splat.load(record);
  var direction = Splat.dzero();
  direction.center = float2(1.0, 0.0);
  const DifferentialPair<float> weight =
      fwd_diff(splatWeight<Kernel>)(diffPair(splat, direction), input.pixel);
  SplatTargets targets;
  targets.accumulation = float4(weight.p * splat.color, weight.p);
  targets.sensitivity = float4(weight.d * splat.color, weight.d);
  return targets;
}

struct CompositeVarying {
  float4 position : SV_Position;
  [[vk::location(0)]] float2 uv : TEXCOORD0;
};

[shader("vertex")]
CompositeVarying compositeVertex(uint vertexId: IGL_SV_VERTEX_ID) {
  const float2 uv = float2(float((vertexId << 1u) & 2u), float(vertexId & 2u));
  CompositeVarying output;
  output.position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
  output.uv = uv;
  return output;
}

[shader("fragment")]
float4 compositeFragment(CompositeVarying input) : SV_Target0 {
  const float2 panel = floor(input.uv * 2.0);
  const float2 uv = frac(input.uv * 2.0);
  const float2 splatUv = float2(uv.x, renderParams.flip.y > 0.5 ? 1.0 - uv.y : uv.y);
  const float4 sum = IGL_SAMPLE2D_LEVEL(accumulationMap, splatUv, 0.0);
  const float totalWeight = sum.a + kWeightEpsilon;
  const float3 fit = sum.rgb / totalWeight;
  const float3 target = IGL_SAMPLE2D_LEVEL(targetMap, uv, 0.0).rgb;
  float3 color;
  if (panel.y < 1.0) {
    color = panel.x < 1.0 ? target : fit;
  } else if (panel.x < 1.0) {
    color = abs(fit - target) * renderParams.gains.x;
  } else {
    // d(sum(w c) / sum(w))/dx = (d sum(w c) - C d sum(w)) / sum(w)
    const float4 d = IGL_SAMPLE2D_LEVEL(sensitivityMap, splatUv, 0.0);
    const float3 dColor = (d.rgb - fit * d.a) / totalWeight;
    const float s = dot(dColor, float3(0.299, 0.587, 0.114)) * renderParams.gains.y;
    color = s > 0.0 ? float3(s, 0.45 * s, 0.0) : float3(0.0, -0.45 * s, -s);
  }
  // The image is display-referred; an sRGB surface would encode it a second time.
  color = saturate(color);
  return float4(renderParams.gains.z > 0.5 ? pow(color, 2.2) : color, 1.0);
}
