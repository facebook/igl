/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../data/ShaderData.h"
#include "../util/Common.h"
#include "../util/TestDevice.h"
#include "UniformTests.h"

#include <IGLU/uniform/Collection.h>
#include <IGLU/uniform/Descriptor.h>
#include <gtest/gtest.h>
#include <string>

namespace iglu::tests {

//
// UniformDescriptorTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class UniformDescriptorTest : public ::testing::Test {
 private:
 public:
  UniformDescriptorTest() = default;
  ~UniformDescriptorTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
  }

  void TearDown() override {}
};

// ----------------------------------------------------------------------------

//
// Value Test
//
TEST_F(UniformDescriptorTest, DescriptorValue) {
  const bool boolValue = true;
  uniform::DescriptorValue<bool> boolUniform(boolValue);
  TestIndex(boolUniform);
  TestUniformData(boolValue, boolUniform);

  const int intValue = 1;
  uniform::DescriptorValue<int> intUniform(intValue);
  TestIndex(intUniform);
  TestUniformData(intValue, intUniform);

  const glm::ivec2 ivec2Value(2);
  uniform::DescriptorValue<glm::ivec2> ivec2Uniform(ivec2Value);
  TestIndex(ivec2Uniform);
  TestUniformData(ivec2Value, ivec2Uniform);

  const glm::ivec3 ivec3Value(3);
  uniform::DescriptorValue<glm::ivec3> ivec3Uniform(ivec3Value);
  TestIndex(ivec3Uniform);
  TestUniformData(ivec3Value, ivec3Uniform);

  const glm::ivec4 ivec4Value(4);
  uniform::DescriptorValue<glm::ivec4> ivec4Uniform(ivec4Value);
  TestIndex(ivec4Uniform);
  TestUniformData(ivec4Value, ivec4Uniform);

  const float floatValue(1.f);
  uniform::DescriptorValue<float> floatUniform(floatValue);
  TestIndex(floatUniform);
  TestUniformData(floatValue, floatUniform);

  const glm::vec2 vec2Value(2.f);
  uniform::DescriptorValue<glm::vec2> vec2Uniform(vec2Value);
  TestIndex(vec2Uniform);
  TestUniformData(vec2Value, vec2Uniform);

  const glm::vec3 vec3Value(3.f);
  uniform::DescriptorValue<glm::vec3> vec3Uniform(vec3Value);
  TestIndex(vec3Uniform);
  TestUniformData(vec3Value, vec3Uniform);

  const glm::vec4 vec4Value(4.f);
  uniform::DescriptorValue<glm::vec4> vec4Uniform(vec4Value);
  TestIndex(vec4Uniform);
  TestUniformData(vec4Value, vec4Uniform);

  const glm::mat2 mat2Value(2.f);
  uniform::DescriptorValue<glm::mat2> mat2Uniform(mat2Value);
  TestIndex(mat2Uniform);
  TestUniformData(mat2Value, mat2Uniform);

  const glm::mat3 mat3Value(3.f);
  uniform::DescriptorValue<glm::mat3> mat3Uniform(mat3Value);
  TestIndex(mat3Uniform);
  TestUniformData(mat3Value, mat3Uniform);

  const glm::mat4 mat4Value(4.f);
  uniform::DescriptorValue<glm::mat4> mat4Uniform(mat4Value);
  TestIndex(mat4Uniform);
  TestUniformData(mat4Value, mat4Uniform);
}

//
// Vector Test
//
TEST_F(UniformDescriptorTest, DescriptorVector) {
  // NOTE: std::vector<bool> does not have the data() method
  // std::vector<bool> boolVector = {true, false};
  // uniform::DescriptorVector<bool> boolUniform(boolVector);
  // TestIndex(boolUniform);
  // TestUniformData(boolVector, boolUniform);

  const std::vector<int> intVector = {1, 2, 3, 4, 5, -5, -4, -3, -2 - 1, 0};
  uniform::DescriptorVector<int> intUniform(intVector);
  TestIndex(intUniform);
  TestUniformData(intVector, intUniform);

  const std::vector<glm::ivec2> ivec2Vector = {glm::ivec2(2),
                                               glm::ivec2(3),
                                               glm::ivec2(4),
                                               glm::ivec2(5),
                                               glm::ivec2(6),
                                               glm::ivec2(-5),
                                               glm::ivec2(-4),
                                               glm::ivec2(-3),
                                               glm::ivec2(-2),
                                               glm::ivec2(-1),
                                               glm::ivec2(0)};
  uniform::DescriptorVector<glm::ivec2> ivec2Uniform(ivec2Vector);
  TestIndex(ivec2Uniform);
  TestUniformData(ivec2Vector, ivec2Uniform);

  const std::vector<glm::ivec3> ivec3Vector = {glm::ivec3(3),
                                               glm::ivec3(4),
                                               glm::ivec3(5),
                                               glm::ivec3(6),
                                               glm::ivec3(7),
                                               glm::ivec3(-5),
                                               glm::ivec3(-4),
                                               glm::ivec3(-3),
                                               glm::ivec3(-2),
                                               glm::ivec3(-1),
                                               glm::ivec3(0)};
  uniform::DescriptorVector<glm::ivec3> ivec3Uniform(ivec3Vector);
  TestIndex(ivec3Uniform);
  TestUniformData(ivec3Vector, ivec3Uniform);

  const std::vector<glm::ivec4> ivec4Vector = {glm::ivec4(4),
                                               glm::ivec4(5),
                                               glm::ivec4(6),
                                               glm::ivec4(7),
                                               glm::ivec4(8),
                                               glm::ivec4(-5),
                                               glm::ivec4(-4),
                                               glm::ivec4(-3),
                                               glm::ivec4(-2),
                                               glm::ivec4(-1),
                                               glm::ivec4(0)};
  uniform::DescriptorVector<glm::ivec4> ivec4Uniform(ivec4Vector);
  TestIndex(ivec4Uniform);
  TestUniformData(ivec4Vector, ivec4Uniform);

  const std::vector<float> floatVector = {1.f, 2.f, 3.f, 4.f, 5.f, -5.f, -4.f, -3.f, -2 - 1.f, 0.f};
  uniform::DescriptorVector<float> floatUniform(floatVector);
  TestIndex(floatUniform);
  TestUniformData(floatVector, floatUniform);

  const std::vector<glm::vec2> vec2Vector = {
      glm::vec2(2.f),
      glm::vec2(3.f),
      glm::vec2(4.f),
      glm::vec2(5.f),
      glm::vec2(6.f),
      glm::vec2(-5.f),
      glm::vec2(-4.f),
      glm::vec2(-3.f),
      glm::vec2(-2.f),
      glm::vec2(-1.f),
      glm::vec2(0.f),
  };
  uniform::DescriptorVector<glm::vec2> vec2Uniform(vec2Vector);
  TestIndex(vec2Uniform);
  TestUniformData(vec2Vector, vec2Uniform);

  const std::vector<glm::vec3> vec3Vector = {
      glm::vec3(3.f),
      glm::vec3(4.f),
      glm::vec3(5.f),
      glm::vec3(6.f),
      glm::vec3(7.f),
      glm::vec3(-5.f),
      glm::vec3(-4.f),
      glm::vec3(-3.f),
      glm::vec3(-2.f),
      glm::vec3(-1.f),
      glm::vec3(0.f),
  };
  uniform::DescriptorVector<glm::vec3> vec3Uniform(vec3Vector);
  TestIndex(vec3Uniform);
  TestUniformData(vec3Vector, vec3Uniform);

  const std::vector<glm::vec4> vec4Vector = {
      glm::vec4(4.f),
      glm::vec4(5.f),
      glm::vec4(6.f),
      glm::vec4(7.f),
      glm::vec4(8.f),
      glm::vec4(-5.f),
      glm::vec4(-4.f),
      glm::vec4(-3.f),
      glm::vec4(-2.f),
      glm::vec4(-1.f),
      glm::vec4(0.f),
  };
  uniform::DescriptorVector<glm::vec4> vec4Uniform(vec4Vector);
  TestIndex(vec4Uniform);
  TestUniformData(vec4Vector, vec4Uniform);

  const std::vector<glm::mat2> mat2Vector = {
      glm::mat2(2.f),
      glm::mat2(3.f),
      glm::mat2(4.f),
      glm::mat2(5.f),
      glm::mat2(6.f),
      glm::mat2(-5.f),
      glm::mat2(-4.f),
      glm::mat2(-3.f),
      glm::mat2(-2.f),
      glm::mat2(-1.f),
      glm::mat2(0.f),
  };
  uniform::DescriptorVector<glm::mat2> mat2Uniform(mat2Vector);
  TestIndex(mat2Uniform);
  TestUniformData(mat2Vector, mat2Uniform);

  const std::vector<glm::mat3> mat3Vector = {
      glm::mat3(3.f),
      glm::mat3(4.f),
      glm::mat3(5.f),
      glm::mat3(6.f),
      glm::mat3(7.f),
      glm::mat3(-5.f),
      glm::mat3(-4.f),
      glm::mat3(-3.f),
      glm::mat3(-2.f),
      glm::mat3(-1.f),
      glm::mat3(0.f),
  };
  uniform::DescriptorVector<glm::mat3> mat3Uniform(mat3Vector);
  TestIndex(mat3Uniform);
  TestUniformData(mat3Vector, mat3Uniform);

  const std::vector<glm::mat4> mat4Vector = {
      glm::mat4(4.f),
      glm::mat4(5.f),
      glm::mat4(6.f),
      glm::mat4(7.f),
      glm::mat4(8.f),
      glm::mat4(-5.f),
      glm::mat4(-4.f),
      glm::mat4(-3.f),
      glm::mat4(-2.f),
      glm::mat4(-1.f),
      glm::mat4(0.f),
  };
  uniform::DescriptorVector<glm::mat4> mat4Uniform(mat4Vector);
  TestIndex(mat4Uniform);
  TestUniformData(mat4Vector, mat4Uniform);
}

} // namespace iglu::tests
