/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <igl/IGL.h>

namespace igl::shell {

// Fits 2048 anisotropic splats to an image by gradient descent on the GPU, with every shader
// written once in Slang (renderSessions/slang) and differentiated by Slang. Shows the target, the
// fit, the error, and the fit's forward-mode derivative as the splats move right. Backends
// without compute show the splats as initialized.
class SlangSplatsSession : public RenderSession {
 public:
  explicit SlangSplatsSession(std::shared_ptr<Platform> platform) :
    RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
  void createTrainingPipelines(IDevice& device);
  void encodeTrainingStep(ICommandBuffer& commandBuffer);

  std::shared_ptr<IBuffer> splats_;
  std::shared_ptr<IBuffer> tiles_;
  std::shared_ptr<IBuffer> overflowCount_;
  std::shared_ptr<IBuffer> targetPixels_;
  std::shared_ptr<IBuffer> pixels_;
  std::shared_ptr<IBuffer> gradients_;
  std::shared_ptr<IBuffer> moments_;
  std::shared_ptr<IBuffer> renderParams_;

  std::shared_ptr<IComputePipelineState> binPipeline_;
  std::shared_ptr<IComputePipelineState> forwardPipeline_;
  std::shared_ptr<IComputePipelineState> backwardPipeline_;
  std::shared_ptr<IComputePipelineState> adamPipeline_;

  std::shared_ptr<ITexture> targetTexture_;
  std::shared_ptr<ITexture> accumulationTexture_;
  std::shared_ptr<ITexture> sensitivityTexture_;
  std::shared_ptr<IFramebuffer> splatFramebuffer_;
  std::shared_ptr<IRenderPipelineState> splatPipeline_;
  std::shared_ptr<IRenderPipelineState> compositePipeline_;
  std::shared_ptr<IVertexInputState> splatInput_;
  std::shared_ptr<ISamplerState> sampler_;
};

} // namespace igl::shell
