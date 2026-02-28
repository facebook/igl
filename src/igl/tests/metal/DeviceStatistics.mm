/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/metal/DeviceStatistics.h>

#include "../data/ShaderData.h"
#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <igl/IGL.h>
#include <igl/metal/Device.h>

namespace igl::tests {

//
// MetalDeviceStatisticsTest
//
// This test covers device statistics tracking on Metal.
// Tests draw count and shader compilation count.
//
class MetalDeviceStatisticsTest : public ::testing::Test {
 public:
  MetalDeviceStatisticsTest() = default;
  ~MetalDeviceStatisticsTest() override = default;

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
// InitialDrawCountZero
//
// Test that draw count starts at 0 for a fresh device.
//
TEST_F(MetalDeviceStatisticsTest, InitialDrawCountZero) {
  // A freshly created device should have a draw count of 0
  // (assuming no other tests have used this device)
  size_t drawCount = device_->getCurrentDrawCount();
  ASSERT_EQ(drawCount, 0u);
}

//
// ShaderCompilationCountTracking
//
// Test that shader compilation count tracks compilations.
//
TEST_F(MetalDeviceStatisticsTest, ShaderCompilationCountTracking) {
  const size_t countBefore = device_->getShaderCompilationCount();

  Result res;
  const auto shaderSource = std::string(data::shader::kMtlSimpleShader);

  std::vector<ShaderModuleInfo> moduleInfo = {
      {ShaderStage::Vertex, std::string(data::shader::kSimpleVertFunc)},
      {ShaderStage::Fragment, std::string(data::shader::kSimpleFragFunc)},
  };

  auto libraryDesc =
      ShaderLibraryDesc::fromStringInput(shaderSource.c_str(), moduleInfo, "statsTestLibrary");

  auto library = device_->createShaderLibrary(libraryDesc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;

  const size_t countAfter = device_->getShaderCompilationCount();
  ASSERT_GT(countAfter, countBefore) << "Shader compilation count should have incremented";
}

} // namespace igl::tests
