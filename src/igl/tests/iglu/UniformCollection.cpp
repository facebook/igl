/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "UniformTests.h"

#include <IGLU/uniform/Collection.h>
#include <IGLU/uniform/Descriptor.h>
#include <string>
#include <igl/NameHandle.h>

namespace iglu::tests {

//
// UniformCollectionTest
//
// Test fixture for all the tests in this file. Takes care of common
// initialization and allocating of common resources.
//
class UniformCollectionTest : public ::testing::Test {
 private:
 public:
  UniformCollectionTest() = default;
  ~UniformCollectionTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
  }

  void TearDown() override {}
};

// ----------------------------------------------------------------------------

//
// Value Test
//
TEST_F(UniformCollectionTest, DescriptorValue) {
  uniform::Collection c;

  const bool boolValue = true;
  uniform::DescriptorValue<bool> boolUniform(boolValue);
  testIndex(boolUniform);
  testUniformData(boolValue, boolUniform);
  auto boolUniformNameHandle = igl::genNameHandle("boolUniform");
  c.set(boolUniformNameHandle, boolValue);
  testUniformData(boolValue, c.getOrCreate<bool>(boolUniformNameHandle));

  const int intValue = 1;
  uniform::DescriptorValue<int> intUniform(intValue);
  testIndex(intUniform);
  testUniformData(intValue, intUniform);
  auto intUniformNameHandle = igl::genNameHandle("intUniform");
  c.set(intUniformNameHandle, intValue);
  testUniformData(intValue, c.getOrCreate<int>(intUniformNameHandle));

  const glm::ivec2 ivec2Value(2);
  uniform::DescriptorValue<glm::ivec2> ivec2Uniform(ivec2Value);
  testIndex(ivec2Uniform);
  testUniformData(ivec2Value, ivec2Uniform);
  auto ivec2UniformNameHandle = igl::genNameHandle("ivec2Uniform");
  c.set(ivec2UniformNameHandle, ivec2Value);
  testUniformData(ivec2Value, c.getOrCreate<glm::ivec2>(ivec2UniformNameHandle));

  const glm::ivec3 ivec3Value(3);
  uniform::DescriptorValue<glm::ivec3> ivec3Uniform(ivec3Value);
  testIndex(ivec3Uniform);
  testUniformData(ivec3Value, ivec3Uniform);
  auto ivec3UniformNameHandle = igl::genNameHandle("ivec3Uniform");
  c.set(ivec3UniformNameHandle, ivec3Value);
  testUniformData(ivec3Value, c.getOrCreate<glm::ivec3>(ivec3UniformNameHandle));

  const glm::ivec4 ivec4Value(4);
  uniform::DescriptorValue<glm::ivec4> ivec4Uniform(ivec4Value);
  testIndex(ivec4Uniform);
  testUniformData(ivec4Value, ivec4Uniform);
  auto ivec4UniformNameHandle = igl::genNameHandle("ivec4Uniform");
  c.set(ivec4UniformNameHandle, ivec4Value);
  testUniformData(ivec4Value, c.getOrCreate<glm::ivec4>(ivec4UniformNameHandle));

  const float floatValue(1.f);
  uniform::DescriptorValue<float> floatUniform(floatValue);
  testIndex(floatUniform);
  testUniformData(floatValue, floatUniform);
  auto floatUniformNameHandle = igl::genNameHandle("floatUniform");
  c.set(floatUniformNameHandle, floatValue);
  testUniformData(floatValue, c.getOrCreate<float>(floatUniformNameHandle));

  const glm::vec2 vec2Value(2.f);
  uniform::DescriptorValue<glm::vec2> vec2Uniform(vec2Value);
  testIndex(vec2Uniform);
  testUniformData(vec2Value, vec2Uniform);
  auto vec2UniformNameHandle = igl::genNameHandle("vec2Uniform");
  c.set(vec2UniformNameHandle, vec2Value);
  testUniformData(vec2Value, c.getOrCreate<glm::vec2>(vec2UniformNameHandle));

  const glm::vec3 vec3Value(3.f);
  uniform::DescriptorValue<glm::vec3> vec3Uniform(vec3Value);
  testIndex(vec3Uniform);
  testUniformData(vec3Value, vec3Uniform);
  auto vec3UniformNameHandle = igl::genNameHandle("vec3Uniform");
  c.set(vec3UniformNameHandle, vec3Value);
  testUniformData(vec3Value, c.getOrCreate<glm::vec3>(vec3UniformNameHandle));

  const glm::vec4 vec4Value(4.f);
  uniform::DescriptorValue<glm::vec4> vec4Uniform(vec4Value);
  testIndex(vec4Uniform);
  testUniformData(vec4Value, vec4Uniform);
  auto vec4UniformNameHandle = igl::genNameHandle("vec4Uniform");
  c.set(vec4UniformNameHandle, vec4Value);
  testUniformData(vec4Value, c.getOrCreate<glm::vec4>(vec4UniformNameHandle));

  const glm::mat2 mat2Value(2.f);
  uniform::DescriptorValue<glm::mat2> mat2Uniform(mat2Value);
  testIndex(mat2Uniform);
  testUniformData(mat2Value, mat2Uniform);
  auto mat2UniformNameHandle = igl::genNameHandle("mat2Uniform");
  c.set(mat2UniformNameHandle, mat2Value);
  testUniformData(mat2Value, c.getOrCreate<glm::mat2>(mat2UniformNameHandle));

  const glm::mat3 mat3Value(3.f);
  uniform::DescriptorValue<glm::mat3> mat3Uniform(mat3Value);
  testIndex(mat3Uniform);
  testUniformData(mat3Value, mat3Uniform);
  auto mat3UniformNameHandle = igl::genNameHandle("mat3Uniform");
  c.set(mat3UniformNameHandle, mat3Value);
  testUniformData(mat3Value, c.getOrCreate<glm::mat3>(mat3UniformNameHandle));

  const glm::mat4 mat4Value(4.f);
  uniform::DescriptorValue<glm::mat4> mat4Uniform(mat4Value);
  testIndex(mat4Uniform);
  testUniformData(mat4Value, mat4Uniform);
  auto mat4UniformNameHandle = igl::genNameHandle("mat4Uniform");
  c.set(mat4UniformNameHandle, mat4Value);
  testUniformData(mat4Value, c.getOrCreate<glm::mat4>(mat4UniformNameHandle));
}

//
// Vector Test
//
TEST_F(UniformCollectionTest, DescriptorVector) {
  uniform::Collection c;

  // NOTE: std::vector<bool> does not have the data() method
  // std::vector<bool> boolVector = {true, false};
  // uniform::DescriptorVector<bool> boolUniform(boolVector);
  // testIndex(boolUniform);
  // testUniformData(boolVector, boolUniform);

  const std::vector<int> intVector = {1, 2, 3, 4, 5, -5, -4, -3, -2 - 1, 0};
  uniform::DescriptorVector<int> intUniform(intVector);
  testIndex(intUniform);
  testUniformData(intVector, intUniform);
  auto intUniformNameHandle = igl::genNameHandle("intUniform");
  c.set(intUniformNameHandle, intVector);
  testUniformData(intVector, c.getOrCreate<std::vector<int>>(intUniformNameHandle));

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
  testIndex(ivec2Uniform);
  testUniformData(ivec2Vector, ivec2Uniform);
  auto ivec2UniformNameHandle = igl::genNameHandle("ivec2Uniform");
  c.set(ivec2UniformNameHandle, ivec2Vector);
  testUniformData(ivec2Vector, c.getOrCreate<std::vector<glm::ivec2>>(ivec2UniformNameHandle));

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
  testIndex(ivec3Uniform);
  testUniformData(ivec3Vector, ivec3Uniform);
  auto ivec3UniformNameHandle = igl::genNameHandle("ivec3Uniform");
  c.set(ivec3UniformNameHandle, ivec3Vector);
  testUniformData(ivec3Vector, c.getOrCreate<std::vector<glm::ivec3>>(ivec3UniformNameHandle));

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
  testIndex(ivec4Uniform);
  testUniformData(ivec4Vector, ivec4Uniform);
  auto ivec4UniformNameHandle = igl::genNameHandle("ivec4Uniform");
  c.set(ivec4UniformNameHandle, ivec4Vector);
  testUniformData(ivec4Vector, c.getOrCreate<std::vector<glm::ivec4>>(ivec4UniformNameHandle));

  const std::vector<float> floatVector = {1.f, 2.f, 3.f, 4.f, 5.f, -5.f, -4.f, -3.f, -2 - 1.f, 0.f};
  uniform::DescriptorVector<float> floatUniform(floatVector);
  testIndex(floatUniform);
  testUniformData(floatVector, floatUniform);
  auto floatUniformNameHandle = igl::genNameHandle("floatUniform");
  c.set(floatUniformNameHandle, floatVector);
  testUniformData(floatVector, c.getOrCreate<std::vector<float>>(floatUniformNameHandle));

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
  testIndex(vec2Uniform);
  testUniformData(vec2Vector, vec2Uniform);
  auto vec2UniformNameHandle = igl::genNameHandle("vec2Uniform");
  c.set(vec2UniformNameHandle, vec2Vector);
  testUniformData(vec2Vector, c.getOrCreate<std::vector<glm::vec2>>(vec2UniformNameHandle));

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
  testIndex(vec3Uniform);
  testUniformData(vec3Vector, vec3Uniform);
  auto vec3UniformNameHandle = igl::genNameHandle("vec3Uniform");
  c.set(vec3UniformNameHandle, vec3Vector);
  testUniformData(vec3Vector, c.getOrCreate<std::vector<glm::vec3>>(vec3UniformNameHandle));

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
  testIndex(vec4Uniform);
  testUniformData(vec4Vector, vec4Uniform);
  auto vec4UniformNameHandle = igl::genNameHandle("vec4Uniform");
  c.set(vec4UniformNameHandle, vec4Vector);
  testUniformData(vec4Vector, c.getOrCreate<std::vector<glm::vec4>>(vec4UniformNameHandle));

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
  testIndex(mat2Uniform);
  testUniformData(mat2Vector, mat2Uniform);
  auto mat2UniformNameHandle = igl::genNameHandle("mat2Uniform");
  c.set(mat2UniformNameHandle, mat2Vector);
  testUniformData(mat2Vector, c.getOrCreate<std::vector<glm::mat2>>(mat2UniformNameHandle));

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
  testIndex(mat3Uniform);
  testUniformData(mat3Vector, mat3Uniform);
  auto mat3UniformNameHandle = igl::genNameHandle("mat3Uniform");
  c.set(mat3UniformNameHandle, mat3Vector);
  testUniformData(mat3Vector, c.getOrCreate<std::vector<glm::mat3>>(mat3UniformNameHandle));

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
  testIndex(mat4Uniform);
  testUniformData(mat4Vector, mat4Uniform);
  auto mat4UniformNameHandle = igl::genNameHandle("mat4Uniform");
  c.set(mat4UniformNameHandle, mat4Vector);
  testUniformData(mat4Vector, c.getOrCreate<std::vector<glm::mat4>>(mat4UniformNameHandle));
}

} // namespace iglu::tests
