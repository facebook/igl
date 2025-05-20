/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <CoreVideo/CVOpenGLESTextureCache.h>
#include <igl/opengl/IContext.h>

@class EAGLContext;

namespace igl::opengl::ios {

class Context final : public IContext {
 public:
  /// Create a new context with new EAGLContext.
  explicit Context(BackendVersion backendVersion);
  Context(BackendVersion backendVersion, Result* result);
  /// Create a new context with existing EAGLContext.
  explicit Context(EAGLContext* context);
  Context(EAGLContext* context, Result* result);
  ~Context() override;

  void setCurrent() override;
  void clearCurrentContext() const override;
  bool isCurrentContext() const override;
  bool isCurrentSharegroup() const override;
  void present(std::shared_ptr<ITexture> surface) const override;

  /// Creates a shared context, matching format based on the current context.
  std::unique_ptr<IContext> createShareContext(Result* outResult) override;

  CVOpenGLESTextureCacheRef getTextureCache();

 private:
  EAGLContext* const context_;
  CVOpenGLESTextureCacheRef textureCache_ = nullptr;
};

} // namespace igl::opengl::ios
