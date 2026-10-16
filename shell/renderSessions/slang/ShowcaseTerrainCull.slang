/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// Task and mesh shaders for chapter 4 of SlangShowcaseSession. A task group tests 32 meshlets
// against the culling frustum and passes the survivors to mesh groups, which evaluate the
// heightfield once per vertex. They are a program of their own because Slang's Metal output
// declares groupshared variables in every entry point, which vertex and fragment functions reject.
#include "igl_abi.slangh"
import ShowcaseTerrainCommon;

static const uint kTaskGroupSize = 32;

IGL_UNIFORM_BUFFER(TerrainUniforms, terrain, 1);

struct MeshPayload {
  uint meshlets[kTaskGroupSize];
};

groupshared MeshPayload payload;
groupshared uint visibleCount;

// Slang has no wave operations in Metal object shaders, so survivors are compacted with a
// threadgroup atomic.
[shader("amplification")]
[numthreads(kTaskGroupSize, 1, 1)]
void terrainTask(uint thread: SV_GroupThreadID, uint3 group: SV_GroupID) {
  if (thread == 0u) {
    visibleCount = 0u;
  }
  GroupMemoryBarrierWithGroupSync();
  const uint meshlet = group.x * kTaskGroupSize + thread;
  if (meshlet < kMeshlets && meshletVisible(terrain, meshlet)) {
    uint slot = 0u;
    InterlockedAdd(visibleCount, 1u, slot);
    payload.meshlets[slot] = meshlet;
  }
  GroupMemoryBarrierWithGroupSync();
  DispatchMesh(visibleCount, 1, 1, payload);
}

[shader("mesh")]
[numthreads(kMeshletVertices, 1, 1)]
[outputtopology("triangle")]
void terrainMesh(uint thread: SV_GroupThreadID,
                 uint3 group: SV_GroupID,
                 in payload MeshPayload incoming,
                 OutputVertices<TerrainVertex, kMeshletVertices> vertices,
                 OutputIndices<uint3, kMeshletTriangles> triangles) {
  SetMeshOutputCounts(kMeshletVertices, kMeshletTriangles);
  const uint meshlet = incoming.meshlets[group.x];
  vertices[thread] = terrainVertex(terrain, meshlet, thread);
  for (uint t = thread; t < kMeshletTriangles; t += kMeshletVertices) {
    triangles[t] = meshletTriangle(t);
  }
}
