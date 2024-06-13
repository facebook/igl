/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>
#include <igl/vulkan/Common.h>
#include <vulkan/vulkan_core.h>

#ifdef __ANDROID__
#include <vulkan/vulkan_android.h>
#endif

namespace igl::tests {

//
// VulkanHelpersTest
//
// Unit tests for functions in VulkanHelpers.{h|c}.
//

// ivkGetDescriptorSetLayoutBinding **************************************************************

class DescriptorSetLayoutTest
  : public ::testing::TestWithParam<std::tuple<uint32_t, VkDescriptorType, uint32_t>> {};

TEST_P(DescriptorSetLayoutTest, GetDescriptorSetLayoutBinding) {
  const uint32_t binding = std::get<0>(GetParam());
  const VkDescriptorType descriptorType = std::get<1>(GetParam());
  const uint32_t count = std::get<2>(GetParam());

  const auto descSetLayoutBinding =
      ivkGetDescriptorSetLayoutBinding(binding, descriptorType, count);
  EXPECT_EQ(descSetLayoutBinding.binding, binding);
  EXPECT_EQ(descSetLayoutBinding.descriptorType, descriptorType);
  EXPECT_EQ(descSetLayoutBinding.descriptorCount, count);
  EXPECT_EQ(descSetLayoutBinding.stageFlags,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                VK_SHADER_STAGE_COMPUTE_BIT);
  EXPECT_EQ(descSetLayoutBinding.pImmutableSamplers, nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    DescriptorSetLayoutTest,
    ::testing::Combine(::testing::Values(0, 1, 2),
                       ::testing::Values(VK_DESCRIPTOR_TYPE_SAMPLER,
                                         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                         VK_DESCRIPTOR_TYPE_STORAGE_BUFFER),
                       ::testing::Values(0, 1, 2)),
    [](const testing::TestParamInfo<DescriptorSetLayoutTest::ParamType>& info) {
      const std::string name = "binding_" + std::to_string(std::get<0>(info.param)) +
                               "__descriptorType_" + std::to_string(std::get<1>(info.param)) +
                               "__count_" + std::to_string(std::get<2>(info.param));
      return name;
    });

// ivkGetAttachmentDescription **************************************************************

class AttachmentDescriptionTest
  : public ::testing::TestWithParam<std::tuple<VkFormat,
                                               VkAttachmentLoadOp,
                                               VkAttachmentStoreOp,
                                               VkImageLayout,
                                               VkImageLayout,
                                               VkSampleCountFlagBits>> {};

TEST_P(AttachmentDescriptionTest, GetAttachmentDescription) {
  const VkFormat format = std::get<0>(GetParam());
  const VkAttachmentLoadOp loadOp = std::get<1>(GetParam());
  const VkAttachmentStoreOp storeOp = std::get<2>(GetParam());
  const VkImageLayout initialLayout = std::get<3>(GetParam());
  const VkImageLayout finalLayout = std::get<4>(GetParam());
  const VkSampleCountFlagBits samples = std::get<5>(GetParam());

  const auto attachmentDescription =
      ivkGetAttachmentDescription(format, loadOp, storeOp, initialLayout, finalLayout, samples);
  EXPECT_EQ(attachmentDescription.flags, 0);
  EXPECT_EQ(attachmentDescription.format, format);
  EXPECT_EQ(attachmentDescription.samples, samples);
  EXPECT_EQ(attachmentDescription.loadOp, loadOp);
  EXPECT_EQ(attachmentDescription.storeOp, storeOp);
  EXPECT_EQ(attachmentDescription.stencilLoadOp, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
  EXPECT_EQ(attachmentDescription.stencilStoreOp, VK_ATTACHMENT_STORE_OP_DONT_CARE);
  EXPECT_EQ(attachmentDescription.initialLayout, initialLayout);
  EXPECT_EQ(attachmentDescription.finalLayout, finalLayout);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    AttachmentDescriptionTest,
    ::testing::Combine(
        ::testing::Values(VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8_SRGB),
        ::testing::Values(VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_LOAD),
        ::testing::Values(VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE),
        ::testing::Values(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        ::testing::Values(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        ::testing::Values(VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_4_BIT)),
    [](const testing::TestParamInfo<AttachmentDescriptionTest::ParamType>& info) {
      const std::string name = "format_" + std::to_string(std::get<0>(info.param)) + "__loadOp_" +
                               std::to_string(std::get<1>(info.param)) + "__storeOp_" +
                               std::to_string(std::get<2>(info.param)) + "__initialLayout_" +
                               std::to_string(std::get<3>(info.param)) + "__finalLayout_" +
                               std::to_string(std::get<4>(info.param)) + "__samples_" +
                               std::to_string(std::get<5>(info.param));
      return name;
    });

// ivkGetAttachmentReference **************************************************************

class AttachmentReferenceTest
  : public ::testing::TestWithParam<std::tuple<uint32_t, VkImageLayout>> {};

TEST_P(AttachmentReferenceTest, GetAttachmentDescription) {
  const uint32_t attachmentId = std::get<0>(GetParam());
  const VkImageLayout layout = std::get<1>(GetParam());

  const auto attachmentReference = ivkGetAttachmentReference(attachmentId, layout);
  EXPECT_EQ(attachmentReference.attachment, attachmentId);
  EXPECT_EQ(attachmentReference.layout, layout);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    AttachmentReferenceTest,
    ::testing::Combine(::testing::Values(0, 1),
                       ::testing::Values(VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)),
    [](const testing::TestParamInfo<AttachmentReferenceTest::ParamType>& info) {
      const std::string name = "attachment_" + std::to_string(std::get<0>(info.param)) +
                               "__layout_" + std::to_string(std::get<1>(info.param));
      return name;
    });

// ivkGetSubpassDescription **************************************************************

// Parameter list:
//   1. Number of attachments (total, including color, resolve, and depth)
//   2. MSAA enabled. Resolve attachments are created if true
//   3. Depth attachment present?
class SubpassDescriptionTest : public ::testing::TestWithParam<std::tuple<uint32_t, bool, bool>> {};

TEST_P(SubpassDescriptionTest, GetSubpassDescription) {
  const uint32_t numColorAttachments = std::get<0>(GetParam());
  const bool withResolveAttachments = std::get<1>(GetParam());
  const bool withDepthAttachment = std::get<2>(GetParam());

  std::vector<VkAttachmentReference> colorAttachmentReferences(numColorAttachments);
  std::vector<VkAttachmentReference> resolveAttachmentReferences(numColorAttachments);
  VkAttachmentReference depthAttachment =
      ivkGetAttachmentReference(numColorAttachments, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  for (uint32_t i = 0; i < numColorAttachments; ++i) {
    colorAttachmentReferences.emplace_back(
        ivkGetAttachmentReference(i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    if (withResolveAttachments) {
      resolveAttachmentReferences.emplace_back(
          ivkGetAttachmentReference(i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    }
  }

  const auto subpassDescription = ivkGetSubpassDescription(
      numColorAttachments,
      colorAttachmentReferences.data(),
      withResolveAttachments ? resolveAttachmentReferences.data() : nullptr,
      withDepthAttachment ? &depthAttachment : nullptr);

  EXPECT_EQ(subpassDescription.flags, 0);
  EXPECT_EQ(subpassDescription.pipelineBindPoint, VK_PIPELINE_BIND_POINT_GRAPHICS);
  EXPECT_EQ(subpassDescription.inputAttachmentCount, 0);
  EXPECT_EQ(subpassDescription.pInputAttachments, nullptr);
  EXPECT_EQ(subpassDescription.colorAttachmentCount, numColorAttachments);
  EXPECT_EQ(subpassDescription.pColorAttachments, colorAttachmentReferences.data());
  EXPECT_EQ(subpassDescription.pResolveAttachments,
            withResolveAttachments ? resolveAttachmentReferences.data() : nullptr);
  EXPECT_EQ(subpassDescription.pDepthStencilAttachment,
            withDepthAttachment ? &depthAttachment : nullptr);
  EXPECT_EQ(subpassDescription.preserveAttachmentCount, 0);
  EXPECT_EQ(subpassDescription.pPreserveAttachments, nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    SubpassDescriptionTest,
    ::testing::Combine(::testing::Values(1, 2), ::testing::Bool(), ::testing::Bool()),
    [](const testing::TestParamInfo<SubpassDescriptionTest::ParamType>& info) {
      const std::string name = "numberOfAttachments_" + std::to_string(std::get<0>(info.param)) +
                               "__withResolveAttachment_" +
                               std::to_string(std::get<1>(info.param)) + "__withDepthAttachment_" +
                               std::to_string(std::get<2>(info.param));
      return name;
    });

// ivkGetSubpassDependency **************************************************************

class SubpassDependencyTest : public ::testing::Test {};

TEST_F(SubpassDependencyTest, GetAttachmentDescription) {
  const auto subpassDependency = ivkGetSubpassDependency();
  EXPECT_EQ(subpassDependency.srcSubpass, 0);
  EXPECT_EQ(subpassDependency.dstSubpass, VK_SUBPASS_EXTERNAL);
  EXPECT_EQ(subpassDependency.srcStageMask, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
  EXPECT_EQ(subpassDependency.dstStageMask, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  EXPECT_EQ(subpassDependency.srcAccessMask, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  EXPECT_EQ(subpassDependency.dstAccessMask, VK_ACCESS_SHADER_READ_BIT);
}

// ivkGetRenderPassMultiviewCreateInfo ***************************************************

class RenderPassMultiviewCreateInfoTest : public ::testing::Test {};

TEST_F(RenderPassMultiviewCreateInfoTest, GetRenderPassMultiviewCreateInfo) {
  constexpr uint32_t viewMask = 0;
  constexpr uint32_t correlationMask = 0;

  const auto renderPassMultiviewCreateInfo =
      ivkGetRenderPassMultiviewCreateInfo(&viewMask, &correlationMask);
  EXPECT_EQ(renderPassMultiviewCreateInfo.sType,
            VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO);
  EXPECT_EQ(renderPassMultiviewCreateInfo.pNext, nullptr);
  EXPECT_EQ(renderPassMultiviewCreateInfo.subpassCount, 1);
  EXPECT_EQ(renderPassMultiviewCreateInfo.pViewMasks, &viewMask);
  EXPECT_EQ(renderPassMultiviewCreateInfo.dependencyCount, 0);
  EXPECT_EQ(renderPassMultiviewCreateInfo.pViewOffsets, nullptr);
  EXPECT_EQ(renderPassMultiviewCreateInfo.correlationMaskCount, 1);
  EXPECT_EQ(renderPassMultiviewCreateInfo.pCorrelationMasks, &correlationMask);
}

// ivkGetClearColorValue ***************************************************

class ClearColorValueTest
  : public ::testing::TestWithParam<std::tuple<float, float, float, float>> {};

TEST_P(ClearColorValueTest, GetClearColorValue) {
  const float r = std::get<0>(GetParam());
  const float g = std::get<1>(GetParam());
  const float b = std::get<2>(GetParam());
  const float a = std::get<3>(GetParam());

  const auto clearValue = ivkGetClearColorValue(r, g, b, a);
  EXPECT_EQ(clearValue.color.float32[0], r);
  EXPECT_EQ(clearValue.color.float32[1], g);
  EXPECT_EQ(clearValue.color.float32[2], b);
  EXPECT_EQ(clearValue.color.float32[3], a);
}

INSTANTIATE_TEST_SUITE_P(AllCombinations,
                         ClearColorValueTest,
                         ::testing::Combine(::testing::Values(0.f, 1.0f),
                                            ::testing::Values(0.f, 1.0f),
                                            ::testing::Values(0.f, 1.0f),
                                            ::testing::Values(0.f, 1.0f)));

// ivkGetClearDepthStencilValue ***************************************************

class ClearDepthStencilValueTest : public ::testing::TestWithParam<std::tuple<float, uint32_t>> {};

TEST_P(ClearDepthStencilValueTest, GetClearDepthStencilValue) {
  const float depth = std::get<0>(GetParam());
  const uint32_t stencil = std::get<1>(GetParam());

  const auto clearValue = ivkGetClearDepthStencilValue(depth, stencil);
  EXPECT_EQ(clearValue.depthStencil.depth, depth);
  EXPECT_EQ(clearValue.depthStencil.stencil, stencil);
}

INSTANTIATE_TEST_SUITE_P(AllCombinations,
                         ClearDepthStencilValueTest,
                         ::testing::Combine(::testing::Values(0.f, 1.0f), ::testing::Values(0, 1)));

// ivkGetBufferCreateInfo ***************************************************

class BufferCreateInfoTest
  : public ::testing::TestWithParam<std::tuple<uint64_t, VkBufferUsageFlags>> {};

TEST_P(BufferCreateInfoTest, GetBufferCreateInfo) {
  const uint64_t size = std::get<0>(GetParam());
  const VkBufferUsageFlags usage = std::get<1>(GetParam());

  const auto bufferCreateInfo = ivkGetBufferCreateInfo(size, usage);
  EXPECT_EQ(bufferCreateInfo.sType, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
  EXPECT_EQ(bufferCreateInfo.pNext, nullptr);
  EXPECT_EQ(bufferCreateInfo.flags, 0);
  EXPECT_EQ(bufferCreateInfo.size, size);
  EXPECT_EQ(bufferCreateInfo.usage, usage);
  EXPECT_EQ(bufferCreateInfo.sharingMode, VK_SHARING_MODE_EXCLUSIVE);
  EXPECT_EQ(bufferCreateInfo.queueFamilyIndexCount, 0);
  EXPECT_EQ(bufferCreateInfo.pQueueFamilyIndices, nullptr);
}

INSTANTIATE_TEST_SUITE_P(AllCombinations,
                         BufferCreateInfoTest,
                         ::testing::Combine(::testing::Values(100, 1'000),
                                            ::testing::Values(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT)),
                         [](const testing::TestParamInfo<BufferCreateInfoTest::ParamType>& info) {
                           const std::string name =
                               "size_" + std::to_string(std::get<0>(info.param)) + "__usageFlags_" +
                               std::to_string(std::get<1>(info.param));
                           return name;
                         });

// ivkGetImageCreateInfo ***************************************************

class ImageCreateInfoTest : public ::testing::TestWithParam<std::tuple<VkImageType,
                                                                       VkFormat,
                                                                       VkImageTiling,
                                                                       VkImageUsageFlags,
                                                                       VkExtent3D,
                                                                       uint32_t,
                                                                       uint32_t,
                                                                       VkImageCreateFlags,
                                                                       VkSampleCountFlags>> {};

TEST_P(ImageCreateInfoTest, GetImageCreateInfo) {
  const VkImageType imageType = std::get<0>(GetParam());
  const VkFormat format = std::get<1>(GetParam());
  const VkImageTiling tiling = std::get<2>(GetParam());
  const VkImageUsageFlags usage = std::get<3>(GetParam());
  const VkExtent3D extent = std::get<4>(GetParam());
  const uint32_t mipLevels = std::get<5>(GetParam());
  const uint32_t arrayLayers = std::get<6>(GetParam());
  const VkImageCreateFlags flags = std::get<7>(GetParam());
  const VkSampleCountFlags samples = std::get<8>(GetParam());

  const auto imageCreateInfo = ivkGetImageCreateInfo(
      imageType, format, tiling, usage, extent, mipLevels, arrayLayers, flags, samples);
  EXPECT_EQ(imageCreateInfo.sType, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
  EXPECT_EQ(imageCreateInfo.pNext, nullptr);
  EXPECT_EQ(imageCreateInfo.flags, flags);
  EXPECT_EQ(imageCreateInfo.imageType, imageType);
  EXPECT_EQ(imageCreateInfo.format, format);
  EXPECT_EQ(imageCreateInfo.extent.width, extent.width);
  EXPECT_EQ(imageCreateInfo.extent.height, extent.height);
  EXPECT_EQ(imageCreateInfo.extent.depth, extent.depth);
  EXPECT_EQ(imageCreateInfo.mipLevels, mipLevels);
  EXPECT_EQ(imageCreateInfo.arrayLayers, arrayLayers);
  EXPECT_EQ(imageCreateInfo.samples, samples);
  EXPECT_EQ(imageCreateInfo.tiling, tiling);
  EXPECT_EQ(imageCreateInfo.sharingMode, VK_SHARING_MODE_EXCLUSIVE);
  EXPECT_EQ(imageCreateInfo.queueFamilyIndexCount, 0);
  EXPECT_EQ(imageCreateInfo.pQueueFamilyIndices, nullptr);
  EXPECT_EQ(imageCreateInfo.initialLayout, VK_IMAGE_LAYOUT_UNDEFINED);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    ImageCreateInfoTest,
    ::testing::Combine(::testing::Values(VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D),
                       ::testing::Values(VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_SRGB),
                       ::testing::Values(VK_IMAGE_TILING_LINEAR, VK_IMAGE_TILING_OPTIMAL),
                       ::testing::Values(VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_USAGE_STORAGE_BIT),
                       ::testing::Values(VkExtent3D{50, 50, 1}, VkExtent3D{100, 100, 1}),
                       ::testing::Values(1, 2),
                       ::testing::Values(1, 2),
                       ::testing::Values(0, VK_IMAGE_CREATE_SPARSE_BINDING_BIT),
                       ::testing::Values(VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_4_BIT)),
    [](const testing::TestParamInfo<ImageCreateInfoTest::ParamType>& info) {
      const std::string name = "imageType_" + std::to_string(std::get<0>(info.param)) +
                               "__format_" + std::to_string(std::get<1>(info.param)) + "__tiling_" +
                               std::to_string(std::get<2>(info.param)) + "__usage_" +
                               std::to_string(std::get<3>(info.param)) + "__extent_" +
                               std::to_string(std::get<4>(info.param).width) + "_" +
                               std::to_string(std::get<4>(info.param).height) + "_" +
                               std::to_string(std::get<4>(info.param).depth) + "__mipLevels_" +
                               std::to_string(std::get<5>(info.param)) + "__arrayLayers_" +
                               std::to_string(std::get<6>(info.param)) + "__flags_" +
                               std::to_string(std::get<7>(info.param)) + "__sampleCount_" +
                               std::to_string(std::get<8>(info.param));
      return name;
    });

// ivkGetPipelineVertexInputStateCreateInfo_Empty ***********************************************

class PipelineVertexInpusStateCreateInfoTest_Empty : public ::testing::Test {};

TEST_F(PipelineVertexInpusStateCreateInfoTest_Empty, GetPipelineVertexInputStateCreateInfo_Empty) {
  const VkPipelineVertexInputStateCreateInfo pipelineVertexInputCreateInfo =
      ivkGetPipelineVertexInputStateCreateInfo_Empty();

  EXPECT_EQ(pipelineVertexInputCreateInfo.sType,
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
  EXPECT_EQ(pipelineVertexInputCreateInfo.pNext, nullptr);
  EXPECT_EQ(pipelineVertexInputCreateInfo.flags, 0);
  EXPECT_EQ(pipelineVertexInputCreateInfo.vertexBindingDescriptionCount, 0);
  EXPECT_EQ(pipelineVertexInputCreateInfo.pVertexBindingDescriptions, nullptr);
  EXPECT_EQ(pipelineVertexInputCreateInfo.vertexAttributeDescriptionCount, 0);
  EXPECT_EQ(pipelineVertexInputCreateInfo.pVertexAttributeDescriptions, nullptr);
}

// ivkGetPipelineInputAssemblyStateCreateInfo ***************************************************

class PipelineInputAssemblyStateCreateInfoTest
  : public ::testing::TestWithParam<std::tuple<VkPrimitiveTopology, VkBool32>> {};

TEST_P(PipelineInputAssemblyStateCreateInfoTest, GetImageCreateInfo) {
  const VkPrimitiveTopology topology = std::get<0>(GetParam());
  const VkBool32 primitiveRestart = std::get<1>(GetParam());

  const auto pipelineInputAssemblyStateCreateInfo =
      ivkGetPipelineInputAssemblyStateCreateInfo(topology, primitiveRestart);
  EXPECT_EQ(pipelineInputAssemblyStateCreateInfo.sType,
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
  EXPECT_EQ(pipelineInputAssemblyStateCreateInfo.pNext, nullptr);
  EXPECT_EQ(pipelineInputAssemblyStateCreateInfo.flags, 0);
  EXPECT_EQ(pipelineInputAssemblyStateCreateInfo.topology, topology);
  EXPECT_EQ(pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable, primitiveRestart);
}

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    PipelineInputAssemblyStateCreateInfoTest,
    ::testing::Combine(::testing::Values(VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
                                         VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST),
                       ::testing::Values(VK_TRUE, VK_FALSE)),
    [](const testing::TestParamInfo<PipelineInputAssemblyStateCreateInfoTest::ParamType>& info) {
      const std::string name = "topology_" + std::to_string(std::get<0>(info.param)) +
                               "__primitiveType_" + std::to_string(std::get<1>(info.param));
      return name;
    });

} // namespace igl::tests
