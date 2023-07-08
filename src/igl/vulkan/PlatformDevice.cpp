/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/PlatformDevice.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanSwapchain.h>

namespace igl {
namespace vulkan {

PlatformDevice::PlatformDevice(Device& device) : device_(device) {}

std::shared_ptr<ITexture> PlatformDevice::createTextureFromNativeDrawable(Result* outResult) {
  IGL_PROFILER_FUNCTION();

  const auto& ctx = device_.getVulkanContext();
  auto& swapChain = ctx.swapchain_;

  if (!IGL_VERIFY(ctx.hasSwapchain())) {
    nativeDrawableTextures_.clear();

    Result::setResult(outResult, Result::Code::InvalidOperation, "No Swapchain has been allocated");
    return nullptr;
  };

  std::shared_ptr<VulkanTexture> vkTex = swapChain->getCurrentVulkanTexture();

  if (!IGL_VERIFY(vkTex != nullptr)) {
    Result::setResult(outResult, Result::Code::InvalidOperation, "Swapchain has no valid texture");
    return nullptr;
  }

  IGL_ASSERT_MSG(vkTex->getVulkanImage().imageFormat_ != VK_FORMAT_UNDEFINED,
                 "Invalid image format");

  const auto iglFormat = vkFormatToTextureFormat(vkTex->getVulkanImage().imageFormat_);
  if (!IGL_VERIFY(iglFormat != igl::TextureFormat::Invalid)) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Invalid surface color format");
    return nullptr;
  }

  const auto width = (size_t)swapChain->getWidth();
  const auto height = (size_t)swapChain->getHeight();
  const auto currentImageIndex = swapChain->getCurrentImageIndex();

  // resize nativeDrawableTextures_ pushing null pointers
  // null pointers will be allocated later as needed
  if (currentImageIndex >= nativeDrawableTextures_.size()) {
    nativeDrawableTextures_.resize((size_t)currentImageIndex + 1, nullptr);
  }

  const auto result = nativeDrawableTextures_[currentImageIndex];

  // allocate new drawable textures if its null or mismatches in size or format
  if (!result || width != result->getDimensions().width ||
      height != result->getDimensions().height || iglFormat != result->getFormat()) {
    const TextureDesc desc = TextureDesc::new2D(
        iglFormat, width, height, TextureDesc::TextureUsageBits::Attachment, "SwapChain Texture");
    nativeDrawableTextures_[currentImageIndex] =
        std::make_shared<igl::vulkan::Texture>(device_, std::move(vkTex), desc);
  }

  Result::setResult(outResult, Result::Code::Ok);

  return nativeDrawableTextures_[currentImageIndex];
}

} // namespace vulkan
} // namespace igl
