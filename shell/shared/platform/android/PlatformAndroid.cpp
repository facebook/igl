/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/platform/android/PlatformAndroid.h>

#include <shell/shared/fileLoader/android/FileLoaderAndroid.h>
#include <shell/shared/imageLoader/android/ImageLoaderAndroid.h>
#include <shell/shared/imageWriter/ImageWriter.h>
#include <shell/shared/imageWriter/android/ImageWriterAndroid.h>

#if IGL_BACKEND_VULKAN
#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/VulkanContext.h>
#endif

#include <glm/gtc/matrix_transform.hpp>

namespace igl::shell {

#if IGL_BACKEND_VULKAN
enum SurfaceTransformRotate {
  kSurfaceTransformRotate90 = 0,
  kSurfaceTransformRotate180,
  kSurfaceTransformRotate270,
  kSurfaceTransformRotateNum,
};
#endif

PlatformAndroid::PlatformAndroid(std::shared_ptr<igl::IDevice> device, bool useFakeLoader) :
  device_(std::move(device)) {
  fileLoader_ = std::make_unique<igl::shell::FileLoaderAndroid>();
  if (useFakeLoader) {
    imageLoader_ = std::make_unique<igl::shell::ImageLoader>(*fileLoader_);
  } else {
    imageLoader_ = std::make_unique<igl::shell::ImageLoaderAndroid>(*fileLoader_);
    imageWriter_ = std::make_unique<igl::shell::ImageWriterAndroid>();
  }

#if IGL_BACKEND_VULKAN
  static_assert(kSurfaceTransformRotateNum ==
                sizeof(surfaceTransformRotateMatrix_) / sizeof(surfaceTransformRotateMatrix_[0]));
  const float kRotateAngle[kSurfaceTransformRotateNum] = {-90, -180, -270};
  for (int i = 0; i != kSurfaceTransformRotateNum; ++i) {
    const glm::mat4x4 preRotateMat = glm::mat4x4(1.0f);
    const glm::vec3 rotationAxis = glm::vec3(0.0f, 0.0f, 1.0f);
    surfaceTransformRotateMatrix_[i] =
        glm::rotate(preRotateMat, glm::radians(kRotateAngle[i]), rotationAxis);
  }
#endif
}

igl::IDevice& PlatformAndroid::getDevice() noexcept {
  return *device_;
}

std::shared_ptr<igl::IDevice> PlatformAndroid::getDevicePtr() const noexcept {
  return device_;
}

ImageLoader& PlatformAndroid::getImageLoader() noexcept {
  return *imageLoader_;
}

const ImageWriter& PlatformAndroid::getImageWriter() const noexcept {
  return *imageWriter_;
}

FileLoader& PlatformAndroid::getFileLoader() const noexcept {
  return *fileLoader_;
}

const glm::mat4x4& PlatformAndroid::getPreRotationMatrix() const noexcept {
  static const glm::mat4x4 kIdentity(1.0f);
  if (!device_) {
    return kIdentity;
  }

  if (device_->getBackendType() != igl::BackendType::Vulkan) {
    return kIdentity;
  }

#if IGL_BACKEND_VULKAN
  igl::vulkan::Device& vulkanDevice = static_cast<igl::vulkan::Device&>(*device_);
  igl::vulkan::VulkanContext& context = vulkanDevice.getVulkanContext();
  const VkSurfaceCapabilitiesKHR& surfaceCapabilities = context.getSurfaceCapabilities();

  switch (surfaceCapabilities.currentTransform) {
  case VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR:
    return kIdentity;

  case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
    return surfaceTransformRotateMatrix_[kSurfaceTransformRotate90];

  case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
    return surfaceTransformRotateMatrix_[kSurfaceTransformRotate180];

  case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
    return surfaceTransformRotateMatrix_[kSurfaceTransformRotate270];

  default:
    break;
  }
#endif
  return kIdentity;
}

} // namespace igl::shell
