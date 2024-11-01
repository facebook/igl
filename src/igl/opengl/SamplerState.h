/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/SamplerState.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>

namespace igl {
namespace opengl {

class SamplerState final : public WithContext, public ISamplerState {
 public:
  SamplerState(IContext& context, const SamplerStateDesc& desc);
  void bind(ITexture* texture);

  static GLint convertMinMipFilter(SamplerMinMagFilter minFilter, SamplerMipFilter mipFilter);
  static GLint convertMagFilter(SamplerMinMagFilter magFilter);
  static GLint convertAddressMode(SamplerAddressMode addressMode);
  static SamplerAddressMode convertGLAddressMode(GLint glAddressMode);
  static SamplerMinMagFilter convertGLMagFilter(GLint magFilter);
  static SamplerMinMagFilter convertGLMinFilter(GLint minFilter);
  static SamplerMipFilter convertGLMipFilter(GLint minFilter);

  /**
   * @brief Returns true if this sampler is a YUV sampler.
   */
  [[nodiscard]] bool isYUV() const noexcept override;

 private:
  size_t hash_ = std::numeric_limits<size_t>::max();
  GLint minMipFilter_;
  GLint magFilter_;
  GLfloat mipLodMin_;
  GLfloat mipLodMax_;
  GLfloat maxAnisotropy_;

  GLint addressU_;
  GLint addressV_;
  GLint addressW_;

  GLint depthCompareFunction_;
  bool depthCompareEnabled_;
  bool isYUV_;
};

} // namespace opengl
} // namespace igl
