/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Device.h>

#include <algorithm>
#include <utility>

namespace igl {

bool IDevice::defaultVerifyScope() {
  return scopeDepth_ > 0;
}

TextureDesc IDevice::sanitize(const TextureDesc& desc) const {
  TextureDesc sanitized = desc;
  if (desc.width == 0 || desc.height == 0 || desc.depth == 0 || desc.numLayers == 0 ||
      desc.numSamples == 0 || desc.numMipLevels == 0) {
    sanitized.width = std::max(sanitized.width, 1u);
    sanitized.height = std::max(sanitized.height, 1u);
    sanitized.depth = std::max(sanitized.depth, 1u);
    sanitized.numLayers = std::max(sanitized.numLayers, 1u);
    sanitized.numSamples = std::max(sanitized.numSamples, 1u);
    sanitized.numMipLevels = std::max(sanitized.numMipLevels, 1u);
    IGL_LOG_ERROR(
        "width (%u), height (%u), depth (%u), numLayers (%u), numSamples (%u) and numMipLevels "
        "(%u) should be at least 1.\n",
        desc.width,
        desc.height,
        desc.depth,
        desc.numLayers,
        desc.numSamples,
        desc.numMipLevels);
  }

  return sanitized;
}

Color IDevice::backendDebugColor() const noexcept {
  switch (getBackendType()) {
  case BackendType::Invalid:
    return {0.f, 0.f, 0.f, 0.f};
  case BackendType::OpenGL:
    return {1.f, 1.f, 0.f, 1.f};
  case BackendType::Metal:
    return {1.f, 0.f, 1.f, 1.f};
  case BackendType::Vulkan:
    return {0.f, 1.f, 1.f, 1.f};
  // @fb-only
    // @fb-only
  case BackendType::Custom:
    return {0.f, 0.f, 1.f, 1.f};
  }
  IGL_UNREACHABLE_RETURN(Color(0.f, 0.f, 0.f, 0.f))
}

DeviceScope::DeviceScope(IDevice& device) : device_(device) {
  device_.beginScope();
}

DeviceScope::~DeviceScope() {
  device_.endScope();
}

} // namespace igl
