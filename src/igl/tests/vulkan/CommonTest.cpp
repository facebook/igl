/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/vulkan/Common.h>

#include <igl/tests/util/device/TestDevice.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanTexture.h>

#if IGL_PLATFORM_WINDOWS || IGL_PLATFORM_ANDROID || IGL_PLATFORM_LINUX
namespace igl::tests {

//
// CommonTest
//
// Unit tests for Common.cpp
//

class CommonTest : public ::testing::Test {};

// getResultFromVkResult ***********************************************************************
TEST(CommonTest, GetResultFromVkResultTest) {
  EXPECT_TRUE(igl::vulkan::getResultFromVkResult(VK_SUCCESS).isOk());

  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_LAYER_NOT_PRESENT).code,
            Result::Code::Unimplemented);
  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_EXTENSION_NOT_PRESENT).code,
            Result::Code::Unimplemented);
  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_FEATURE_NOT_PRESENT).code,
            Result::Code::Unimplemented);

  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_INCOMPATIBLE_DRIVER).code,
            Result::Code::Unsupported);
  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_FORMAT_NOT_SUPPORTED).code,
            Result::Code::Unsupported);

  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_OUT_OF_HOST_MEMORY).code,
            Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_OUT_OF_DEVICE_MEMORY).code,
            Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_OUT_OF_POOL_MEMORY).code,
            Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(igl::vulkan::getResultFromVkResult(VK_ERROR_TOO_MANY_OBJECTS).code,
            Result::Code::ArgumentOutOfRange);
}

// setResultFrom ***********************************************************************
TEST(CommonTest, SetResultFromTest) {
  Result result;
  igl::vulkan::setResultFrom(&result, VK_SUCCESS);
  EXPECT_TRUE(result.isOk());

  igl::vulkan::setResultFrom(&result, VK_ERROR_LAYER_NOT_PRESENT);
  EXPECT_EQ(result.code, Result::Code::Unimplemented);
  igl::vulkan::setResultFrom(&result, VK_ERROR_EXTENSION_NOT_PRESENT);
  EXPECT_EQ(result.code, Result::Code::Unimplemented);
  igl::vulkan::setResultFrom(&result, VK_ERROR_FEATURE_NOT_PRESENT);
  EXPECT_EQ(result.code, Result::Code::Unimplemented);

  igl::vulkan::setResultFrom(&result, VK_ERROR_INCOMPATIBLE_DRIVER);
  EXPECT_EQ(result.code, Result::Code::Unsupported);
  igl::vulkan::setResultFrom(&result, VK_ERROR_FORMAT_NOT_SUPPORTED);
  EXPECT_EQ(result.code, Result::Code::Unsupported);

  igl::vulkan::setResultFrom(&result, VK_ERROR_OUT_OF_HOST_MEMORY);
  EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
  igl::vulkan::setResultFrom(&result, VK_ERROR_OUT_OF_DEVICE_MEMORY);
  EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
  igl::vulkan::setResultFrom(&result, VK_ERROR_OUT_OF_POOL_MEMORY);
  EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
  igl::vulkan::setResultFrom(&result, VK_ERROR_TOO_MANY_OBJECTS);
  EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
}

// stencilOperationToVkStencilOp ***********************************************************
TEST(CommonTest, StencilOperationToVkStencilOpTest) {
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::Keep),
            VK_STENCIL_OP_KEEP);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::Zero),
            VK_STENCIL_OP_ZERO);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::Replace),
            VK_STENCIL_OP_REPLACE);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::IncrementClamp),
            VK_STENCIL_OP_INCREMENT_AND_CLAMP);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::DecrementClamp),
            VK_STENCIL_OP_DECREMENT_AND_CLAMP);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::Invert),
            VK_STENCIL_OP_INVERT);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::IncrementWrap),
            VK_STENCIL_OP_INCREMENT_AND_WRAP);
  EXPECT_EQ(igl::vulkan::stencilOperationToVkStencilOp(igl::StencilOperation::DecrementWrap),
            VK_STENCIL_OP_DECREMENT_AND_WRAP);
}

// compareFunctionToVkCompareOp ********************************************************
TEST(CommonTest, CompareFunctionToVkCompareOpTest) {
  EXPECT_EQ(igl::vulkan::compareFunctionToVkCompareOp(igl::CompareFunction::Never),
            VK_COMPARE_OP_NEVER);
  EXPECT_EQ(igl::vulkan::compareFunctionToVkCompareOp(igl::CompareFunction::Less),
            VK_COMPARE_OP_LESS);
  EXPECT_EQ(igl::vulkan::compareFunctionToVkCompareOp(igl::CompareFunction::Equal),
            VK_COMPARE_OP_EQUAL);
  EXPECT_EQ(igl::vulkan::compareFunctionToVkCompareOp(igl::CompareFunction::LessEqual),
            VK_COMPARE_OP_LESS_OR_EQUAL);
  EXPECT_EQ(igl::vulkan::compareFunctionToVkCompareOp(igl::CompareFunction::Greater),
            VK_COMPARE_OP_GREATER);
  EXPECT_EQ(igl::vulkan::compareFunctionToVkCompareOp(igl::CompareFunction::NotEqual),
            VK_COMPARE_OP_NOT_EQUAL);
  EXPECT_EQ(igl::vulkan::compareFunctionToVkCompareOp(igl::CompareFunction::GreaterEqual),
            VK_COMPARE_OP_GREATER_OR_EQUAL);
  EXPECT_EQ(igl::vulkan::compareFunctionToVkCompareOp(igl::CompareFunction::AlwaysPass),
            VK_COMPARE_OP_ALWAYS);
}

// getVulkanSampleCountFlags **************************************************************
TEST(CommonTest, GetVulkanSampleCountFlagsTest) {
  EXPECT_EQ(igl::vulkan::getVulkanSampleCountFlags(1u), VK_SAMPLE_COUNT_1_BIT);
  EXPECT_EQ(igl::vulkan::getVulkanSampleCountFlags(2u), VK_SAMPLE_COUNT_2_BIT);
  EXPECT_EQ(igl::vulkan::getVulkanSampleCountFlags(4u), VK_SAMPLE_COUNT_4_BIT);
  EXPECT_EQ(igl::vulkan::getVulkanSampleCountFlags(8u), VK_SAMPLE_COUNT_8_BIT);
  EXPECT_EQ(igl::vulkan::getVulkanSampleCountFlags(16u), VK_SAMPLE_COUNT_16_BIT);
  EXPECT_EQ(igl::vulkan::getVulkanSampleCountFlags(32u), VK_SAMPLE_COUNT_32_BIT);
  EXPECT_EQ(igl::vulkan::getVulkanSampleCountFlags(64u), VK_SAMPLE_COUNT_64_BIT);
}

// atVkLayer *******************************************************************************
TEST(CommonTest, AtVkLayerTest) {
  const igl::TextureRangeDesc texRangeDesc = TextureRangeDesc::newCube(0, 0, 1, 1, 0, 1);
  constexpr uint32_t layerOrFaceId = 7;

  constexpr TextureType textureTypes[] = {
      TextureType::Invalid,
      TextureType::TwoD,
      TextureType::TwoDArray,
      TextureType::ThreeD,
      TextureType::Cube,
      TextureType::ExternalImage,
  };

  for (const auto textureType : textureTypes) {
    const auto newTexRangeDesc = igl::vulkan::atVkLayer(textureType, texRangeDesc, layerOrFaceId);

    EXPECT_EQ(newTexRangeDesc.face, textureType == TextureType::Cube ? layerOrFaceId : 0);
    EXPECT_EQ(newTexRangeDesc.layer, textureType == TextureType::Cube ? 0 : layerOrFaceId);
    EXPECT_EQ(newTexRangeDesc.numFaces, textureType == TextureType::Cube ? 1 : 6);
    EXPECT_EQ(newTexRangeDesc.x, texRangeDesc.x);
    EXPECT_EQ(newTexRangeDesc.y, texRangeDesc.y);
    EXPECT_EQ(newTexRangeDesc.z, texRangeDesc.z);
    EXPECT_EQ(newTexRangeDesc.width, texRangeDesc.width);
    EXPECT_EQ(newTexRangeDesc.height, texRangeDesc.height);
    EXPECT_EQ(newTexRangeDesc.depth, texRangeDesc.depth);
    EXPECT_EQ(newTexRangeDesc.mipLevel, texRangeDesc.mipLevel);
    EXPECT_EQ(newTexRangeDesc.numMipLevels, texRangeDesc.numMipLevels);
    EXPECT_EQ(newTexRangeDesc.numLayers, 1);
  }
}

// invertRedAndBlue **************************************************************************
TEST(CommonTest, InvertRedAndBlueTest) {
  EXPECT_EQ(igl::vulkan::invertRedAndBlue(VK_FORMAT_B8G8R8A8_UNORM), VK_FORMAT_R8G8B8A8_UNORM);
  EXPECT_EQ(igl::vulkan::invertRedAndBlue(VK_FORMAT_R8G8B8A8_UNORM), VK_FORMAT_B8G8R8A8_UNORM);
  EXPECT_EQ(igl::vulkan::invertRedAndBlue(VK_FORMAT_R8G8B8A8_SRGB), VK_FORMAT_B8G8R8A8_SRGB);
  EXPECT_EQ(igl::vulkan::invertRedAndBlue(VK_FORMAT_B8G8R8A8_SRGB), VK_FORMAT_R8G8B8A8_SRGB);
  EXPECT_EQ(igl::vulkan::invertRedAndBlue(VK_FORMAT_A2R10G10B10_UNORM_PACK32),
            VK_FORMAT_A2B10G10R10_UNORM_PACK32);
  EXPECT_EQ(igl::vulkan::invertRedAndBlue(VK_FORMAT_A2B10G10R10_UNORM_PACK32),
            VK_FORMAT_A2R10G10B10_UNORM_PACK32);
}

// isTextureFormatRGB / isTextureFormatBGR ***************************************************
TEST(CommonTest, IsTextureFormatRGBTest) {
  EXPECT_TRUE(igl::vulkan::isTextureFormatRGB(VK_FORMAT_R8G8B8A8_UNORM));
  EXPECT_TRUE(igl::vulkan::isTextureFormatRGB(VK_FORMAT_R8G8B8A8_SRGB));
  EXPECT_TRUE(igl::vulkan::isTextureFormatRGB(VK_FORMAT_A2R10G10B10_UNORM_PACK32));
  EXPECT_FALSE(igl::vulkan::isTextureFormatRGB(VK_FORMAT_B8G8R8A8_UNORM));
  EXPECT_FALSE(igl::vulkan::isTextureFormatRGB(VK_FORMAT_B8G8R8A8_SRGB));
  EXPECT_FALSE(igl::vulkan::isTextureFormatRGB(VK_FORMAT_R16G16B16A16_SFLOAT));
}

TEST(CommonTest, IsTextureFormatBGRTest) {
  EXPECT_TRUE(igl::vulkan::isTextureFormatBGR(VK_FORMAT_B8G8R8A8_UNORM));
  EXPECT_TRUE(igl::vulkan::isTextureFormatBGR(VK_FORMAT_B8G8R8A8_SRGB));
  EXPECT_TRUE(igl::vulkan::isTextureFormatBGR(VK_FORMAT_A2B10G10R10_UNORM_PACK32));
  EXPECT_FALSE(igl::vulkan::isTextureFormatBGR(VK_FORMAT_R8G8B8A8_UNORM));
  EXPECT_FALSE(igl::vulkan::isTextureFormatBGR(VK_FORMAT_R8G8B8A8_SRGB));
  EXPECT_FALSE(igl::vulkan::isTextureFormatBGR(VK_FORMAT_R16G16B16A16_SFLOAT));
}

// hasDepth / hasStencil *********************************************************************
TEST(CommonTest, HasDepthTest) {
  EXPECT_TRUE(igl::vulkan::hasDepth(VK_FORMAT_D16_UNORM));
  EXPECT_TRUE(igl::vulkan::hasDepth(VK_FORMAT_D32_SFLOAT));
  EXPECT_TRUE(igl::vulkan::hasDepth(VK_FORMAT_D24_UNORM_S8_UINT));
  EXPECT_TRUE(igl::vulkan::hasDepth(VK_FORMAT_D32_SFLOAT_S8_UINT));
  EXPECT_FALSE(igl::vulkan::hasDepth(VK_FORMAT_S8_UINT));
  EXPECT_FALSE(igl::vulkan::hasDepth(VK_FORMAT_R8G8B8A8_UNORM));
}

TEST(CommonTest, HasStencilTest) {
  EXPECT_TRUE(igl::vulkan::hasStencil(VK_FORMAT_S8_UINT));
  EXPECT_TRUE(igl::vulkan::hasStencil(VK_FORMAT_D24_UNORM_S8_UINT));
  EXPECT_TRUE(igl::vulkan::hasStencil(VK_FORMAT_D32_SFLOAT_S8_UINT));
  EXPECT_FALSE(igl::vulkan::hasStencil(VK_FORMAT_D16_UNORM));
  EXPECT_FALSE(igl::vulkan::hasStencil(VK_FORMAT_D32_SFLOAT));
  EXPECT_FALSE(igl::vulkan::hasStencil(VK_FORMAT_R8G8B8A8_UNORM));
}

// getVkLayer ********************************************************************************
TEST(CommonTest, GetVkLayerTest) {
  EXPECT_EQ(igl::vulkan::getVkLayer(TextureType::Cube, 3, 0), 3);
  EXPECT_EQ(igl::vulkan::getVkLayer(TextureType::TwoD, 0, 5), 5);
  EXPECT_EQ(igl::vulkan::getVkLayer(TextureType::TwoDArray, 0, 7), 7);
  EXPECT_EQ(igl::vulkan::getVkLayer(TextureType::ThreeD, 0, 2), 2);
}

// getNumImagePlanes ************************************************************************
TEST(CommonTest, GetNumImagePlanesTest) {
  EXPECT_EQ(igl::vulkan::getNumImagePlanes(VK_FORMAT_UNDEFINED), 0);
  EXPECT_EQ(igl::vulkan::getNumImagePlanes(VK_FORMAT_G8_B8R8_2PLANE_420_UNORM), 2);
  EXPECT_EQ(igl::vulkan::getNumImagePlanes(VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM), 3);
  EXPECT_EQ(igl::vulkan::getNumImagePlanes(VK_FORMAT_R8G8B8A8_UNORM), 1);
  EXPECT_EQ(igl::vulkan::getNumImagePlanes(VK_FORMAT_R8G8B8A8_SRGB), 1);
  EXPECT_EQ(igl::vulkan::getNumImagePlanes(VK_FORMAT_R8G8B8A8_SINT), 1);
  EXPECT_EQ(igl::vulkan::getNumImagePlanes(VK_FORMAT_R8G8B8A8_UINT), 1);
}

// textureFormatToVkFormat / vkFormatToTextureFormat *******************************************
TEST(CommonTest, TextureFormatRoundTripTest) {
  const std::pair<TextureFormat, VkFormat> formats[] = {
      {TextureFormat::RGBA_UNorm8, VK_FORMAT_R8G8B8A8_UNORM},
      {TextureFormat::BGRA_UNorm8, VK_FORMAT_B8G8R8A8_UNORM},
      {TextureFormat::R_UNorm8, VK_FORMAT_R8_UNORM},
      {TextureFormat::RG_UNorm8, VK_FORMAT_R8G8_UNORM},
      {TextureFormat::RGBA_F16, VK_FORMAT_R16G16B16A16_SFLOAT},
      {TextureFormat::R_F32, VK_FORMAT_R32_SFLOAT},
      {TextureFormat::RGBA_F32, VK_FORMAT_R32G32B32A32_SFLOAT},
      {TextureFormat::RGBA_SRGB, VK_FORMAT_R8G8B8A8_SRGB},
      {TextureFormat::BGRA_SRGB, VK_FORMAT_B8G8R8A8_SRGB},
      {TextureFormat::Z_UNorm16, VK_FORMAT_D16_UNORM},
      {TextureFormat::Z_UNorm32, VK_FORMAT_D32_SFLOAT},
      {TextureFormat::S_UInt8, VK_FORMAT_S8_UINT},
  };
  for (const auto& [iglFormat, vkFormat] : formats) {
    EXPECT_EQ(igl::vulkan::textureFormatToVkFormat(iglFormat), vkFormat);
    EXPECT_EQ(igl::vulkan::vkFormatToTextureFormat(vkFormat), iglFormat);
  }
  EXPECT_EQ(igl::vulkan::textureFormatToVkFormat(TextureFormat::Invalid), VK_FORMAT_UNDEFINED);
}

// colorSpaceToVkColorSpace / vkColorSpaceToColorSpace ****************************************
TEST(CommonTest, ColorSpaceRoundTripTest) {
  const std::pair<ColorSpace, VkColorSpaceKHR> spaces[] = {
      {ColorSpace::SRGBNonlinear, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
      {ColorSpace::DisplayP3Nonlinear, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT},
      {ColorSpace::DisplayP3Linear, VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT},
      {ColorSpace::ExtendedSRGBLinear, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT},
      {ColorSpace::BT709Nonlinear, VK_COLOR_SPACE_BT709_NONLINEAR_EXT},
      {ColorSpace::BT2020Linear, VK_COLOR_SPACE_BT2020_LINEAR_EXT},
      {ColorSpace::HDR10St2084, VK_COLOR_SPACE_HDR10_ST2084_EXT},
      {ColorSpace::DolbyVision, VK_COLOR_SPACE_DOLBYVISION_EXT},
      {ColorSpace::HDR10HLG, VK_COLOR_SPACE_HDR10_HLG_EXT},
      {ColorSpace::AdobeRGBLinear, VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT},
      {ColorSpace::AdobeRGBNonlinear, VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT},
      {ColorSpace::PassThrough, VK_COLOR_SPACE_PASS_THROUGH_EXT},
      {ColorSpace::ExtendedSRGBNonlinear, VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT},
      {ColorSpace::DisplayNativeAMD, VK_COLOR_SPACE_DISPLAY_NATIVE_AMD},
  };
  for (const auto& [iglSpace, vkSpace] : spaces) {
    EXPECT_EQ(igl::vulkan::colorSpaceToVkColorSpace(iglSpace), vkSpace);
    EXPECT_EQ(igl::vulkan::vkColorSpaceToColorSpace(vkSpace), iglSpace);
  }
}

// resourceStorageToVkMemoryPropertyFlags *******************************************************
TEST(CommonTest, ResourceStoragePrivateReturnsDeviceLocal) {
  const VkMemoryPropertyFlags flags =
      igl::vulkan::resourceStorageToVkMemoryPropertyFlags(ResourceStorage::Private);
  EXPECT_EQ(flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

TEST(CommonTest, ResourceStorageSharedReturnsHostVisibleCoherent) {
  const VkMemoryPropertyFlags flags =
      igl::vulkan::resourceStorageToVkMemoryPropertyFlags(ResourceStorage::Shared);
  EXPECT_EQ(flags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

TEST(CommonTest, ResourceStorageManagedReturnsHostVisibleCoherent) {
  const VkMemoryPropertyFlags flags =
      igl::vulkan::resourceStorageToVkMemoryPropertyFlags(ResourceStorage::Managed);
  EXPECT_EQ(flags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

TEST(CommonTest, ResourceStorageMemorylessWithoutPropertiesReturnsDeviceLocal) {
  const VkMemoryPropertyFlags flags =
      igl::vulkan::resourceStorageToVkMemoryPropertyFlags(ResourceStorage::Memoryless, nullptr);
  EXPECT_EQ(flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

TEST(CommonTest, ResourceStorageMemorylessWithLazilyAllocated) {
  VkPhysicalDeviceMemoryProperties memProps = {};
  memProps.memoryTypeCount = 1;
  memProps.memoryTypes[0].propertyFlags =
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;

  const VkMemoryPropertyFlags flags =
      igl::vulkan::resourceStorageToVkMemoryPropertyFlags(ResourceStorage::Memoryless, &memProps);
  EXPECT_EQ(flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
}

TEST(CommonTest, ResourceStorageMemorylessWithoutLazilyAllocated) {
  VkPhysicalDeviceMemoryProperties memProps = {};
  memProps.memoryTypeCount = 1;
  memProps.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

  const VkMemoryPropertyFlags flags =
      igl::vulkan::resourceStorageToVkMemoryPropertyFlags(ResourceStorage::Memoryless, &memProps);
  EXPECT_EQ(flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

// componentMappingToVkComponentMapping *******************************************************
TEST(CommonTest, ComponentMappingToVkComponentMappingTest) {
  const ComponentMapping identity{};
  const VkComponentMapping vkIdentity = igl::vulkan::componentMappingToVkComponentMapping(identity);
  EXPECT_EQ(vkIdentity.r, VK_COMPONENT_SWIZZLE_IDENTITY);
  EXPECT_EQ(vkIdentity.g, VK_COMPONENT_SWIZZLE_IDENTITY);
  EXPECT_EQ(vkIdentity.b, VK_COMPONENT_SWIZZLE_IDENTITY);
  EXPECT_EQ(vkIdentity.a, VK_COMPONENT_SWIZZLE_IDENTITY);

  const ComponentMapping swizzled{.r = Swizzle_0, .g = Swizzle_1, .b = Swizzle_A, .a = Swizzle_R};
  const VkComponentMapping vkSwizzled = igl::vulkan::componentMappingToVkComponentMapping(swizzled);
  EXPECT_EQ(vkSwizzled.r, VK_COMPONENT_SWIZZLE_ZERO);
  EXPECT_EQ(vkSwizzled.g, VK_COMPONENT_SWIZZLE_ONE);
  EXPECT_EQ(vkSwizzled.b, VK_COMPONENT_SWIZZLE_A);
  EXPECT_EQ(vkSwizzled.a, VK_COMPONENT_SWIZZLE_R);
}

class CommonWithDeviceTest : public ::testing::Test {
 public:
  // Set up common resources.
  void SetUp() override {
    // Turn off debug break so unit tests can run
    igl::setDebugBreakEnabled(false);

    device_ = igl::tests::util::device::createTestDevice(igl::BackendType::Vulkan);
    ASSERT_TRUE(device_ != nullptr);
    auto& device = static_cast<igl::vulkan::Device&>(*device_);
    context_ = &device.getVulkanContext();
    ASSERT_TRUE(context_ != nullptr);
  }

 protected:
  std::shared_ptr<IDevice> device_;
  vulkan::VulkanContext* context_ = nullptr;
};

// transitionToGeneral ********************************************************
TEST_F(CommonWithDeviceTest, TransitionToGeneralTest) {
  Result result;

  const CommandQueueDesc queueDesc{};
  auto commandQueue = device_->createCommandQueue(queueDesc, &result);
  EXPECT_TRUE(result.isOk());

  const CommandBufferDesc cmdBufferDesc{};
  const auto cmdBuffer = commandQueue->createCommandBuffer(cmdBufferDesc, &result);
  EXPECT_TRUE(result.isOk());

  const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                 1,
                                                 1,
                                                 TextureDesc::TextureUsageBits::Sampled |
                                                     TextureDesc::TextureUsageBits::Storage);
  const auto texture = device_->createTexture(texDesc, &result);
  EXPECT_TRUE(result.isOk());

  igl::vulkan::transitionToGeneral(
      static_cast<const igl::vulkan::CommandBuffer*>(cmdBuffer.get())->getVkCommandBuffer(),
      texture.get());

  const igl::vulkan::Texture& tex = static_cast<igl::vulkan::Texture&>(*texture);
  const vulkan::VulkanImage& img = tex.getVulkanTexture().image;

  EXPECT_EQ(img.imageLayout_, VK_IMAGE_LAYOUT_GENERAL);
}

// transitionToColorAttachment ********************************************************
TEST_F(CommonWithDeviceTest, TransitionToColorAttachmentTest) {
  Result result;

  const CommandQueueDesc queueDesc{};
  auto commandQueue = device_->createCommandQueue(queueDesc, &result);
  EXPECT_TRUE(result.isOk());

  const CommandBufferDesc cmdBufferDesc{};
  const auto cmdBuffer = commandQueue->createCommandBuffer(cmdBufferDesc, &result);
  EXPECT_TRUE(result.isOk());

  const TextureDesc texDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 1, 1, TextureDesc::TextureUsageBits::Attachment);
  const auto texture = device_->createTexture(texDesc, &result);
  EXPECT_TRUE(result.isOk());

  igl::vulkan::transitionToColorAttachment(
      static_cast<const igl::vulkan::CommandBuffer*>(cmdBuffer.get())->getVkCommandBuffer(),
      texture.get());

  const igl::vulkan::Texture& tex = static_cast<igl::vulkan::Texture&>(*texture);
  const vulkan::VulkanImage& img = tex.getVulkanTexture().image;

  EXPECT_EQ(img.imageLayout_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

// transitionToShaderReadOnly ********************************************************
TEST_F(CommonWithDeviceTest, TransitionToShaderReadOnlyTest) {
  Result result;

  const CommandQueueDesc queueDesc{};
  auto commandQueue = device_->createCommandQueue(queueDesc, &result);
  EXPECT_TRUE(result.isOk());

  const CommandBufferDesc cmdBufferDesc{};
  const auto cmdBuffer = commandQueue->createCommandBuffer(cmdBufferDesc, &result);
  EXPECT_TRUE(result.isOk());

  const TextureDesc texDesc =
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 1, 1, TextureDesc::TextureUsageBits::Sampled);
  const auto texture = device_->createTexture(texDesc, &result);
  EXPECT_TRUE(result.isOk());

  igl::vulkan::transitionToShaderReadOnly(
      static_cast<const igl::vulkan::CommandBuffer*>(cmdBuffer.get())->getVkCommandBuffer(),
      texture.get());

  const igl::vulkan::Texture& tex = static_cast<igl::vulkan::Texture&>(*texture);
  const vulkan::VulkanImage& img = tex.getVulkanTexture().image;

  EXPECT_EQ(img.imageLayout_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// overrideImageLayout ********************************************************
TEST_F(CommonWithDeviceTest, OverrideImageLayoutTest) {
  Result result;

  const TextureDesc texDesc =
      TextureDesc::new2D(TextureFormat::RGBA_UNorm8, 1, 1, TextureDesc::TextureUsageBits::Sampled);
  const auto texture = device_->createTexture(texDesc, &result);
  EXPECT_TRUE(result.isOk());

  igl::vulkan::overrideImageLayout(texture.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  const igl::vulkan::Texture& tex = static_cast<igl::vulkan::Texture&>(*texture);
  const vulkan::VulkanImage& img = tex.getVulkanTexture().image;

  EXPECT_EQ(img.imageLayout_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
}

} // namespace igl::tests

#endif
