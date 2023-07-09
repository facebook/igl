/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/CommandBuffer.h>

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/ComputePipelineState.h>
#include <igl/vulkan/Framebuffer.h>
#include <igl/vulkan/RenderCommandEncoder.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanTexture.h>

namespace igl {
namespace vulkan {

CommandBuffer::CommandBuffer(VulkanContext& ctx) :
  ctx_(ctx), wrapper_(ctx_.immediate_->acquire()) {}

namespace {

void transitionColorAttachment(VkCommandBuffer buffer, const std::shared_ptr<ITexture>& colorTex) {
  // We really shouldn't get a null here, but just in case.
  if (!IGL_VERIFY(colorTex)) {
    return;
  }

  const auto& vkTex = static_cast<Texture&>(*colorTex);
  const auto& colorImg = vkTex.getVulkanTexture().getVulkanImage();
  if (IGL_UNEXPECTED(colorImg.isDepthFormat_ || colorImg.isStencilFormat_)) {
    IGL_ASSERT_MSG(false, "Color attachments cannot have depth/stencil formats");
    return;
  }
  IGL_ASSERT_MSG(colorImg.imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid color attachment format");
  colorImg.transitionLayout(
      buffer,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for all subsequent fragment/compute shaders
      VkImageSubresourceRange{
          VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
}

} // namespace

std::unique_ptr<IRenderCommandEncoder> CommandBuffer::createRenderCommandEncoder(
    const RenderPassDesc& renderPass,
    std::shared_ptr<IFramebuffer> framebuffer,
    Result* outResult) {
  IGL_PROFILER_FUNCTION();
  IGL_ASSERT(framebuffer);

  framebuffer_ = framebuffer;

  // prepare all the color attachments
  const auto& indices = framebuffer->getColorAttachmentIndices();
  for (auto i : indices) {
    const auto colorTex = framebuffer->getColorAttachment(i);
    transitionColorAttachment(wrapper_.cmdBuf_, colorTex);
    // handle MSAA
    const auto colorResolveTex = framebuffer->getResolveColorAttachment(i);
    if (colorResolveTex) {
      transitionColorAttachment(wrapper_.cmdBuf_, colorResolveTex);
    }
  }

  // prepare depth attachment
  const auto depthTex = framebuffer->getDepthAttachment();
  if (depthTex) {
    const auto& vkDepthTex = static_cast<Texture&>(*depthTex);
    const auto& depthImg = vkDepthTex.getVulkanTexture().getVulkanImage();
    IGL_ASSERT_MSG(depthImg.imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid depth attachment format");
    const VkImageAspectFlags flags =
        vkDepthTex.getVulkanTexture().getVulkanImage().getImageAspectFlags();
    depthImg.transitionLayout(
        wrapper_.cmdBuf_,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // wait for subsequent fragment shaders
        VkImageSubresourceRange{flags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }

  auto encoder =
      RenderCommandEncoder::create(shared_from_this(), ctx_, renderPass, framebuffer, outResult);

  return encoder;
}

void CommandBuffer::present(std::shared_ptr<ITexture> surface) const {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(surface);

  const auto& vkTex = static_cast<Texture&>(*surface);
  const VulkanTexture& tex = vkTex.getVulkanTexture();
  const VulkanImage& img = tex.getVulkanImage();

  // prepare image for presentation
  if (vkTex.isSwapchainTexture()) {
    isFromSwapchain_ = true;
    // the image might be coming from a compute shader
    const VkPipelineStageFlagBits srcStage = (img.imageLayout_ == VK_IMAGE_LAYOUT_GENERAL)
                                                 ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                                 : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    img.transitionLayout(
        wrapper_.cmdBuf_,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        srcStage,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // wait for all subsequent operations
        VkImageSubresourceRange{
            VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
    return;
  }

  isFromSwapchain_ = false;

  // transition only non-multisampled images - MSAA images cannot be accessed from shaders
  if (img.samples_ == VK_SAMPLE_COUNT_1_BIT) {
    const VkImageAspectFlags flags =
        vkTex.getVulkanTexture().getVulkanImage().getImageAspectFlags();
    const VkPipelineStageFlags srcStage = vkTex.getProperties().isDepthOrStencil()
                                              ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                                              : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // set the result of the previous render pass
    img.transitionLayout(
        wrapper_.cmdBuf_,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        srcStage,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for subsequent fragment/compute shaders
        VkImageSubresourceRange{flags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }
}

void CommandBuffer::waitUntilCompleted() {
  ctx_.immediate_->wait(lastSubmitHandle_);

  lastSubmitHandle_ = VulkanImmediateCommands::SubmitHandle();
}

void CommandBuffer::bindComputePipelineState(
    const std::shared_ptr<IComputePipelineState>& pipelineState) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(pipelineState != nullptr)) {
    return;
  }

  const igl::vulkan::ComputePipelineState* cps =
      static_cast<igl::vulkan::ComputePipelineState*>(pipelineState.get());

  IGL_ASSERT(cps);

  VkPipeline pipeline = cps->getVkPipeline();

  if (lastPipelineBound_ != pipeline) {
    lastPipelineBound_ = pipeline;
    if (pipeline != VK_NULL_HANDLE) {
      vkCmdBindPipeline(wrapper_.cmdBuf_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    }
  }
}

void CommandBuffer::dispatchThreadGroups(const Dimensions& threadgroupCount) {
  ctx_.checkAndUpdateDescriptorSets();
  ctx_.bindDefaultDescriptorSets(wrapper_.cmdBuf_, VK_PIPELINE_BIND_POINT_COMPUTE);

  vkCmdDispatch(
      wrapper_.cmdBuf_, threadgroupCount.width, threadgroupCount.height, threadgroupCount.depth);
}

void CommandBuffer::pushDebugGroupLabel(const std::string& label,
                                                const igl::Color& color) const {
  IGL_ASSERT(!label.empty());

  ivkCmdBeginDebugUtilsLabel(wrapper_.cmdBuf_, label.c_str(), color.toFloatPtr());
}

void CommandBuffer::insertDebugEventLabel(const std::string& label,
                                                  const igl::Color& color) const {
  IGL_ASSERT(!label.empty());

  ivkCmdInsertDebugUtilsLabel(wrapper_.cmdBuf_, label.c_str(), color.toFloatPtr());
}

void CommandBuffer::popDebugGroupLabel() const {
  ivkCmdEndDebugUtilsLabel(wrapper_.cmdBuf_);
}

void CommandBuffer::useComputeTexture(const std::shared_ptr<ITexture>& texture) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(texture);
  const igl::vulkan::Texture* tex = static_cast<igl::vulkan::Texture*>(texture.get());
  const igl::vulkan::VulkanTexture& vkTex = tex->getVulkanTexture();
  const igl::vulkan::VulkanImage& vkImage = vkTex.getVulkanImage();
  if (!vkImage.isStorageImage()) {
    IGL_ASSERT_MSG(false, "Did you forget to specify TextureUsageBits::Storage on your texture?");
    return;
  }

  // "frame graph" heuristics: if we are already in VK_IMAGE_LAYOUT_GENERAL, wait for the previous
  // compute shader
  const VkPipelineStageFlags srcStage = (vkImage.imageLayout_ == VK_IMAGE_LAYOUT_GENERAL)
                                            ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                            : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  vkImage.transitionLayout(
      wrapper_.cmdBuf_,
      VK_IMAGE_LAYOUT_GENERAL,
      srcStage,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VkImageSubresourceRange{
          vkImage.getImageAspectFlags(), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
}

void CommandBuffer::bindPushConstants(size_t offset, const void* data, size_t length) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(length % 4 == 0); // VUID-vkCmdPushConstants-size-00369: size must be a multiple of 4

  // check push constant size is within max size
  const VkPhysicalDeviceLimits& limits = ctx_.getVkPhysicalDeviceProperties().limits;
  const size_t size = offset + length;
  if (!IGL_VERIFY(size <= limits.maxPushConstantsSize)) {
    LLOGW(
        "Push constants size exceeded %u (max %u bytes)", size, limits.maxPushConstantsSize);
  }

  vkCmdPushConstants(wrapper_.cmdBuf_,
                     ctx_.pipelineLayoutCompute_->getVkPipelineLayout(),
                     VK_SHADER_STAGE_COMPUTE_BIT,
                     (uint32_t)offset,
                     (uint32_t)length,
                     data);
}

} // namespace vulkan
} // namespace igl
