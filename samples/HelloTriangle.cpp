/*
* LightweightVK
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
layout (location=0) out vec4 out_FragColor;

void main() {
	out_FragColor = vec4(color, 1.0);
};
)";

GLFWwindow* window_ = nullptr;
uint32_t width_ = 800;
uint32_t height_ = 600;
FramesPerSecondCounter fps_;

lvk::Holder<lvk::RenderPipelineHandle> renderPipelineState_Triangle_;
std::unique_ptr<lvk::IDevice> device_;

void render() {
  if (!width_ || !height_) {
    return;
  }

  std::shared_ptr<lvk::ICommandBuffer> buffer = device_->createCommandBuffer();

  // This will clear the framebuffer
  buffer->cmdBeginRendering(
      {.colorAttachments = {{.loadOp = lvk::LoadOp_Clear, .clearColor = {1.0f, 1.0f, 1.0f, 1.0f}}}},
      {.colorAttachments = {{.texture = device_->getCurrentSwapchainTexture()}}});
  buffer->cmdBindRenderPipeline(renderPipelineState_Triangle_);
  buffer->cmdPushDebugGroupLabel("Render Triangle", lvk::Color(1, 0, 0));
  buffer->cmdDraw(lvk::PrimitiveType::Triangle, 0, 3);
  buffer->cmdPopDebugGroupLabel();
  buffer->cmdEndRendering();
  device_->submit(
      *buffer, lvk::CommandQueueType::Graphics, device_->getCurrentSwapchainTexture().get());
}

int main(int argc, char* argv[]) {
  minilog::initialize(nullptr, {.threadNames = false});

  window_ = lvk::initWindow("Vulkan Hello Triangle", width_, height_);

  device_ = lvk::createVulkanDeviceWithSwapchain(window_, width_, height_, {});
  renderPipelineState_Triangle_ = device_->createRenderPipeline(
      {.shaderStages = device_->createShaderStages(
           codeVS, "Shader Module: main (vert)", codeFS, "Shader Module: main (frag)"),
       .colorAttachments = {{.textureFormat = device_->getCurrentSwapchainTexture()->getFormat()}}},
      nullptr);

  IGL_ASSERT(renderPipelineState_Triangle_.valid());

  glfwSetWindowSizeCallback(window_, [](GLFWwindow*, int width, int height) {
    width_ = width;
    height_ = height;
    lvk::vulkan::Device* vulkanDevice = static_cast<lvk::vulkan::Device*>(device_.get());
    vulkanDevice->getVulkanContext().initSwapchain(width_, height_);
  });

  double prevTime = glfwGetTime();

  // main loop
  while (!glfwWindowShouldClose(window_)) {
    const double newTime = glfwGetTime();
    fps_.tick(newTime - prevTime);
    prevTime = newTime;
    render();
    glfwPollEvents();
  }

  // destroy all the Vulkan stuff before closing the window
  renderPipelineState_Triangle_ = nullptr;
  device_ = nullptr;

  glfwDestroyWindow(window_);
  glfwTerminate();

  return 0;
}
