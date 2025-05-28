/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/PlatformDevice.h>

#include "../util/Common.h"

#include <gtest/gtest.h>
#include <igl/IGL.h>

namespace igl::tests {

class PlatformDeviceMetalTest : public ::testing::Test {
 public:
  PlatformDeviceMetalTest() = default;
  ~PlatformDeviceMetalTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);

    ASSERT_NE(iglDev_, nullptr);
    ASSERT_NE(cmdQueue_, nullptr);
  }
  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

// Test Cases
TEST_F(PlatformDeviceMetalTest, GetPlatformDeviceParentCls) {
  auto* pd = iglDev_->getPlatformDevice<metal::PlatformDevice>();
  ASSERT_NE(pd, nullptr);
}

} // namespace igl::tests
