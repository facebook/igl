/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include "demo.h"
#include "perf.h"
#include <IGLU/nanovg/nanovg_igl.h>
#include <igl/IGL.h>
#include <nanovg.h>
#include <shell/shared/input/InputDispatcher.h>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>

namespace igl::shell {

class MouseListener : public IMouseListener {
 public:
  bool process(const MouseButtonEvent& event) override {
    return true;
  }
  bool process(const MouseMotionEvent& event) override {
    mouseX = event.x;
    mouseY = event.y;
    return true;
  }
  bool process(const MouseWheelEvent& event) override {
    return true;
  }

  int mouseX;
  int mouseY;
};

class TouchListener : public ITouchListener {
 public:
  bool process(const TouchEvent& event) override {
    touchX = event.x;
    touchY = event.y;
    return true;
  }

  int touchX;
  int touchY;
};

class NanovgSession : public RenderSession {
 public:
  explicit NanovgSession(std::shared_ptr<Platform> platform) : RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;

 private:
  void drawNanovg(float width, float height, std::shared_ptr<igl::IRenderCommandEncoder> command);
  int loadDemoData(NVGcontext* vg, DemoData* data);

 private:
  std::shared_ptr<ICommandQueue> commandQueue_;
  RenderPassDesc renderPass_;

  NVGcontext* nvgContext_ = NULL;
  int times_ = 0;
  DemoData nvgDemoData_;

  std::shared_ptr<MouseListener> mouseListener_;
  std::shared_ptr<TouchListener> touchListener_;

  PerfGraph fps_, cpuGraph_, gpuGraph_;
  double preTimestamp_;
};

} // namespace igl::shell
