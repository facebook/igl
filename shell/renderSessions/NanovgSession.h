/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <igl/IGL.h>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>

#include "NanovgSession/demo.h"
#include "NanovgSession/perf.h"
#include <IGLU/nanovg/nanovg_igl.h>
#include <nanovg.h>

namespace igl::shell {

class NanovgSession : public RenderSession {
 public:
  explicit NanovgSession(std::shared_ptr<Platform> platform) : RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;

 private:
  void drawNanovg(float width, float height, std::shared_ptr<igl::IRenderCommandEncoder> command);

 private:
  std::shared_ptr<ICommandQueue> commandQueue_;
  RenderPassDesc renderPass_;

  NVGcontext* nvgContext_ = NULL;
  int times_;
  DemoData nvgDemoData_;

  PerfGraph fps, cpuGraph, gpuGraph;
  double preTimestamp_;
};

} // namespace igl::shell
