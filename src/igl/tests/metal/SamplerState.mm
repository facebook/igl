/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/metal/SamplerState.h>

#include <gtest/gtest.h>
#include <igl/IGL.h>

namespace igl::tests {

class SamplerStateMTLTest : public ::testing::Test {
 public:
  SamplerStateMTLTest() = default;
  ~SamplerStateMTLTest() override = default;

  void SetUp() override {
    setDebugBreakEnabled(false);

    samplerState_ = std::make_shared<igl::metal::SamplerState>(nil);
  }
  void TearDown() override {}

 public:
  std::shared_ptr<igl::metal::SamplerState> samplerState_;
};

TEST_F(SamplerStateMTLTest, ConvertMinMagFilter) {
  MTLSamplerMinMagFilter res;

  res = samplerState_->convertMinMagFilter(SamplerMinMagFilter::Linear);
  ASSERT_EQ(res, MTLSamplerMinMagFilterLinear);

  res = samplerState_->convertMinMagFilter(SamplerMinMagFilter::Nearest);
  ASSERT_EQ(res, MTLSamplerMinMagFilterNearest);
}

TEST_F(SamplerStateMTLTest, ConvertMipFilter) {
  MTLSamplerMipFilter res;

  res = samplerState_->convertMipFilter(SamplerMipFilter::Disabled);
  ASSERT_EQ(res, MTLSamplerMipFilterNotMipmapped);

  res = samplerState_->convertMipFilter(SamplerMipFilter::Nearest);
  ASSERT_EQ(res, MTLSamplerMipFilterNearest);

  res = samplerState_->convertMipFilter(SamplerMipFilter::Linear);
  ASSERT_EQ(res, MTLSamplerMipFilterLinear);
}

TEST_F(SamplerStateMTLTest, ConvertAddressMode) {
  MTLSamplerAddressMode res;

  res = samplerState_->convertAddressMode(SamplerAddressMode::Repeat);
  ASSERT_EQ(res, MTLSamplerAddressModeRepeat);

  res = samplerState_->convertAddressMode(SamplerAddressMode::Clamp);
  ASSERT_EQ(res, MTLSamplerAddressModeClampToEdge);

  res = samplerState_->convertAddressMode(SamplerAddressMode::MirrorRepeat);
  ASSERT_EQ(res, MTLSamplerAddressModeMirrorRepeat);
}

} // namespace igl::tests
