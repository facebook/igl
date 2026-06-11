/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IGLU/uniform/Trait.h>
#include <glm/glm.hpp>

namespace iglu::tests {

using igl::UniformType;
using uniform::Trait;

// ----------------------------------------------------------------------------
// kValue: every supported C++ type maps to the expected UniformType.
static_assert(Trait<bool>::kValue == UniformType::Boolean);
static_assert(Trait<int>::kValue == UniformType::Int);
static_assert(Trait<glm::ivec2>::kValue == UniformType::Int2);
static_assert(Trait<glm::ivec3>::kValue == UniformType::Int3);
static_assert(Trait<glm::ivec4>::kValue == UniformType::Int4);
static_assert(Trait<float>::kValue == UniformType::Float);
static_assert(Trait<glm::vec2>::kValue == UniformType::Float2);
static_assert(Trait<glm::vec3>::kValue == UniformType::Float3);
static_assert(Trait<glm::vec4>::kValue == UniformType::Float4);
static_assert(Trait<glm::mat2>::kValue == UniformType::Mat2x2);
static_assert(Trait<glm::mat3>::kValue == UniformType::Mat3x3);
static_assert(Trait<glm::mat4>::kValue == UniformType::Mat4x4);

// Unspecialized types fall back to the primary template's Invalid default.
static_assert(Trait<double>::kValue == UniformType::Invalid);

// ----------------------------------------------------------------------------
// kPadding: packed types need none; 3-component types pad up to 16-byte rows.
static_assert(Trait<bool>::kPadding == 0);
static_assert(Trait<int>::kPadding == 0);
static_assert(Trait<glm::ivec2>::kPadding == 0);
static_assert(Trait<glm::ivec4>::kPadding == 0);
static_assert(Trait<float>::kPadding == 0);
static_assert(Trait<glm::vec2>::kPadding == 0);
static_assert(Trait<glm::vec4>::kPadding == 0);
static_assert(Trait<glm::mat2>::kPadding == 0);
static_assert(Trait<glm::mat4>::kPadding == 0);
static_assert(Trait<glm::ivec3>::kPadding == sizeof(glm::ivec4) - sizeof(glm::ivec3));
static_assert(Trait<glm::vec3>::kPadding == sizeof(glm::vec4) - sizeof(glm::vec3));
static_assert(Trait<glm::mat3>::kPadding == 3 * sizeof(glm::vec4) - sizeof(glm::mat3));

// ----------------------------------------------------------------------------
// Aligned: 3-component types widen to a 16-byte-aligned representation.
static_assert(sizeof(Trait<glm::ivec3>::Aligned) == sizeof(glm::ivec4));
static_assert(sizeof(Trait<glm::vec3>::Aligned) == sizeof(glm::vec4));
static_assert(sizeof(Trait<glm::mat3>::Aligned) == 3 * sizeof(glm::vec4));

// ----------------------------------------------------------------------------
// Trait<glm::mat3>::toAligned() copies each column into the first three floats of
// a 16-byte-aligned vec4 row, leaving the fourth (padding) float untouched.
TEST(UniformTraitTest, Mat3ToAligned) {
  // glm is column-major: this lays out columns (1,2,3), (4,5,6), (7,8,9).
  const glm::mat3 src(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f);

  Trait<glm::mat3>::Aligned aligned{}; // zero-init so the padding float stays 0
  Trait<glm::mat3>::toAligned(aligned, src);

  for (int row = 0; row < 3; ++row) {
    EXPECT_FLOAT_EQ(aligned[row].x, static_cast<float>(row * 3 + 1));
    EXPECT_FLOAT_EQ(aligned[row].y, static_cast<float>(row * 3 + 2));
    EXPECT_FLOAT_EQ(aligned[row].z, static_cast<float>(row * 3 + 3));
    EXPECT_FLOAT_EQ(aligned[row].w, 0.0f); // padding left untouched
  }
}

// toAligned() copies exactly three floats per row and must not write the fourth
// (padding) float. Seeding the padding slot with a sentinel proves the copy
// leaves it untouched rather than merely happening to keep a zero from init.
TEST(UniformTraitTest, Mat3ToAlignedPreservesPadding) {
  const glm::mat3 src(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f);

  Trait<glm::mat3>::Aligned aligned{};
  for (auto& row : aligned) {
    row.w = -1.0f; // sentinel in the padding slot
  }
  Trait<glm::mat3>::toAligned(aligned, src);

  for (int row = 0; row < 3; ++row) {
    EXPECT_FLOAT_EQ(aligned[row].x, static_cast<float>(row * 3 + 1));
    EXPECT_FLOAT_EQ(aligned[row].y, static_cast<float>(row * 3 + 2));
    EXPECT_FLOAT_EQ(aligned[row].z, static_cast<float>(row * 3 + 3));
    EXPECT_FLOAT_EQ(aligned[row].w, -1.0f); // padding preserved by toAligned()
  }
}

} // namespace iglu::tests
