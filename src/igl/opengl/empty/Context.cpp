/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/empty/Context.h>

#include <igl/opengl/Texture.h>

namespace igl::opengl::empty {

Context::Context(RenderingAPI /*api*/) {
  igl::Result result;
  // Initialize through base class.
  initialize(&result);
  IGL_DEBUG_ASSERT(result.isOk());
}

void Context::setCurrent() {
  // Intentionally does nothing.
}

void Context::clearCurrentContext() const {
  // Intentionally does nothing.
}

bool Context::isCurrentContext() const {
  return true;
}

bool Context::isCurrentSharegroup() const {
  return false;
}

void Context::present(std::shared_ptr<ITexture> surface) const {
  // Intentionally does nothing.
}

std::unique_ptr<IContext> Context::createShareContext(Result* outResult) {
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
  Result::setResult(outResult, Result::Code::Unimplemented, "Implement as needed");
  return nullptr;
}

///--------------------------------------
/// MARK: - GL APIs Overrides

void Context::blendFunc(GLenum sfactor, GLenum dfactor) {
  // Intentionally does nothing.
}

void Context::cullFace(GLint mode) {
  // Intentionally does nothing.
}

void Context::disable(GLenum cap) {
  // Intentionally does nothing.
}

void Context::enable(GLenum cap) {
  // Intentionally does nothing.
}

void Context::frontFace(GLenum mode) {
  // Intentionally does nothing.
}

GLenum Context::getError() const {
  return GL_NO_ERROR;
}

GLenum Context::checkFramebufferStatus(GLenum /*target*/) {
  return GL_FRAMEBUFFER_COMPLETE;
}

const GLubyte* Context::getString(GLenum /*name*/) const {
  static const char* val = "n/a";
  return reinterpret_cast<const GLubyte*>(val);
}

void Context::setEnabled(bool shouldEnable, GLenum cap) {
  // Intentionally does nothing.
}

} // namespace igl::opengl::empty
