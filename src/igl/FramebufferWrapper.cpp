/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/FramebufferWrapper.h>

namespace igl {

FramebufferWrapper::FramebufferWrapper(std::shared_ptr<IFramebuffer> framebuffer) :
  framebuffer_(std::move(framebuffer)) {}

base::IAttachmentInterop* IGL_NULLABLE FramebufferWrapper::getColorAttachment(size_t index) const {
  if (!framebuffer_) {
    return nullptr;
  }
  return framebuffer_->getColorAttachment(index).get();
}

base::IAttachmentInterop* IGL_NULLABLE FramebufferWrapper::getDepthAttachment() const {
  if (!framebuffer_) {
    return nullptr;
  }
  return framebuffer_->getDepthAttachment().get();
}

void* IGL_NULLABLE FramebufferWrapper::getNativeFramebuffer() const {
  // This method can be overridden by the platform-specific implementation
  return nullptr;
}

} // namespace igl
