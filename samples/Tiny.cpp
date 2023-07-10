/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shared/UtilsFPS.h>

#include <lvk/LVK.h>
#include <lvk/HelpersGLFW.h>

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
uint32_t width_ = 800;
uint32_t height_ = 600;
FramesPerSecondCounter fps_;

struct VulkanObjects {
  void init();
  void createFramebuffer();
  void render();
  igl::Framebuffer fb_ = {};
  std::shared_ptr<IRenderPipelineState> renderPipelineState_Triangle_;
  std::unique_ptr<IDevice> device_;
} vk;

void VulkanObjects::init() {
  device_ = lvk::createVulkanDeviceWithSwapchain(
      window_, width_, height_, {.maxTextures = 8, .maxSamplers = 8});

  createFramebuffer();

  const RenderPipelineDesc desc = {
      .shaderStages = device_->createShaderStages(
          codeVS, "Shader Module: main (vert)", codeFS, "Shader Module: main (frag)"),
      .numColorAttachments = kNumColorAttachments,
      .colorAttachments = {
          {fb_.colorAttachments[0].texture->getFormat()},
          {fb_.colorAttachments[1].texture->getFormat()},
          {fb_.colorAttachments[2].texture->getFormat()},
          {fb_.colorAttachments[3].texture->getFormat()},
      }};

  renderPipelineState_Triangle_ = device_->createRenderPipeline(desc, nullptr);

  IGL_ASSERT(renderPipelineState_Triangle_.get());
}

void VulkanObjects::createFramebuffer() {
  auto texSwapchain = device_->getCurrentSwapchainTexture();

  fb_ = {
      .numColorAttachments = kNumColorAttachments,
      .colorAttachments = {{.texture = texSwapchain}},
  };

  for (uint32_t i = 1; i < kNumColorAttachments; i++) {
    char attachmentName[256] = {0};
    snprintf(attachmentName, sizeof(attachmentName) - 1, "%s C%u", fb_.debugName, i - 1);
    fb_.colorAttachments[i].texture = device_->createTexture(
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
}

void VulkanObjects::render() {
  fb_.colorAttachments[0].texture = device_->getCurrentSwapchainTexture();

  std::shared_ptr<ICommandBuffer> buffer = device_->createCommandBuffer();

  // This will clear the framebuffer
  buffer->cmdBeginRendering(
      {
          .numColorAttachments = kNumColorAttachments,
          .colorAttachments = {{.clearColor = {1.0f, 1.0f, 1.0f, 1.0f}},
                               {.clearColor = {1.0f, 0.0f, 0.0f, 1.0f}},
                               {.clearColor = {0.0f, 1.0f, 0.0f, 1.0f}},
                               {.clearColor = {0.0f, 0.0f, 1.0f, 1.0f}}},
      },
      fb_);
  {
    buffer->cmdBindRenderPipelineState(renderPipelineState_Triangle_);
    buffer->cmdBindViewport({0.0f, 0.0f, (float)width_, (float)height_, 0.0f, +1.0f});
    buffer->cmdBindScissorRect({0, 0, width_, height_});
    buffer->cmdPushDebugGroupLabel("Render Triangle", igl::Color(1, 0, 0));
    buffer->cmdDraw(PrimitiveType::Triangle, 0, 3);
    buffer->cmdPopDebugGroupLabel();
  }
  buffer->cmdEndRendering();

  buffer->present(fb_.colorAttachments[0].texture);

  device_->submit(*buffer, igl::CommandQueueType::Graphics, true);
}

int main(int argc, char* argv[]) {
  minilog::initialize(nullptr, {.threadNames = false});

  window_ = lvk::initWindow("Vulkan Triangle", width_, height_);
  vk.init();

  glfwSetWindowSizeCallback(window_, [](GLFWwindow*, int width, int height) {
    printf("Window resized! width=%d, height=%d\n", width, height);
    width_ = width;
    height_ = height;
    vulkan::Device* vulkanDevice = static_cast<vulkan::Device*>(vk.device_.get());
    vulkanDevice->getVulkanContext().initSwapchain(width_, height_);
    vk.createFramebuffer();
  });

  double prevTime = glfwGetTime();

  // main loop
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
