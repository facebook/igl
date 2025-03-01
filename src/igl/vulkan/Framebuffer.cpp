/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Framebuffer.h"

#include <igl/CommandBuffer.h>
#include <igl/RenderPass.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/CommandQueue.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/ComputePipelineState.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanFramebuffer.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanStagingDevice.h>
#include <igl/vulkan/VulkanTexture.h>

namespace igl::vulkan {

std::vector<size_t> Framebuffer::getColorAttachmentIndices() const {
  std::vector<size_t> indices;

  indices.reserve(IGL_COLOR_ATTACHMENTS_MAX);

  for (size_t i = 0; i != IGL_COLOR_ATTACHMENTS_MAX; i++) {
    if (desc_.colorAttachments[i].texture || desc_.colorAttachments[i].resolveTexture) {
      indices.push_back(i);
    }
  }

  return indices;
}

std::shared_ptr<ITexture> Framebuffer::getColorAttachment(size_t index) const {
  IGL_DEBUG_ASSERT(index < IGL_COLOR_ATTACHMENTS_MAX);
  return desc_.colorAttachments[index].texture;
}

std::shared_ptr<ITexture> Framebuffer::getResolveColorAttachment(size_t index) const {
  IGL_DEBUG_ASSERT(index < IGL_COLOR_ATTACHMENTS_MAX);
  return desc_.colorAttachments[index].resolveTexture;
}

std::shared_ptr<ITexture> Framebuffer::getDepthAttachment() const {
  return desc_.depthAttachment.texture;
}

std::shared_ptr<ITexture> Framebuffer::getResolveDepthAttachment() const {
  return desc_.depthAttachment.resolveTexture;
}

std::shared_ptr<ITexture> Framebuffer::getStencilAttachment() const {
  return desc_.stencilAttachment.texture;
}

void Framebuffer::copyBytesColorAttachment(ICommandQueue& /* Not Used */,
                                           size_t index,
                                           void* pixelBytes,
                                           const TextureRangeDesc& range,
                                           size_t bytesPerRow) const {
  IGL_DEBUG_ASSERT(range.numFaces == 1, "range.numFaces MUST be 1");
  IGL_DEBUG_ASSERT(range.numLayers == 1, "range.numLayers MUST be 1");
  IGL_DEBUG_ASSERT(range.numMipLevels == 1, "range.numMipLevels MUST be 1");
  IGL_PROFILER_FUNCTION();
  if (!IGL_DEBUG_VERIFY(pixelBytes)) {
    return;
  }

  const auto& itexture = getColorAttachment(index);
  if (!IGL_DEBUG_VERIFY(itexture)) {
    return;
  }

  // If we're doing MSAA, we should be using the resolve color attachment
  const auto& vkTex = static_cast<Texture&>(
      itexture->getSamples() == 1 ? *itexture : *getResolveColorAttachment(index));
  const VkRect2D imageRegion = {
      VkOffset2D{static_cast<int32_t>(range.x), static_cast<int32_t>(range.y)},
      VkExtent2D{range.width, range.height},
  };

  if (bytesPerRow == 0) {
    bytesPerRow = itexture->getProperties().getBytesPerRow(range);
  }
  // Vulkan uses array layer to represent either cube face or array layer. IGL's TextureRangeDesc
  // represents these separately. This gets the correct vulkan array layer for the either the
  // range's cube face or array layer.
  const auto layer = getVkLayer(itexture->getType(), range.face, range.layer);

  const VulkanContext& ctx = device_.getVulkanContext();
  ctx.stagingDevice_->getImageData2D(vkTex.getVkImage(),
                                     range.mipLevel,
                                     layer, // Layer is either cube face or array layer
                                     imageRegion,
                                     vkTex.getProperties(),
                                     VK_FORMAT_R8G8B8A8_UNORM,
                                     vkTex.getVulkanTexture().image_.imageLayout_,
                                     pixelBytes,
                                     static_cast<uint32_t>(bytesPerRow),
                                     true); // Flip the image vertically
}

void Framebuffer::copyBytesDepthAttachment(ICommandQueue& /*cmdQueue*/,
                                           void* /*pixelBytes*/,
                                           const TextureRangeDesc& /*range*/,
                                           size_t /*bytesPerRow*/) const {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
}

void Framebuffer::copyBytesStencilAttachment(ICommandQueue& /*cmdQueue*/,
                                             void* /*pixelBytes*/,
                                             const TextureRangeDesc& /*range*/,
                                             size_t /*bytesPerRow*/) const {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
}

void Framebuffer::copyTextureColorAttachment(ICommandQueue& cmdQueue,
                                             size_t index,
                                             std::shared_ptr<ITexture> destTexture,
                                             const TextureRangeDesc& range) const {
  IGL_PROFILER_FUNCTION();
  // Currently doesn't support mipmaps
  if (!IGL_DEBUG_VERIFY(range.mipLevel == 0 && range.numMipLevels == 1)) {
    return;
  }

  const auto& ctx = device_.getVulkanContext();

  // Extract the underlying VkCommandBuffer
  const CommandBufferDesc cbDesc;
  const std::shared_ptr<ICommandBuffer> buffer = cmdQueue.createCommandBuffer(cbDesc, nullptr);
  const auto& vulkanBuffer = static_cast<CommandBuffer&>(*buffer);
  VkCommandBuffer cmdBuf = vulkanBuffer.getVkCommandBuffer();

  const std::shared_ptr<ITexture>& srcTexture = getColorAttachment(index);
  if (!IGL_DEBUG_VERIFY(srcTexture)) {
    return;
  }
  // If we're doing MSAA, we should be using the resolve color attachment
  const igl::vulkan::Texture& srcVkTex = static_cast<Texture&>(
      srcTexture->getSamples() == 1 ? *srcTexture : *getResolveColorAttachment(index));

  if (!IGL_DEBUG_VERIFY(destTexture)) {
    return;
  }
  const igl::vulkan::Texture& dstVkTex = static_cast<Texture&>(*destTexture);

  // 1. Transition dst into TRANSFER_DST_OPTIMAL
  ivkImageMemoryBarrier(&ctx.vf_,
                        cmdBuf,
                        dstVkTex.getVkImage(),
                        0, // srcAccessMask
                        VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask
                        VK_IMAGE_LAYOUT_UNDEFINED, // Discard content since we are writing
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // Don't wait for anything
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  // 2. Transition src into TRANSFER_SRC_OPTIMAL
  srcVkTex.getVulkanTexture().image_.transitionLayout(
      cmdBuf,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // Wait for all previous operation to
                                            // be done
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
  // 3. Copy Image
  const VkImageCopy copy =
      ivkGetImageCopy2D(VkOffset2D{static_cast<int32_t>(range.x), static_cast<int32_t>(range.y)},
                        VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                        VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                        VkExtent2D{range.width, range.height});

  ctx.vf_.vkCmdCopyImage(cmdBuf,
                         srcVkTex.getVkImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         dstVkTex.getVkImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1,
                         &copy);

  // 4. Transition images back
  srcVkTex.getVulkanTexture().image_.transitionLayout(
      cmdBuf,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT, // Wait for Copy to be done
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // Don't start anything until Copy is
                                         // done
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
  dstVkTex.getVulkanTexture().image_.transitionLayout(
      cmdBuf,
      dstVkTex.isSwapchainTexture() ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT, // Wait for vkCmdCopyImage()
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // Don't start anything until Copy is
                                         // done
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  cmdQueue.submit(*buffer);
}

void Framebuffer::updateDrawable(std::shared_ptr<ITexture> texture) {
  updateDrawableInternal({std::move(texture), nullptr}, false);
}

void Framebuffer::updateDrawable(SurfaceTextures surfaceTextures) {
  updateDrawableInternal(std::move(surfaceTextures), true);
}

void Framebuffer::updateResolveAttachment(std::shared_ptr<ITexture> texture) {
  if (getColorAttachment(0) && getResolveColorAttachment(0) != texture) {
    desc_.colorAttachments[0].resolveTexture = std::move(texture);
    validateAttachments();
  }
}

void Framebuffer::updateDrawableInternal(SurfaceTextures surfaceTextures, bool updateDepthStencil) {
  IGL_PROFILER_FUNCTION();

  bool updated = false;
  if (getColorAttachment(0) != surfaceTextures.color) {
    if (!surfaceTextures.color) {
      desc_.colorAttachments[0] = {};
    } else {
      desc_.colorAttachments[0].texture = std::move(surfaceTextures.color);
    }
    updated = true;
  }

  if (updateDepthStencil) {
    if (surfaceTextures.depth && surfaceTextures.depth->getProperties().hasStencil()) {
      if (getStencilAttachment() != surfaceTextures.depth) {
        desc_.stencilAttachment.texture = surfaceTextures.depth;
        updated = true;
      }
    } else {
      desc_.stencilAttachment.texture = nullptr;
      updated = true;
    }
    if (getDepthAttachment() != surfaceTextures.depth) {
      desc_.depthAttachment.texture = std::move(surfaceTextures.depth);
      updated = true;
    }
  }
  if (updated) {
    validateAttachments();
  }
}

Framebuffer::Framebuffer(const Device& device, FramebufferDesc desc) :
  device_(device), desc_(std::move(desc)) {
  validateAttachments();
}

void Framebuffer::validateAttachments() {
  width_ = 0u;
  height_ = 0u;

  IGL_PROFILER_FUNCTION();
  auto ensureSize = [this](const vulkan::Texture& tex) {
    const uint32_t attachmentWidth = tex.getDimensions().width;
    const uint32_t attachmentHeight = tex.getDimensions().height;

    IGL_DEBUG_ASSERT(attachmentWidth);
    IGL_DEBUG_ASSERT(attachmentHeight);

    // Initialize width/height
    if (!width_ || !height_) {
      width_ = attachmentWidth;
      height_ = attachmentHeight;
    } else {
      // We expect all subsequent color attachments to have the same size.
      IGL_DEBUG_ASSERT(width_ == attachmentWidth);
      IGL_DEBUG_ASSERT(height_ == attachmentHeight);
    }

    IGL_DEBUG_ASSERT(tex.getVkFormat() != VK_FORMAT_UNDEFINED,
                     "Invalid texture format: %d",
                     static_cast<int>(tex.getVkFormat()));
  };

  for (const auto& attachment : desc_.colorAttachments) {
    if (!attachment.texture) {
      continue;
    }
    const auto& colorTexture = static_cast<Texture&>(*attachment.texture);
    ensureSize(colorTexture);
    IGL_DEBUG_ASSERT(
        (colorTexture.getUsage() & TextureDesc::TextureUsageBits::Attachment) != 0,
        "Did you forget to specify TextureUsageBits::Attachment on your color texture?");
  }

  const auto* depthTexture = static_cast<Texture*>(desc_.depthAttachment.texture.get());

  if (depthTexture) {
    ensureSize(*depthTexture);
    IGL_DEBUG_ASSERT(
        (depthTexture->getUsage() & TextureDesc::TextureUsageBits::Attachment) != 0,
        "Did you forget to specify TextureUsageBits::Attachment on your depth texture?");
  }

  IGL_DEBUG_ASSERT(width_);
  IGL_DEBUG_ASSERT(height_);
}

VkFramebuffer Framebuffer::getVkFramebuffer(uint32_t mipLevel,
                                            uint32_t layer,
                                            VkRenderPass pass) const {
  IGL_PROFILER_FUNCTION();
  // Because Vulkan framebuffers are immutable and we have a method updateDrawable() which can
  // change an attachment, we have to maintain a collection of attachments and map it into a
  // VulkanFramebuffer via unordered_map. The vector of attachments is a key in the hash table.
  Attachments attachments;

  for (const auto& colorAttachment : desc_.colorAttachments) {
    // skip invalid attachments
    if (!colorAttachment.texture) {
      continue;
    }
    IGL_DEBUG_ASSERT(colorAttachment.texture);

    const auto& colorTexture = static_cast<Texture&>(*colorAttachment.texture);
    attachments.attachments_.push_back(
        colorTexture.getVkImageViewForFramebuffer(mipLevel, layer, desc_.mode));
    // handle color MSAA
    if (colorAttachment.resolveTexture) {
      IGL_DEBUG_ASSERT(mipLevel == 0);
      const auto& colorResolveTexture = static_cast<Texture&>(*colorAttachment.resolveTexture);
      attachments.attachments_.push_back(
          colorResolveTexture.getVkImageViewForFramebuffer(0, layer, desc_.mode));
    }
  }
  // depth
  {
    const auto* depthTexture = static_cast<Texture*>(desc_.depthAttachment.texture.get());
    if (depthTexture) {
      attachments.attachments_.push_back(
          depthTexture->getVkImageViewForFramebuffer(mipLevel, layer, desc_.mode));
    }
  }
  // handle depth MSAA
  {
    const auto* depthResolveTexture =
        static_cast<Texture*>(desc_.depthAttachment.resolveTexture.get());
    if (depthResolveTexture) {
      attachments.attachments_.push_back(
          depthResolveTexture->getVkImageViewForFramebuffer(mipLevel, layer, desc_.mode));
    }
  }

  // now we can find a corresponding framebuffer
  auto it = framebuffers_.find(attachments);

  if (it != framebuffers_.end()) {
    return it->second->getVkFramebuffer();
  }

  const VulkanContext& ctx = device_.getVulkanContext();

  const uint32_t fbWidth = std::max(width_ >> mipLevel, 1u);
  const uint32_t fbHeight = std::max(height_ >> mipLevel, 1u);

  auto fb = std::make_shared<VulkanFramebuffer>(ctx,
                                                ctx.device_->getVkDevice(),
                                                fbWidth,
                                                fbHeight,
                                                pass,
                                                (uint32_t)attachments.attachments_.size(),
                                                attachments.attachments_.data(),
                                                desc_.debugName.c_str());

  framebuffers_[attachments] = fb;

  return fb->getVkFramebuffer();
}

uint64_t Framebuffer::HashFunction::operator()(const Attachments& attachments) const {
  uint64_t hash = 0;

  for (const auto& a : attachments.attachments_) {
    hash ^= std::hash<VkImageView>()(a);
  }

  return hash;
}

VkRenderPassBeginInfo Framebuffer::getRenderPassBeginInfo(VkRenderPass renderPass,
                                                          uint32_t mipLevel,
                                                          uint32_t layer,
                                                          uint32_t numClearValues,
                                                          const VkClearValue* clearValues) const {
  IGL_PROFILER_FUNCTION();

  VkRenderPassBeginInfo bi = {};
  bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  bi.pNext = nullptr;
  bi.renderPass = renderPass;
  bi.framebuffer = getVkFramebuffer(mipLevel, layer, renderPass);
  bi.renderArea =
      VkRect2D{VkOffset2D{0, 0},
               VkExtent2D{std::max(width_ >> mipLevel, 1u), std::max(height_ >> mipLevel, 1u)}};
  bi.clearValueCount = numClearValues;
  bi.pClearValues = clearValues;
  return bi;
}

FramebufferMode Framebuffer::getMode() const {
  return desc_.mode;
}

bool Framebuffer::isSwapchainBound() const {
  if (auto tex = getColorAttachment(0)) {
    return tex->isSwapchainTexture();
  }
  return false;
}

} // namespace igl::vulkan
