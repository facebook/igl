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
#include <shared/UtilsFPS.h>

#include <lvk/LVK.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>

constexpr uint32_t kNumColorAttachments = 4;

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
layout (location=0) out vec4 out_FragColor0;
layout (location=1) out vec4 out_FragColor1;

void main() {
	out_FragColor0 = vec4(color, 1.0);
	out_FragColor1 = vec4(1.0, 1.0, 0.0, 1.0);
};
)";

using namespace igl;

GLFWwindow* window_ = nullptr;
int width_ = 0;
int height_ = 0;
FramesPerSecondCounter fps_;

struct VulkanObjects {
  void init();
  void createFramebuffer();
  void render();
  igl::RenderPass renderPass_ = {};
  igl::Framebuffer framebuffer_ = {};
  std::shared_ptr<IRenderPipelineState> renderPipelineState_Triangle_;
  std::unique_ptr<IDevice> device_;
} vk;

GLFWwindow* initWindow() {
  if (!glfwInit()) {
    return nullptr;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Triangle", nullptr, nullptr);

  if (!window) {
    glfwTerminate();
    return nullptr;
  }

  glfwSetErrorCallback([](int error, const char* description) {
    printf("GLFW Error (%i): %s\n", error, description);
  });

  glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
  });

  glfwSetWindowSizeCallback(window, [](GLFWwindow*, int width, int height) {
    printf("Window resized! width=%d, height=%d\n", width, height);
    width_ = width;
    height_ = height;
    vulkan::Device* vulkanDevice = static_cast<vulkan::Device*>(vk.device_.get());
    vulkanDevice->getVulkanContext().initSwapchain(width_, height_);
    vk.createFramebuffer();
  });

  glfwGetWindowSize(window, &width_, &height_);

  return window;
}

void VulkanObjects::init() {
  {
    const igl::vulkan::VulkanContextConfig cfg{
        .maxTextures = 8,
        .maxSamplers = 8,
        .terminateOnValidationError = true,
        .swapChainColorSpace = igl::ColorSpace::SRGB_LINEAR,
    };
#ifdef _WIN32
    auto ctx = vulkan::Device::createContext(cfg, (void*)glfwGetWin32Window(window_));
#elif defined(__linux__)
    auto ctx = vulkan::Device::createContext(
        cfg, (void*)glfwGetX11Window(window_), 0, nullptr, (void*)glfwGetX11Display());
#else
#error Unsupported OS
#endif

    std::vector<HWDeviceDesc> devices =
        vulkan::Device::queryDevices(*ctx.get(), HWDeviceType::DiscreteGpu, nullptr);
    if (devices.empty()) {
      devices = vulkan::Device::queryDevices(*ctx.get(), HWDeviceType::IntegratedGpu, nullptr);
    }
    device_ =
        vulkan::Device::create(std::move(ctx), devices[0], (uint32_t)width_, (uint32_t)height_);
    IGL_ASSERT(device_.get());
  }

  renderPass_ = {
      .numColorAttachments = kNumColorAttachments,
      .depthAttachment =
          {
              .loadAction = LoadAction::DontCare,
              .storeAction = StoreAction::DontCare,
          },
  };

  const Color colors[4] = {
      {1.0f, 1.0f, 1.0f, 1.0f},
      {1.0f, 0.0f, 0.0f, 1.0f},
      {0.0f, 1.0f, 0.0f, 1.0f},
      {0.0f, 0.0f, 1.0f, 1.0f},
  };

  // first color attachment
  for (uint32_t i = 0; i < kNumColorAttachments; ++i) {
    renderPass_.colorAttachments[i] = igl::AttachmentDesc{
        .loadAction = LoadAction::Clear,
        .storeAction = StoreAction::Store,
        .clearColor = colors[i],
    };
  }

  auto texSwapchain = device_->getCurrentSwapchainTexture();

  createFramebuffer();

  const RenderPipelineDesc desc = {
      .shaderStages = device_->createShaderStages(
          codeVS, "Shader Module: main (vert)", codeFS, "Shader Module: main (frag)"),
      .numColorAttachments = kNumColorAttachments,
      .colorAttachments = {
          {framebuffer_.colorAttachments[0].texture->getFormat()},
          {framebuffer_.colorAttachments[1].texture->getFormat()},
          {framebuffer_.colorAttachments[2].texture->getFormat()},
          {framebuffer_.colorAttachments[3].texture->getFormat()},
      }};

  renderPipelineState_Triangle_ = device_->createRenderPipeline(desc, nullptr);

  IGL_ASSERT(renderPipelineState_Triangle_.get());
}

void VulkanObjects::createFramebuffer() {
  auto texSwapchain = device_->getCurrentSwapchainTexture();

  Framebuffer fb = {
      .numColorAttachments = kNumColorAttachments,
      .colorAttachments = {{.texture = texSwapchain}},
  };

  for (uint32_t i = 1; i < kNumColorAttachments; i++) {
    char attachmentName[256] = {0};
    snprintf(attachmentName, sizeof(attachmentName) - 1, "%s C%u", fb.debugName, i - 1);
    fb.colorAttachments[i].texture = device_->createTexture(
        {
            .type = TextureType::TwoD,
            .format = texSwapchain->getFormat(),
            .width = texSwapchain->getDimensions().width,
            .height = texSwapchain->getDimensions().height,
            .usage = igl::TextureUsageBits_Attachment | igl::TextureUsageBits_Sampled,
            .debugName = attachmentName,
        },
        nullptr);
  }
  framebuffer_ = fb;
}

void VulkanObjects::render() {
  framebuffer_.colorAttachments[0].texture = device_->getCurrentSwapchainTexture();

  // Command buffers (1-N per thread): create, submit and forget
  std::shared_ptr<ICommandBuffer> buffer = device_->createCommandBuffer();

  const igl::Viewport viewport = {0.0f, 0.0f, (float)width_, (float)height_, 0.0f, +1.0f};
  const igl::ScissorRect scissor = {0, 0, (uint32_t)width_, (uint32_t)height_};

  // This will clear the framebuffer
  buffer->cmdBeginRendering(renderPass_, framebuffer_);
  {
    buffer->cmdBindRenderPipelineState(renderPipelineState_Triangle_);
    buffer->cmdBindViewport(viewport);
    buffer->cmdBindScissorRect(scissor);
    buffer->cmdPushDebugGroupLabel("Render Triangle", igl::Color(1, 0, 0));
    buffer->cmdDraw(PrimitiveType::Triangle, 0, 3);
    buffer->cmdPopDebugGroupLabel();
  }
  buffer->cmdEndRendering();

  buffer->present(framebuffer_.colorAttachments[0].texture);

  device_->submit(*buffer, igl::CommandQueueType::Graphics, true);
}

int main(int argc, char* argv[]) {
  minilog::initialize(nullptr, {.threadNames = false});

  window_ = initWindow();
  vk.init();

  double prevTime = glfwGetTime();

  // Main loop
  while (!glfwWindowShouldClose(window_)) {
    const double newTime = glfwGetTime();
    fps_.tick(newTime - prevTime);
    prevTime = newTime;
    vk.render();
    glfwPollEvents();
  }

  // destroy all the Vulkan stuff before closing the window
  vk = {};

  glfwDestroyWindow(window_);
  glfwTerminate();

  return 0;
}
