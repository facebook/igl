/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/Texture.h>
#include <igl/opengl/webgl/Context.h>

namespace igl::opengl::webgl {

Context::Context(RenderingAPI api, const char* canvasName) {
  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.majorVersion = api == RenderingAPI::GLES3 ? 2 : 1;
  attrs.minorVersion = 0;
  attrs.premultipliedAlpha = false;
  attrs.alpha = false;
  attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_DEFAULT;
  initialize(attrs, canvasName, -1, -1);
}

Context::Context(EmscriptenWebGLContextAttributes& attributes,
                 const char* canvasName,
                 int width,
                 int height) {
  initialize(attributes, canvasName, width, height);
}

void Context::initialize(EmscriptenWebGLContextAttributes& attributes,
                         const char* canvasName,
                         int width,
                         int height) {
  context_ = emscripten_webgl_create_context(canvasName, &attributes);
  if (width != -1 && height != -1) {
    emscripten_set_canvas_element_size(canvasName, width, height);
  }
  if (context_) {
    IContext::registerContext((void*)context_, this);
    setCurrent();

    igl::Result result;
    // Initialize through base class.
    IContext::initialize(&result);
    IGL_ASSERT(result.isOk());
  }
}

Context::~Context() {
  getAdapterPool().clear();
  // Unregister eglContext
  IContext::unregisterContext((void*)context_);
  emscripten_webgl_destroy_context(context_);
}
void Context::setCurrent() {
  emscripten_webgl_make_context_current(context_);
}

void Context::clearCurrentContext() const {
  // Intentionally does nothing. No such option on WebGL
}

bool Context::isCurrentContext() const {
  return emscripten_webgl_get_current_context() == context_;
}

bool Context::isCurrentSharegroup() const {
  return true;
}

void Context::present(std::shared_ptr<ITexture> surface) const {
  emscripten_webgl_commit_frame();
}

} // namespace igl::opengl::webgl
