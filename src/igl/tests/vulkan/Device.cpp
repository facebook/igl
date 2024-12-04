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
#include <igl/vulkan/PlatformDevice.h>
#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanFeatures.h>
#include <igl/vulkan/VulkanTexture.h>
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

TEST_F(DeviceVulkanTest, PlatformDevice) {
  auto& platformDevice = iglDev_->getPlatformDevice();
  auto& vulkanPlatformDevice = static_cast<vulkan::PlatformDevice&>(platformDevice);
  Result ret;
  auto depthTexture = vulkanPlatformDevice.createTextureFromNativeDepth(2, 2, &ret);
  ASSERT_TRUE(ret.isOk());
  // ASSERT_TRUE(depthTexture != nullptr); // no swapchain so null
  auto texture = vulkanPlatformDevice.createTextureFromNativeDrawable(&ret);
  ASSERT_TRUE(ret.isOk());
  // ASSERT_TRUE(texture != nullptr); // no swapchain so null

  CommandQueueDesc desc{};

  desc.type = CommandQueueType::Graphics;

  auto cmdQueue = iglDev_->createCommandQueue(desc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(cmdQueue, nullptr);
  auto cmdBuf = cmdQueue->createCommandBuffer(CommandBufferDesc(), &ret);
  auto submitHandle = cmdQueue->submit(*cmdBuf);

  auto fence1 = vulkanPlatformDevice.getVkFenceFromSubmitHandle(submitHandle);
  ASSERT_NE(fence1, VK_NULL_HANDLE);

  vulkanPlatformDevice.waitOnSubmitHandle(submitHandle);
}

TEST_F(DeviceVulkanTest, PlatformDeviceSampler) {
  Result ret;
  TextureDesc textureDesc = TextureDesc::new2D(
      igl::TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Sampled);
  auto texture = iglDev_->createTexture(textureDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_TRUE(texture != nullptr);
  auto* vulkanTexture = static_cast<vulkan::Texture*>(texture.get());
  auto& innerVulkanTexture = vulkanTexture->getVulkanTexture();
  (void)innerVulkanTexture.imageView_;
  ASSERT_TRUE(innerVulkanTexture.textureId_ != 0);
  SamplerStateDesc samplerDesc;
  auto samplerState = iglDev_->createSamplerState(samplerDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  auto* vulkanSamplerState = static_cast<vulkan::SamplerState*>(samplerState.get());
  auto samplerId = vulkanSamplerState->getSamplerId();
  ASSERT_EQ(samplerId, 1);
  ASSERT_FALSE(vulkanSamplerState->isYUV());

  CommandQueueDesc cmdQueueDesc{};

  cmdQueueDesc.type = CommandQueueType::Graphics;

  auto cmdQueue = iglDev_->createCommandQueue(cmdQueueDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message.c_str();
  ASSERT_NE(cmdQueue, nullptr);
  auto cmdBuf = cmdQueue->createCommandBuffer(CommandBufferDesc(), &ret);
  cmdQueue->submit(*cmdBuf);
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

TEST_F(DeviceVulkanTest, DestroyEmptyHandles) {
  igl::destroy(iglDev_.get(), igl::BindGroupTextureHandle{});
  igl::destroy(iglDev_.get(), igl::BindGroupBufferHandle{});
  igl::destroy(iglDev_.get(), igl::TextureHandle{});
  igl::destroy(iglDev_.get(), igl::SamplerHandle{});
  igl::destroy(iglDev_.get(), igl::DepthStencilStateHandle{});
}

TEST_F(DeviceVulkanTest, CurrentThreadIdTest) {
  igl::vulkan::VulkanContext& ctx =
      static_cast<igl::vulkan::Device*>(iglDev_.get())->getVulkanContext();

  ctx.ensureCurrentContextThread();
}

TEST_F(DeviceVulkanTest, UpdateGlslangResource) {
  const igl::vulkan::VulkanContext& ctx =
      static_cast<const igl::vulkan::Device*>(iglDev_.get())->getVulkanContext();

  ivkUpdateGlslangResource(nullptr, nullptr);

  glslang_resource_t res = {};
  const VkPhysicalDeviceProperties& props = ctx.getVkPhysicalDeviceProperties();

  ivkUpdateGlslangResource(&res, &props);

  ASSERT_EQ(res.max_vertex_attribs, (int)props.limits.maxVertexInputAttributes);
  ASSERT_EQ(res.max_clip_distances, (int)props.limits.maxClipDistances);
  ASSERT_EQ(res.max_compute_work_group_count_x, (int)props.limits.maxComputeWorkGroupCount[0]);
  ASSERT_EQ(res.max_compute_work_group_count_y, (int)props.limits.maxComputeWorkGroupCount[1]);
  ASSERT_EQ(res.max_compute_work_group_count_z, (int)props.limits.maxComputeWorkGroupCount[2]);
  ASSERT_EQ(res.max_compute_work_group_size_x, (int)props.limits.maxComputeWorkGroupSize[0]);
  ASSERT_EQ(res.max_compute_work_group_size_y, (int)props.limits.maxComputeWorkGroupSize[1]);
  ASSERT_EQ(res.max_compute_work_group_size_z, (int)props.limits.maxComputeWorkGroupSize[2]);
  ASSERT_EQ(res.max_vertex_output_components, (int)props.limits.maxVertexOutputComponents);
  ASSERT_EQ(res.max_geometry_input_components, (int)props.limits.maxGeometryInputComponents);
  ASSERT_EQ(res.max_geometry_output_components, (int)props.limits.maxGeometryOutputComponents);
  ASSERT_EQ(res.max_fragment_input_components, (int)props.limits.maxFragmentInputComponents);
  ASSERT_EQ(res.max_geometry_output_vertices, (int)props.limits.maxGeometryOutputVertices);
  ASSERT_EQ(res.max_geometry_total_output_components,
            (int)props.limits.maxGeometryTotalOutputComponents);
  ASSERT_EQ(res.max_tess_control_input_components,
            (int)props.limits.maxTessellationControlPerVertexInputComponents);
  ASSERT_EQ(res.max_tess_control_output_components,
            (int)props.limits.maxTessellationControlPerVertexOutputComponents);
  ASSERT_EQ(res.max_tess_evaluation_input_components,
            (int)props.limits.maxTessellationEvaluationInputComponents);
  ASSERT_EQ(res.max_tess_evaluation_output_components,
            (int)props.limits.maxTessellationEvaluationOutputComponents);
  ASSERT_EQ(res.max_viewports, (int)props.limits.maxViewports);
  ASSERT_EQ(res.max_cull_distances, (int)props.limits.maxCullDistances);
  ASSERT_EQ(res.max_combined_clip_and_cull_distances,
            (int)props.limits.maxCombinedClipAndCullDistances);
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
    igl::vulkan::VulkanFeatures features(VK_API_VERSION_1_1, config);
    features.populateWithAvailablePhysicalDeviceFeatures(*ctx, (VkPhysicalDevice)devices[0].guid);

    const VkPhysicalDeviceBufferDeviceAddressFeaturesKHR& bdaFeatures =
        features.VkPhysicalDeviceBufferDeviceAddressFeaturesKHR_;
    if (!bdaFeatures.bufferDeviceAddress) {
      return;
    }

    const std::vector<const char*> extraDeviceExtensions;
    iglDev = igl::vulkan::HWDevice::create(std::move(ctx),
                                           devices[0],
                                           0, // width
                                           0, // height,
                                           0,
                                           nullptr,
                                           &features,
                                           "DeviceVulkanTest",
                                           &ret);

    if (!ret.isOk()) {
      iglDev = nullptr;
    }
  }

  const bool deviceSupportsBufferDeviceAddress =
      ret.message.empty() ||
      ret.message.find("VK_KHR_buffer_device_address is not supported") == std::string::npos;
  if (!deviceSupportsBufferDeviceAddress) {
    return;
  }

  EXPECT_TRUE(ret.isOk());
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
    igl::vulkan::VulkanFeatures features(VK_API_VERSION_1_1, config);
    features.populateWithAvailablePhysicalDeviceFeatures(*ctx, (VkPhysicalDevice)devices[0].guid);

    const VkPhysicalDeviceDescriptorIndexingFeaturesEXT& diFeatures =
        features.VkPhysicalDeviceDescriptorIndexingFeaturesEXT_;
    if (!diFeatures.shaderSampledImageArrayNonUniformIndexing ||
        !diFeatures.descriptorBindingUniformBufferUpdateAfterBind ||
        !diFeatures.descriptorBindingSampledImageUpdateAfterBind ||
        !diFeatures.descriptorBindingStorageImageUpdateAfterBind ||
        !diFeatures.descriptorBindingStorageBufferUpdateAfterBind ||
        !diFeatures.descriptorBindingUpdateUnusedWhilePending ||
        !diFeatures.descriptorBindingPartiallyBound || !diFeatures.runtimeDescriptorArray) {
      return;
    }

    const std::vector<const char*> extraDeviceExtensions;
    iglDev = igl::vulkan::HWDevice::create(std::move(ctx),
                                           devices[0],
                                           0, // width
                                           0, // height,
                                           0,
                                           nullptr,
                                           &features,
                                           "VulkanContext Test",
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
