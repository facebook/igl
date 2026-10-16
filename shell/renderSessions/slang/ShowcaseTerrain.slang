/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Chapter 4 of SlangShowcaseSession: shades the terrain for both geometry paths, and draws it
// without mesh shaders as one instance per meshlet, evaluating the heightfield for every corner
// of every triangle.
#include "igl_abi.slangh"
import ShowcaseMaterials;
import ShowcaseTerrainCommon;

IGL_UNIFORM_BUFFER(TerrainUniforms, terrain, 1);

[shader("vertex")]
TerrainVertex terrainVertexFallback(uint vertexId: IGL_SV_VERTEX_ID, uint instance: IGL_SV_INSTANCE_ID) {
  const uint3 corners = meshletTriangle(vertexId / 3);
  const uint corner = vertexId % 3;
  return terrainVertex(terrain, instance, corner == 0 ? corners.x : (corner == 1 ? corners.y : corners.z));
}

[shader("fragment")]
float4 terrainFragment(TerrainVertex input) : SV_Target0 {
  SurfaceSample s;
  s.normal = normalize(input.normal);
  s.tangent = normalize(cross(float3(0.0, 0.0, 1.0), s.normal));
  s.bitangent = cross(s.normal, s.tangent);
  s.view = normalize(terrain.camera.xyz - input.world);
  // Grass on gentle slopes, rock on steep ones, snow up high.
  const float steep = smoothstep(0.75, 0.55, s.normal.y);
  const float snow = smoothstep(0.72, 0.85, input.world.y / kTerrainHeight) * (1.0 - steep);
  MaterialData d;
  d.base = float4(lerp(lerp(float3(0.12, 0.22, 0.05), float3(0.3, 0.26, 0.22), steep), float3(0.8), snow),
                  lerp(0.8, 0.45, snow));
  d.extra = float4(0.0);
  float3 color = radiance<MetalRough<float>>(d, s, input.world, terrain.lighting);
  if (terrain.display.y > 0.5) {
    const uint h = hash(uint2(input.meshlet, 7u));
    color = float3(h & 0xffu, (h >> 8) & 0xffu, (h >> 16) & 0xffu) / 255.0 * (0.25 + 0.75 * s.normal.y);
  }
  const float fog = 1.0 - exp(-terrain.display.z * length(terrain.camera.xyz - input.world));
  color = lerp(color, terrain.lighting.sky.rgb, fog);
  color = saturate(color * (2.51 * color + 0.03) / (color * (2.43 * color + 0.59) + 0.14));
  return float4(terrain.display.x > 0.5 ? pow(color, 1.0 / 2.2) : color, 1.0);
}
