/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace igl {
class ITexture;
namespace opengl {
namespace wgl {

class Context : public IContext {
 public:
  /// Create a new context for current device context.
  Context();
  /// Create a new context from a given device and render contexts.
  Context(HDC deviceContext, HGLRC renderContext);
  /// Create a new context with existing HGLRC and share contexts the share context's
  /// must be setup ahead of calling this constructor. (eg. via QOpenGLContext->setShareContext) and
  /// should not be modified during the existence of this IContext
  Context(HDC deviceContext, HGLRC renderContext, std::vector<HGLRC> shareContexts);

  ~Context() override;

  void setCurrent() override;
  void clearCurrentContext() const override;
  bool isCurrentContext() const override;
  bool isCurrentSharegroup() const override;
  void present(std::shared_ptr<ITexture> surface) const override;

  /// Creates a shared context, matching format based on the current context.
  std::unique_ptr<IContext> createShareContext(Result* outResult) override;

  HDC getDeviceContext() const {
    return deviceContext_;
  }

  HGLRC getRenderContext() const {
    return renderContext_;
  }

 private:
  const bool contextOwned_ = false;
  HDC deviceContext_;
  HGLRC renderContext_;
  HWND dummyWindow_;
  std::vector<HGLRC> sharegroup_;
};

} // namespace wgl
} // namespace opengl
} // namespace igl
