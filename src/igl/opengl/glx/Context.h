/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>

#include <X11/Xlib.h>

#include <cstdint>
#include <memory>

namespace igl::opengl::glx {

using GLXDrawable = XID;
using GLXContext = struct __GLXcontext*;
struct GLXSharedModule;

class Context : public IContext {
 public:
  explicit Context(std::shared_ptr<GLXSharedModule> module,
                   bool offscreen = false,
                   uint32_t width = 0,
                   uint32_t height = 0);
  Context(std::shared_ptr<GLXSharedModule> module,
          Display* display,
          GLXDrawable windowHandle,
          GLXContext contextHandle);
  ~Context() override;

  void setCurrent() override;
  void clearCurrentContext() const override;
  bool isCurrentContext() const override;
  bool isCurrentSharegroup() const override;
  void present(std::shared_ptr<ITexture> surface) const override;

  /// Creates a shared context, matching format based on the current context.
  std::unique_ptr<IContext> createShareContext(Result* outResult) override;

  std::shared_ptr<GLXSharedModule> getSharedModule() const;

 private:
  const bool contextOwned_ = false;
  const bool offscreen_ = false;
  std::shared_ptr<GLXSharedModule> module_;
  Display* display_ = nullptr;
  GLXDrawable windowHandle_ = 0;
  GLXContext contextHandle_ = nullptr;
};

} // namespace igl::opengl::glx
