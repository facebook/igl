/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <memory>
#include <igl/CommandBuffer.h>
#include <igl/CommandQueue.h>
#include <igl/ComputePipelineState.h>
#include <igl/Framebuffer.h>
#include <igl/Timer.h>
#include <igl/tests/util/device/TestDevice.h>
#include <igl/webgpu/PlatformDevice.h>
#include <igl/webgpu/WebGPUContext.h>

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

TEST_F(WebGPUDeviceTest, FactoriesReportResult) {
  Result ret;
  auto timer = device_->createTimer(&ret);
  EXPECT_EQ(timer != nullptr, device_->hasFeature(DeviceFeatures::Timers));
  EXPECT_EQ(ret.code,
            device_->hasFeature(DeviceFeatures::Timers) ? Result::Code::Ok
                                                        : Result::Code::Unsupported);

  ret = Result();
  EXPECT_EQ(device_->createTimestampQueries(0, &ret), nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);

  ret = Result();
  EXPECT_EQ(device_->createComputePipeline({}, &ret), nullptr);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
}

TEST_F(WebGPUDeviceTest, CreateCallsFailAfterDeviceLoss) {
  Result ret;
  auto queue = device_->createCommandQueue({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto buffer = device_->createBuffer(
      {.type = BufferDesc::BufferTypeBits::Uniform, .data = nullptr, .length = 256}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto texture = device_->createTexture(
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled),
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  const webgpu::WebGPUContext& ctx =
      device_->getPlatformDevice<webgpu::PlatformDevice>()->getContext();
  wgpuDeviceDestroy(static_cast<WGPUDevice>(device_->getNativeDevice()));
  for (int i = 0; i < 100 && !device_->isDeviceLost(); ++i) {
    ctx.processEvents();
  }
  ASSERT_TRUE(device_->isDeviceLost());

  const auto expectLost = [](const auto& object, const Result& result) {
    EXPECT_EQ(object, nullptr);
    EXPECT_EQ(result.code, Result::Code::DeviceLost);
  };
  expectLost(device_->createCommandQueue({}, &ret), ret);
  expectLost(queue->createCommandBuffer({}, &ret), ret);
  expectLost(
      device_->createBuffer({.type = BufferDesc::BufferTypeBits::Uniform, .length = 256}, &ret),
      ret);
  expectLost(device_->createTexture(
                 TextureDesc::new2D(
                     TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled),
                 &ret),
             ret);
  expectLost(device_->createTextureView(texture, {}, &ret), ret);
  expectLost(device_->createSamplerState({}, &ret), ret);
  expectLost(device_->createFramebuffer({}, &ret), ret);
  expectLost(device_->createTimer(&ret), ret);
  expectLost(device_->createTimestampQueries(2, &ret), ret);
  expectLost(device_->createShaderModule(ShaderModuleDesc::fromStringInput(
                                             "@compute @workgroup_size(1) fn main() {}",
                                             {.stage = ShaderStage::Compute, .entryPoint = "main"},
                                             "lost"),
                                         &ret),
             ret);
  expectLost(
      device_->createShaderLibrary(ShaderLibraryDesc::fromStringInput(
                                       "@compute @workgroup_size(1) fn main() {}",
                                       {{.stage = ShaderStage::Compute, .entryPoint = "main"}},
                                       "lost"),
                                   &ret),
      ret);
  expectLost(device_->createComputePipeline({}, &ret), ret);
  expectLost(device_->createRenderPipeline({}, &ret), ret);

  // Resources created before the loss stay usable as CPU objects.
  const uint32_t word = 7;
  EXPECT_TRUE(buffer->upload(&word, {sizeof(word), 0}).isOk());
  EXPECT_EQ(ctx.getUncapturedErrorCount(), 0u);
}

} // namespace igl::tests
