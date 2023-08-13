/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shared/UtilsFPS.h>

#include <lvk/LVK.h>
#include <lvk/HelpersGLFW.h>

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

GLFWwindow* window_ = nullptr;
int width_ = 800;
int height_ = 600;
FramesPerSecondCounter fps_;

std::unique_ptr<lvk::IDevice> device_;

struct VulkanObjects {
  void init();
  void createFramebuffer();
  void render();
  lvk::Framebuffer fb_ = {};
  lvk::Holder<lvk::RenderPipelineHandle> renderPipelineState_Triangle_;
} vk;

void VulkanObjects::init() {
  device_ = lvk::createVulkanDeviceWithSwapchain(window_, width_, height_, {});

  createFramebuffer();

  renderPipelineState_Triangle_ = device_->createRenderPipeline(
      {
          .smVert = device_->createShaderModule({codeVS, lvk::Stage_Vert, "Shader Module: main (vert)"}).release(),
          .smFrag = device_->createShaderModule({codeFS, lvk::Stage_Frag, "Shader Module: main (frag)"}).release(),
          .color = {{device_->getFormat(fb_.color[0].texture)},
                    {device_->getFormat(fb_.color[1].texture)},
                    {device_->getFormat(fb_.color[2].texture)},
                    {device_->getFormat(fb_.color[3].texture)}},
      },
      nullptr);
  LVK_ASSERT(renderPipelineState_Triangle_.valid());
}

void VulkanObjects::createFramebuffer() {
  lvk::TextureHandle texSwapchain = device_->getCurrentSwapchainTexture();

  {
    const lvk::TextureDesc desc = {
        .type = lvk::TextureType_2D,
        .format = device_->getFormat(texSwapchain),
        .dimensions = device_->getDimensions(texSwapchain),
        .usage = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
    };

    fb_ = {.color = {{.texture = texSwapchain},
                     {.texture = device_->createTexture(desc, "Framebuffer C1").release()},
                     {.texture = device_->createTexture(desc, "Framebuffer C2").release()},
                     {.texture = device_->createTexture(desc, "Framebuffer C3").release()}}};
  }
}

void VulkanObjects::render() {
  if (!width_ || !height_) {
    return;
  }

  fb_.color[0].texture = device_->getCurrentSwapchainTexture();

  lvk::ICommandBuffer& buffer = device_->acquireCommandBuffer();

  // This will clear the framebuffer
  buffer.cmdBeginRendering(
      {.color = {{.loadOp = lvk::LoadOp_Clear, .clearColor = {1.0f, 1.0f, 1.0f, 1.0f}},
                 {.loadOp = lvk::LoadOp_Clear, .clearColor = {1.0f, 0.0f, 0.0f, 1.0f}},
                 {.loadOp = lvk::LoadOp_Clear, .clearColor = {0.0f, 1.0f, 0.0f, 1.0f}},
                 {.loadOp = lvk::LoadOp_Clear, .clearColor = {0.0f, 0.0f, 1.0f, 1.0f}}}},
      fb_);
  {
    buffer.cmdBindRenderPipeline(renderPipelineState_Triangle_);
    buffer.cmdBindViewport({0.0f, 0.0f, (float)width_, (float)height_, 0.0f, +1.0f});
    buffer.cmdBindScissorRect({0, 0, (uint32_t)width_, (uint32_t)height_});
    buffer.cmdPushDebugGroupLabel("Render Triangle", 0xff0000ff);
    buffer.cmdDraw(lvk::Primitive_Triangle, 0, 3);
    buffer.cmdPopDebugGroupLabel();
  }
  buffer.cmdEndRendering();
  device_->submit(buffer, fb_.color[0].texture);
}

int main(int argc, char* argv[]) {
  minilog::initialize(nullptr, {.threadNames = false});

  window_ = lvk::initWindow("Vulkan Triangle", width_, height_, true);
  vk.init();

  glfwSetWindowSizeCallback(window_, [](GLFWwindow*, int width, int height) {
    width_ = width;
    height_ = height;
    device_->recreateSwapchain(width_, height_);
    if (width && height) {
      vk.createFramebuffer();
    }
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
  device_->destroy(vk.fb_.color[1].texture);
  device_->destroy(vk.fb_.color[2].texture);
  device_->destroy(vk.fb_.color[3].texture);
  vk = {};

  device_ = nullptr;

  glfwDestroyWindow(window_);
  glfwTerminate();

  return 0;
}
