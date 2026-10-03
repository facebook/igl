/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/SamplerState.h>

#include <memory>
#include <igl/tests/util/device/webgpu/TestDevice.h>

namespace igl::tests {

class WebGPUSamplerStateTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    device_ = util::device::webgpu::createTestDevice();
    ASSERT_NE(device_, nullptr);
  }

  void TearDown() override {
    EXPECT_EQ(device_->getContext().getUncapturedErrorCount(), 0u);
  }

 protected:
  [[nodiscard]] std::shared_ptr<webgpu::SamplerState> create(const SamplerStateDesc& desc,
                                                             Result* outResult) const {
    return std::static_pointer_cast<webgpu::SamplerState>(
        device_->createSamplerState(desc, outResult));
  }

  std::unique_ptr<webgpu::Device> device_;
};

TEST_F(WebGPUSamplerStateTest, NearestIsNotFiltering) {
  Result ret;
  auto sampler = create({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(sampler, nullptr);
  EXPECT_FALSE(sampler->isFiltering());
  EXPECT_FALSE(sampler->isComparison());
  EXPECT_FALSE(sampler->isYUV());
  EXPECT_EQ(sampler->getNonFilteringSampler(), sampler->getWGPUSampler());
}

TEST_F(WebGPUSamplerStateTest, LinearHasNonFilteringTwin) {
  Result ret;
  auto sampler = create(SamplerStateDesc::newLinearMipmapped(), &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(sampler, nullptr);
  EXPECT_TRUE(sampler->isFiltering());
  WGPUSampler twin = sampler->getNonFilteringSampler();
  EXPECT_NE(twin, nullptr);
  EXPECT_NE(twin, sampler->getWGPUSampler());
  EXPECT_EQ(sampler->getNonFilteringSampler(), twin);
}

TEST_F(WebGPUSamplerStateTest, AnisotropyWithNearestFilteringIsDropped) {
  Result ret;
  auto sampler = create({.maxAnisotropic = 8}, &ret);
  EXPECT_TRUE(ret.isOk()) << ret.message;
  EXPECT_NE(sampler, nullptr);

  sampler = create(
      {
          .minFilter = SamplerMinMagFilter::Linear,
          .magFilter = SamplerMinMagFilter::Linear,
          .mipFilter = SamplerMipFilter::Linear,
          .maxAnisotropic = 16,
      },
      &ret);
  EXPECT_TRUE(ret.isOk()) << ret.message;
  EXPECT_NE(sampler, nullptr);
}

TEST_F(WebGPUSamplerStateTest, DepthCompare) {
  Result ret;
  auto sampler =
      create({.depthCompareFunction = CompareFunction::Less, .depthCompareEnabled = true}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(sampler, nullptr);
  EXPECT_TRUE(sampler->isComparison());
}

TEST_F(WebGPUSamplerStateTest, UnsupportedModes) {
  Result ret;
  auto sampler = create({.addressModeV = SamplerAddressMode::ClampToBorder}, &ret);
  EXPECT_EQ(sampler, nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);

  sampler = create(SamplerStateDesc::newYUV(TextureFormat::YUV_NV12, "yuv"), &ret);
  EXPECT_EQ(sampler, nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unsupported);
}

TEST_F(WebGPUSamplerStateTest, UniqueIds) {
  auto a = create({}, nullptr);
  auto b = create({}, nullptr);
  ASSERT_TRUE(a && b);
  EXPECT_NE(a->getSamplerId(), b->getSamplerId());
}

} // namespace igl::tests
