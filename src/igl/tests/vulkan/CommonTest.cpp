/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanTexture.h>

#include <igl/tests/util/device/TestDevice.h>

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
  const vulkan::VulkanImage& img = tex.getVulkanTexture().image_;

  EXPECT_EQ(img.imageLayout_, VK_IMAGE_LAYOUT_GENERAL);
}
} // namespace igl::tests

#endif
