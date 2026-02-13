/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/FramebufferWrapper.h>
#include <igl/opengl/Framebuffer.h>

namespace igl::opengl {

/// @brief OpenGL-specific FramebufferWrapper that returns the GL framebuffer ID
class FramebufferWrapper final : public igl::FramebufferWrapper {
 public:
  using igl::FramebufferWrapper::FramebufferWrapper;

  [[nodiscard]] void* IGL_NULLABLE getNativeFramebuffer() const override {
    const auto& fb = getFramebuffer();
    if (!fb) {
      return nullptr;
    }
    auto* oglFb = static_cast<Framebuffer*>(fb.get());
    nativeId_ = oglFb->getId();
    return &nativeId_;
  }

 private:
  mutable GLuint nativeId_ = 0;
};

} // namespace igl::opengl
