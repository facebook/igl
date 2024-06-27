/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cassert>

// This would use IGL defines, but we can avoid importing all of IGL by doing this
#if defined(__APPLE__)
#include <simd/simd.h>
#endif // defined(__APPLE__)

/// Polyfill to make it so we don't have to have ifdefs in code

namespace iglu::simdtypes {

// Use Apple-provided simd if available.
#if defined(__APPLE__)

using float4x4 = simd::float4x4;
using float3x3 = simd::float3x3;
using float3x4 = simd::float3x4;
using float2x2 = simd::float2x2;
using float4 = simd::float4;
using float3 = simd::float3;
using float2 = simd::float2;
using float1 = simd::float1;
using int1 = simd::int1;
using int2 = simd::int2;
using int3 = simd::int3;
using int4 = simd::int4;
using bool1 = simd_bool;

#else

// Apple simd is not available, make use custom.
#if defined(__clang__)

using float4 = float __attribute__((ext_vector_type(4)));

// simd/vector_types.h
// Vectors of this type are padded to have the same size and alignment as simd_float4.
using float3 = float __attribute__((ext_vector_type(4)));

using float2 = float __attribute__((ext_vector_type(2)));
using float1 = float;
using int1 = int;
using int2 = int __attribute__((ext_vector_type(2)));

// Vectors of this type are padded to have the same size and alignment as simd_float4.
using int3 = int __attribute__((ext_vector_type(4)));

using int4 = int __attribute__((ext_vector_type(4)));
using bool1 = bool;

#elif defined(__GNUC__)

using float4 = float __attribute__((vector_size(sizeof(float) * 4)));

// simd/vector_types.h
// Vectors of this type are padded to have the same size and alignment as simd_float4.
using float3 = float __attribute__((vector_size(sizeof(float) * 4)));

using float2 = float __attribute__((vector_size(sizeof(float) * 2)));
using float1 = float;
using int1 = int;
using int2 = int __attribute__((vector_size(sizeof(float) * 2)));

// Vectors of this type are padded to have the same size and alignment as simd_float4.
using int3 = int __attribute__((vector_size(sizeof(int) * 4)));

using int4 = int __attribute__((vector_size(sizeof(float) * 4)));
using bool1 = bool;

#else

// Not available clang or GCC vector extensions - create custom structs

struct float4 final {
  float x;
  float y;
  float z;
  float w;

  float operator[](const unsigned index) const {
    assert(index < 4);
    return (&x)[index];
  }

  float& operator[](const unsigned index) {
    assert(index < 4);
    return (&x)[index];
  }
};
static_assert(sizeof(float4) == 4 * sizeof(float));

// simd/vector_types.h
// Vectors of this type are padded to have the same size and alignment as simd_float4.
using float3 = float4;

struct float2 final {
  float x;
  float y;

  float operator[](const unsigned index) const {
    assert(index < 2);
    return (&x)[index];
  }

  float& operator[](const unsigned index) {
    assert(index < 2);
    return (&x)[index];
  }
};
static_assert(sizeof(float2) == 2 * sizeof(float));

using float1 = float;

struct int4 final {
  int x;
  int y;
  int z;
  int w;

  int operator[](const unsigned index) const {
    assert(index < 4);
    return (&x)[index];
  }

  int& operator[](const unsigned index) {
    assert(index < 4);
    return (&x)[index];
  }
};
static_assert(sizeof(int4) == 4 * sizeof(int));

// Vectors of this type are padded to have the same size and alignment as simd_int4.
using int3 = int4;

struct int2 final {
  int x;
  int y;

  int operator[](const unsigned index) const {
    assert(index < 2);
    return (&x)[index];
  }

  int& operator[](const unsigned index) {
    assert(index < 2);
    return (&x)[index];
  }
};
static_assert(sizeof(int2) == 2 * sizeof(int));

using int1 = int;

using bool1 = bool;

#endif

struct float4x4 {
  float4 columns[4];
  float4x4() = default;
  explicit float4x4(float val) {
    columns[0] = float4{val, 0.0, 0.0, 0.0};
    columns[1] = float4{0.0, val, 0.0, 0.0};
    columns[2] = float4{0.0, 0.0, val, 0.0};
    columns[3] = float4{0.0, 0.0, 0.0, val};
  }
  explicit float4x4(float4 diag) {
    columns[0] = float4{diag[0], 0.0, 0.0, 0.0};
    columns[1] = float4{0.0, diag[1], 0.0, 0.0};
    columns[2] = float4{0.0, 0.0, diag[2], 0.0};
    columns[3] = float4{0.0, 0.0, 0.0, diag[3]};
  }
  float4x4(float4 c0, float4 c1, float4 c2, float4 c3) {
    columns[0] = c0;
    columns[1] = c1;
    columns[2] = c2;
    columns[3] = c3;
  }
  // TODO BE remove this float*
  explicit float4x4(const float* vals) {
    columns[0] = float4{vals[0], vals[1], vals[2], vals[3]};
    columns[1] = float4{vals[4], vals[5], vals[6], vals[7]};
    columns[2] = float4{vals[8], vals[9], vals[10], vals[11]};
    columns[3] = float4{vals[12], vals[13], vals[14], vals[15]};
  }
};

struct float3x4 {
  float4 columns[3];
  float3x4() = default;
  explicit float3x4(float val) {
    columns[0] = float4{val, 0.0, 0.0, 0.0};
    columns[1] = float4{0.0, val, 0.0, 0.0};
    columns[2] = float4{0.0, 0.0, val, 0.0};
  }
  explicit float3x4(float3 diag) {
    columns[0] = float4{diag[0], 0.0, 0.0, 0.0};
    columns[1] = float4{0.0, diag[1], 0.0, 0.0};
    columns[2] = float4{0.0, 0.0, diag[2], 0.0};
  }
  float3x4(float4 c0, float4 c1, float4 c2) {
    columns[0] = c0;
    columns[1] = c1;
    columns[2] = c2;
  }
  // TODO BE remove this float*
  explicit float3x4(const float* vals) {
    columns[0] = float4{vals[0], vals[1], vals[2], vals[3]};
    columns[1] = float4{vals[4], vals[5], vals[6], vals[7]};
    columns[2] = float4{vals[8], vals[9], vals[10], vals[11]};
  }
};

struct float3x3 {
  float3 columns[3];
  float3x3() = default;
  explicit float3x3(float val) {
    columns[0] = float3{val, 0.0, 0.0};
    columns[1] = float3{0.0, val, 0.0};
    columns[2] = float3{0.0, 0.0, val};
  }
  explicit float3x3(float3 diag) {
    columns[0] = float3{diag[0], 0.0, 0.0};
    columns[1] = float3{0.0, diag[1], 0.0};
    columns[2] = float3{0.0, 0.0, diag[2]};
  }
  float3x3(float3 c0, float3 c1, float3 c2) {
    columns[0] = c0;
    columns[1] = c1;
    columns[2] = c2;
  }
};

struct float2x2 {
  float2 columns[2];
  float2x2() = default;
  explicit float2x2(float val) {
    columns[0] = float2{val, 0.0};
    columns[1] = float2{0.0, val};
  }
  explicit float2x2(float2 diag) {
    columns[0] = float2{diag[0], 0.0};
    columns[1] = float2{0.0, diag[1]};
  }
  float2x2(float2 c0, float2 c1) {
    columns[0] = c0;
    columns[1] = c1;
  }
};

#endif // defined(__APPLE__)

} // namespace iglu::simdtypes
