/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "ITextureAccessor.h"
#include <igl/CommandQueue.h>
#include <igl/IGL.h>
#include <igl/Texture.h>
#if IGL_BACKEND_VULKAN
#include <igl/vulkan/VulkanImage.h>
#endif

namespace iglu::textureaccessor {

class VulkanTextureAccessor : public ITextureAccessor {
 public:
  explicit VulkanTextureAccessor(std::shared_ptr<igl::ITexture> texture);

  void requestBytes(igl::ICommandQueue& commandQueue,
                    std::shared_ptr<igl::ITexture> texture = nullptr) override;
  RequestStatus getRequestStatus() override;
  std::vector<unsigned char>& getBytes() override;
  size_t copyBytes(unsigned char* ptr, size_t length) override;

 private:
  void assignTexture(std::shared_ptr<igl::ITexture> texture);

 private:
  std::vector<unsigned char> latestBytesRead_;
  RequestStatus status_ = RequestStatus::NotInitialized;
#if IGL_BACKEND_VULKAN
  const igl::vulkan::VulkanContext* ctx_ = nullptr;
  VkImage vkImage_ = VK_NULL_HANDLE;
  VkFormat vkImageFormat_ = VK_FORMAT_UNDEFINED;
  VkImageLayout vkImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageAspectFlags vkImageAspectFlags_ = 0;
  uint32_t textureWidth_ = 0;
  uint32_t textureHeight_ = 0;
  size_t bytesPerRow_ = 0;
#endif
  size_t numBytesRequired_ = 0;
};
} // namespace iglu::textureaccessor
