/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Shades a grid of spheres. Chapter 1 shades them three ways from one generic function: a pipeline
// per material type (static specialization), one pipeline for all types (dynamic dispatch on a type
// id), and a pipeline per type with every lobe evaluated in half precision. Chapter 2 splits each
// sphere into the target and the fit, or shows how its color changes with roughness.
#include "igl_abi.slangh"
import ShowcaseMaterials;

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

IGL_UNIFORM_BUFFER(SceneUniforms, scene, 1);
IGL_READONLY_STORAGE_BUFFER(SphereInstance, spheres, 2);
IGL_READONLY_STORAGE_BUFFER(MaterialData, fitted, 3);

struct SphereVertex {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float4 tangent : TANGENT;
};

struct ShadedVarying {
  float4 position : SV_Position;
  [[vk::location(0)]] float3 world : TEXCOORD0;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float4 tangent : TANGENT;
  [[vk::location(3)]] nointerpolation uint instance : TEXCOORD1;
};

[shader("vertex")]
ShadedVarying sphereVertex(SphereVertex vertex, uint instance: IGL_SV_INSTANCE_ID) {
  const float4 placement = spheres[instance].placement;
  const float3 world = placement.xyz + vertex.position * placement.w;
  ShadedVarying output;
  output.position = mul(float4(world, 1.0), scene.viewProjection);
  output.world = world;
  output.normal = vertex.normal;
  output.tangent = vertex.tangent;
  output.instance = instance;
  return output;
}

float3 encode(float3 color) {
  return scene.display.x > 0.5 ? pow(color, 1.0 / 2.2) : color;
}

float3 toneMap(float3 color) {
  // Narkowicz's ACES fit; sRGB surfaces encode gamma themselves.
  return encode(saturate(color * (2.51 * color + 0.03) / (color * (2.43 * color + 0.59) + 0.14)));
}

SurfaceSample surfaceSample(ShadedVarying input) {
  SurfaceSample s;
  s.normal = normalize(input.normal);
  s.tangent = normalize(input.tangent.xyz - s.normal * dot(s.normal, input.tangent.xyz));
  s.bitangent = cross(s.normal, s.tangent) * input.tangent.w;
  s.view = normalize(scene.camera.xyz - input.world);
  return s;
}

float4 shadePixel<M : IMaterial>(M material, ShadedVarying input) {
  return float4(toneMap(material.shade(surfaceSample(input), input.world, scene.lighting)), 1.0);
}

[shader("fragment")]
float4 fragmentDynamic(ShadedVarying input) : SV_Target0 {
  const SphereInstance sphere = spheres[input.instance];
  IMaterial material = createDynamicObject<IMaterial>(sphere.type.x, sphere.material);
  return shadePixel(material, input);
}

[shader("fragment")]
float4 fragmentMetalRough(ShadedVarying input) : SV_Target0 {
  return shadePixel(Material<MetalRough<float>>(spheres[input.instance].material), input);
}

[shader("fragment")]
float4 fragmentClearcoat(ShadedVarying input) : SV_Target0 {
  return shadePixel(Material<Clearcoat<float>>(spheres[input.instance].material), input);
}

[shader("fragment")]
float4 fragmentSheen(ShadedVarying input) : SV_Target0 {
  return shadePixel(Material<Sheen<float>>(spheres[input.instance].material), input);
}

[shader("fragment")]
float4 fragmentAnisotropic(ShadedVarying input) : SV_Target0 {
  return shadePixel(Material<Anisotropic<float>>(spheres[input.instance].material), input);
}

[shader("fragment")]
float4 fragmentMetalRoughHalf(ShadedVarying input) : SV_Target0 {
  return shadePixel(Material<MetalRough<half>>(spheres[input.instance].material), input);
}

[shader("fragment")]
float4 fragmentClearcoatHalf(ShadedVarying input) : SV_Target0 {
  return shadePixel(Material<Clearcoat<half>>(spheres[input.instance].material), input);
}

[shader("fragment")]
float4 fragmentSheenHalf(ShadedVarying input) : SV_Target0 {
  return shadePixel(Material<Sheen<half>>(spheres[input.instance].material), input);
}

[shader("fragment")]
float4 fragmentAnisotropicHalf(ShadedVarying input) : SV_Target0 {
  return shadePixel(Material<Anisotropic<half>>(spheres[input.instance].material), input);
}

float3 radianceOfType(uint type, MaterialData d, SurfaceSample s, float3 position) {
  switch (type) {
  case 0:
    return radiance<MetalRough<float>>(d, s, position, scene.lighting);
  case 1:
    return radiance<Clearcoat<float>>(d, s, position, scene.lighting);
  case 2:
    return radiance<Sheen<float>>(d, s, position, scene.lighting);
  default:
    return radiance<Anisotropic<float>>(d, s, position, scene.lighting);
  }
}

// The left half of each sphere has the hidden parameters, the right half the fitted ones. Static
// dispatch: the Adreno 829 Vulkan compiler mis-shades createDynamicObject() when the payload is
// chosen per pixel.
[shader("fragment")]
float4 fragmentInverse(ShadedVarying input) : SV_Target0 {
  const SphereInstance sphere = spheres[input.instance];
  const MaterialData d =
      input.world.x > sphere.placement.x ? fitted[input.instance] : sphere.material;
  return float4(toneMap(radianceOfType(sphere.type.x, d, surfaceSample(input), input.world)), 1.0);
}

float3 roughnessSensitivity<M : IMaterialModel>(MaterialData d, SurfaceSample s, float3 position) {
  MaterialData direction;
  direction.base = float4(0.0, 0.0, 0.0, 1.0);
  direction.extra = float4(0.0);
  return fwd_diff(radiance<M>)(diffPair(d, direction), s, position, scene.lighting).d;
}

// How the fitted color changes with roughness: orange where it brightens, blue where it darkens.
[shader("fragment")]
float4 fragmentSensitivity(ShadedVarying input) : SV_Target0 {
  const SurfaceSample s = surfaceSample(input);
  const MaterialData d = fitted[input.instance];
  float3 derivative;
  switch (spheres[input.instance].type.x) {
  case 0:
    derivative = roughnessSensitivity<MetalRough<float>>(d, s, input.world);
    break;
  case 1:
    derivative = roughnessSensitivity<Clearcoat<float>>(d, s, input.world);
    break;
  case 2:
    derivative = roughnessSensitivity<Sheen<float>>(d, s, input.world);
    break;
  default:
    derivative = roughnessSensitivity<Anisotropic<float>>(d, s, input.world);
    break;
  }
  const float v = dot(derivative, float3(0.2126, 0.7152, 0.0722));
  const float magnitude = 1.0 - exp(-2.0 * abs(v));
  return float4(encode(v > 0.0 ? magnitude * float3(1.0, 0.45, 0.0) : magnitude * float3(0.0, 0.45, 1.0)),
                1.0);
}
