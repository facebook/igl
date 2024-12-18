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
#include <shell/shared/platform/DisplayContext.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {

int NanovgSession::loadDemoData(NVGcontext* vg, DemoData* data) {
  auto getImageFullPath = ([this](const std::string& name) {
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

  if (vg == NULL) {
    IGL_DEBUG_ASSERT(false);
    return -1;
  }

  for (int i = 0; i < 12; i++) {
    char file[128];
    snprintf(file, 128, "image%d.jpg", i + 1);

    std::string full_file = getImageFullPath(file);
    data->images[i] = nvgCreateImage(vg, full_file.c_str(), 0);
    if (data->images[i] == 0) {
      IGL_DEBUG_ASSERT(false, "Could not load %s.\n", file);
      return -1;
    }
  }

  data->fontIcons = nvgCreateFont(vg, "icons", getImageFullPath("entypo.ttf").c_str());
  if (data->fontIcons == -1) {
    IGL_DEBUG_ASSERT(false, "Could not add font icons.\n");
    return -1;
  }
  data->fontNormal = nvgCreateFont(vg, "sans", getImageFullPath("Roboto-Regular.ttf").c_str());
  if (data->fontNormal == -1) {
    IGL_DEBUG_ASSERT(false, "Could not add font italic.\n");
    return -1;
  }
  data->fontBold = nvgCreateFont(vg, "sans-bold", getImageFullPath("Roboto-Bold.ttf").c_str());
  if (data->fontBold == -1) {
    IGL_DEBUG_ASSERT(false, "Could not add font bold.\n");
    return -1;
  }
  data->fontEmoji = nvgCreateFont(vg, "emoji", getImageFullPath("NotoEmoji-Regular.ttf").c_str());
  if (data->fontEmoji == -1) {
    IGL_DEBUG_ASSERT(false, "Could not add font emoji.\n");
    return -1;
  }
  nvgAddFallbackFontId(vg, data->fontNormal, data->fontEmoji);
  nvgAddFallbackFontId(vg, data->fontBold, data->fontEmoji);

  return 0;
}

void NanovgSession::initialize() noexcept {
  const CommandQueueDesc desc;
  commandQueue_ = getPlatform().getDevice().createCommandQueue(desc, nullptr);

  renderPass_.colorAttachments.resize(1);

  renderPass_.colorAttachments[0] = igl::RenderPassDesc::ColorAttachmentDesc{};
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = igl::Color(0.3f, 0.3f, 0.32f, 1.0f);
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass_.depthAttachment.clearDepth = 1.0;
  renderPass_.stencilAttachment.loadAction = LoadAction::Clear;
  renderPass_.stencilAttachment.clearStencil = 0;

  mouseListener_ = std::make_shared<MouseListener>();
  getPlatform().getInputDispatcher().addMouseListener(mouseListener_);

  touchListener_ = std::make_shared<TouchListener>();
  getPlatform().getInputDispatcher().addTouchListener(touchListener_);

  nvgContext_ = iglu::nanovg::CreateContext(
      &getPlatform().getDevice(), iglu::nanovg::NVG_ANTIALIAS | iglu::nanovg::NVG_STENCIL_STROKES);

  if (this->loadDemoData(nvgContext_, &nvgDemoData_) != 0) {
    IGL_DEBUG_ASSERT(false);
  }

  initGraph(&fps_, GRAPH_RENDER_FPS, "Frame Time");
  initGraph(&cpuGraph_, GRAPH_RENDER_MS, "CPU Time");
  initGraph(&gpuGraph_, GRAPH_RENDER_MS, "GPU Time");
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

#if IGL_PLATFORM_IOS || IGL_PLATFORM_ANDROID
  int mx = touchListener_->touchX;
  int my = touchListener_->touchY;
#else
  int mx = mouseListener_->mouseX;
  int my = mouseListener_->mouseY;
#endif

  auto start = getSeconds();

  nvgBeginFrame(vg, width, height, pxRatio);
  iglu::nanovg::SetRenderCommandEncoder(
      vg,
      framebuffer_.get(),
      command.get(),
      (float*)&getPlatform().getDisplayContext().preRotationMatrix);

  times_++;

  renderDemo(vg, mx, my, width, height, times_ / 60.0f, 0, &nvgDemoData_);

  renderGraph(vg, 5, 5, &fps_);
  renderGraph(vg, 5 + 200 + 5, 5, &cpuGraph_);
  renderGraph(vg, 5 + 200 + 5 + 200 + 5, 5, &gpuGraph_);

  nvgEndFrame(vg);

  auto end = getSeconds();

  updateGraph(&fps_, getDeltaSeconds());
  updateGraph(&cpuGraph_, (end - start));
}

} // namespace igl::shell
