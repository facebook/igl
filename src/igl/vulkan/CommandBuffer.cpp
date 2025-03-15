/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/CommandBuffer.h>

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/ComputeCommandEncoder.h>
#include <igl/vulkan/EnhancedShaderDebuggingStore.h>
#include <igl/vulkan/Framebuffer.h>
#include <igl/vulkan/RenderCommandEncoder.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanTexture.h>

namespace igl::vulkan {

CommandBuffer::CommandBuffer(VulkanContext& ctx, CommandBufferDesc desc) :
  ctx_(ctx), wrapper_(ctx_.immediate_->acquire()), desc_(std::move(desc)) {
  IGL_DEBUG_ASSERT(wrapper_.cmdBuf_ != VK_NULL_HANDLE);
}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  return std::make_unique<ComputeCommandEncoder>(shared_from_this(), ctx_);
}

std::unique_ptr<IRenderCommandEncoder> CommandBuffer::createRenderCommandEncoder(
    const RenderPassDesc& renderPass,
    const std::shared_ptr<IFramebuffer>& framebuffer,
    const Dependencies& dependencies,
    Result* outResult) {
  IGL_PROFILER_FUNCTION();
  IGL_DEBUG_ASSERT(framebuffer);

  framebuffer_ = framebuffer;

  // prepare all the color attachments
  for (const auto i : framebuffer->getColorAttachmentIndices()) {
    ITexture* colorTex = framebuffer->getColorAttachment(i).get();
    transitionToColorAttachment(wrapper_.cmdBuf_, colorTex);
    // handle MSAA
    ITexture* colorResolveTex = framebuffer->getResolveColorAttachment(i).get();
    transitionToColorAttachment(wrapper_.cmdBuf_, colorResolveTex);
  }

  // prepare depth attachment
  const auto depthTex = framebuffer->getDepthAttachment();
  if (depthTex) {
    const auto& vkDepthTex = static_cast<Texture&>(*depthTex);
    const igl::vulkan::VulkanImage& depthImg = vkDepthTex.getVulkanTexture().image_;
    IGL_DEBUG_ASSERT(depthImg.imageFormat_ != VK_FORMAT_UNDEFINED,
                     "Invalid depth attachment format");
    const VkImageAspectFlags flags = vkDepthTex.getVulkanTexture().image_.getImageAspectFlags();
    depthImg.transitionLayout(
        wrapper_.cmdBuf_,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VkImageSubresourceRange{flags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }

  auto encoder = RenderCommandEncoder::create(
      shared_from_this(), ctx_, renderPass, framebuffer, dependencies, outResult);

  if (encoder && ctx_.enhancedShaderDebuggingStore_) {
    encoder->binder().bindBuffer(
        EnhancedShaderDebuggingStore::kBufferIndex,
        static_cast<Buffer*>(ctx_.enhancedShaderDebuggingStore_->vertexBuffer().get()),
        0,
        0);
  }

  return encoder;
}

void CommandBuffer::present(const std::shared_ptr<ITexture>& surface) const {
  IGL_PROFILER_FUNCTION();

  IGL_DEBUG_ASSERT(surface);

  presentedSurface_ = surface;

  const auto& vkTex = static_cast<Texture&>(*surface);
  const VulkanTexture& tex = vkTex.getVulkanTexture();
  const VulkanImage& img = tex.image_;

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
    const VkImageAspectFlags flags = vkTex.getVulkanTexture().image_.getImageAspectFlags();
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

void CommandBuffer::pushDebugGroupLabel(const char* label, const igl::Color& color) const {
  IGL_DEBUG_ASSERT(label != nullptr && *label);
  ivkCmdBeginDebugUtilsLabel(&ctx_.vf_, wrapper_.cmdBuf_, label, color.toFloatPtr());
}

void CommandBuffer::popDebugGroupLabel() const {
  ivkCmdEndDebugUtilsLabel(&ctx_.vf_, wrapper_.cmdBuf_);
}

void CommandBuffer::copyBuffer(IBuffer& src,
                               IBuffer& dst,
                               uint64_t srcOffset,
                               uint64_t dstOffset,
                               uint64_t size) {
  IGL_PROFILER_FUNCTION();

  auto& bufSrc = static_cast<Buffer&>(src);
  auto& bufDst = static_cast<Buffer&>(dst);

  ivkBufferBarrier(&ctx_.vf_,
                   wrapper_.cmdBuf_,
                   bufSrc.getVkBuffer(),
                   bufSrc.getBufferUsageFlags(),
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT);
  ivkBufferBarrier(&ctx_.vf_,
                   wrapper_.cmdBuf_,
                   bufDst.getVkBuffer(),
                   bufDst.getBufferUsageFlags(),
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT);

  const VkBufferCopy region = {
      .srcOffset = srcOffset,
      .dstOffset = dstOffset,
      .size = size,
  };

  ctx_.vf_.vkCmdCopyBuffer(
      wrapper_.cmdBuf_, bufSrc.getVkBuffer(), bufDst.getVkBuffer(), 1, &region);

  ivkBufferBarrier(&ctx_.vf_,
                   wrapper_.cmdBuf_,
                   bufSrc.getVkBuffer(),
                   bufSrc.getBufferUsageFlags(),
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  ivkBufferBarrier(&ctx_.vf_,
                   wrapper_.cmdBuf_,
                   bufDst.getVkBuffer(),
                   bufDst.getBufferUsageFlags(),
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

void CommandBuffer::copyTextureToBuffer(ITexture& src,
                                        IBuffer& dst,
                                        uint64_t dstOffset,
                                        uint32_t level,
                                        uint32_t layer) {
  auto& texSrc = static_cast<Texture&>(src);
  auto& bufDst = static_cast<Buffer&>(dst);

  VulkanImage& image = texSrc.getVulkanTexture().image_;

  const VkImageLayout oldLayout = image.imageLayout_;

  IGL_DEBUG_ASSERT(oldLayout != VK_IMAGE_LAYOUT_UNDEFINED);

  ivkBufferBarrier(&ctx_.vf_,
                   wrapper_.cmdBuf_,
                   bufDst.getVkBuffer(),
                   bufDst.getBufferUsageFlags(),
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT);

  const VkImageAspectFlags aspectMask =
      image.isDepthFormat_
          ? VK_IMAGE_ASPECT_DEPTH_BIT
          : (image.isStencilFormat_ ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT);

  const VkImageSubresourceRange range = {
      .aspectMask = aspectMask,
      .baseMipLevel = level,
      .levelCount = 1u,
      .baseArrayLayer = layer,
      .layerCount = texSrc.getNumFaces() == 6 ? 6u : 1u,
  };

  image.transitionLayout(wrapper_.cmdBuf_,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         range);

  const VkBufferImageCopy region = {
      .bufferOffset = dstOffset,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource =
          {
              .aspectMask = aspectMask,
              .mipLevel = level,
              .baseArrayLayer = layer,
              .layerCount = texSrc.getNumFaces() == 6 ? 6u : 1u,
          },
      .imageOffset = {},
      .imageExtent = image.extent_,
  };

  ctx_.vf_.vkCmdCopyImageToBuffer(wrapper_.cmdBuf_,
                                  texSrc.getVkImage(),
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  bufDst.getVkBuffer(),
                                  1u,
                                  &region);

  ivkBufferBarrier(&ctx_.vf_,
                   wrapper_.cmdBuf_,
                   bufDst.getVkBuffer(),
                   bufDst.getBufferUsageFlags(),
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

  image.transitionLayout(wrapper_.cmdBuf_,
                         oldLayout,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         range);
}

void CommandBuffer::waitUntilCompleted() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_WAIT);

  ctx_.immediate_->wait(lastSubmitHandle_, ctx_.config_.fenceTimeoutNanoseconds);

  lastSubmitHandle_ = VulkanImmediateCommands::SubmitHandle();
}

void CommandBuffer::waitUntilScheduled() {}

const std::shared_ptr<IFramebuffer>& CommandBuffer::getFramebuffer() const {
  return framebuffer_;
}

const std::shared_ptr<ITexture>& CommandBuffer::getPresentedSurface() const {
  return presentedSurface_;
}

} // namespace igl::vulkan
