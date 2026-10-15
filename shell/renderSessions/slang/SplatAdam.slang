/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// One Adam step per splat, then clamps the parameters to a sensible range.
#include "igl_abi.slangh"
import SplatCommon;

struct SplatMoments {
  SplatRecord mean;
  SplatRecord variance;
  float4 step; // x: steps taken
};

IGL_STORAGE_BUFFER(SplatRecord, splats, 0);
IGL_STORAGE_BUFFER(SplatRecord, gradients, 1);
IGL_STORAGE_BUFFER(SplatMoments, moments, 2);

static const float4 kShapeRate = float4(0.25, 0.25, 0.02, 0.02);
static const float4 kExtraRate = float4(0.02, 0.05, 0.0, 0.0);
static const float4 kColorRate = float4(0.01, 0.01, 0.01, 0.0);
static const float kBeta1 = 0.9;
static const float kBeta2 = 0.999;

float4 adam(float4 value, float4 gradient, inout float4 mean, inout float4 variance, float4 rate, float t) {
  mean = kBeta1 * mean + (1.0 - kBeta1) * gradient;
  variance = kBeta2 * variance + (1.0 - kBeta2) * gradient * gradient;
  const float4 meanHat = mean / (1.0 - pow(kBeta1, t));
  const float4 varianceHat = variance / (1.0 - pow(kBeta2, t));
  return value - rate * meanHat / (sqrt(varianceHat) + 1e-12);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void adamStep(uint3 dispatchId: SV_DispatchThreadID) {
  const uint i = dispatchId.x;
  if (i >= kSplatCount) {
    return;
  }
  SplatMoments m = moments[i];
  const float t = m.step.x + 1.0;
  SplatRecord record = splats[i];
  const SplatRecord g = gradients[i];
  record.shape = adam(record.shape, g.shape, m.mean.shape, m.variance.shape, kShapeRate, t);
  record.extra = adam(record.extra, g.extra, m.mean.extra, m.variance.extra, kExtraRate, t);
  record.color = adam(record.color, g.color, m.mean.color, m.variance.color, kColorRate, t);
  record.shape.xy = clamp(record.shape.xy, float2(0.0), float2(float(kImageSize)));
  record.shape.zw = clamp(record.shape.zw, float2(log(0.35)), float2(log(48.0)));
  record.extra.y = clamp(record.extra.y, -6.0, 6.0);
  record.color = float4(saturate(record.color.rgb), 0.0);
  m.step.x = t;
  moments[i] = m;
  splats[i] = record;
}
