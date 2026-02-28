/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../data/ShaderData.h"
#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <igl/IGL.h>
#include <igl/metal/Device.h>

namespace igl::tests {

//
// MetalShaderLibraryTest
//
// This test covers creation and error paths of shader libraries on Metal.
//
class MetalShaderLibraryTest : public ::testing::Test {
 public:
  MetalShaderLibraryTest() = default;
  ~MetalShaderLibraryTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);
    util::createDeviceAndQueue(device_, cmdQueue_);
    ASSERT_NE(device_, nullptr);
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> device_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

//
// CreateFromSource
//
// Test creating a shader library from a Metal source string.
//
TEST_F(MetalShaderLibraryTest, CreateFromSource) {
  Result res;

  const auto shaderSource = std::string(data::shader::kMtlSimpleShader);

  std::vector<ShaderModuleInfo> moduleInfo = {
      {ShaderStage::Vertex, std::string(data::shader::kSimpleVertFunc)},
      {ShaderStage::Fragment, std::string(data::shader::kSimpleFragFunc)},
  };

  auto libraryDesc =
      ShaderLibraryDesc::fromStringInput(shaderSource.c_str(), moduleInfo, "testLibrary");

  auto library = device_->createShaderLibrary(libraryDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(library, nullptr);

  // Verify we can retrieve individual shader modules
  auto vertModule = library->getShaderModule(std::string(data::shader::kSimpleVertFunc));
  ASSERT_NE(vertModule, nullptr);

  auto fragModule = library->getShaderModule(std::string(data::shader::kSimpleFragFunc));
  ASSERT_NE(fragModule, nullptr);
}

//
// InvalidSourceReturnsError
//
// Test that creating a shader library with invalid shader source returns an error.
//
TEST_F(MetalShaderLibraryTest, InvalidSourceReturnsError) {
  Result res;

  const char* invalidSource = "this is not valid Metal shader code!!!";

  std::vector<ShaderModuleInfo> moduleInfo = {
      {ShaderStage::Vertex, "nonExistentFunc"},
  };

  auto libraryDesc =
      ShaderLibraryDesc::fromStringInput(invalidSource, moduleInfo, "invalidLibrary");

  auto library = device_->createShaderLibrary(libraryDesc, &res);
  ASSERT_FALSE(res.isOk());
}

//
// ShaderCompilationCountIncrements
//
// Test that shader compilation count increments after compiling a shader.
//
TEST_F(MetalShaderLibraryTest, ShaderCompilationCountIncrements) {
  const size_t countBefore = device_->getShaderCompilationCount();

  Result res;
  const auto shaderSource = std::string(data::shader::kMtlSimpleShader);

  std::vector<ShaderModuleInfo> moduleInfo = {
      {ShaderStage::Vertex, std::string(data::shader::kSimpleVertFunc)},
      {ShaderStage::Fragment, std::string(data::shader::kSimpleFragFunc)},
  };

  auto libraryDesc =
      ShaderLibraryDesc::fromStringInput(shaderSource.c_str(), moduleInfo, "countTestLibrary");

  auto library = device_->createShaderLibrary(libraryDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  const size_t countAfter = device_->getShaderCompilationCount();
  ASSERT_GT(countAfter, countBefore);
}

} // namespace igl::tests
