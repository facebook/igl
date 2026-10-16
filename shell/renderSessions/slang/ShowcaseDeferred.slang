/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Chapter 5 of SlangShowcaseSession: deferred lighting. The spheres write a G-buffer, and a
// fullscreen pass lights every pixel of it with up to 64 point lights, calling the material model
// of the sphere that covers the pixel. With subpasses, the G-buffer never leaves tile memory: the
// lighting subpass reads it as input attachments. Otherwise a G-buffer pass stores it and the
// lighting pass samples it. Both lighting entry points share one shading function.
#include "igl_abi.slangh"
import ShowcaseMaterials;

static const uint kMaxLights = 64;

// Mirrors ShowcaseMaterialsPass.slang.
struct SceneUniforms {
  float4x4 viewProjection;
  float4 camera; // position
  Lighting lighting;
  float4 display; // x: 1 to gamma-encode for a UNorm surface
};

struct SphereInstance {
  float4 placement; // center, radius
  MaterialData material;
  uint4 type; // x: material type id
};

struct PointLight {
  float4 position; // position, intensity
  float4 color;
};

struct DeferredUniforms {
  uint4 lightCount; // x: number of point lights
};

IGL_UNIFORM_BUFFER(SceneUniforms, scene, 1);
IGL_READONLY_STORAGE_BUFFER(SphereInstance, spheres, 2);
IGL_READONLY_STORAGE_BUFFER(PointLight, lights, 3);
IGL_UNIFORM_BUFFER(DeferredUniforms, deferred, 4);

// The G-buffer in tile memory: color attachments 1 to 3, after the image in attachment 0.
IGL_SUBPASS_INPUT(tileNormal, 1);
IGL_SUBPASS_INPUT(tileTangent, 2);
IGL_SUBPASS_INPUT(tilePosition, 3);

// The stored G-buffer, read at the pixel of the fragment.
IGL_TEXTURE2D(storedNormal, 0);
IGL_TEXTURE2D(storedTangent, 1);
IGL_TEXTURE2D(storedPosition, 2);

// Three RGBA16F attachments. A sphere's tangent sign is never zero, so a zero marks the background,
// where the G-buffer keeps its clear value.
struct GBuffer {
  float4 normal; // normal, tangent sign
  float4 tangent; // tangent
  float4 position; // world position, sphere instance
};

struct SphereVertex {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float4 tangent : TANGENT;
};

struct GBufferVarying {
  float4 position : SV_Position;
  [[vk::location(0)]] float3 world : TEXCOORD0;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float4 tangent : TANGENT;
  [[vk::location(3)]] nointerpolation uint instance : TEXCOORD1;
};

[shader("vertex")]
GBufferVarying deferredVertex(SphereVertex vertex, uint instance: IGL_SV_INSTANCE_ID) {
  const float4 placement = spheres[instance].placement;
  const float3 world = placement.xyz + vertex.position * placement.w;
  GBufferVarying output;
  output.position = mul(float4(world, 1.0), scene.viewProjection);
  output.world = world;
  output.normal = vertex.normal;
  output.tangent = vertex.tangent;
  output.instance = instance;
  return output;
}

GBuffer gbuffer(GBufferVarying input) {
  GBuffer g;
  g.normal = float4(input.normal, input.tangent.w);
  g.tangent = float4(input.tangent.xyz, 0.0);
  g.position = float4(input.world, float(input.instance));
  return g;
}

// Color attachment 0 holds the image, which the G-buffer subpass leaves alone. Metal takes the
// attachments from SV_Target, but SPIR-V numbers the outputs from 0 without vk::location.
struct GBufferSubpassOutput {
  [[vk::location(1)]] float4 normal : SV_Target1;
  [[vk::location(2)]] float4 tangent : SV_Target2;
  [[vk::location(3)]] float4 position : SV_Target3;
};

struct GBufferStoredOutput {
  float4 normal : SV_Target0;
  float4 tangent : SV_Target1;
  float4 position : SV_Target2;
};

[shader("fragment")]
GBufferSubpassOutput gbufferSubpass(GBufferVarying input) {
  const GBuffer g = gbuffer(input);
  GBufferSubpassOutput output;
  output.normal = g.normal;
  output.tangent = g.tangent;
  output.position = g.position;
  return output;
}

[shader("fragment")]
GBufferStoredOutput gbufferStored(GBufferVarying input) {
  const GBuffer g = gbuffer(input);
  GBufferStoredOutput output;
  output.normal = g.normal;
  output.tangent = g.tangent;
  output.position = g.position;
  return output;
}

[shader("vertex")]
float4 lightingVertex(uint vertexId: IGL_SV_VERTEX_ID) : SV_Position {
  const float2 uv = float2(float((vertexId << 1u) & 2u), float(vertexId & 2u));
  return float4(uv * 2.0 - 1.0, 0.0, 1.0);
}

float3 toneMap(float3 color) {
  // Narkowicz's ACES fit; sRGB surfaces encode gamma themselves.
  const float3 mapped = saturate(color * (2.51 * color + 0.03) / (color * (2.43 * color + 0.59) + 0.14));
  return scene.display.x > 0.5 ? pow(mapped, 1.0 / 2.2) : mapped;
}

// Radiance toward the viewer under the sky and every point light.
float3 pointLightRadiance<M : IMaterialModel>(MaterialData d, SurfaceSample s, float3 position) {
  float3 color = M.ambient(d, s, scene.lighting.sky.rgb, scene.lighting.ground.rgb);
  const uint count = min(deferred.lightCount.x, kMaxLights);
  for (uint i = 0; i < count; ++i) {
    const PointLight light = lights[i];
    const float3 toLight = light.position.xyz - position;
    const float distance2 = dot(toLight, toLight);
    color += M.lightResponse(d, s, toLight * rsqrt(distance2)) * light.color.rgb *
             (light.position.w / distance2);
  }
  return color;
}

// Static dispatch on the type id that the G-buffer leads to: the Adreno 829 Vulkan compiler
// mis-shades createDynamicObject() when the payload is chosen per pixel.
float3 radianceOfType(uint type, MaterialData d, SurfaceSample s, float3 position) {
  switch (type) {
  case 0:
    return pointLightRadiance<MetalRough<float>>(d, s, position);
  case 1:
    return pointLightRadiance<Clearcoat<float>>(d, s, position);
  case 2:
    return pointLightRadiance<Sheen<float>>(d, s, position);
  default:
    return pointLightRadiance<Anisotropic<float>>(d, s, position);
  }
}

// Lights a pixel of the G-buffer, wherever the G-buffer comes from.
float4 shade(GBuffer g) {
  if (g.normal.w == 0.0) {
    discard;
  }
  const SphereInstance sphere = spheres[uint(g.position.w + 0.5)];
  SurfaceSample s;
  s.normal = normalize(g.normal.xyz);
  s.tangent = normalize(g.tangent.xyz - s.normal * dot(s.normal, g.tangent.xyz));
  s.bitangent = cross(s.normal, s.tangent) * g.normal.w;
  s.view = normalize(scene.camera.xyz - g.position.xyz);
  return float4(toneMap(radianceOfType(sphere.type.x, sphere.material, s, g.position.xyz)), 1.0);
}

[shader("fragment")]
float4 lightingSubpass() : SV_Target0 {
  GBuffer g;
  g.normal = tileNormal.SubpassLoad();
  g.tangent = tileTangent.SubpassLoad();
  g.position = tilePosition.SubpassLoad();
  return shade(g);
}

[shader("fragment")]
float4 lightingStored(float4 position: SV_Position) : SV_Target0 {
  const int2 pixel = int2(position.xy);
  GBuffer g;
  g.normal = IGL_LOAD2D(storedNormal, pixel, 0);
  g.tangent = IGL_LOAD2D(storedTangent, pixel, 0);
  g.position = IGL_LOAD2D(storedPosition, pixel, 0);
  return shade(g);
}
