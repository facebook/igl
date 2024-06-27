/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <IGLU/simdtypes/SimdTypes.h>
#include <igl/IGL.h>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>

namespace igl::shell {

struct FragmentFormat {
  iglu::simdtypes::float3 color;
};

class TQSession : public RenderSession {
 public:
  TQSession(std::shared_ptr<Platform> platform) : RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;

 private:
  std::shared_ptr<ICommandQueue> _commandQueue;
  std::shared_ptr<IRenderPipelineState> _pipelineState;
  std::shared_ptr<IVertexInputState> _vertexInput0;
  std::shared_ptr<ISamplerState> _samp0;

  std::shared_ptr<IShaderStages> _shaderStages;
  std::shared_ptr<IBuffer> _vb0;
  std::shared_ptr<IBuffer> _ib0;
  std::shared_ptr<IBuffer> _fragmentParamBuffer;
  std::shared_ptr<ITexture> _depthTexture;
  std::shared_ptr<ITexture> _tex0;
  RenderPassDesc _renderPass;
  std::shared_ptr<IFramebuffer> _framebuffer;
  FragmentFormat _fragmentParameters{};
  std::vector<UniformDesc> _fragmentUniformDescriptors;
  std::vector<UniformDesc> _vertexUniformDescriptors;
};

} // namespace igl::shell
