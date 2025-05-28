/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/HWDevice.h>

#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <gtest/gtest.h>
#include <memory>
#include <igl/IGL.h>

namespace igl::tests {

class HWDeviceTest : public ::testing::Test {
 public:
  HWDeviceTest() = default;
  ~HWDeviceTest() override = default;

  // Set up common resources. This will create a device
  void SetUp() override {
    // Turn off debug break so unit tests can run
    setDebugBreakEnabled(false);

    iglHWDev_ = std::make_shared<metal::HWDevice>();

    ASSERT_TRUE(iglHWDev_ != nullptr);

    // Without a device, querying for a HWDevice with a specific display ID fails
    iglDev_ = util::createTestDevice();
    ASSERT_TRUE(iglDev_ != nullptr);
  }

  void TearDown() override {}

  // Member variables
 protected:
  std::shared_ptr<metal::HWDevice> iglHWDev_;
  std::shared_ptr<IDevice> iglDev_;
};

/// This test ensures devices are returned correctly when being queried
TEST_F(HWDeviceTest, QueryDevicesSanityTest) {
  const HWDeviceQueryDesc queryDesc(HWDeviceType::DiscreteGpu);
  Result result;

  const std::vector<HWDeviceDesc> devices = iglHWDev_->queryDevices(queryDesc, &result);

  // Currently HWDevice always returns ok when being queried
  ASSERT_TRUE(result.isOk());

  const HWDeviceQueryDesc queryDesc2(HWDeviceType::Unknown);
  Result result2;

  const std::vector<HWDeviceDesc> devices2 = iglHWDev_->queryDevices(queryDesc2, &result2);

  // Currently HWDevice always returns ok when being queried
  ASSERT_TRUE(result2.isOk());

  const HWDeviceQueryDesc queryDesc3(HWDeviceType::DiscreteGpu, 8);
  Result result3;

  const std::vector<HWDeviceDesc> devices3 = iglHWDev_->queryDevices(queryDesc3, &result3);

  // Currently HWDevice always returns ok when being queried
  ASSERT_TRUE(result3.isOk());
}

TEST_F(HWDeviceTest, DeviceCreationFailureTest) {
  const uintptr_t guid = 0;
  const HWDeviceType type = HWDeviceType::Unknown;
  const HWDeviceDesc deviceDesc(guid, type);

  Result result;

  const std::unique_ptr<IDevice> device = iglHWDev_->create(deviceDesc, &result);

  // Ensure the result of the device creation is null
  ASSERT_FALSE(result.isOk());
}

TEST_F(HWDeviceTest, SystemDefaultDeviceCreation) {
  Result result;
  const std::unique_ptr<IDevice> device = iglHWDev_->createWithSystemDefaultDevice(&result);
  ASSERT_TRUE(result.isOk());
  ASSERT_NE(device, nullptr);
}

} // namespace igl::tests
