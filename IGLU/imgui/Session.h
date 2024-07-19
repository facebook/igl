/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <IGLU/imgui/InputListener.h>
#include <igl/CommandBuffer.h>
#include <igl/Core.h>
#include <igl/Device.h>
#include <igl/Framebuffer.h>
#include <igl/RenderCommandEncoder.h>
#include <memory>
#include <shell/shared/input/InputDispatcher.h>

#include "imgui.h"

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

  igl::shell::InputDispatcher& _inputDispatcher;
  std::shared_ptr<InputListener> _inputListener;
  ImGuiContext* _context;
  std::unique_ptr<Renderer> _renderer;
  bool _isInitialized = false;

  void makeCurrentContext() const;
};

} // namespace iglu::imgui
