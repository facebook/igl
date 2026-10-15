/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Inverse rendering for chapter 2 of SlangShowcaseSession: recovers every sphere's material
// parameters from images rendered with hidden ones. Each step casts a ray per pixel against the
// analytic spheres, renders the hit point with the hidden and the fitted parameters through the
// same differentiable materials that draw the scene, and back-propagates the difference with
// bwd_diff. A group per sphere then sums its pixels' gradients in a fixed order, so runs are
// deterministic, and takes an Adam step.
#include "igl_abi.slangh"
import ShowcaseMaterials;

static const uint kFitSize = 128;
static const uint kSpheres = 16;
static const uint kReduceThreads = 256;

struct SphereInstance {
  float4 placement; // center, radius
  MaterialData material;
  uint4 type; // x: material type id
};

struct AdamState {
  MaterialData m;
  MaterialData v;
  uint4 step; // x: steps taken
};

struct PixelGradient {
  MaterialData gradient;
  uint4 sphere; // x: the sphere hit, or kSpheres
};

struct InverseUniforms {
  float4x4 inverseViewProjection;
  float4 camera; // position
  Lighting lighting; // light positions follow a schedule of their own
};

IGL_READONLY_STORAGE_BUFFER(SphereInstance, spheres, 0);
IGL_STORAGE_BUFFER(MaterialData, fitted, 1);
IGL_STORAGE_BUFFER(AdamState, adam, 2);
IGL_STORAGE_BUFFER(PixelGradient, pixels, 3);
IGL_UNIFORM_BUFFER(InverseUniforms, uniforms, 4);

// Lights circle the spheres a little further at every step, so each sees them from many sides.
Lighting trainingLighting(uint step) {
  Lighting lighting = uniforms.lighting;
  [ForceUnroll]
  for (int i = 0; i < 3; ++i) {
    const float angle = 0.618034 * float(step) + 2.0944 * float(i);
    lighting.lights[i].xyz = float3(7.0 * cos(angle), 3.0 + 1.5 * float(i), -6.0 + 4.0 * sin(angle));
  }
  return lighting;
}

// log(1 + x) evens out highlights and shadows without clamping their gradients away.
[Differentiable]
float3 observe<M : IMaterialModel>(MaterialData d, SurfaceSample s, no_diff float3 position, Lighting lighting) {
  return log(1.0 + radiance<M>(d, s, position, lighting));
}

[Differentiable]
float pixelLoss<M : IMaterialModel>(MaterialData d,
                                    SurfaceSample s,
                                    no_diff float3 position,
                                    Lighting lighting,
                                    no_diff float3 target) {
  const float3 residual = observe<M>(d, s, position, lighting) - target;
  return dot(residual, residual);
}

// The target is what the hidden parameters look like; the fit never reads them otherwise.
MaterialData gradientOf<M : IMaterialModel>(MaterialData hidden,
                                            MaterialData d,
                                            SurfaceSample s,
                                            float3 position,
                                            Lighting lighting) {
  const float3 target = observe<M>(hidden, s, position, lighting);
  var parameters = diffPair(d);
  bwd_diff(pixelLoss<M>)(parameters, s, position, lighting, target, 1.0);
  return parameters.d;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void inverseGradients(uint3 id: SV_DispatchThreadID) {
  const uint index = id.y * kFitSize + id.x;
  const float2 ndc = float2((float(id.x) + 0.5) / kFitSize * 2.0 - 1.0, 1.0 - (float(id.y) + 0.5) / kFitSize * 2.0);
  const float4 far = mul(float4(ndc, 1.0, 1.0), uniforms.inverseViewProjection);
  const float3 origin = uniforms.camera.xyz;
  const float3 direction = normalize(far.xyz / far.w - origin);

  float nearest = 1e30;
  uint hit = kSpheres;
  for (uint i = 0; i < kSpheres; ++i) {
    const float4 placement = spheres[i].placement;
    const float3 offset = origin - placement.xyz;
    const float b = dot(offset, direction);
    const float discriminant = b * b - (dot(offset, offset) - placement.w * placement.w);
    const float t = -b - sqrt(max(discriminant, 0.0));
    if (discriminant > 0.0 && t > 0.0 && t < nearest) {
      nearest = t;
      hit = i;
    }
  }
  PixelGradient result;
  result.gradient.base = float4(0.0);
  result.gradient.extra = float4(0.0);
  result.sphere = uint4(hit, 0, 0, 0);
  if (hit == kSpheres) {
    pixels[index] = result;
    return;
  }

  const SphereInstance sphere = spheres[hit];
  const float3 position = origin + nearest * direction;
  SurfaceSample s;
  s.normal = (position - sphere.placement.xyz) / sphere.placement.w;
  s.tangent = normalize(float3(-s.normal.z, 0.0, s.normal.x));
  s.bitangent = cross(s.normal, s.tangent);
  s.view = -direction;
  const Lighting lighting = trainingLighting(adam[hit].step.x);
  const MaterialData d = fitted[hit];
  switch (sphere.type.x) {
  case 0:
    result.gradient = gradientOf<MetalRough<float>>(sphere.material, d, s, position, lighting);
    break;
  case 1:
    result.gradient = gradientOf<Clearcoat<float>>(sphere.material, d, s, position, lighting);
    break;
  case 2:
    result.gradient = gradientOf<Sheen<float>>(sphere.material, d, s, position, lighting);
    break;
  default:
    result.gradient = gradientOf<Anisotropic<float>>(sphere.material, d, s, position, lighting);
    break;
  }
  pixels[index] = result;
}

groupshared float4 sharedBase[kReduceThreads];
groupshared float4 sharedExtra[kReduceThreads];

// One group per sphere.
[shader("compute")]
[numthreads(kReduceThreads, 1, 1)]
void inverseAdam(uint3 group: SV_GroupID, uint3 thread: SV_GroupThreadID) {
  const uint sphere = group.x;
  float4 base = float4(0.0);
  float4 extra = float4(0.0);
  for (uint i = thread.x; i < kFitSize * kFitSize; i += kReduceThreads) {
    const PixelGradient pixel = pixels[i];
    if (pixel.sphere.x == sphere) {
      base += pixel.gradient.base;
      extra += pixel.gradient.extra;
    }
  }
  sharedBase[thread.x] = base;
  sharedExtra[thread.x] = extra;
  GroupMemoryBarrierWithGroupSync();
  [ForceUnroll]
  for (uint stride = kReduceThreads / 2; stride > 0; stride /= 2) {
    if (thread.x < stride) {
      sharedBase[thread.x] += sharedBase[thread.x + stride];
      sharedExtra[thread.x] += sharedExtra[thread.x + stride];
    }
    GroupMemoryBarrierWithGroupSync();
  }
  if (thread.x != 0) {
    return;
  }

  const float kRate = 0.02;
  const float kBeta1 = 0.9;
  const float kBeta2 = 0.999;
  AdamState state = adam[sphere];
  const float step = float(state.step.x + 1);
  state.m.base = lerp(sharedBase[0], state.m.base, kBeta1);
  state.m.extra = lerp(sharedExtra[0], state.m.extra, kBeta1);
  state.v.base = lerp(sharedBase[0] * sharedBase[0], state.v.base, kBeta2);
  state.v.extra = lerp(sharedExtra[0] * sharedExtra[0], state.v.extra, kBeta2);
  const float scale = kRate * sqrt(1.0 - pow(kBeta2, step)) / (1.0 - pow(kBeta1, step));
  MaterialData d = fitted[sphere];
  d.base = clamp(d.base - scale * state.m.base / (sqrt(state.v.base) + 1e-8),
                 float4(0.0, 0.0, 0.0, 0.05),
                 float4(1.0));
  d.extra = saturate(d.extra - scale * state.m.extra / (sqrt(state.v.extra) + 1e-8));
  fitted[sphere] = d;
  state.step.x += 1;
  adam[sphere] = state;
}
