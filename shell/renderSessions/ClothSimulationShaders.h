/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Macros.h>

// Inline GLSL shader sources for ClothSimulationSession on Vulkan. Buffers must
// declare `layout(set = 1, binding = N)` because IGL Vulkan requires UBOs and
// SSBOs to live in descriptor set `kBindPoint_Buffers (=1)`.
//
// Local sizes are kept at 1x1x1 so the same `dispatchThreadGroups((kN, kN, 1),
// (1, 1, 1))` call works on every backend (Metal computes thread_position_in_grid
// from groupCount * threadsPerGroup with no baked-in local size).
//
// All struct field layouts MUST stay in sync with the CPU-side `UBO` and
// `ClothVertex` in ClothSimulationSession.cpp.
namespace igl::shell::cloth_shaders {

// ---------------------------------------------------------------------------
// Cloth render shader (vertex + fragment)
// ---------------------------------------------------------------------------
inline constexpr const char* kVulkanClothVS = R"(#version 460
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;

layout(set = 1, binding = 1, std140) uniform UBO {
  float ballRadius;
  float fov;
  float aspectRatio;
  int N;
  float cellSize;
  float gravity;
  float stiffness;
  float damping;
  float dt;
  vec3 eye;
  vec3 ballCenter;
  vec3 lightPos;
  mat4 view;
  mat4 projection;
  mat4 viewProjection;
} ubo;

void main() {
  gl_Position = ubo.viewProjection * vec4(position, 1.0);
  fragPos = position;
  fragNormal = normal;
}
)";

inline constexpr const char* kVulkanClothFS = R"(#version 460
layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 1, std140) uniform UBO {
  float ballRadius;
  float fov;
  float aspectRatio;
  int N;
  float cellSize;
  float gravity;
  float stiffness;
  float damping;
  float dt;
  vec3 eye;
  vec3 ballCenter;
  vec3 lightPos;
  mat4 view;
  mat4 projection;
  mat4 viewProjection;
} ubo;

void main() {
  vec3 n = normalize(fragNormal);
  vec3 fragToLight = normalize(ubo.lightPos - fragPos);
  float c = abs(dot(n, fragToLight));
  outColor = vec4(c, c, c, 1.0);
}
)";

// ---------------------------------------------------------------------------
// Obstacle (sphere imposter) render shader (vertex + fragment)
// ---------------------------------------------------------------------------
inline constexpr const char* kVulkanObstacleVS = R"(#version 460
layout(location = 0) in vec2 position;

layout(location = 0) out vec2 pointCoord;
layout(location = 1) out vec3 centerPosCameraSpace;

layout(set = 1, binding = 1, std140) uniform UBO {
  float ballRadius;
  float fov;
  float aspectRatio;
  int N;
  float cellSize;
  float gravity;
  float stiffness;
  float damping;
  float dt;
  vec3 eye;
  vec3 ballCenter;
  vec3 lightPos;
  mat4 view;
  mat4 projection;
  mat4 viewProjection;
} ubo;

void main() {
  float tanHalfFov = tan(ubo.fov / 2.0);
  float dist = length(ubo.ballCenter - ubo.eye);
  float screenRadius = ubo.ballRadius / (tanHalfFov * dist);
  vec4 clipPos = ubo.viewProjection * vec4(ubo.ballCenter, 1.0);
  clipPos.y += screenRadius * position.y * clipPos.w;
  clipPos.x += (screenRadius * position.x * clipPos.w) / ubo.aspectRatio;
  gl_Position = clipPos;
  pointCoord = position;
  centerPosCameraSpace = (ubo.view * vec4(ubo.ballCenter, 1.0)).xyz;
}
)";

inline constexpr const char* kVulkanObstacleFS = R"(#version 460
layout(location = 0) in vec2 pointCoord;
layout(location = 1) in vec3 centerPosCameraSpace;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 1, std140) uniform UBO {
  float ballRadius;
  float fov;
  float aspectRatio;
  int N;
  float cellSize;
  float gravity;
  float stiffness;
  float damping;
  float dt;
  vec3 eye;
  vec3 ballCenter;
  vec3 lightPos;
  mat4 view;
  mat4 projection;
  mat4 viewProjection;
} ubo;

void main() {
  float dist = length(pointCoord);
  if (dist > 1.0) {
    discard;
  }
  float zInSphere = sqrt(1.0 - dist * dist);
  vec3 normalCameraSpace = vec3(pointCoord, zInSphere);
  vec3 fragPosCameraSpace = centerPosCameraSpace + normalCameraSpace * ubo.ballRadius * 0.99;
  vec4 clipPos = ubo.projection * vec4(fragPosCameraSpace, 1.0);
  gl_FragDepth = clipPos.z / clipPos.w;
  vec3 lightPosCameraSpace = ubo.lightPos - ubo.eye;
  vec3 lightDir = normalize(lightPosCameraSpace - fragPosCameraSpace);
  outColor = vec4(dot(lightDir, normalCameraSpace), 0.0, 0.0, 1.0);
}
)";

// ---------------------------------------------------------------------------
// Compute shader: integrate velocity (gravity + spring forces from neighbours)
// ---------------------------------------------------------------------------
inline constexpr const char* kVulkanUpdateVelocityCS = R"(#version 460
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct Vertex {
  vec3 position;
  vec3 velocity;
  vec3 normal;
};

layout(set = 1, binding = 1, std140) uniform UBO {
  float ballRadius;
  float fov;
  float aspectRatio;
  int N;
  float cellSize;
  float gravity;
  float stiffness;
  float damping;
  float dt;
  vec3 eye;
  vec3 ballCenter;
  vec3 lightPos;
  mat4 view;
  mat4 projection;
  mat4 viewProjection;
} ubo;

layout(set = 1, binding = 0, std140) buffer VertexSSBO {
  Vertex vertices[];
} ssbo;

void main() {
  int x = int(gl_GlobalInvocationID.x);
  int y = int(gl_GlobalInvocationID.y);
  int vertexId = x * 128 + y;

  ssbo.vertices[vertexId].velocity.y -= ubo.gravity * ubo.dt;

  ivec2 links[8] = ivec2[8](ivec2(0,1), ivec2(0,-1), ivec2(1,0), ivec2(-1,0),
                            ivec2(1,1), ivec2(1,-1), ivec2(-1,1), ivec2(-1,-1));
  vec3 force = vec3(0.0);
  for (int i = 0; i < 8; ++i) {
    ivec2 link = links[i];
    int linkedX = clamp(x + link.x, 0, 127);
    int linkedY = clamp(y + link.y, 0, 127);
    int linkedVertex = linkedX * 128 + linkedY;
    vec3 relativePos = ssbo.vertices[linkedVertex].position - ssbo.vertices[vertexId].position;
    float currentLength = length(relativePos);
    float originalLength = ubo.cellSize * length(vec2(float(linkedX - x), float(linkedY - y)));
    if (originalLength != 0.0) {
      force += (ubo.stiffness * normalize(relativePos) * (currentLength - originalLength)) / originalLength;
    }
  }
  ssbo.vertices[vertexId].velocity += force * ubo.dt;
}
)";

// ---------------------------------------------------------------------------
// Compute shader: integrate position, apply damping and ball collision
// ---------------------------------------------------------------------------
inline constexpr const char* kVulkanUpdatePositionCS = R"(#version 460
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct Vertex {
  vec3 position;
  vec3 velocity;
  vec3 normal;
};

layout(set = 1, binding = 1, std140) uniform UBO {
  float ballRadius;
  float fov;
  float aspectRatio;
  int N;
  float cellSize;
  float gravity;
  float stiffness;
  float damping;
  float dt;
  vec3 eye;
  vec3 ballCenter;
  vec3 lightPos;
  mat4 view;
  mat4 projection;
  mat4 viewProjection;
} ubo;

layout(set = 1, binding = 0, std140) buffer VertexSSBO {
  Vertex vertices[];
} ssbo;

void main() {
  int x = int(gl_GlobalInvocationID.x);
  int y = int(gl_GlobalInvocationID.y);
  int vertexId = x * 128 + y;

  ssbo.vertices[vertexId].velocity *= exp(-ubo.damping * ubo.dt);
  if (length(ssbo.vertices[vertexId].position - ubo.ballCenter) <= ubo.ballRadius) {
    ssbo.vertices[vertexId].velocity = vec3(0.0);
  }
  ssbo.vertices[vertexId].position += ssbo.vertices[vertexId].velocity * ubo.dt;
}
)";

// ---------------------------------------------------------------------------
// Compute shader: recompute per-vertex normals by averaging incident triangles
// ---------------------------------------------------------------------------
inline constexpr const char* kVulkanUpdateNormalCS = R"(#version 460
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct Vertex {
  vec3 position;
  vec3 velocity;
  vec3 normal;
};

layout(set = 1, binding = 1, std140) uniform UBO {
  float ballRadius;
  float fov;
  float aspectRatio;
  int N;
  float cellSize;
  float gravity;
  float stiffness;
  float damping;
  float dt;
  vec3 eye;
  vec3 ballCenter;
  vec3 lightPos;
  mat4 view;
  mat4 projection;
  mat4 viewProjection;
} ubo;

layout(set = 1, binding = 0, std140) buffer VertexSSBO {
  Vertex vertices[];
} ssbo;

vec3 getPos(int x, int y) {
  return ssbo.vertices[x * 128 + y].position;
}

vec3 computeNormal(vec3 a, vec3 b, vec3 c) {
  return normalize(cross(a - b, a - c));
}

void main() {
  int x = int(gl_GlobalInvocationID.x);
  int y = int(gl_GlobalInvocationID.y);

  vec3 normal = vec3(0.0);
  int count = 0;

  if (x < 127 && y < 127) {
    normal += computeNormal(getPos(x, y), getPos(x + 1, y), getPos(x, y + 1));
    count += 1;
  }
  if (x > 0 && y < 127) {
    normal += computeNormal(getPos(x, y), getPos(x, y + 1), getPos(x - 1, y));
    count += 1;
  }
  if (x > 0 && y > 0) {
    normal += computeNormal(getPos(x, y), getPos(x - 1, y), getPos(x, y - 1));
    count += 1;
  }
  if (x < 127 && y > 0) {
    normal += computeNormal(getPos(x, y), getPos(x, y - 1), getPos(x + 1, y));
    count += 1;
  }
  normal = normal / float(count);
  ssbo.vertices[x * 128 + y].normal = normal;
}
)";

} // namespace igl::shell::cloth_shaders
