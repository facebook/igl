/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/IGL.h>

#include "../util/TestDevice.h"

#if IGL_PLATFORM_WIN || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOS || IGL_PLATFORM_LINUX
#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/VulkanContext.h>
#endif

namespace igl::tests {

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
  CommandQueueDesc desc{};

  desc.type = CommandQueueType::Graphics;

  auto cmdQueue = iglDev_->createCommandQueue(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(cmdQueue, nullptr);
}

#if IGL_PLATFORM_WIN || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOS || IGL_PLATFORM_LINUX
TEST_F(DeviceVulkanTest, StagingDeviceLargeBufferTest) {
  Result ret;

  // create a GPU device-local storage buffer large enough to force Vulkan staging device to upload
  // it in multiple chunks
  BufferDesc bufferDesc;
  bufferDesc.type = BufferDesc::BufferTypeBits::Storage;
  bufferDesc.storage = ResourceStorage::Private;

  igl::vulkan::VulkanContext& ctx =
      static_cast<igl::vulkan::Device*>(iglDev_.get())->getVulkanContext();

  const VkDeviceSize kMaxStagingBufferSize = ctx.stagingDevice_->getMaxStagingBufferSize();

  const std::array<VkDeviceSize, 2> kDesiredBufferSizes = {kMaxStagingBufferSize * 2u,
                                                           kMaxStagingBufferSize + 2u};

  size_t maxBufferLength = 0;
  iglDev_->getFeatureLimits(DeviceFeatureLimits::MaxStorageBufferBytes, maxBufferLength);

  for (auto kDesiredBufferSize : kDesiredBufferSizes) {
    bufferDesc.length = std::min<VkDeviceSize>(kDesiredBufferSize, maxBufferLength);

    ASSERT_TRUE(bufferDesc.length % 2 == 0);

    const std::shared_ptr<IBuffer> buffer = iglDev_->createBuffer(bufferDesc, &ret);

    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(buffer != nullptr);

    // upload
    {
      std::vector<uint16_t> bufferData(bufferDesc.length / 2);

      uint16_t* data = bufferData.data();

      for (size_t i = 0; i != bufferDesc.length / 2; i++) {
        data[i] = uint16_t(i & 0xffff);
      }

      ret = buffer->upload(data, BufferRange(bufferDesc.length, 0));

      ASSERT_EQ(ret.code, Result::Code::Ok);
    }
    // download
    {
      // map() will create a CPU-copy of data
      const auto* data =
          static_cast<uint16_t*>(buffer->map(BufferRange(bufferDesc.length, 0), &ret));

      ASSERT_EQ(ret.code, Result::Code::Ok);

      for (size_t i = 0; i != bufferDesc.length / 2; i++) {
        ASSERT_EQ(data[i], uint16_t(i & 0xffff));
      }

      buffer->unmap();
    }

    ASSERT_EQ(ret.code, Result::Code::Ok);
  }
}

GTEST_TEST(VulkanContext, BufferDeviceAddress) {
  std::shared_ptr<igl::IDevice> iglDev = nullptr;

  igl::vulkan::VulkanContextConfig config;
#if IGL_PLATFORM_MACOS
  config.terminateOnValidationError = false;
#elif IGL_DEBUG
  config.enableValidation = true;
  config.terminateOnValidationError = true;
#else
  config.enableValidation = true;
  config.terminateOnValidationError = false;
#endif
#ifdef IGL_DISABLE_VALIDATION
  config.enableValidation = false;
  config.terminateOnValidationError = false;
#endif
  config.enableExtraLogs = true;
  config.enableBufferDeviceAddress = true;

  auto ctx = igl::vulkan::HWDevice::createContext(config, nullptr);

  Result ret;

  std::vector<HWDeviceDesc> devices =
      igl::vulkan::HWDevice::queryDevices(*ctx, HWDeviceQueryDesc(HWDeviceType::Unknown), &ret);

  ASSERT_TRUE(!devices.empty());

  if (ret.isOk()) {
    const std::vector<const char*> extraDeviceExtensions;
    iglDev = igl::vulkan::HWDevice::create(std::move(ctx),
                                           devices[0],
                                           0, // width
                                           0, // height,
                                           0,
                                           nullptr,
                                           &ret);

    if (!ret.isOk()) {
      iglDev = nullptr;
    }
  }

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(iglDev, nullptr);

  if (!iglDev) {
    return;
  }

  auto buffer = iglDev->createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Uniform, nullptr, 256, ResourceStorage::Shared), &ret);

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);

  if (!buffer) {
    return;
  }

  ASSERT_NE(buffer->gpuAddress(), 0u);
}

GTEST_TEST(VulkanContext, DescriptorIndexing) {
  std::shared_ptr<igl::IDevice> iglDev = nullptr;

  igl::vulkan::VulkanContextConfig config;
#if IGL_PLATFORM_MACOS
  config.terminateOnValidationError = false;
#elif IGL_DEBUG
  config.enableValidation = true;
  config.terminateOnValidationError = true;
#else
  config.enableValidation = true;
  config.terminateOnValidationError = false;
#endif
#ifdef IGL_DISABLE_VALIDATION
  config.enableValidation = false;
  config.terminateOnValidationError = false;
#endif
  config.enableExtraLogs = true;
  config.enableDescriptorIndexing = true;

  auto ctx = igl::vulkan::HWDevice::createContext(config, nullptr);

  Result ret;

  std::vector<HWDeviceDesc> devices =
      igl::vulkan::HWDevice::queryDevices(*ctx, HWDeviceQueryDesc(HWDeviceType::Unknown), &ret);

  ASSERT_TRUE(!devices.empty());

  if (ret.isOk()) {
    const std::vector<const char*> extraDeviceExtensions;
    iglDev = igl::vulkan::HWDevice::create(std::move(ctx),
                                           devices[0],
                                           0, // width
                                           0, // height,
                                           0,
                                           nullptr,
                                           &ret);

    if (!ret.isOk()) {
      iglDev = nullptr;
    }
  }

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(iglDev, nullptr);

  if (!iglDev) {
    return;
  }

  const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                 1,
                                                 1,
                                                 TextureDesc::TextureUsageBits::Sampled |
                                                     TextureDesc::TextureUsageBits::Attachment);

  auto texture = iglDev->createTexture(texDesc, &ret);
  ASSERT_EQ(ret.code, Result::Code::Ok);
  ASSERT_NE(texture, nullptr);

  if (!texture) {
    return;
  }

  ASSERT_NE(texture->getTextureId(), 0u);
}
#endif

} // namespace igl::tests
