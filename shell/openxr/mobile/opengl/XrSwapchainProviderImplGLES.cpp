/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/openxr/mobile/opengl/XrSwapchainProviderImplGLES.h>

#include <algorithm>

#include <igl/opengl/Device.h>
#include <igl/opengl/PlatformDevice.h>
#include <igl/opengl/TextureBufferExternal.h>

#include <shell/openxr/XrLog.h>

#include <algorithm>
#include <iterator>

namespace igl::shell::openxr::mobile {
namespace {
void enumerateSwapchainImages(igl::IDevice& device,
                              XrSwapchain swapchain,
                              std::vector<uint32_t>& outImages) {
  uint32_t numImages = 0;
  XR_CHECK(xrEnumerateSwapchainImages(swapchain, 0, &numImages, nullptr));

  IGL_LOG_INFO("XRSwapchain numImages: %d\n", numImages);

#if IGL_WGL
  std::vector<XrSwapchainImageOpenGLKHR> xrImages(numImages,
                                                  {
                                                      .type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR,
                                                      .next = nullptr,
                                                  });
#else
  std::vector<XrSwapchainImageOpenGLESKHR> xrImages(
      numImages,
      {
          .type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR,
          .next = nullptr,
      });
#endif // IGL_WGL
  XR_CHECK(xrEnumerateSwapchainImages(
      swapchain, numImages, &numImages, (XrSwapchainImageBaseHeader*)xrImages.data()));

  outImages.resize(0);
  std::transform(xrImages.cbegin(),
                 xrImages.cend(),
                 std::back_inserter(outImages),
                 [](const auto& xrImage) { return xrImage.image; });
}

std::shared_ptr<igl::ITexture> getSurfaceTexture(
    igl::IDevice& device,
    const XrSwapchain& swapchain,
    const impl::SwapchainImageInfo& swapchainImageInfo,
    uint8_t numViews,
    const std::vector<uint32_t>& images,
    igl::TextureFormat externalTextureFormat,
    std::vector<std::shared_ptr<igl::ITexture>>& inOutTextures) {
  uint32_t imageIndex;
  XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  XR_CHECK(xrAcquireSwapchainImage(swapchain, &acquireInfo, &imageIndex));

  XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  waitInfo.timeout = XR_INFINITE_DURATION;
  XR_CHECK(xrWaitSwapchainImage(swapchain, &waitInfo));

  const auto glTexture = images[imageIndex];

  if (imageIndex >= inOutTextures.size()) {
    inOutTextures.resize(static_cast<size_t>(imageIndex) + 1, nullptr);
  }

  auto texture = inOutTextures[imageIndex];
  if (!texture || swapchainImageInfo.imageWidth != texture->getSize().width ||
      swapchainImageInfo.imageHeight != texture->getSize().height) {
    const auto platformDevice = device.getPlatformDevice<igl::opengl::PlatformDevice>();
    texture = platformDevice->createTextureBufferExternal(
        glTexture,
        numViews > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D,
        igl::TextureDesc::TextureUsageBits::Attachment,
        swapchainImageInfo.imageWidth,
        swapchainImageInfo.imageHeight,
        externalTextureFormat,
        numViews);
    if (!texture) {
      IGL_LOG_ERROR("Failed to create ITexture from swapchain image.\n");
      return {};
    }
    inOutTextures[imageIndex] = std::move(texture);
  }

  return inOutTextures[imageIndex];
}
} // namespace

void XrSwapchainProviderImplGLES::enumerateImages(
    igl::IDevice& device,
    XrSwapchain colorSwapchain,
    XrSwapchain depthSwapchain,
    const impl::SwapchainImageInfo& /* swapchainImageInfo */,
    uint8_t /* numViews */) noexcept {
  enumerateSwapchainImages(device, colorSwapchain, colorImages_);
  enumerateSwapchainImages(device, depthSwapchain, depthImages_);
}

igl::SurfaceTextures XrSwapchainProviderImplGLES::getSurfaceTextures(
    igl::IDevice& device,
    XrSwapchain colorSwapchain,
    XrSwapchain depthSwapchain,
    const impl::SwapchainImageInfo& swapchainImageInfo,
    uint8_t numViews) noexcept {
  // Assume sized format so format / type are not needed.
  auto iglColorFormat = igl::opengl::Texture::glInternalFormatToTextureFormat(
      static_cast<GLuint>(swapchainImageInfo.colorFormat), 0, 0);
  auto colorTexture = getSurfaceTexture(device,
                                        colorSwapchain,
                                        swapchainImageInfo,
                                        numViews,
                                        colorImages_,
                                        iglColorFormat,
                                        colorTextures_);

  auto iglDepthFormat = igl::opengl::Texture::glInternalFormatToTextureFormat(
      static_cast<GLuint>(swapchainImageInfo.depthFormat), 0, 0);
  auto depthTexture = getSurfaceTexture(device,
                                        depthSwapchain,
                                        swapchainImageInfo,
                                        numViews,
                                        depthImages_,
                                        iglDepthFormat,
                                        depthTextures_);

  return {colorTexture, depthTexture};
}
} // namespace igl::shell::openxr::mobile
