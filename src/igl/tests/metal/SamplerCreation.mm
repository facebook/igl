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
#include <igl/metal/SamplerState.h>

namespace igl::tests {

//
// MetalSamplerCreationTest
//
// This test covers creation of Metal sampler states with various configurations.
//
class MetalSamplerCreationTest : public ::testing::Test {
 public:
  MetalSamplerCreationTest() = default;
  ~MetalSamplerCreationTest() override = default;

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
// DefaultSampler
//
// Test creating a sampler with default descriptor values.
//
TEST_F(MetalSamplerCreationTest, DefaultSampler) {
  Result res;
  SamplerStateDesc desc;
  desc.debugName = "defaultSampler";

  auto sampler = device_->createSamplerState(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(sampler, nullptr);
}

//
// SamplerWithFilters
//
// Test creating a sampler with specific min, mag, and mip filters.
//
TEST_F(MetalSamplerCreationTest, SamplerWithFilters) {
  Result res;
  SamplerStateDesc desc;
  desc.minFilter = SamplerMinMagFilter::Linear;
  desc.magFilter = SamplerMinMagFilter::Linear;
  desc.mipFilter = SamplerMipFilter::Linear;
  desc.debugName = "linearSampler";

  auto sampler = device_->createSamplerState(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(sampler, nullptr);
}

//
// SamplerWithAddressModes
//
// Test creating a sampler with specific address modes.
//
TEST_F(MetalSamplerCreationTest, SamplerWithAddressModes) {
  Result res;
  SamplerStateDesc desc;
  desc.addressModeU = SamplerAddressMode::Clamp;
  desc.addressModeV = SamplerAddressMode::MirrorRepeat;
  desc.addressModeW = SamplerAddressMode::Repeat;
  desc.debugName = "addressModeSampler";

  auto sampler = device_->createSamplerState(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(sampler, nullptr);
}

//
// SamplerIsYUVFalse
//
// Test that a standard sampler reports isYUV as false.
//
TEST_F(MetalSamplerCreationTest, SamplerIsYUVFalse) {
  Result res;
  SamplerStateDesc desc = SamplerStateDesc::newLinear();

  auto sampler = device_->createSamplerState(desc, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(sampler, nullptr);
  ASSERT_FALSE(sampler->isYUV());
}

} // namespace igl::tests
