/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <IGLU/imgui/Session.h>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <igl/FPSCounter.h>
#include <igl/IGL.h>

namespace igl::shell {

class TinyMeshBindGroupSession : public RenderSession {
 public:
  explicit TinyMeshBindGroupSession(std::shared_ptr<Platform> platform);
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;
  std::shared_ptr<ITexture> getVulkanNativeDepth();
  void createRenderPipeline();

 private:
  IDevice* device_{};
  std::shared_ptr<ICommandQueue> commandQueue_;
  RenderPassDesc renderPass_;
  FramebufferDesc framebufferDesc_;
  std::shared_ptr<IFramebuffer> framebuffer_;
  std::shared_ptr<IRenderPipelineState> renderPipelineState_Mesh_;
  std::shared_ptr<IBuffer> vb0_, ib0_; // buffers for vertices and indices
  std::vector<std::shared_ptr<IBuffer>> ubPerFrame_, ubPerObject_;
  std::shared_ptr<IVertexInputState> vertexInput0_;
  std::shared_ptr<IDepthStencilState> depthStencilState_;
  std::shared_ptr<ITexture> texture0_, texture1_;
  std::shared_ptr<ISamplerState> sampler_;
  Holder<BindGroupTextureHandle> bindGroupTextures_;
  Holder<BindGroupTextureHandle> bindGroupNoTexture1_;
  std::vector<Holder<BindGroupBufferHandle>> bindGroupBuffers_;
  uint32_t frameIndex_{0};

  std::unique_ptr<iglu::imgui::Session> imguiSession_;

  struct Listener : public IKeyListener {
    explicit Listener(TinyMeshBindGroupSession& session) : session(session) {}
    bool process(const CharEvent& event) override;
    TinyMeshBindGroupSession& session;
  };

  std::shared_ptr<Listener> listener_;

  FPSCounter fps_;
  double currentTime_ = 0;
};

} // namespace igl::shell
