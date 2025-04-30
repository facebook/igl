/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/IContext.h>

namespace igl::opengl::empty {

class Context final : public IContext {
 public:
  Context();
  void setCurrent() override;
  void clearCurrentContext() const override;
  bool isCurrentContext() const override;
  bool isCurrentSharegroup() const override;
  void present(std::shared_ptr<ITexture> surface) const override;

  /// Creates a shared context, matching format based on the current context.
  std::unique_ptr<IContext> createShareContext(Result* outResult) override;

  ///--------------------------------------
  /// MARK: - GL APIs Overrides

  void blendFunc(GLenum sfactor, GLenum dfactor) override;
  void cullFace(GLint mode) override;
  void disable(GLenum cap) override;
  void enable(GLenum cap) override;
  void frontFace(GLenum mode) override;
  GLenum getError() const override;
  GLenum checkFramebufferStatus(GLenum target) override;
  const GLubyte* getString(GLenum name) const override;
  void setEnabled(bool shouldEnable, GLenum cap) override;
};

} // namespace igl::opengl::empty
