/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "Framebuffer.h"

#include <igl/CommandBuffer.h>
#include <igl/RenderPass.h>
#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/CommandQueue.h>
#include <igl/vulkan/ComputePipelineState.h>
#include <igl/vulkan/DepthStencilState.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/PlatformDevice.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/ShaderModule.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VertexInputState.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanFramebuffer.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanStagingDevice.h>
#include <igl/vulkan/VulkanTexture.h>

namespace igl {
namespace vulkan {

std::vector<size_t> Framebuffer::getColorAttachmentIndices() const {
  std::vector<size_t> indices;

  indices.reserve(desc_.colorAttachments.size());

  for (const auto& attachment : desc_.colorAttachments) {
    indices.push_back(attachment.first);
  }

  return indices;
}

std::shared_ptr<igl::ITexture> Framebuffer::getColorAttachment(size_t index) const {
  const auto it = desc_.colorAttachments.find(index);

  return it != desc_.colorAttachments.end() ? it->second.texture : nullptr;
}

std::shared_ptr<ITexture> Framebuffer::getResolveColorAttachment(size_t index) const {
  const auto it = desc_.colorAttachments.find(index);

  return it != desc_.colorAttachments.end() ? it->second.resolveTexture : nullptr;
}

std::shared_ptr<igl::ITexture> Framebuffer::getDepthAttachment() const {
  return desc_.depthAttachment.texture;
}

std::shared_ptr<ITexture> Framebuffer::getResolveDepthAttachment() const {
  return desc_.depthAttachment.resolveTexture;
}

std::shared_ptr<igl::ITexture> Framebuffer::getStencilAttachment() const {
  return desc_.stencilAttachment.texture;
}

void Framebuffer::copyBytesColorAttachment(ICommandQueue& /* Not Used */,
                                           size_t index,
                                           void* pixelBytes,
                                           const TextureRangeDesc& range,
                                           size_t bytesPerRow) const {
  IGL_ASSERT_MSG(range.numFaces == 1, "range.numFaces MUST be 1");
  IGL_ASSERT_MSG(range.numLayers == 1, "range.numLayers MUST be 1");
  IGL_ASSERT_MSG(range.numMipLevels == 1, "range.numMipLevels MUST be 1");
  IGL_PROFILER_FUNCTION();
  if (!IGL_VERIFY(pixelBytes)) {
    return;
  }

  const auto& itexture = getColorAttachment(index);
  if (!IGL_VERIFY(itexture)) {
    return;
  }

  const auto& vkTex = static_cast<Texture&>(*itexture);
  const VkRect2D imageRegion = {
      VkOffset2D{static_cast<int32_t>(range.x), static_cast<int32_t>(range.y)},
      VkExtent2D{static_cast<uint32_t>(range.width), static_cast<uint32_t>(range.height)},
  };

  if (bytesPerRow == 0) {
    bytesPerRow = itexture->getProperties().getBytesPerRow(range);
  }
  // Vulkan uses array layer to represent either cube face or array layer. IGL's TextureRangeDesc
  // represents these separately. This gets the correct vulkan array layer for the either the
  // range's cube face or array layer.
  const auto layer = Texture::getVkLayer(
      itexture->getType(), static_cast<uint32_t>(range.face), static_cast<uint32_t>(range.layer));

  const VulkanContext& ctx = device_.getVulkanContext();
  ctx.stagingDevice_->getImageData2D(vkTex.getVkImage(),
                                     static_cast<uint32_t>(range.mipLevel),
                                     layer, // Layer is either cube face or array layer
                                     imageRegion,
                                     vkTex.getProperties(),
                                     VK_FORMAT_R8G8B8A8_UNORM,
                                     vkTex.getVulkanTexture().getVulkanImage().imageLayout_,
                                     pixelBytes,
                                     static_cast<uint32_t>(bytesPerRow),
                                     true); // Flip the image vertically
}

void Framebuffer::copyBytesDepthAttachment(ICommandQueue& /*cmdQueue*/,
                                           void* /*pixelBytes*/,
                                           const TextureRangeDesc& /*range*/,
                                           size_t /*bytesPerRow*/) const {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void Framebuffer::copyBytesStencilAttachment(ICommandQueue& /*cmdQueue*/,
                                             void* /*pixelBytes*/,
                                             const TextureRangeDesc& /*range*/,
                                             size_t /*bytesPerRow*/) const {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void Framebuffer::copyTextureColorAttachment(ICommandQueue& cmdQueue,
                                             size_t index,
                                             std::shared_ptr<ITexture> destTexture,
                                             const TextureRangeDesc& range) const {
  IGL_PROFILER_FUNCTION();
  // Currently doesn't support mipmaps
  if (!IGL_VERIFY(range.mipLevel == 0 && range.numMipLevels == 1)) {
    return;
  }

  // Extract the underlying VkCommandBuffer
  CommandBufferDesc cbDesc;
  std::shared_ptr<ICommandBuffer> buffer = cmdQueue.createCommandBuffer(cbDesc, nullptr);
  CommandBuffer& vulkanBuffer = static_cast<CommandBuffer&>(*buffer);
  VkCommandBuffer cmdBuf = vulkanBuffer.getVkCommandBuffer();

  const std::shared_ptr<igl::ITexture>& srcTexture = getColorAttachment(index);
  if (!IGL_VERIFY(srcTexture)) {
    return;
  }
  const igl::vulkan::Texture& srcVkTex = static_cast<Texture&>(*srcTexture);

  if (!IGL_VERIFY(destTexture)) {
    return;
  }
  const igl::vulkan::Texture& dstVkTex = static_cast<Texture&>(*destTexture);

  // 1. Transition dst into TRANSFER_DST_OPTIMAL
  ivkImageMemoryBarrier(cmdBuf,
                        dstVkTex.getVkImage(),
                        0, // srcAccessMask
                        VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask
                        VK_IMAGE_LAYOUT_UNDEFINED, // Discard content since we are writing
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // Don't wait for anything
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  // 2. Transition src into TRANSFER_SRC_OPTIMAL
  srcVkTex.getVulkanTexture().getVulkanImage().transitionLayout(
      cmdBuf,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // Wait for all previous operation to
                                            // be done
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
  // 3. Copy Image
  const VkImageCopy copy = ivkGetImageCopy2D(
      VkOffset2D{static_cast<int32_t>(range.x), static_cast<int32_t>(range.y)},
      VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
      VkExtent2D{static_cast<uint32_t>(range.width), static_cast<uint32_t>(range.height)});

  vkCmdCopyImage(cmdBuf,
                 srcVkTex.getVkImage(),
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 dstVkTex.getVkImage(),
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 1,
                 &copy);

  // 4. Transition images back
  srcVkTex.getVulkanTexture().getVulkanImage().transitionLayout(
      cmdBuf,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT, // Wait for Copy to be done
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // Don't start anything until Copy is
                                         // done
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
  dstVkTex.getVulkanTexture().getVulkanImage().transitionLayout(
      cmdBuf,
      dstVkTex.isSwapchainTexture() ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_PIPELINE_STAGE_TRANSFER_BIT, // Wait for vkCmdCopyImage()
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // Don't start anything until Copy is
                                         // done
      VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  cmdQueue.submit(*buffer);
}

std::shared_ptr<igl::ITexture> Framebuffer::updateDrawable(std::shared_ptr<ITexture> texture) {
  IGL_PROFILER_FUNCTION();

  if (!texture && getColorAttachment(0)) {
    desc_.colorAttachments.erase(0);
  }

  if (texture && getColorAttachment(0) != texture) {
    desc_.colorAttachments[0].texture = texture;
  }

  return texture;
}

Framebuffer::Framebuffer(const Device& device, FramebufferDesc desc) :
  device_(device), desc_(std::move(desc)) {
  IGL_PROFILER_FUNCTION();
  auto ensureSize = [this](const vulkan::Texture& tex) {
    const uint32_t attachmentWidth = static_cast<uint32_t>(tex.getDimensions().width);
    const uint32_t attachmentHeight = static_cast<uint32_t>(tex.getDimensions().height);

    IGL_ASSERT(attachmentWidth);
    IGL_ASSERT(attachmentHeight);

    // Initialize width/height
    if (!width_ || !height_) {
      width_ = attachmentWidth;
      height_ = attachmentHeight;
    } else {
      // We expect all subsequent color attachments to have the same size.
      IGL_ASSERT(width_ == attachmentWidth);
      IGL_ASSERT(height_ == attachmentHeight);
    }

    IGL_ASSERT_MSG(tex.getVkFormat() != VK_FORMAT_UNDEFINED, "Invalid texture format");
  };

  for (const auto& attachment : desc_.colorAttachments) {
    const auto& colorTexture = static_cast<vulkan::Texture&>(*attachment.second.texture);
    ensureSize(colorTexture);
  }

  const auto* depthTexture = static_cast<vulkan::Texture*>(desc_.depthAttachment.texture.get());

  if (depthTexture) {
    ensureSize(*depthTexture);
  }

  IGL_ASSERT(width_);
  IGL_ASSERT(height_);
}

VkFramebuffer Framebuffer::getVkFramebuffer(uint32_t mipLevel, VkRenderPass pass) const {
  IGL_PROFILER_FUNCTION();
  // Because Vulkan framebuffers are immutable and we have a method updateDrawable() which can
  // change an attachment, we have to maintain a collection of attachments and map it into a
  // VulkanFramebuffer via unordered_map. The vector of attachments is a key in the hash table.
  Attachments attachments;

  size_t largestIndexPlusOne = 0;
  for (const auto& attachment : desc_.colorAttachments) {
    largestIndexPlusOne = largestIndexPlusOne < attachment.first ? attachment.first
                                                                 : largestIndexPlusOne;
  }

  largestIndexPlusOne += 1;

  for (size_t i = 0; i < largestIndexPlusOne; ++i) {
    auto it = desc_.colorAttachments.find(i);
    // skip invalid attachments
    if (it == desc_.colorAttachments.end()) {
      continue;
    }
    IGL_ASSERT(it->second.texture);

    const auto& colorTexture = static_cast<vulkan::Texture&>(*it->second.texture);
    attachments.attachments_.push_back(
        colorTexture.getVkImageViewForFramebuffer(mipLevel, desc_.mode));
    // handle color MSAA
    if (it->second.resolveTexture) {
      IGL_ASSERT(mipLevel == 0);
      const auto& colorResolveTexture = static_cast<vulkan::Texture&>(*it->second.resolveTexture);
      attachments.attachments_.push_back(
          colorResolveTexture.getVkImageViewForFramebuffer(0, desc_.mode));
    }
  }
  // depth
  {
    const auto* depthTexture = static_cast<vulkan::Texture*>(desc_.depthAttachment.texture.get());
    if (depthTexture) {
      attachments.attachments_.push_back(depthTexture->getVkImageViewForFramebuffer(0, desc_.mode));
    }
  }
  // handle depth MSAA
  {
    const auto* depthResolveTexture =
        static_cast<vulkan::Texture*>(desc_.depthAttachment.resolveTexture.get());
    if (depthResolveTexture) {
      attachments.attachments_.push_back(
          depthResolveTexture->getVkImageViewForFramebuffer(0, desc_.mode));
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
                                                          uint32_t numClearValues,
                                                          const VkClearValue* clearValues) const {
  VkRenderPassBeginInfo bi = {};
  bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  bi.pNext = nullptr;
  bi.renderPass = renderPass;
  bi.framebuffer = getVkFramebuffer(mipLevel, renderPass);
  bi.renderArea =
      VkRect2D{VkOffset2D{0, 0},
               VkExtent2D{std::max(width_ >> mipLevel, 1u), std::max(height_ >> mipLevel, 1u)}};
  bi.clearValueCount = numClearValues;
  bi.pClearValues = clearValues;
  return bi;
}

} // namespace vulkan
} // namespace igl
