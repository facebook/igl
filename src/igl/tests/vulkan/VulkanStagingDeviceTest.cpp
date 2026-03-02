/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/VulkanStagingDevice.h>

#include "../util/TestDevice.h"

#include <cstring>
#include <vector>
#include <igl/Buffer.h>
#include <igl/CommandBuffer.h>
#include <igl/Framebuffer.h>
#include <igl/Texture.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class VulkanStagingDeviceTest : public ::testing::Test {
 public:
  VulkanStagingDeviceTest() = default;
  ~VulkanStagingDeviceTest() override = default;

  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_EQ(iglDev_->getBackendType(), BackendType::Vulkan) << "Test requires Vulkan backend";
  }

  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;

  igl::vulkan::VulkanContext& getVulkanContext() {
    auto& device = static_cast<igl::vulkan::Device&>(*iglDev_);
    return device.getVulkanContext();
  }
};

TEST_F(VulkanStagingDeviceTest, BufferSubDataSmallUpload) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.stagingDevice_, nullptr);

  Result ret;
  BufferDesc bufferDesc;
  bufferDesc.type = BufferDesc::BufferTypeBits::Storage;
  bufferDesc.storage = ResourceStorage::Private;
  bufferDesc.length = 256;

  auto buffer = iglDev_->createBuffer(bufferDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);

  std::vector<uint8_t> srcData(256);
  for (size_t i = 0; i < srcData.size(); ++i) {
    srcData[i] = static_cast<uint8_t>(i & 0xFF);
  }

  ret = buffer->upload(srcData.data(), BufferRange(256, 0));
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  const auto* downloadedData = static_cast<uint8_t*>(buffer->map(BufferRange(256, 0), &ret));
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  for (size_t i = 0; i < 256; ++i) {
    EXPECT_EQ(downloadedData[i], srcData[i]);
  }

  buffer->unmap();
}

TEST_F(VulkanStagingDeviceTest, BufferSubDataLargerThanStaging) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.stagingDevice_, nullptr);

  const VkDeviceSize maxStagingSize = ctx.stagingDevice_->getMaxStagingBufferSize();

  size_t maxBufferLength = 0;
  iglDev_->getFeatureLimits(DeviceFeatureLimits::MaxStorageBufferBytes, maxBufferLength);

  const VkDeviceSize desiredSize = std::min<VkDeviceSize>(maxStagingSize * 2, maxBufferLength);

  if (desiredSize < maxStagingSize) {
    GTEST_SKIP() << "Max buffer size is smaller than staging buffer, cannot test chunked upload.";
  }

  Result ret;
  BufferDesc bufferDesc;
  bufferDesc.type = BufferDesc::BufferTypeBits::Storage;
  bufferDesc.storage = ResourceStorage::Private;
  bufferDesc.length = desiredSize;

  auto buffer = iglDev_->createBuffer(bufferDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);

  std::vector<uint16_t> srcData(desiredSize / 2);
  for (size_t i = 0; i < srcData.size(); ++i) {
    srcData[i] = static_cast<uint16_t>(i & 0xFFFF);
  }

  ret = buffer->upload(srcData.data(), BufferRange(desiredSize, 0));
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  const auto* downloadedData =
      static_cast<uint16_t*>(buffer->map(BufferRange(desiredSize, 0), &ret));
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  for (size_t i = 0; i < srcData.size(); ++i) {
    ASSERT_EQ(downloadedData[i], srcData[i]) << "Mismatch at index " << i;
  }

  buffer->unmap();
}

TEST_F(VulkanStagingDeviceTest, ImageDataUpload) {
  Result ret;

  TextureDesc texDesc =
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled);
  auto texture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(texture, nullptr);

  const size_t dataSize = 4 * 4 * 4;
  std::vector<uint8_t> pixelData(dataSize);
  for (size_t i = 0; i < dataSize; ++i) {
    pixelData[i] = static_cast<uint8_t>(i % 256);
  }

  ret = texture->upload(texture->getFullRange(0), pixelData.data());
  EXPECT_TRUE(ret.isOk()) << ret.message.c_str();
}

TEST_F(VulkanStagingDeviceTest, GetImageData2D) {
  Result ret;

  TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                           2,
                                           2,
                                           TextureDesc::TextureUsageBits::Sampled |
                                               TextureDesc::TextureUsageBits::Attachment);
  auto texture = iglDev_->createTexture(texDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(texture, nullptr);

  constexpr uint32_t kColor = 0xAABBCCDD;
  const std::array<uint32_t, 4> srcData = {kColor, kColor, kColor, kColor};

  ret = texture->upload(texture->getFullRange(0), srcData.data());
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  CommandQueueDesc queueDesc{};
  auto cmdQueue = iglDev_->createCommandQueue(queueDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  auto cmdBuf = cmdQueue->createCommandBuffer(CommandBufferDesc(), &ret);
  ASSERT_TRUE(ret.isOk());
  cmdQueue->submit(*cmdBuf);

  std::array<uint32_t, 4> downloadedData = {};
  auto fbDesc = FramebufferDesc();
  fbDesc.colorAttachments[0].texture = texture;
  auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  fb->copyBytesColorAttachment(*cmdQueue, 0, downloadedData.data(), texture->getFullRange(0));

  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(downloadedData[i], kColor);
  }
}

TEST_F(VulkanStagingDeviceTest, PartialBufferUpdate) {
  Result ret;

  BufferDesc bufferDesc;
  bufferDesc.type = BufferDesc::BufferTypeBits::Storage;
  bufferDesc.storage = ResourceStorage::Private;
  bufferDesc.length = 256;

  auto buffer = iglDev_->createBuffer(bufferDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);

  std::vector<uint8_t> zeros(256, 0);
  ret = buffer->upload(zeros.data(), BufferRange(256, 0));
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  std::vector<uint8_t> partialData(64, 0xAB);
  const size_t offset = 64;
  ret = buffer->upload(partialData.data(), BufferRange(64, offset));
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  const auto* downloadedData = static_cast<uint8_t*>(buffer->map(BufferRange(256, 0), &ret));
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();

  for (size_t i = 0; i < offset; ++i) {
    EXPECT_EQ(downloadedData[i], 0) << "Non-zero before offset at index " << i;
  }
  for (size_t i = offset; i < offset + 64; ++i) {
    EXPECT_EQ(downloadedData[i], 0xAB) << "Mismatch in partial update region at index " << i;
  }

  buffer->unmap();
}

TEST_F(VulkanStagingDeviceTest, MaxStagingBufferSizePositive) {
  auto& ctx = getVulkanContext();
  ASSERT_NE(ctx.stagingDevice_, nullptr);

  EXPECT_GT(ctx.stagingDevice_->getMaxStagingBufferSize(), 0u);
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
