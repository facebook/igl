/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <android/log.h>
#include <android_native_app_glue.h>
#include <cassert>
#include <chrono>
#include <igl/IGL.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/PlatformDevice.h>
#include <igl/vulkan/VulkanContext.h>
#include <jni.h>
#include <stdio.h>
#include <thread>

#define IGL_SAMPLE_LOG_INFO(...) \
  __android_log_print(ANDROID_LOG_INFO, "libsampleVulkanJni", __VA_ARGS__)
#define IGL_SAMPLE_LOG_ERROR(...) \
  __android_log_print(ANDROID_LOG_ERROR, "libsampleVulkanJni", __VA_ARGS__)

namespace igl_samples {
namespace android {

using namespace igl;

std::unique_ptr<IDevice> device_;
std::shared_ptr<ICommandQueue> commandQueue_;
std::shared_ptr<IRenderPipelineState> pipelineState_;
std::shared_ptr<IVertexInputState> vertexInputState_;
std::shared_ptr<IShaderStages> shaderStages_;
std::shared_ptr<IBuffer> vertexBuffer_;
std::shared_ptr<IBuffer> indexBuffer_;
std::shared_ptr<IFramebuffer> framebuffer_;
RenderPassDesc renderPass_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_Triangle_;

ANativeWindow* window_;
uint32_t width_, height_;
bool initialized_;

namespace {

const char* codeVS = R"(
#version 460
layout (location=0) out vec3 color;
const vec2 pos[3] = vec2[3](
	vec2(-0.6, -0.4),
	vec2( 0.6, -0.4),
	vec2( 0.0,  0.6)
);
const vec3 col[3] = vec3[3](
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0)
);
void main() {
	gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
	color = col[gl_VertexIndex];
}
)";

const char* codeFS = R"(
#version 460
layout (location=0) in vec3 color;
layout (location=0) out vec4 out_FragColor;
void main() {
	out_FragColor = vec4(color, 1.0);
};
)";
} // namespace

void initWindow(ANativeWindow* window) {
  // Get the Window first
  ANativeWindow_acquire(window);
  window_ = window;
  if (window_ == nullptr) {
    IGL_SAMPLE_LOG_ERROR("ANativeWindow is null");
    return;
  }

  width_ = ANativeWindow_getWidth(window_);
  height_ = ANativeWindow_getHeight(window_);
  IGL_SAMPLE_LOG_INFO("window size: [%d, %d]", width_, height_);
}

void initIGL() {
  // create a device
  const igl::vulkan::VulkanContextConfig ctxConfig;
  auto ctx = vulkan::HWDevice::createContext(ctxConfig, window_);
  std::vector<HWDeviceDesc> devices = vulkan::HWDevice::queryDevices(
      *ctx.get(), HWDeviceQueryDesc(HWDeviceType::IntegratedGpu), nullptr);
  device_ =
      vulkan::HWDevice::create(std::move(ctx), devices[0], (uint32_t)width_, (uint32_t)height_);
  IGL_DEBUG_ASSERT(device_);

  // Command queue: backed by different types of GPU HW queues
  CommandQueueDesc desc{CommandQueueType::Graphics};
  commandQueue_ = device_->createCommandQueue(desc, nullptr);

  renderPass_.colorAttachments.push_back(igl::RenderPassDesc::ColorAttachmentDesc{});
  renderPass_.colorAttachments.back().loadAction = LoadAction::Clear;
  renderPass_.colorAttachments.back().storeAction = StoreAction::Store;
  renderPass_.colorAttachments.back().clearColor = {0.4f, 0.0f, 0.0f, 1.0f};
  renderPass_.depthAttachment.loadAction = LoadAction::DontCare;
}

void createFramebuffer(const std::shared_ptr<ITexture>& nativeDrawable) {
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = nativeDrawable;
  framebuffer_ = device_->createFramebuffer(framebufferDesc, nullptr);
  IGL_DEBUG_ASSERT(framebuffer_);
}

void createRenderPipeline() {
  if (renderPipelineState_Triangle_) {
    return;
  }

  IGL_DEBUG_ASSERT(framebuffer_);

  RenderPipelineDesc desc;

  desc.targetDesc.colorAttachments.resize(1);
  desc.targetDesc.colorAttachments[0].textureFormat =
      framebuffer_->getColorAttachment(0)->getProperties().format;

  if (framebuffer_->getDepthAttachment()) {
    desc.targetDesc.depthAttachmentFormat =
        framebuffer_->getDepthAttachment()->getProperties().format;
  }

  desc.shaderStages = ShaderStagesCreator::fromModuleStringInput(
      *device_, codeVS, "main", "", codeFS, "main", "", nullptr);
  renderPipelineState_Triangle_ = device_->createRenderPipeline(desc, nullptr);
}

std::shared_ptr<ITexture> getVulkanNativeDrawable() {
  const auto& vkPlatformDevice = device_->getPlatformDevice<igl::vulkan::PlatformDevice>();

  IGL_DEBUG_ASSERT(vkPlatformDevice != nullptr);

  Result ret;
  std::shared_ptr<ITexture> drawable = vkPlatformDevice->createTextureFromNativeDrawable(&ret);

  IGL_DEBUG_ASSERT(ret.isOk());
  return drawable;
}

void render() {
  if (!initialized_) {
    return;
  }

  auto nativeDrawable = getVulkanNativeDrawable();
  framebuffer_->updateDrawable(nativeDrawable);

  // Command buffers (1-N per thread): create, submit and forget
  CommandBufferDesc cbDesc;
  std::shared_ptr<ICommandBuffer> buffer = commandQueue_->createCommandBuffer(cbDesc, nullptr);

  const igl::Viewport viewport = {0.0f, 0.0f, (float)width_, (float)height_, 0.0f, +1.0f};
  const igl::ScissorRect scissor = {0, 0, (uint32_t)width_, (uint32_t)height_};

  // This will clear the framebuffer
  auto commands = buffer->createRenderCommandEncoder(renderPass_, framebuffer_);

  commands->bindRenderPipelineState(renderPipelineState_Triangle_);
  commands->bindViewport(viewport);
  commands->bindScissorRect(scissor);

  // VK_EXT_debug_utils support doesn't exist yet
  // commands->pushDebugGroupLabel("Render Triangle", igl::Color(1, 0, 0));
  commands->draw(PrimitiveType::Triangle, 0, 3);
  // commands->popDebugGroupLabel();
  commands->endEncoding();

  buffer->present(nativeDrawable);

  commandQueue_->submit(*buffer);
}

void initialize(android_app* app) {
  if (initialized_) {
    return;
  }

  initWindow(app->window);
  initIGL();

  createFramebuffer(getVulkanNativeDrawable());
  createRenderPipeline();
  initialized_ = true;
}

// Android NativeActivity functions
extern "C" {
void handle_cmd(struct android_app* app, int32_t cmd) {}
static void handleAppCmd(struct android_app* app, int32_t appCmd) {
  switch (appCmd) {
  case APP_CMD_SAVE_STATE:
    break;
  case APP_CMD_INIT_WINDOW:
    initialize(app);
    break;
  case APP_CMD_TERM_WINDOW:
    break;
  case APP_CMD_GAINED_FOCUS:
    break;
  case APP_CMD_LOST_FOCUS:
    break;
  case APP_CMD_PAUSE:
  case APP_CMD_DESTROY:
  case APP_CMD_STOP:
    // destroy all the Vulkan stuff before closing the window
    renderPipelineState_Triangle_ = nullptr;
    framebuffer_ = nullptr;
    device_.reset(nullptr);
    initialized_ = false;
    break;
  }
}

void android_main(struct android_app* app) {
  app->onAppCmd = handleAppCmd;

  while (app->destroyRequested == 0) {
    for (;;) {
      int events;
      struct android_poll_source* source = nullptr;
      if (ALooper_pollAll(0, nullptr, &events, (void**)&source) < 0) {
        break;
      }
      if (source != nullptr) {
        source->process(app, source);
      }
    }

    render();
  }
}
}

} // namespace android
} // namespace igl_samples
