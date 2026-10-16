/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// The procedural terrain of chapter 4 of SlangShowcaseSession: 5329 meshlets of 8x8 vertices and 98
// triangles, generated from a heightfield with no vertex or index buffers.
module ShowcaseTerrainCommon;

import ShowcaseMaterials;

public static const uint kMeshletsPerRow = 73;
public static const uint kMeshlets = kMeshletsPerRow * kMeshletsPerRow;
public static const uint kMeshletQuads = 7;
public static const uint kMeshletVertices = 64;
public static const uint kMeshletTriangles = 98;
public static const float kTerrainSize = 96.0;
public static const float kTerrainHeight = 14.0;
static const float kQuadSize = kTerrainSize / float(kMeshletsPerRow * kMeshletQuads);

public struct TerrainUniforms {
  public float4x4 viewProjection;
  public float4 camera; // position
  public float4 cullPlanes[6]; // inside where dot(plane.xyz, p) + plane.w >= 0
  public Lighting lighting;
  public float4 display; // x: 1 to gamma-encode for a UNorm surface, y: 1 to color by meshlet,
                         // z: fog density
};

public uint hash(uint2 p) {
  uint h = p.x * 0x8da6b343u ^ p.y * 0xd8163841u;
  h ^= h >> 16;
  h *= 0x7feb352du;
  h ^= h >> 15;
  return h;
}

float valueNoise(float2 p) {
  const float2 cell = floor(p);
  const float2 f = p - cell;
  const float2 u = f * f * (3.0 - 2.0 * f);
  const uint2 i = uint2(int2(cell) + 4096);
  const float a = float(hash(i) & 0xffffu);
  const float b = float(hash(i + uint2(1, 0)) & 0xffffu);
  const float c = float(hash(i + uint2(0, 1)) & 0xffffu);
  const float d = float(hash(i + uint2(1, 1)) & 0xffffu);
  return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y) / 65535.0;
}

// Fractal noise, squared for broad valleys and sharp peaks, in [0, kTerrainHeight]: the culling
// bounds rely on that range.
float terrainHeight(float2 xz) {
  float2 p = xz * 0.035;
  float sum = 0.0;
  float amplitude = 0.5;
  [ForceUnroll]
  for (int octave = 0; octave < 5; ++octave) {
    sum += amplitude * valueNoise(p);
    p = p * 2.03 + float2(17.3, 5.1);
    amplitude *= 0.5;
  }
  return saturate(1.25 * sum * sum) * kTerrainHeight;
}

float2 meshletCorner(uint meshlet) {
  return float2(meshlet % kMeshletsPerRow, meshlet / kMeshletsPerRow) * float(kMeshletQuads) * kQuadSize -
         0.5 * kTerrainSize;
}

public struct TerrainVertex {
  public float4 position : SV_Position;
  [[vk::location(0)]] public float3 world : TEXCOORD0;
  [[vk::location(1)]] public float3 normal : NORMAL;
  [[vk::location(2)]] public nointerpolation uint meshlet : TEXCOORD1;
};

public TerrainVertex terrainVertex(TerrainUniforms terrain, uint meshlet, uint index) {
  const float2 xz = meshletCorner(meshlet) + float2(index % 8, index / 8) * kQuadSize;
  const float e = 0.5 * kQuadSize;
  TerrainVertex v;
  v.world = float3(xz.x, terrainHeight(xz), xz.y);
  v.normal = normalize(float3(terrainHeight(xz - float2(e, 0.0)) - terrainHeight(xz + float2(e, 0.0)),
                              2.0 * e,
                              terrainHeight(xz - float2(0.0, e)) - terrainHeight(xz + float2(0.0, e))));
  v.position = mul(float4(v.world, 1.0), terrain.viewProjection);
  v.meshlet = meshlet;
  return v;
}

// Two triangles per quad of the 7x7-quad patch.
public uint3 meshletTriangle(uint triangle) {
  const uint quad = triangle / 2;
  const uint a = (quad / kMeshletQuads) * 8 + quad % kMeshletQuads;
  return (triangle & 1u) == 0u ? uint3(a, a + 8, a + 1) : uint3(a + 1, a + 8, a + 9);
}

public bool meshletVisible(TerrainUniforms terrain, uint meshlet) {
  const float2 corner = meshletCorner(meshlet);
  const float3 lo = float3(corner.x, 0.0, corner.y);
  const float3 hi = lo + float3(kMeshletQuads * kQuadSize, kTerrainHeight, kMeshletQuads * kQuadSize);
  [ForceUnroll]
  for (int i = 0; i < 6; ++i) {
    const float4 plane = terrain.cullPlanes[i];
    const float3 farthest = float3(plane.x >= 0.0 ? hi.x : lo.x,
                                   plane.y >= 0.0 ? hi.y : lo.y,
                                   plane.z >= 0.0 ? hi.z : lo.z);
    if (dot(plane.xyz, farthest) + plane.w < 0.0) {
      return false;
    }
  }
  return true;
}
