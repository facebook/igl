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
  Context(BackendVersion backendVersion, Result* IGL_NULLABLE result);
  /// Create a new context with existing EAGLContext.
  explicit Context(EAGLContext* IGL_NULLABLE context);
  Context(EAGLContext* IGL_NULLABLE context, Result* IGL_NULLABLE result);
  ~Context() override;

  void setCurrent() override;
  void clearCurrentContext() const override;
  bool isCurrentContext() const override;
  bool isCurrentSharegroup() const override;
  void present(std::shared_ptr<ITexture> surface) const override;

  /// Creates a shared context, matching format based on the current context.
  std::unique_ptr<IContext> createShareContext(Result* IGL_NULLABLE outResult) override;

  CVOpenGLESTextureCacheRef IGL_NULLABLE getTextureCache();

 private:
  EAGLContext* IGL_NULLABLE const context_;
  CVOpenGLESTextureCacheRef IGL_NULLABLE textureCache_ = nullptr;
};

} // namespace igl::opengl::ios
