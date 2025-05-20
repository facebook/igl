/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#ifndef __EMSCRIPTEN__
#error "Platform not supported"
#endif

#include <emscripten.h>
#include <emscripten/html5.h>
#include <igl/opengl/IContext.h>

namespace igl {
class ITexture;
namespace opengl::webgl {

class Context final : public ::igl::opengl::IContext {
 public:
  explicit Context(const char* canvasName = "#canvas");
  explicit Context(BackendVersion backendVersion, const char* canvasName = "#canvas");
  Context(EmscriptenWebGLContextAttributes& attributes,
          const char* canvasName = "#canvas",
          int width = -1,
          int height = -1);
  ~Context() override;
  void setCurrent() override;
  void clearCurrentContext() const override;
  bool isCurrentContext() const override;
  bool isCurrentSharegroup() const override;
  void present(std::shared_ptr<ITexture> surface) const override;

  /// Creates a shared context, matching format based on the current context.
  std::unique_ptr<IContext> createShareContext(Result* outResult) override;

  void setCanvasBufferSize(int width, int height);

  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE getWebGLContext() const {
    return context_;
  }

 private:
  void initialize(EmscriptenWebGLContextAttributes& attributes,
                  const char* canvasName,
                  int width = -1,
                  int height = -1);

  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context_;
  std::string canvasName_;
};

} // namespace opengl::webgl
} // namespace igl
