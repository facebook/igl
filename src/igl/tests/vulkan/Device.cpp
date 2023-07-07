/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/IGL.h>

#include "../util/TestDevice.h"

namespace igl {
namespace tests {

class DeviceVulkanTest : public ::testing::Test {
 public:
  DeviceVulkanTest() = default;
  ~DeviceVulkanTest() override = default;

  // Set up common resources. This will create a device
  void SetUp() override {
    // Turn off debug break so unit tests can run
    igl::setDebugBreakEnabled(false);

    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
  }

  void TearDown() override {}

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
};

/// CreateCommandQueue
/// Once the backend is more mature, we will use the IGL level test. For now
/// this is just here as a proof of concept.
TEST_F(DeviceVulkanTest, CreateCommandQueue) {
  Result ret;
  CommandQueueDesc desc;

  desc.type = CommandQueueType::Graphics;

  auto cmdQueue = iglDev_->createCommandQueue(desc, &ret);
  ASSERT_TRUE(ret.isOk());
  ASSERT_NE(cmdQueue, nullptr);
}

} // namespace tests
} // namespace igl
