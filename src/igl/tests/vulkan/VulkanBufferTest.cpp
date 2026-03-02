/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/VulkanBuffer.h>

#include "../util/TestDevice.h"

#include <igl/Buffer.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX

namespace igl::tests {

class VulkanBufferTest : public ::testing::Test {
 public:
  VulkanBufferTest() = default;
  ~VulkanBufferTest() override = default;

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

TEST_F(VulkanBufferTest, CreateDeviceLocal) {
  auto& ctx = getVulkanContext();

  Result ret;
  auto buffer =
      ctx.createBuffer(256,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                       &ret,
                       "testDeviceLocalBuffer");

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);

  EXPECT_NE(buffer->getVkBuffer(), VK_NULL_HANDLE);
  EXPECT_EQ(buffer->getSize(), 256u);
  EXPECT_TRUE(buffer->getMemoryPropertyFlags() & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

TEST_F(VulkanBufferTest, CreateHostVisible) {
  auto& ctx = getVulkanContext();

  Result ret;
  auto buffer =
      ctx.createBuffer(512,
                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                       &ret,
                       "testHostVisibleBuffer");

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);

  EXPECT_NE(buffer->getVkBuffer(), VK_NULL_HANDLE);
  EXPECT_EQ(buffer->getSize(), 512u);
  EXPECT_TRUE(buffer->isMapped());
  EXPECT_NE(buffer->getMappedPtr(), nullptr);
}

TEST_F(VulkanBufferTest, BufferDeviceAddress) {
  auto& ctx = getVulkanContext();

  if (!ctx.features().has_VK_KHR_buffer_device_address) {
    GTEST_SKIP() << "VK_KHR_buffer_device_address not supported.";
  }

  Result ret;

  auto buffer = iglDev_->createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Uniform, nullptr, 256, ResourceStorage::Shared), &ret);

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);
  EXPECT_NE(buffer->gpuAddress(), 0u);
}

TEST_F(VulkanBufferTest, GetVkBuffer) {
  auto& ctx = getVulkanContext();

  Result ret;
  auto buffer = ctx.createBuffer(128,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                 &ret,
                                 "testGetVkBuffer");

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);

  VkBuffer vkBuf = buffer->getVkBuffer();
  EXPECT_NE(vkBuf, VK_NULL_HANDLE);
}

TEST_F(VulkanBufferTest, HostVisibleBufferSubData) {
  auto& ctx = getVulkanContext();

  Result ret;
  auto buffer =
      ctx.createBuffer(256,
                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &ret,
                       "testSubData");

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);
  ASSERT_TRUE(buffer->isMapped());

  std::vector<uint32_t> srcData(64, 0xDEADBEEF);
  buffer->bufferSubData(0, srcData.size() * sizeof(uint32_t), srcData.data());

  std::vector<uint32_t> dstData(64, 0);
  buffer->getBufferSubData(0, dstData.size() * sizeof(uint32_t), dstData.data());

  for (size_t i = 0; i < 64; ++i) {
    EXPECT_EQ(dstData[i], 0xDEADBEEF);
  }
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
