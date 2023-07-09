/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#elif __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#else
#error Unsupported OS
#endif

#include <GLFW/glfw3native.h>

#include <cassert>
#include <format>
#include <regex>
#include <stdio.h>

#include <igl/CommandBuffer.h>
#include <igl/Device.h>
#include <igl/RenderPipelineState.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/PlatformDevice.h>
#include <igl/vulkan/VulkanContext.h>

#define ENABLE_MULTIPLE_COLOR_ATTACHMENTS 0

#if ENABLE_MULTIPLE_COLOR_ATTACHMENTS
static const uint32_t kNumColorAttachments = 4;
#else
static const uint32_t kNumColorAttachments = 1;
#endif

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

#if ENABLE_MULTIPLE_COLOR_ATTACHMENTS
const char* codeFS = R"(
#version 460
layout (location=0) in vec3 color;
layout (location=0) out vec4 out_FragColor;
layout (location=1) out vec4 out_FragColor1;

void main() {
	out_FragColor = vec4(color, 1.0);
	out_FragColor1 = vec4(1.0, 0.0, 0.0, 1.0);
};
)";
#else
const char* codeFS = R"(
#version 460
layout (location=0) in vec3 color;
layout (location=0) out vec4 out_FragColor;
void main() {
	out_FragColor = vec4(color, 1.0);
};
)";
#endif

using namespace igl;

GLFWwindow* window_ = nullptr;
int width_ = 0;
int height_ = 0;

std::unique_ptr<IDevice> device_;
RenderPassDesc renderPass_;
std::shared_ptr<IFramebuffer> framebuffer_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_Triangle_;

static bool initWindow(GLFWwindow** outWindow) {
  if (!glfwInit()) {
    return false;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Triangle", nullptr, nullptr);

  if (!window) {
    glfwTerminate();
    return false;
  }

  glfwSetErrorCallback([](int error, const char* description) {
    printf("GLFW Error (%i): %s\n", error, description);
  });

  glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
  });

  // @lint-ignore CLANGTIDY
  glfwSetWindowSizeCallback(window, [](GLFWwindow* /*window*/, int width, int height) {
    printf("Window resized! width=%d, height=%d\n", width, height);
    width_ = width;
    height_ = height;
#if !USE_OPENGL_BACKEND
    auto* vulkanDevice = static_cast<vulkan::Device*>(device_.get());
    auto& ctx = vulkanDevice->getVulkanContext();
    ctx.initSwapchain(width_, height_);
#endif
  });

  glfwGetWindowSize(window, &width_, &height_);

  if (outWindow) {
    *outWindow = window;
  }

  return true;
}

static void initIGL() {
  // create a device
  {
    const igl::vulkan::VulkanContextConfig cfg{
        .maxTextures = 8,
        .maxSamplers = 8,
        .terminateOnValidationError = true,
        .swapChainColorSpace = igl::ColorSpace::SRGB_LINEAR,
    };
#ifdef _WIN32
    auto ctx = vulkan::HWDevice::createContext(cfg, (void*)glfwGetWin32Window(window_));
#elif defined(__linux__)
    auto ctx = vulkan::HWDevice::createContext(
        cfg, (void*)glfwGetX11Window(window_), 0, nullptr, (void*)glfwGetX11Display());
#else
#error Unsupported OS
#endif

    std::vector<HWDeviceDesc> devices =
        vulkan::HWDevice::queryDevices(*ctx.get(), HWDeviceType::DiscreteGpu, nullptr);
    if (devices.empty()) {
      devices = vulkan::HWDevice::queryDevices(*ctx.get(), HWDeviceType::IntegratedGpu, nullptr);
    }
    device_ =
        vulkan::HWDevice::create(std::move(ctx), devices[0], (uint32_t)width_, (uint32_t)height_);
    IGL_ASSERT(device_.get());
  }

  // first color attachment
  for (auto i = 0; i < kNumColorAttachments; ++i) {
    // Generate sparse color attachments by skipping alternate slots
    if (i & 0x1) {
      continue;
    }
    renderPass_.colorAttachments[i] = igl::AttachmentDesc{
        .loadAction = LoadAction::Clear,
        .storeAction = StoreAction::Store,
        .clearColor = {1.0f, 1.0f, 1.0f, 1.0f},
    };
  }
  renderPass_.depthStencilAttachment = {
      .loadAction = LoadAction::DontCare,
      .storeAction = StoreAction::DontCare,
  };
}

static void createRenderPipeline() {
  if (renderPipelineState_Triangle_) {
    return;
  }

  IGL_ASSERT(framebuffer_.get());

  RenderPipelineDesc desc;

  desc.targetDesc.colorAttachments.resize(kNumColorAttachments);

  for (auto i = 0; i < kNumColorAttachments; ++i) {
    if (framebuffer_->getColorAttachment(i)) {
      desc.targetDesc.colorAttachments[i].textureFormat =
          framebuffer_->getColorAttachment(i)->getFormat();
    }
  }

  if (framebuffer_->getDepthAttachment()) {
    desc.targetDesc.depthAttachmentFormat = framebuffer_->getDepthAttachment()->getFormat();
  }

  desc.shaderStages = device_->createShaderStages(
      codeVS, "Shader Module: main (vert)", codeFS, "Shader Module: main (frag)");
  renderPipelineState_Triangle_ = device_->createRenderPipeline(desc, nullptr);
  IGL_ASSERT(renderPipelineState_Triangle_.get());
}

static std::shared_ptr<ITexture> getNativeDrawable() {
  const auto& platformDevice = device_->getPlatformDevice<igl::vulkan::PlatformDevice>();
  IGL_ASSERT(platformDevice != nullptr);

  Result ret;
  std::shared_ptr<ITexture> drawable = platformDevice->createTextureFromNativeDrawable(&ret);

  IGL_ASSERT(ret.isOk());
  return drawable;
}

static void createFramebuffer(const std::shared_ptr<ITexture>& nativeDrawable) {
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = nativeDrawable;

  for (auto i = 1; i < kNumColorAttachments; ++i) {
    // Generate sparse color attachments by skipping alternate slots
    if (i & 0x1) {
      continue;
    }
    const TextureDesc desc = TextureDesc::new2D(
        nativeDrawable->getFormat(),
        nativeDrawable->getDimensions().width,
        nativeDrawable->getDimensions().height,
        TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Sampled,
        std::format("{}C{}", framebufferDesc.debugName.c_str(), i - 1).c_str());

    framebufferDesc.colorAttachments[i].texture = device_->createTexture(desc, nullptr);
  }
  framebuffer_ = device_->createFramebuffer(framebufferDesc, nullptr);
  IGL_ASSERT(framebuffer_.get());
}

static void render(const std::shared_ptr<ITexture>& nativeDrawable) {
  const auto size = framebuffer_->getColorAttachment(0)->getSize();
  if (size.width != width_ || size.height != height_) {
    createFramebuffer(nativeDrawable);
  } else {
    framebuffer_->updateDrawable(nativeDrawable);
  }

  // Command buffers (1-N per thread): create, submit and forget
  std::shared_ptr<ICommandBuffer> buffer = device_->createCommandBuffer();

  const igl::Viewport viewport = {0.0f, 0.0f, (float)width_, (float)height_, 0.0f, +1.0f};
  const igl::ScissorRect scissor = {0, 0, (uint32_t)width_, (uint32_t)height_};

  // This will clear the framebuffer
  buffer->cmdBeginRenderPass(renderPass_, framebuffer_);
  {
    buffer->cmdBindRenderPipelineState(renderPipelineState_Triangle_);
    buffer->cmdBindViewport(viewport);
    buffer->cmdBindScissorRect(scissor);
    buffer->cmdPushDebugGroupLabel("Render Triangle", igl::Color(1, 0, 0));
    buffer->cmdDraw(PrimitiveType::Triangle, 0, 3);
    buffer->cmdPopDebugGroupLabel();
  }
  buffer->cmdEndRenderPass();

  buffer->present(nativeDrawable);

  device_->submit(igl::CommandQueueType::Graphics, *buffer, true);
}

int main(int argc, char* argv[]) {
  minilog::initialize(nullptr, {.threadNames = false});

  renderPass_.colorAttachments.resize(kNumColorAttachments);
  initWindow(&window_);
  initIGL();

  createFramebuffer(getNativeDrawable());
  createRenderPipeline();

  // Main loop
  while (!glfwWindowShouldClose(window_)) {
    render(getNativeDrawable());
    glfwPollEvents();
  }

  // destroy all the Vulkan stuff before closing the window
  renderPipelineState_Triangle_ = nullptr;
  framebuffer_ = nullptr;
  device_.reset(nullptr);

  glfwDestroyWindow(window_);
  glfwTerminate();

  return 0;
}
