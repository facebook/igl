/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "NanovgSession.h"

#include <chrono>
#include <filesystem>
#include <igl/opengl/Device.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/RenderCommandEncoder.h>
#include <regex>
#include <shell/shared/fileLoader/FileLoader.h>
#include <shell/shared/imageLoader/ImageLoader.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {

double getMilliSeconds() {
  return std::chrono::duration<double>(std::chrono::time_point_cast<std::chrono::milliseconds>(
                                           std::chrono::high_resolution_clock::now())
                                           .time_since_epoch())
      .count();
}

void NanovgSession::initialize() noexcept {
  // Command queue: backed by different types of GPU HW queues
  const CommandQueueDesc desc{CommandQueueType::Graphics};
  commandQueue_ = getPlatform().getDevice().createCommandQueue(desc, nullptr);

  renderPass_.colorAttachments.resize(1);

  renderPass_.colorAttachments[0] = igl::RenderPassDesc::ColorAttachmentDesc{};
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor =
      igl::Color(0.3f, 0.3f, 0.32f, 1.0f); // getPreferredClearColor();
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass_.depthAttachment.clearDepth = 1.0;
  renderPass_.stencilAttachment.loadAction = LoadAction::Clear;
  renderPass_.stencilAttachment.clearStencil = 0;

  nvgContext_ = getPlatform().nanovgContext;

  SetImageFullPathCallback([this](const std::string& name) {
#if IGL_PLATFORM_ANDROID
    auto fullPath = std::filesystem::path("/data/data/com.facebook.igl.shell/files/") / name;
    if (std::filesystem::exists(fullPath)) {
      return fullPath.string();
    } else {
      IGL_DEBUG_ASSERT(false);
      return std::string("");
    }
#else
    return getPlatform().getImageLoader().fileLoader().fullPath(name);
#endif
  });

  if (loadDemoData(nvgContext_, &nvgDemoData_) == -1) {
    assert(false);
  }

  initGraph(&fps, GRAPH_RENDER_FPS, "Frame Time");
  initGraph(&cpuGraph, GRAPH_RENDER_MS, "CPU Time");
  initGraph(&gpuGraph, GRAPH_RENDER_MS, "GPU Time");
  times_ = 0;
}

void NanovgSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
  framebufferDesc.depthAttachment.texture = surfaceTextures.depth;
  framebufferDesc.stencilAttachment.texture = surfaceTextures.depth;

  const auto dimensions = surfaceTextures.color->getDimensions();
  framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, nullptr);
  IGL_DEBUG_ASSERT(framebuffer_);
  framebuffer_->updateDrawable(surfaceTextures.color);

  // Command buffers (1-N per thread): create, submit and forget
  const CommandBufferDesc cbDesc;
  const std::shared_ptr<ICommandBuffer> buffer =
      commandQueue_->createCommandBuffer(cbDesc, nullptr);

  // This will clear the framebuffer
  std::shared_ptr<igl::IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);

  drawNanovg((float)dimensions.width, (float)dimensions.height, commands);

  commands->endEncoding();

  if (shellParams().shouldPresent) {
    buffer->present(surfaceTextures.color);
  }

  commandQueue_->submit(*buffer);
  RenderSession::update(surfaceTextures);
}

void NanovgSession::drawNanovg(float __width,
                               float __height,
                               std::shared_ptr<igl::IRenderCommandEncoder> command) {
  NVGcontext* vg = nvgContext_;

  float pxRatio = 2.0f;

  const float width = __width / pxRatio;
  const float height = __height / pxRatio;

  int mx = 0;
  int my = 0;
  int blowup = 0;

  auto start_ms = getMilliSeconds();

  nvgBeginFrame(vg, width, height, pxRatio);
  nvgSetColorTexture(vg, framebuffer_, command);

  times_++;
  renderDemo(vg, mx, my, width, height, times_ / 60.0f, blowup, &nvgDemoData_);

  renderGraph(vg, 5, 5, &fps);
  renderGraph(vg, 5 + 200 + 5, 5, &cpuGraph);
  renderGraph(vg, 5 + 200 + 5 + 200 + 5, 5, &gpuGraph);

  {
    //        //绘制一个矩形
    //        nvgBeginPath(vg);
    //        nvgRect(vg, 100,100, 10,10);
    //        nvgFillColor(vg, nvgRGBA(255,192,0,255));
    //        nvgFill(vg);
    //
    //        //绘制扣洞矩形
    //        nvgBeginPath(vg);
    //        nvgRect(vg, 100,100, 50,50);
    //        nvgRect(vg, 110,110, 10,10);
    //        nvgPathWinding(vg, NVG_HOLE);    // Mark circle as a hole.
    //        nvgFillColor(vg, nvgRGBA(255,192,0,255));
    //        nvgFill(vg);
    //
    //        //绘制图片
    //        NVGpaint imgPaint = nvgImagePattern(vg, 200, 200, 100,100, 0.0f/180.0f*NVG_PI, 2,
    //        0.5); nvgBeginPath(vg); nvgRect(vg, 100,100, 500,500); nvgFillPaint(vg, imgPaint);
    //        nvgFill(vg);
    ////
    ////        //绘制文字
    //        nvgFontSize(vg, 150.0f);
    //        nvgFontFace(vg, "sans-bold");
    //        nvgTextAlign(vg,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
    //        nvgFontBlur(vg,2);
    //        nvgFillColor(vg, nvgRGBA(256,0,0,128));
    //        nvgText(vg, 200, 100, "Hello", NULL);
  }

  nvgEndFrame(vg);

  auto end_ms = getMilliSeconds();

  updateGraph(&fps, (start_ms - preTimestamp_));
  updateGraph(&cpuGraph, (end_ms - start_ms));

  preTimestamp_ = start_ms;
}

} // namespace igl::shell
