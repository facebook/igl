/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "TextureAccessorFactory.h"
#include "ITextureAccessor.h"
#include "OpenGLTextureAccessor.h"
#if IGL_BACKEND_VULKAN
#include "VulkanTextureAccessor.h"
#endif
#include <memory>
#if IGL_PLATFORM_APPLE
#include "MetalTextureAccessor.h"
#endif

namespace iglu::textureaccessor {

std::unique_ptr<ITextureAccessor> TextureAccessorFactory::createTextureAccessor(
    igl::BackendType backendType,
    const std::shared_ptr<igl::ITexture>& texture,
    igl::IDevice& device) {
  switch (backendType) {
#if IGL_BACKEND_OPENGL
  case igl::BackendType::OpenGL:
    return std::make_unique<OpenGLTextureAccessor>(texture, device);
#endif // IGL_BACKEND_OPENGL
#if IGL_PLATFORM_APPLE
  case igl::BackendType::Metal:
    return std::make_unique<MetalTextureAccessor>(texture, device);
#endif // IGL_PLATFORM_APPLE
#if IGL_BACKEND_VULKAN
  case igl::BackendType::Vulkan:
    return std::make_unique<VulkanTextureAccessor>(texture);
#endif
  default:
    IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
    return nullptr;
  }
}

} // namespace iglu::textureaccessor
