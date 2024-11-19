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

#include <IGLU/nanovg/nanovg.h>
#include <IGLU/nanovg/nanovg_mtl.h>
#include <IGLU/nanovg/demo.h>

namespace igl::shell {

class NanovgSession : public RenderSession {
 public:
  explicit NanovgSession(std::shared_ptr<Platform> platform) :
    RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(igl::SurfaceTextures surfaceTextures) noexcept override;
    
private:
    void drawTriangle(igl::SurfaceTextures surfaceTextures);
    void drawNanovg(float width, float height, igl::SurfaceTextures surfaceTextures);

 private:
  std::shared_ptr<ICommandQueue> commandQueue_;
  RenderPassDesc renderPass_;
  std::shared_ptr<IRenderPipelineState> renderPipelineState_Triangle_;
    
    NVGcontext *nvgContext_ = NULL;
    int times_;
    DemoData nvgDemoData_;
};

} // namespace igl::shell
