/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only
// @fb-only

#pragma once

#include <igl/android/NativeHWBuffer.h>

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

#include <igl/vulkan/Texture.h>

namespace igl::vulkan::android {

typedef void AHardwareBufferHelper;

// TextureBuffer encapsulates Vulkan textures
class NativeHWTextureBuffer : public igl::android::INativeHWTextureBuffer, public Texture {
  friend class PlatformDevice;
  using Super = Texture;

 public:
  NativeHWTextureBuffer(Device& device, TextureFormat format);
  ~NativeHWTextureBuffer() override;

  // Returns the context-owned conversion attached to this texture's image view, or VK_NULL_HANDLE
  // when none was created (RGB AHBs and defined-multi-plane AHB imports).
  // Callers must not destroy the returned handle, but still own their VkSampler lifetime.
  // The sampler must use the same conversion handle that created the image view.
  [[nodiscard]] VkSamplerYcbcrConversion getVkSamplerYcbcrConversion() const noexcept;

 protected:
  // Texture overrides
  Result create(const TextureDesc& desc) override;

  Result createTextureInternal(AHardwareBuffer* buffer) override;

  // Resolved usage contracts for a single AHB import: the VkImage usage flags for
  // vkCreateImage and the matching IGL TextureDesc usage advertised on the texture.
  struct AHBImportUsage {
    VkImageUsageFlags imageUsage = 0;
    TextureDesc::TextureUsage textureUsage = 0;
  };

  // Derives both usage contracts from the producer's AHB usage bits, the image aspect,
  // and the resolved external format (0 for defined-format images). Attachment usages
  // are dropped on the external-format path per VUID-VkImageCreateInfo-pNext-09457,
  // since IGL only samples such images and never enables externalFormatResolve.
  [[nodiscard]] static AHBImportUsage resolveAHBImportUsage(uint64_t ahbUsage,
                                                            VkImageAspectFlags aspectMask,
                                                            uint64_t externalFormat) noexcept;

  Result uploadInternal(TextureType type,
                        const TextureRangeDesc& range,
                        const void* IGL_NULLABLE data,
                        size_t bytesPerRow,
                        const uint32_t* IGL_NULLABLE mipLevelBytes) const override;
};

} // namespace igl::vulkan::android

#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
