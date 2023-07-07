/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace simd {

struct float1 {
  float x;
};

struct float2 {
  float x, y;
};

struct float3 {
  float x, y, z;
};

struct float4 {
  float x, y, z, w;
};

struct int1 {
  int x;
};

struct int2 {
  int x, y;
};

struct int3 {
  int x, y, z;
};

struct int4 {
  int x, y, z, w;
};

struct float2x2 {
  float2 columns[2];
};

struct float3x2 {
  float2 columns[3];
};

struct float3x3 {
  float3 columns[3];
};

struct float4x4 {
  float4 columns[4];
};

} // namespace simd
