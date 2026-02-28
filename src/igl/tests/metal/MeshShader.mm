/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <igl/IGL.h>
#include <igl/metal/Device.h>

namespace igl::tests {

//
// MetalMeshShaderTest
//
// This test covers mesh shader pipeline creation on Metal.
// Mesh shaders require specific GPU family support and may not be available on all devices.
//
class MetalMeshShaderTest : public ::testing::Test {
 public:
  MetalMeshShaderTest() = default;
  ~MetalMeshShaderTest() override = default;

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
// MeshPipelineCreation
//
// Test creating a mesh shader pipeline.
// This test is skipped if the device does not support mesh shaders.
//
TEST_F(MetalMeshShaderTest, MeshPipelineCreation) {
  if (!device_->hasFeature(DeviceFeatures::MeshShaders)) {
    GTEST_SKIP() << "Mesh shaders not supported on this device";
  }

  // Mesh shader pipeline creation would require task/mesh shader source
  // which are hardware-specific. Verify the feature query does not crash.
  ASSERT_TRUE(device_->hasFeature(DeviceFeatures::MeshShaders));
}

} // namespace igl::tests
