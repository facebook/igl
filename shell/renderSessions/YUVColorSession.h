/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <IGLU/imgui/Session.h>
#include <IGLU/simdtypes/SimdTypes.h>
#include <igl/IGL.h>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>

namespace igl::shell {

class YUVColorSession : public RenderSession {
  struct FragmentFormat {
    iglu::simdtypes::float3 color;
  };

 public:
  YUVColorSession(std::shared_ptr<Platform> platform);
  // clang-tidy off
  void initialize() noexcept override;
  // clang-tidy on
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;

  void nextFormatDemo();

 private:
  std::shared_ptr<IVertexInputState> vertexInput0_;

  std::shared_ptr<IShaderStages> shaderStages_;
  std::shared_ptr<IBuffer> vb0_;
  std::shared_ptr<IBuffer> ib0_;
  std::shared_ptr<IBuffer> fragmentParamBuffer_;
  std::shared_ptr<ITexture> depthTexture_;
  RenderPassDesc renderPass_;
  // clang-tidy off
  FragmentFormat fragmentParameters_{};
  // clang-tidy on
  std::vector<UniformDesc> fragmentUniformDescriptors_;
  std::vector<UniformDesc> vertexUniformDescriptors_;

  igl::FramebufferDesc framebufferDesc_;
  std::unique_ptr<iglu::imgui::Session> imguiSession_;

  struct YUVFormatDemo {
    const char* name = "";
    std::shared_ptr<ISamplerState> sampler;
    std::shared_ptr<ITexture> texture;
    std::shared_ptr<IRenderPipelineState> pipelineState;
  };

  std::vector<YUVFormatDemo> yuvFormatDemos_;
  size_t currentDemo_ = 0;

  struct Listener : public igl::shell::IMouseListener, igl::shell::IKeyListener {
    explicit Listener(YUVColorSession& session) : session(session) {}
    bool process(const MouseButtonEvent& event) override;
    bool process(const MouseMotionEvent& event) override;
    bool process(const MouseWheelEvent& event) override;
    bool process(const KeyEvent& event) override;
    YUVColorSession& session;
  };

  std::shared_ptr<Listener> listener_;
};

} // namespace igl::shell
