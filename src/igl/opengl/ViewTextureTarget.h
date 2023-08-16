/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/opengl/Texture.h>

namespace igl {
namespace opengl {

namespace egl {
class PlatformDevice;
} // namespace egl

namespace wgl {
class PlatformDevice;
} // namespace wgl

namespace webgl {
class PlatformDevice;
} // namespace webgl

// ViewTextureTarget is a special implementation of opengl::Texture that's only available on certain
// platforms. It represents the "texture" associated with the default framebuffer on OpenGL (i.e.
// framebuffer with ID 0), which is not available on some platforms such as iOS.
class ViewTextureTarget final : public Texture {
  friend class igl::opengl::egl::PlatformDevice;
  friend class igl::opengl::wgl::PlatformDevice;
  friend class igl::opengl::webgl::PlatformDevice;

 public:
  using Super = Texture;
  ViewTextureTarget(IContext& context, TextureFormat format) : Super(context, format) {}

  // ITexture overrides
  TextureType getType() const override;
  ulong_t getUsage() const override;

  // Texture overrides
  void bind() override;
  void bindImage(size_t /*unit*/) override;
  void unbind() override;
  void attachAsColor(uint32_t index,
                     uint32_t face = 0,
                     uint32_t mip = 0,
                     bool read = false) override;
  void detachAsColor(uint32_t index,
                     uint32_t face = 0,
                     uint32_t mip = 0,
                     bool read = false) override;
  void attachAsDepth() override;
  void attachAsStencil() override;

  bool isImplicitStorage() const override;

  // @fb-only
  GLuint getId() const override {
    IGL_ASSERT_NOT_REACHED();
    return 0;
  }
};

} // namespace opengl
} // namespace igl
