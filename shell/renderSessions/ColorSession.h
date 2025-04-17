/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <IGLU/simdtypes/SimdTypes.h>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <igl/IGL.h>

namespace igl::shell {

class ColorSession : public RenderSession {
  struct FragmentFormat {
    iglu::simdtypes::float3 color;
    iglu::simdtypes::float4x4 mvp;
  };

 public:
  explicit ColorSession(std::shared_ptr<Platform> platform) : RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

  enum class ColorTestModes {
    eMacbethTexture,
    eOrangeTexture,
    eOrangeClear,
  };
  void setTestMode(ColorTestModes colorTestModes) noexcept {
    colorTestModes_ = colorTestModes;
  }

 private:
  ColorTestModes colorTestModes_ = ColorTestModes::eMacbethTexture;

  std::shared_ptr<IRenderPipelineState> pipelineState_;
  std::shared_ptr<IVertexInputState> vertexInput0_;
  std::shared_ptr<ISamplerState> samp0_;

  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IBuffer> vb0_;
  std::shared_ptr<IBuffer> ib0_;
  std::shared_ptr<IBuffer> fragmentParamBuffer_;
  std::shared_ptr<ITexture> depthTexture_;
  std::shared_ptr<ITexture> tex0_;
  RenderPassDesc renderPass_;
  FragmentFormat fragmentParameters_{};
  std::vector<UniformDesc> fragmentUniformDescriptors_;
  std::vector<UniformDesc> vertexUniformDescriptors_;
};

} // namespace igl::shell
