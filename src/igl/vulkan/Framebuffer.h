/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_map>

#include <igl/Framebuffer.h>
#include <igl/vulkan/Common.h>

namespace igl {
namespace vulkan {

class Device;
class VulkanFramebuffer;

class Framebuffer final : public IFramebuffer {
 public:
  Framebuffer(const Device& device, FramebufferDesc desc);
  ~Framebuffer() override = default;

  // Accessors
  std::vector<size_t> getColorAttachmentIndices() const override;
  std::shared_ptr<ITexture> getColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getResolveColorAttachment(size_t index) const override;
  std::shared_ptr<ITexture> getDepthAttachment() const override;
  std::shared_ptr<ITexture> getResolveDepthAttachment() const override;
  std::shared_ptr<ITexture> getStencilAttachment() const override;

  // Methods
  void copyBytesColorAttachment(ICommandQueue& /* Not Used */,
                                size_t index,
                                void* pixelBytes,
                                const TextureRangeDesc& range,
                                size_t bytesPerRow = 0) const override;

  void copyBytesDepthAttachment(ICommandQueue& cmdQueue,
                                void* pixelBytes,
                                const TextureRangeDesc& range,
                                size_t bytesPerRow = 0) const override;

  void copyBytesStencilAttachment(ICommandQueue& cmdQueue,
                                  void* pixelBytes,
                                  const TextureRangeDesc& range,
                                  size_t bytesPerRow = 0) const override;

  void copyTextureColorAttachment(ICommandQueue& cmdQueue,
                                  size_t index,
                                  std::shared_ptr<ITexture> destTexture,
                                  const TextureRangeDesc& range) const override;

  std::shared_ptr<ITexture> updateDrawable(std::shared_ptr<ITexture> texture) override;

  VkFramebuffer getVkFramebuffer(uint32_t mipLevel, uint32_t layer, VkRenderPass pass) const;

  uint32_t getWidth() const {
    return width_;
  }
  uint32_t getHeight() const {
    return height_;
  }

  IGL_INLINE const FramebufferDesc& getDesc() const {
    return desc_;
  }

  VkRenderPassBeginInfo getRenderPassBeginInfo(VkRenderPass renderPass,
                                               uint32_t mipLevel,
                                               uint32_t layer,
                                               uint32_t numClearValues,
                                               const VkClearValue* clearValues) const;

  struct Attachments {
    std::vector<VkImageView> attachments_;
    bool operator==(const Attachments& other) const {
      return attachments_ == other.attachments_;
    }
  };

  struct HashFunction {
    uint64_t operator()(const Attachments& attachments) const;
  };

 private:
  const igl::vulkan::Device& device_;
  FramebufferDesc desc_; // attachments

  uint32_t width_ = 0;
  uint32_t height_ = 0;
  mutable std::unordered_map<Attachments, std::shared_ptr<VulkanFramebuffer>, HashFunction>
      framebuffers_;
};

} // namespace vulkan
} // namespace igl
