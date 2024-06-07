/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

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
#include <regex>
#include <stdio.h>

#include <igl/IGL.h>

#define USE_OPENGL_BACKEND 0

#if IGL_BACKEND_OPENGL && !IGL_BACKEND_VULKAN
// no IGL/Vulkan was compiled in, switch to IGL/OpenGL
#undef USE_OPENGL_BACKEND
#define USE_OPENGL_BACKEND 1
#endif

// clang-format off
#if USE_OPENGL_BACKEND
  #if IGL_PLATFORM_WIN
    #include <igl/opengl/wgl/Context.h>
    #include <igl/opengl/wgl/Device.h>
    #include <igl/opengl/wgl/HWDevice.h>
    #include <igl/opengl/wgl/PlatformDevice.h>
  #elif IGL_PLATFORM_LINUX
    #include <igl/opengl/glx/Context.h>
    #include <igl/opengl/glx/Device.h>
    #include <igl/opengl/glx/HWDevice.h>
    #include <igl/opengl/glx/PlatformDevice.h>
  #endif
#else
  #include <igl/vulkan/Common.h>
  #include <igl/vulkan/Device.h>
  #include <igl/vulkan/HWDevice.h>
  #include <igl/vulkan/PlatformDevice.h>
  #include <igl/vulkan/VulkanContext.h>
#endif // USE_OPENGL_BACKEND
// clang-format on

#define ENABLE_MULTIPLE_COLOR_ATTACHMENTS 0

#if ENABLE_MULTIPLE_COLOR_ATTACHMENTS
static const uint32_t kNumColorAttachments = 4;
#else
static const uint32_t kNumColorAttachments = 1;
#endif

#if defined(__cpp_lib_format)
#include <format>
#define IGL_FORMAT std::format
#else
#include <fmt/core.h>
#define IGL_FORMAT fmt::format
#endif // __cpp_lib_format

std::string codeVS = R"(
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
std::shared_ptr<ICommandQueue> commandQueue_;
RenderPassDesc renderPass_;
std::shared_ptr<IFramebuffer> framebuffer_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_Triangle_;

static bool initWindow(GLFWwindow** outWindow) {
  if (!glfwInit()) {
    return false;
  }

#if USE_OPENGL_BACKEND
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
  glfwWindowHint(GLFW_VISIBLE, true);
  glfwWindowHint(GLFW_DOUBLEBUFFER, true);
  glfwWindowHint(GLFW_SRGB_CAPABLE, true);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
#else
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

#if USE_OPENGL_BACKEND
  const char* title = "OpenGL Triangle";
#else
  const char* title = "Vulkan Triangle";
#endif

  GLFWwindow* window = glfwCreateWindow(800, 600, title, nullptr, nullptr);

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
#if USE_OPENGL_BACKEND
#if IGL_PLATFORM_WIN
    auto ctx = std::make_unique<igl::opengl::wgl::Context>(GetDC(glfwGetWin32Window(window_)),
                                                           glfwGetWGLContext(window_));
    device_ = std::make_unique<igl::opengl::wgl::Device>(std::move(ctx));
#elif IGL_PLATFORM_LINUX
    auto ctx = std::make_unique<igl::opengl::glx::Context>(
        nullptr,
        glfwGetX11Display(),
        (igl::opengl::glx::GLXDrawable)glfwGetX11Window(window_),
        (igl::opengl::glx::GLXContext)glfwGetGLXContext(window_));

    device_ = std::make_unique<igl::opengl::glx::Device>(std::move(ctx));
#endif
#else
    const igl::vulkan::VulkanContextConfig cfg{
        .terminateOnValidationError = true,
    };
#ifdef _WIN32
    auto ctx = vulkan::HWDevice::createContext(cfg, (void*)glfwGetWin32Window(window_));
#elif __APPLE__
    auto ctx = vulkan::HWDevice::createContext(cfg, (void*)glfwGetCocoaWindow(window_));
#elif defined(__linux__)
    auto ctx = vulkan::HWDevice::createContext(
        cfg, (void*)glfwGetX11Window(window_), 0, nullptr, (void*)glfwGetX11Display());
#else
#error Unsupported OS
#endif

    std::vector<HWDeviceDesc> devices = vulkan::HWDevice::queryDevices(
        *ctx.get(), HWDeviceQueryDesc(HWDeviceType::DiscreteGpu), nullptr);
    if (devices.empty()) {
      devices = vulkan::HWDevice::queryDevices(
          *ctx.get(), HWDeviceQueryDesc(HWDeviceType::IntegratedGpu), nullptr);
    }
    device_ =
        vulkan::HWDevice::create(std::move(ctx), devices[0], (uint32_t)width_, (uint32_t)height_);
#endif
    IGL_ASSERT(device_);
  }

  // Command queue: backed by different types of GPU HW queues
  CommandQueueDesc desc{CommandQueueType::Graphics};
  commandQueue_ = device_->createCommandQueue(desc, nullptr);

  // first color attachment
  for (auto i = 0; i < kNumColorAttachments; ++i) {
    // Generate sparse color attachments by skipping alternate slots
    if (i & 0x1) {
      continue;
    }
    renderPass_.colorAttachments[i] = igl::RenderPassDesc::ColorAttachmentDesc{};
    renderPass_.colorAttachments[i].loadAction = LoadAction::Clear;
    renderPass_.colorAttachments[i].storeAction = StoreAction::Store;
    renderPass_.colorAttachments[i].clearColor = {1.0f, 1.0f, 1.0f, 1.0f};
  }
  renderPass_.depthAttachment.loadAction = LoadAction::DontCare;
}

static void createRenderPipeline() {
  if (renderPipelineState_Triangle_) {
    return;
  }

  IGL_ASSERT(framebuffer_);

  RenderPipelineDesc desc;

  desc.targetDesc.colorAttachments.resize(kNumColorAttachments);

  for (auto i = 0; i < kNumColorAttachments; ++i) {
    // @fb-only
    if (framebuffer_->getColorAttachment(i)) {
      desc.targetDesc.colorAttachments[i].textureFormat =
          framebuffer_->getColorAttachment(i)->getFormat();
    }
  }

  if (framebuffer_->getDepthAttachment()) {
    desc.targetDesc.depthAttachmentFormat = framebuffer_->getDepthAttachment()->getFormat();
  }

#if USE_OPENGL_BACKEND
  codeVS = std::regex_replace(codeVS, std::regex("gl_VertexIndex"), "gl_VertexID");
#endif

  desc.shaderStages = ShaderStagesCreator::fromModuleStringInput(
      *device_, codeVS.c_str(), "main", "", codeFS, "main", "", nullptr);
  renderPipelineState_Triangle_ = device_->createRenderPipeline(desc, nullptr);
  IGL_ASSERT(renderPipelineState_Triangle_);
}

static std::shared_ptr<ITexture> getNativeDrawable() {
  Result ret;
  std::shared_ptr<ITexture> drawable;
#if USE_OPENGL_BACKEND
#if IGL_PLATFORM_WIN
  const auto& platformDevice = device_->getPlatformDevice<opengl::wgl::PlatformDevice>();
  IGL_ASSERT(platformDevice != nullptr);
  drawable = platformDevice->createTextureFromNativeDrawable(&ret);
#elif IGL_PLATFORM_LINUX
  const auto& platformDevice = device_->getPlatformDevice<opengl::glx::PlatformDevice>();
  IGL_ASSERT(platformDevice != nullptr);
  drawable = platformDevice->createTextureFromNativeDrawable(width_, height_, &ret);
#endif
#else
  const auto& platformDevice = device_->getPlatformDevice<igl::vulkan::PlatformDevice>();
  IGL_ASSERT(platformDevice != nullptr);
  drawable = platformDevice->createTextureFromNativeDrawable(&ret);
#endif
  IGL_ASSERT_MSG(ret.isOk(), ret.message.c_str());
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
        IGL_FORMAT("{}C{}", framebufferDesc.debugName.c_str(), i - 1).c_str());

    framebufferDesc.colorAttachments[i].texture = device_->createTexture(desc, nullptr);
  }
  framebuffer_ = device_->createFramebuffer(framebufferDesc, nullptr);
  IGL_ASSERT(framebuffer_);
}

static void render(const std::shared_ptr<ITexture>& nativeDrawable) {
  if (!nativeDrawable) {
    return;
  }

  const auto size = framebuffer_->getColorAttachment(0)->getSize();
  if (size.width != width_ || size.height != height_) {
    createFramebuffer(nativeDrawable);
  } else {
    framebuffer_->updateDrawable(nativeDrawable);
  }

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
  commands->pushDebugGroupLabel("Render Triangle", igl::Color(1, 0, 0));
  commands->draw(3);
  commands->popDebugGroupLabel();
  commands->endEncoding();

  buffer->present(nativeDrawable);

  commandQueue_->submit(*buffer);
}

int main(int argc, char* argv[]) {
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
