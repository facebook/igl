/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/FramebufferWrapper.h>

#include <igl/Macros.h>

namespace igl {

FramebufferWrapper::FramebufferWrapper(std::shared_ptr<IFramebuffer> framebuffer) :
  framebuffer_(std::move(framebuffer)) {}

base::IAttachmentInterop* IGL_NULLABLE FramebufferWrapper::getColorAttachment(size_t index) const {
  IGL_PROFILER_FUNCTION();
  if (!framebuffer_) {
    return nullptr;
  }
  return framebuffer_->getColorAttachment(index).get();
}

base::IAttachmentInterop* IGL_NULLABLE FramebufferWrapper::getDepthAttachment() const {
  IGL_PROFILER_FUNCTION();
  if (!framebuffer_) {
    return nullptr;
  }
  return framebuffer_->getDepthAttachment().get();
}

void* IGL_NULLABLE FramebufferWrapper::getNativeFramebuffer() const {
  // This method can be overridden by the platform-specific implementation
  return nullptr;
}

std::shared_ptr<ITexture> FramebufferWrapper::getColorAttachmentTexture(
    size_t index) const noexcept {
  IGL_PROFILER_FUNCTION();
  if (!framebuffer_) {
    return nullptr;
  }
  return framebuffer_->getColorAttachment(index);
}

std::shared_ptr<ITexture> FramebufferWrapper::getDepthAttachmentTexture() const noexcept {
  IGL_PROFILER_FUNCTION();
  if (!framebuffer_) {
    return nullptr;
  }
  return framebuffer_->getDepthAttachment();
}

} // namespace igl
