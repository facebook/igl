/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include "imgui.h"

#include <IGLU/imgui/InputListener.h>
#include <memory>
#include <shell/shared/input/InputDispatcher.h>
#include <igl/CommandBuffer.h>
#include <igl/Core.h>
#include <igl/Device.h>
#include <igl/Framebuffer.h>
#include <igl/RenderCommandEncoder.h>

namespace iglu::imgui {

class Session {
 public:
  void beginFrame(const igl::FramebufferDesc& desc, float displayScale);
  void endFrame(igl::IDevice& device, igl::IRenderCommandEncoder& cmdEncoder);

  Session(igl::IDevice& device,
          igl::shell::InputDispatcher& inputDispatcher,
          bool needInitializeSession = true);
  ~Session();

  void initialize(igl::IDevice& device);
  void drawFPS(float fps) const;

 private:
  class Renderer;

  igl::shell::InputDispatcher& inputDispatcher_;
  std::shared_ptr<InputListener> inputListener_;
  ImGuiContext* context_;
  std::unique_ptr<Renderer> renderer_;
  bool isInitialized_ = false;

  void makeCurrentContext() const;
};

} // namespace iglu::imgui
