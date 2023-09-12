/*
* LightweightVK
*
* This source code is licensed under the MIT license found in the
* LICENSE file in the root directory of this source tree.
*/

#include <shared/UtilsFPS.h>

#include <lvk/LVK.h>
#include <GLFW/glfw3.h>

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
int width_ = 800;
int height_ = 600;
FramesPerSecondCounter fps_;

lvk::Holder<lvk::RenderPipelineHandle> renderPipelineState_Triangle_;
std::unique_ptr<lvk::IContext> ctx_;

void render() {
  if (!width_ || !height_) {
    return;
  }

  lvk::ICommandBuffer& buffer = ctx_->acquireCommandBuffer();

  // This will clear the framebuffer
  buffer.cmdBeginRendering(
      {.color = {{.loadOp = lvk::LoadOp_Clear, .clearColor = {1.0f, 1.0f, 1.0f, 1.0f}}}},
      {.color = {{.texture = ctx_->getCurrentSwapchainTexture()}}});
  buffer.cmdBindRenderPipeline(renderPipelineState_Triangle_);
  buffer.cmdPushDebugGroupLabel("Render Triangle", 0xff0000ff);
  buffer.cmdDraw(lvk::Primitive_Triangle, 3);
  buffer.cmdPopDebugGroupLabel();
  buffer.cmdEndRendering();
  ctx_->submit(buffer, ctx_->getCurrentSwapchainTexture());
}

int main(int argc, char* argv[]) {
  minilog::initialize(nullptr, {.threadNames = false});

  window_ = lvk::initWindow("Vulkan Hello Triangle", width_, height_, true);

  ctx_ = lvk::createVulkanContextWithSwapchain(window_, width_, height_, {});

  lvk::Holder<lvk::ShaderModuleHandle> vert = ctx_->createShaderModule({codeVS, lvk::Stage_Vert, "Shader Module: main (vert)"});
  lvk::Holder<lvk::ShaderModuleHandle> frag = ctx_->createShaderModule({codeFS, lvk::Stage_Frag, "Shader Module: main (frag)"});

  renderPipelineState_Triangle_ = ctx_->createRenderPipeline(
      {
          .smVert = vert,
          .smFrag = frag,
          .color = {{.format = ctx_->getSwapchainFormat()}},
      },
      nullptr);

  LVK_ASSERT(renderPipelineState_Triangle_.valid());

  glfwSetWindowSizeCallback(window_, [](GLFWwindow*, int width, int height) {
    width_ = width;
    height_ = height;
    ctx_->recreateSwapchain(width_, height_);
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
  vert = nullptr;
  frag = nullptr;
  renderPipelineState_Triangle_ = nullptr;
  ctx_ = nullptr;

  glfwDestroyWindow(window_);
  glfwTerminate();

  return 0;
}
