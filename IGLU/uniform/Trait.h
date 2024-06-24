/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <igl/Buffer.h>
#include <igl/Uniform.h>

namespace iglu::uniform {

// ----------------------------------------------------------------------------

namespace {

template<typename Test, template<typename...> class Ref>
struct IsSpecialization : std::false_type {};

template<template<typename...> class Ref, typename... Args>
struct IsSpecialization<Ref<Args...>, Ref> : std::true_type {};

} // namespace

// ----------------------------------------------------------------------------

template<typename T>
struct Trait {
  static constexpr igl::UniformType kValue = igl::UniformType::Invalid;
  static constexpr size_t kPadding = 0;
};
template<>
struct Trait<bool> {
  using Aligned = bool;
  static constexpr igl::UniformType kValue = igl::UniformType::Boolean;
  static constexpr size_t kPadding = 0;
};
template<>
struct Trait<int> {
  using Aligned = int;
  static constexpr igl::UniformType kValue = igl::UniformType::Int;
  static constexpr size_t kPadding = 0;
};
template<>
struct Trait<glm::ivec2> {
  using Aligned = glm::ivec2;
  static constexpr igl::UniformType kValue = igl::UniformType::Int2;
  static constexpr size_t kPadding = 0;
};
template<>
struct Trait<glm::ivec3> {
  using Aligned = glm::ivec4;
  static constexpr igl::UniformType kValue = igl::UniformType::Int3;
  static constexpr size_t kPadding = sizeof(Aligned) - sizeof(glm::ivec3);
};
template<>
struct Trait<glm::ivec4> {
  using Aligned = glm::ivec4;
  static constexpr igl::UniformType kValue = igl::UniformType::Int4;
  static constexpr size_t kPadding = 0;
};
template<>
struct Trait<float> {
  using Aligned = float;
  static constexpr igl::UniformType kValue = igl::UniformType::Float;
  static constexpr size_t kPadding = 0;
};
template<>
struct Trait<glm::vec2> {
  using Aligned = glm::vec2;
  static constexpr igl::UniformType kValue = igl::UniformType::Float2;
  static constexpr size_t kPadding = 0;
};
template<>
struct Trait<glm::vec3> {
  using Aligned = glm::vec4;
  static constexpr igl::UniformType kValue = igl::UniformType::Float3;
  static constexpr size_t kPadding = sizeof(Aligned) - sizeof(glm::vec3);
};
template<>
struct Trait<glm::vec4> {
  using Aligned = glm::vec4;
  static constexpr igl::UniformType kValue = igl::UniformType::Float4;
  static constexpr size_t kPadding = 0;
};
template<>
struct Trait<glm::mat2> {
  using Aligned = glm::mat2;
  static constexpr igl::UniformType kValue = igl::UniformType::Mat2x2;
  static constexpr size_t kPadding = 0;
};
template<>
struct Trait<glm::mat3> {
  using Aligned = std::array<glm::vec4, 3>; // each row of matrix is 16-byte aligned
  static_assert(sizeof(Aligned) == 3 * sizeof(glm::vec4), "Aligned is the wrong size!");
  static void toAligned(Aligned& outData, const glm::mat3& src) noexcept {
    const auto* srcMatrix = static_cast<const float*>(glm::value_ptr(src));
    for (int i = 0; i < 3; i++) {
      auto* outRow = static_cast<float*>(glm::value_ptr(outData[i]));
      for (int j = 0; j < 3; j++) {
        *outRow++ = *srcMatrix++;
      }
    }
  }

  static constexpr igl::UniformType kValue = igl::UniformType::Mat3x3;
  static constexpr size_t kPadding = sizeof(Aligned) - sizeof(glm::mat3);
};
template<>
struct Trait<glm::mat4> {
  using Aligned = glm::mat4;
  static constexpr igl::UniformType kValue = igl::UniformType::Mat4x4;
  static constexpr size_t kPadding = 0;
};

} // namespace iglu::uniform
