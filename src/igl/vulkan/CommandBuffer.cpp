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
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanTexture.h>

namespace igl {
namespace vulkan {

CommandBuffer::CommandBuffer(VulkanContext& ctx, CommandBufferDesc desc) :
  ctx_(ctx), wrapper_(ctx_.immediate_->acquire()), desc_(std::move(desc)) {
  IGL_ASSERT(wrapper_.cmdBuf_ != VK_NULL_HANDLE);
}

std::unique_ptr<IComputeCommandEncoder> CommandBuffer::createComputeCommandEncoder() {
  return std::make_unique<ComputeCommandEncoder>(shared_from_this(), ctx_);
}

namespace {

void transitionToColorAttachment(VkCommandBuffer buffer,
                                 const std::shared_ptr<ITexture>& colorTex) {
  // We really shouldn't get a null here, but just in case.
  if (!IGL_VERIFY(colorTex)) {
    return;
  }

  const auto& vkTex = static_cast<Texture&>(*colorTex);
  const auto& colorImg = vkTex.getVulkanTexture().getVulkanImage();
  if (IGL_UNEXPECTED(colorImg.isDepthFormat_ || colorImg.isStencilFormat_)) {
    IGL_ASSERT_MSG(false, "Color attachments cannot have depth/stencil formats");
    IGL_LOG_ERROR("Color attachments cannot have depth/stencil formats");
    return;
  }
  IGL_ASSERT_MSG(colorImg.imageFormat_ != VK_FORMAT_UNDEFINED, "Invalid color attachment format");
  if (!IGL_VERIFY((colorImg.usageFlags_ & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)) {
    IGL_ASSERT_MSG(false, "Did you forget to specify TextureUsageBit::Attachment usage bit?");
    IGL_LOG_ERROR("Did you forget to specify TextureUsageBit::Attachment usage bit?");
  }
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
    transitionToColorAttachment(wrapper_.cmdBuf_, colorTex);
    // handle MSAA
    const auto colorResolveTex = framebuffer->getResolveColorAttachment(i);
    if (colorResolveTex) {
      transitionToColorAttachment(wrapper_.cmdBuf_, colorResolveTex);
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

  if (ctx_.enhancedShaderDebuggingStore_) {
    encoder->binder().bindStorageBuffer(
        EnhancedShaderDebuggingStore::kBufferIndex,
        static_cast<igl::vulkan::Buffer*>(ctx_.enhancedShaderDebuggingStore_->vertexBuffer().get()),
        0);
  }

  return encoder;
}

void CommandBuffer::present(std::shared_ptr<ITexture> surface) const {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(surface);

  presentedSurface_ = surface;

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

void CommandBuffer::pushDebugGroupLabel(const std::string& label, const igl::Color& color) const {
  ivkCmdBeginDebugUtilsLabel(wrapper_.cmdBuf_, label.c_str(), color.toFloatPtr());
}

void CommandBuffer::popDebugGroupLabel() const {
  ivkCmdEndDebugUtilsLabel(wrapper_.cmdBuf_);
}

void CommandBuffer::waitUntilCompleted() {
  ctx_.immediate_->wait(lastSubmitHandle_);

  lastSubmitHandle_ = VulkanImmediateCommands::SubmitHandle();
}

void CommandBuffer::waitUntilScheduled() {}

std::shared_ptr<igl::IFramebuffer> CommandBuffer::getFramebuffer() const {
  return framebuffer_;
}

std::shared_ptr<ITexture> CommandBuffer::getPresentedSurface() const {
  return presentedSurface_;
}

} // namespace vulkan
} // namespace igl
