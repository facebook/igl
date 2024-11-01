/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <CoreVideo/CoreVideo.h>

#include <AppKit/AppKit.h>

#include <CoreVideo/CVOpenGLTextureCache.h>
#include <igl/opengl/IContext.h>

@class NSOpenGLContext;

namespace igl {
namespace opengl::macos {

class Context final : public IContext {
 public:
  /// Creates a shared context, matching format based on the current context.
  std::unique_ptr<IContext> createShareContext(Result* outResult) override;

  /// Create a new context with new NSOpenGLContext.
  static std::unique_ptr<Context> createContext(igl::opengl::RenderingAPI api, Result* outResult);
  /// Create a new context with existing NSOpenGLContext.
  static std::unique_ptr<Context> createContext(NSOpenGLContext* context, Result* outResult);
  /// Creates a shared context, with matching format based on an existing context.
  static std::unique_ptr<Context> createShareContext(Context& existingContext, Result* outResult);
  // Create a new context with existing NSOpenGLContext and its shared contexts. The share context
  // must be setup ahead of calling this constructor, e.g. via QOpenGLContext->setShareContext, and
  // should not be modified during the lifetime of this IContext
  static std::unique_ptr<Context> createContext(
      NSOpenGLContext* context,
      std::shared_ptr<std::vector<NSOpenGLContext*>> shareContexts,
      Result* outResult);

  ~Context() override;

  void setCurrent() override;
  void clearCurrentContext() const override;
  bool isCurrentContext() const override;
  bool isCurrentSharegroup() const override;
  void present(std::shared_ptr<ITexture> surface) const override;

  NSOpenGLContext* getNSContext();
  CVOpenGLTextureCacheRef createTextureCache();

  static NSOpenGLPixelFormat* preferredPixelFormat();

 private:
  Context(NSOpenGLContext* context, std::shared_ptr<std::vector<NSOpenGLContext*>> shareContexts);

  NSOpenGLContext* const context_;

  // Since NSOpenGLContext does not expose a Share Group, this must be set manually via the
  // constructor and should be a list of all the contexts in the group including this context_
  std::shared_ptr<std::vector<NSOpenGLContext*>> sharegroup_;
};

} // namespace opengl::macos
} // namespace igl
