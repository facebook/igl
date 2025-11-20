/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace simd {

struct Float1 {
  float x;
};

struct Float2 {
  float x, y;
};

struct Float3 {
  float x, y, z;
};

struct Float4 {
  float x, y, z, w;
};

struct Int1 {
  int x;
};

struct Int2 {
  int x, y;
};

struct Int3 {
  int x, y, z;
};

struct Int4 {
  int x, y, z, w;
};

struct Float2x2 {
  Float2 columns[2];
};

struct Float3x2 {
  Float2 columns[3];
};

struct Float3x3 {
  Float3 columns[3];
};

struct Float4x4 {
  Float4 columns[4];
};

// Type aliases for backward compatibility with Apple SIMD API
using float1 = Float1;
using float2 = Float2;
using float3 = Float3;
using float4 = Float4;
using int1 = Int1;
using int2 = Int2;
using int3 = Int3;
using int4 = Int4;
using float2x2 = Float2x2;
using float3x2 = Float3x2;
using float3x3 = Float3x3;
using float4x4 = Float4x4;

} // namespace simd
