/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <memory>
#include <igl/RenderPipelineState.h>
#include <igl/SamplerState.h>
#include <igl/Texture.h>
#include <igl/tests/util/device/TestDevice.h>
#include <igl/webgpu/PlatformDevice.h>

namespace igl::tests {

class WebGPUDeviceTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
    device_ = util::device::createTestDevice(BackendType::WebGPU);
    ASSERT_NE(device_, nullptr);
  }

 protected:
  std::unique_ptr<IDevice> device_;
};

TEST_F(WebGPUDeviceTest, BackendIdentity) {
  EXPECT_EQ(device_->getBackendType(), BackendType::WebGPU);
  EXPECT_EQ(device_->getBackendVersion().flavor, BackendFlavor::WebGPU);
  EXPECT_EQ(device_->getShaderVersion().family, ShaderFamily::Wgsl);
  EXPECT_EQ(device_->getNormalizedZRange(), NormalizedZRange::ZeroToOne);
  EXPECT_NE(device_->getNativeDevice(), nullptr);
  EXPECT_FALSE(device_->isDeviceLost());
}

TEST_F(WebGPUDeviceTest, PlatformDeviceIsWebGPU) {
  EXPECT_NE(device_->getPlatformDevice<webgpu::PlatformDevice>(), nullptr);
}

TEST_F(WebGPUDeviceTest, TestDeviceIsSupported) {
  EXPECT_TRUE(util::device::isBackendTypeSupported(BackendType::WebGPU));
}

TEST_F(WebGPUDeviceTest, UnimplementedFactoriesReportResult) {
  Result ret;
  EXPECT_EQ(device_->createTexture(
                TextureDesc::new2D(
                    TextureFormat::RGBA_UNorm8, 1, 1, TextureDesc::TextureUsageBits::Sampled),
                &ret),
            nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unimplemented);

  ret = Result();
  EXPECT_EQ(device_->createSamplerState({}, &ret), nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unimplemented);

  ret = Result();
  EXPECT_EQ(device_->createRenderPipeline({}, &ret), nullptr);
  EXPECT_EQ(ret.code, Result::Code::Unimplemented);
}

} // namespace igl::tests
