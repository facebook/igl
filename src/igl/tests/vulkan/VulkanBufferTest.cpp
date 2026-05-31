/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/VulkanBuffer.h>

#include "../util/TestDevice.h"

#include <array>
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
      BufferDesc{
          .type = BufferDesc::BufferTypeBits::Uniform,
          .length = 256,
          .storage = ResourceStorage::Shared,
      },
      &ret);

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

TEST_F(VulkanBufferTest, GetBufferUsageFlags) {
  auto& ctx = getVulkanContext();

  Result ret;
  const VkBufferUsageFlags expectedFlags =
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  auto buffer = ctx.createBuffer(
      64, expectedFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &ret, "testUsageFlags");

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);

  EXPECT_TRUE(buffer->getBufferUsageFlags() & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  EXPECT_TRUE(buffer->getBufferUsageFlags() & VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
}

TEST_F(VulkanBufferTest, DeviceLocalBufferIsNotMapped) {
  auto& ctx = getVulkanContext();

  Result ret;
  auto buffer = ctx.createBuffer(128,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 &ret,
                                 "testDeviceLocalNotMapped");

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);

  EXPECT_FALSE(buffer->isMapped());
  EXPECT_EQ(buffer->getMappedPtr(), nullptr);
}

TEST_F(VulkanBufferTest, GetSizeMatchesRequested) {
  auto& ctx = getVulkanContext();

  const VkDeviceSize sizes[] = {1, 64, 1024, 65536};

  for (const VkDeviceSize size : sizes) {
    Result ret;
    auto buffer = ctx.createBuffer(size,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                   &ret,
                                   "testGetSize");

    ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->getSize(), size);
  }
}

TEST_F(VulkanBufferTest, BufferSubDataWithOffset) {
  auto& ctx = getVulkanContext();

  Result ret;
  auto buffer =
      ctx.createBuffer(256,
                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &ret,
                       "testSubDataOffset");

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);
  ASSERT_TRUE(buffer->isMapped());

  const std::array<uint32_t, 4> srcData = {0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD};
  const size_t offset = 64;
  buffer->bufferSubData(offset, srcData.size() * sizeof(uint32_t), srcData.data());

  std::array<uint32_t, 4> dstData = {};
  buffer->getBufferSubData(offset, dstData.size() * sizeof(uint32_t), dstData.data());

  EXPECT_EQ(dstData[0], 0xAAAAAAAA);
  EXPECT_EQ(dstData[1], 0xBBBBBBBB);
  EXPECT_EQ(dstData[2], 0xCCCCCCCC);
  EXPECT_EQ(dstData[3], 0xDDDDDDDD);
}

TEST_F(VulkanBufferTest, CoherentMemoryFlag) {
  auto& ctx = getVulkanContext();

  Result ret;
  auto coherentBuffer =
      ctx.createBuffer(128,
                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &ret,
                       "testCoherentBuffer");

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(coherentBuffer, nullptr);

  if (!coherentBuffer->isCoherentMemory()) {
    GTEST_SKIP() << "Allocated memory type does not have VK_MEMORY_PROPERTY_HOST_COHERENT_BIT.";
  }

  auto deviceLocalBuffer = ctx.createBuffer(128,
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                            &ret,
                                            "testDeviceLocalBuffer");

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(deviceLocalBuffer, nullptr);
  EXPECT_FALSE(deviceLocalBuffer->isCoherentMemory());
}

TEST_F(VulkanBufferTest, BufferSubDataWithNullDataZerosMemory) {
  auto& ctx = getVulkanContext();

  Result ret;
  auto buffer =
      ctx.createBuffer(256,
                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &ret,
                       "testNullDataZeros");

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);
  ASSERT_TRUE(buffer->isMapped());

  const std::array<uint32_t, 4> srcData = {0xAAAAAAAA, 0xBBBBBBBB, 0xCCCCCCCC, 0xDDDDDDDD};
  buffer->bufferSubData(0, srcData.size() * sizeof(uint32_t), srcData.data());

  buffer->bufferSubData(0, srcData.size() * sizeof(uint32_t), nullptr);

  std::array<uint32_t, 4> dstData = {};
  dstData.fill(0xFF);
  buffer->getBufferSubData(0, dstData.size() * sizeof(uint32_t), dstData.data());

  for (const uint32_t val : dstData) {
    EXPECT_EQ(val, 0u);
  }
}

TEST_F(VulkanBufferTest, FlushAndInvalidateMappedMemory) {
  auto& ctx = getVulkanContext();

  Result ret;
  auto buffer =
      ctx.createBuffer(256,
                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &ret,
                       "testFlushInvalidate");

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);
  ASSERT_TRUE(buffer->isMapped());

  const std::array<uint32_t, 4> srcData = {1, 2, 3, 4};
  buffer->bufferSubData(0, srcData.size() * sizeof(uint32_t), srcData.data());

  buffer->flushMappedMemory(0, srcData.size() * sizeof(uint32_t));
  buffer->invalidateMappedMemory(0, srcData.size() * sizeof(uint32_t));

  std::array<uint32_t, 4> dstData = {};
  buffer->getBufferSubData(0, dstData.size() * sizeof(uint32_t), dstData.data());

  EXPECT_EQ(dstData[0], 1u);
  EXPECT_EQ(dstData[1], 2u);
  EXPECT_EQ(dstData[2], 3u);
  EXPECT_EQ(dstData[3], 4u);
}

TEST_F(VulkanBufferTest, CreateBufferWithInvalidStorageConvertsToPrivate) {
  Result ret;
  auto buffer = iglDev_->createBuffer(
      BufferDesc{
          .type = BufferDesc::BufferTypeBits::Uniform,
          .length = 256,
          .storage = ResourceStorage::Invalid,
      },
      &ret);

  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(buffer, nullptr);

  auto& ctx = getVulkanContext();
  if (ctx.useStagingForBuffers_) {
    EXPECT_EQ(buffer->storage(), ResourceStorage::Private);
  } else {
    EXPECT_EQ(buffer->storage(), ResourceStorage::Shared);
  }
}

} // namespace igl::tests

#endif // IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_MACOSX || IGL_PLATFORM_LINUX
