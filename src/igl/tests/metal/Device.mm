/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/Device.h>

#include "../util/TestDevice.h"

#include <gtest/gtest.h>
#include <igl/IGL.h>

namespace igl::tests {

class DeviceMetalTest : public ::testing::Test {
 public:
  DeviceMetalTest() = default;
  ~DeviceMetalTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);

    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
  }
  void TearDown() override {}

 public:
  std::shared_ptr<IDevice> iglDev_;
};

// Test Cases
TEST_F(DeviceMetalTest, GetShaderVersion) {
  auto iglShaderVersion = iglDev_->getShaderVersion();
  ASSERT_EQ(iglShaderVersion.family, igl::ShaderFamily::Metal);
  ASSERT_GT(iglShaderVersion.majorVersion, 0);
  ASSERT_GT(iglShaderVersion.minorVersion, 0);
}

} // namespace igl::tests
