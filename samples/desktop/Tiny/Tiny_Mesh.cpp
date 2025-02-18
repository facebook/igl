/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <GLFW/glfw3.h>
#include <cassert>
#if !defined(_USE_MATH_DEFINES)
#define _USE_MATH_DEFINES
#endif // _USE_MATH_DEFINES
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <stdio.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#elif __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#else
#error Unsupported OS
#endif
#include <GLFW/glfw3native.h>
#include <glm/ext.hpp>
#include <glm/gtc/random.hpp>
#include <igl/FPSCounter.h>
#include <igl/IGL.h>
#include <igl/ShaderCreator.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/HWDevice.h>
#include <igl/vulkan/PlatformDevice.h>
#include <igl/vulkan/VulkanContext.h>
#include <stb/stb_image.h>

#define TINY_TEST_USE_DEPTH_BUFFER 1

constexpr uint32_t kNumCubes = 16;

#if IGL_WITH_IGLU
#include <IGLU/imgui/Session.h>

std::unique_ptr<iglu::imgui::Session> imguiSession_;

igl::shell::InputDispatcher inputDispatcher_;
#endif // IGL_WITH_IGLU

const char* codeVS = R"(
layout (location=0) in vec3 pos;
layout (location=1) in vec3 col;
layout (location=2) in vec2 st;
layout (location=0) out vec3 color;
layout (location=1) out vec2 uv;

layout (set = 1, binding = 0, std140) uniform UniformsPerFrame {
  mat4 proj;
  mat4 view;
} perFrame;

layout (set = 1, binding = 1, std140) uniform UniformsPerObject {
  mat4 model;
} perObject;

void main() {
  mat4 proj = perFrame.proj;
  mat4 view = perFrame.view;
  mat4 model = perObject.model;
  gl_Position = proj * view * model * vec4(pos, 1.0);
  color = col;
  uv = st;
}
)";

const char* codeFS = R"(
layout (location=0) in vec3 color;
layout (location=1) in vec2 uv;
layout (location=0) out vec4 out_FragColor;

layout (set = 0, binding = 0) uniform sampler2D uTex0;
layout (set = 0, binding = 1) uniform sampler2D uTex1;

void main() {
  vec4 t0 = texture(uTex0, 2.0 * uv);
  vec4 t1 = texture(uTex1,  uv);
  out_FragColor = vec4(color * (t0.rgb + t1.rgb), 1.0);
};
)";

using namespace igl;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

vec3 axis_[kNumCubes];

GLFWwindow* window_ = nullptr;
int width_ = 0;
int height_ = 0;
igl::FPSCounter fps_;

constexpr uint32_t kNumBufferedFrames = 3;

std::unique_ptr<IDevice> device_;
std::shared_ptr<ICommandQueue> commandQueue_;
RenderPassDesc renderPass_;
FramebufferDesc framebufferDesc_;
std::shared_ptr<IFramebuffer> framebuffer_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_Mesh_;
std::shared_ptr<IBuffer> vb0_, ib0_; // buffers for vertices and indices
std::vector<std::shared_ptr<IBuffer>> ubPerFrame_, ubPerObject_;
std::shared_ptr<IVertexInputState> vertexInput0_;
std::shared_ptr<IDepthStencilState> depthStencilState_;
std::shared_ptr<ITexture> texture0_, texture1_;
std::shared_ptr<ISamplerState> sampler_;

struct VertexPosUvw {
  vec3 position;
  vec3 color;
  vec2 uv;
};

struct UniformsPerFrame {
  mat4 proj;
  mat4 view;
};
struct UniformsPerObject {
  mat4 model;
};

const float half = 1.0f;

// UV-mapped cube with indices: 24 vertices, 36 indices
static VertexPosUvw vertexData0[] = {
    // top
    {{-half, -half, +half}, {0.0, 0.0, 1.0}, {0, 0}}, // 0
    {{+half, -half, +half}, {1.0, 0.0, 1.0}, {1, 0}}, // 1
    {{+half, +half, +half}, {1.0, 1.0, 1.0}, {1, 1}}, // 2
    {{-half, +half, +half}, {0.0, 1.0, 1.0}, {0, 1}}, // 3
    // bottom
    {{-half, -half, -half}, {1.0, 1.0, 1.0}, {0, 0}}, // 4
    {{-half, +half, -half}, {0.0, 1.0, 0.0}, {0, 1}}, // 5
    {{+half, +half, -half}, {1.0, 1.0, 0.0}, {1, 1}}, // 6
    {{+half, -half, -half}, {1.0, 0.0, 0.0}, {1, 0}}, // 7
    // left
    {{+half, +half, -half}, {1.0, 1.0, 0.0}, {1, 0}}, // 8
    {{-half, +half, -half}, {0.0, 1.0, 0.0}, {0, 0}}, // 9
    {{-half, +half, +half}, {0.0, 1.0, 1.0}, {0, 1}}, // 10
    {{+half, +half, +half}, {1.0, 1.0, 1.0}, {1, 1}}, // 11
    // right
    {{-half, -half, -half}, {1.0, 1.0, 1.0}, {0, 0}}, // 12
    {{+half, -half, -half}, {1.0, 0.0, 0.0}, {1, 0}}, // 13
    {{+half, -half, +half}, {1.0, 0.0, 1.0}, {1, 1}}, // 14
    {{-half, -half, +half}, {0.0, 0.0, 1.0}, {0, 1}}, // 15
    // front
    {{+half, -half, -half}, {1.0, 0.0, 0.0}, {0, 0}}, // 16
    {{+half, +half, -half}, {1.0, 1.0, 0.0}, {1, 0}}, // 17
    {{+half, +half, +half}, {1.0, 1.0, 1.0}, {1, 1}}, // 18
    {{+half, -half, +half}, {1.0, 0.0, 1.0}, {0, 1}}, // 19
    // back
    {{-half, +half, -half}, {0.0, 1.0, 0.0}, {1, 0}}, // 20
    {{-half, -half, -half}, {1.0, 1.0, 1.0}, {0, 0}}, // 21
    {{-half, -half, +half}, {0.0, 0.0, 1.0}, {0, 1}}, // 22
    {{-half, +half, +half}, {0.0, 1.0, 1.0}, {1, 1}}, // 23
};

static uint16_t indexData[] = {0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,
                               8,  9,  10, 10, 11, 8,  12, 13, 14, 14, 15, 12,
                               16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};

UniformsPerFrame perFrame;
UniformsPerObject perObject[kNumCubes];

static bool initWindow(GLFWwindow** outWindow) {
  if (!glfwInit())
    return false;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  GLFWwindow* window = glfwCreateWindow(1280, 1024, "Vulkan Mesh", nullptr, nullptr);

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
    if (key == GLFW_KEY_T && action == GLFW_PRESS) {
      texture1_.reset();
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

#if IGL_WITH_IGLU
  glfwSetCursorPosCallback(window, [](auto* window, double x, double y) {
    inputDispatcher_.queueEvent(igl::shell::MouseMotionEvent(x, y, 0, 0));
  });
  glfwSetMouseButtonCallback(window, [](auto* window, int button, int action, int mods) {
    double xpos = 0.0, ypos = 0.0;
    glfwGetCursorPos(window, &xpos, &ypos);
    using igl::shell::MouseButton;
    const MouseButton iglButton =
        (button == GLFW_MOUSE_BUTTON_LEFT)
            ? MouseButton::Left
            : (button == GLFW_MOUSE_BUTTON_RIGHT ? MouseButton::Right : MouseButton::Middle);
    inputDispatcher_.queueEvent(
        igl::shell::MouseButtonEvent(iglButton, action == GLFW_PRESS, (float)xpos, (float)ypos));
  });
#endif // IGL_WITH_IGLU

  glfwGetWindowSize(window, &width_, &height_);

  if (outWindow) {
    *outWindow = window;
  }

  return true;
}

static void initIGL() {
  // create a device
  {
    const igl::vulkan::VulkanContextConfig cfg = {
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
    IGL_DEBUG_ASSERT(device_);
  }

  // Vertex buffer, Index buffer and Vertex Input. Buffers are allocated in GPU memory.
  vb0_ = device_->createBuffer(BufferDesc(BufferDesc::BufferTypeBits::Vertex,
                                          vertexData0,
                                          sizeof(vertexData0),
                                          ResourceStorage::Private,
                                          0,
                                          "Buffer: vertex"),
                               nullptr);
  ib0_ = device_->createBuffer(BufferDesc(BufferDesc::BufferTypeBits::Index,
                                          indexData,
                                          sizeof(indexData),
                                          ResourceStorage::Private,
                                          0,
                                          "Buffer: index"),
                               nullptr);
  // create an Uniform buffers to store uniforms for 2 objects
  for (uint32_t i = 0; i != kNumBufferedFrames; i++) {
    ubPerFrame_.push_back(device_->createBuffer(BufferDesc(BufferDesc::BufferTypeBits::Uniform,
                                                           &perFrame,
                                                           sizeof(UniformsPerFrame),
                                                           ResourceStorage::Shared,
                                                           0,
                                                           "Buffer: uniforms (per frame)"),
                                                nullptr));
    ubPerObject_.push_back(device_->createBuffer(BufferDesc(BufferDesc::BufferTypeBits::Uniform,
                                                            perObject,
                                                            kNumCubes * sizeof(UniformsPerObject),
                                                            ResourceStorage::Shared,
                                                            0,
                                                            "Buffer: uniforms (per object)"),
                                                 nullptr));
  }

  {
    VertexInputStateDesc desc;
    desc.numAttributes = 3;
    desc.attributes[0].format = VertexAttributeFormat::Float3;
    desc.attributes[0].offset = offsetof(VertexPosUvw, position);
    desc.attributes[0].bufferIndex = 0;
    desc.attributes[0].location = 0;
    desc.attributes[1].format = VertexAttributeFormat::Float3;
    desc.attributes[1].offset = offsetof(VertexPosUvw, color);
    desc.attributes[1].bufferIndex = 0;
    desc.attributes[1].location = 1;
    desc.attributes[2].format = VertexAttributeFormat::Float2;
    desc.attributes[2].offset = offsetof(VertexPosUvw, uv);
    desc.attributes[2].bufferIndex = 0;
    desc.attributes[2].location = 2;
    desc.numInputBindings = 1;
    desc.inputBindings[0].stride = sizeof(VertexPosUvw);
    vertexInput0_ = device_->createVertexInputState(desc, nullptr);
  }

  {
    DepthStencilStateDesc desc;
    desc.isDepthWriteEnabled = true;
    desc.compareFunction = igl::CompareFunction::Less;
    depthStencilState_ = device_->createDepthStencilState(desc, nullptr);
  }

  {
    const uint32_t texWidth = 256;
    const uint32_t texHeight = 256;
    const TextureDesc desc = TextureDesc::new2D(igl::TextureFormat::BGRA_UNorm8,
                                                texWidth,
                                                texHeight,
                                                TextureDesc::TextureUsageBits::Sampled,
                                                "XOR pattern");
    texture0_ = device_->createTexture(desc, nullptr);
    std::vector<uint32_t> pixels(texWidth * texHeight);
    for (uint32_t y = 0; y != texHeight; y++) {
      for (uint32_t x = 0; x != texWidth; x++) {
        // create a XOR pattern
        pixels[y * texWidth + x] = 0xFF000000 + ((x ^ y) << 16) + ((x ^ y) << 8) + (x ^ y);
      }
    }
    texture0_->upload(TextureRangeDesc::new2D(0, 0, texWidth, texHeight), pixels.data());
  }
  {
    using namespace std::filesystem;
    path dir = current_path();
    // find IGLU somewhere above our current directory
    // @fb-only
    const char* contentFolder = "third-party/content/src/";
    // @fb-only
    while (dir != current_path().root_path() && !exists(dir / path(contentFolder))) {
      dir = dir.parent_path();
    }
    int32_t texWidth = 0;
    int32_t texHeight = 0;
    int32_t channels = 0;
    uint8_t* pixels = stbi_load(
        (dir / path(contentFolder) / path("bistro/BuildingTextures/wood_polished_01_diff.png"))
            .string()
            .c_str(),
        &texWidth,
        &texHeight,
        &channels,
        4);
    IGL_DEBUG_ASSERT(pixels,
                     "Cannot load textures. Run `deploy_content.py` before running this app.");
    const TextureDesc desc = TextureDesc::new2D(igl::TextureFormat::RGBA_UNorm8,
                                                texWidth,
                                                texHeight,
                                                TextureDesc::TextureUsageBits::Sampled,
                                                "wood_polished_01_diff.png");
    texture1_ = device_->createTexture(desc, nullptr);
    texture1_->upload(TextureRangeDesc::new2D(0, 0, texWidth, texHeight), pixels);
    stbi_image_free(pixels);
  }
  {
    igl::SamplerStateDesc desc = igl::SamplerStateDesc::newLinear();
    desc.addressModeU = igl::SamplerAddressMode::Repeat;
    desc.addressModeV = igl::SamplerAddressMode::Repeat;
    desc.debugName = "Sampler: linear";
    sampler_ = device_->createSamplerState(desc, nullptr);
  }

  // Command queue: backed by different types of GPU HW queues
  CommandQueueDesc desc{};
  commandQueue_ = device_->createCommandQueue(desc, nullptr);

  renderPass_.colorAttachments.push_back(igl::RenderPassDesc::ColorAttachmentDesc{});
  renderPass_.colorAttachments.back().loadAction = LoadAction::Clear;
  renderPass_.colorAttachments.back().storeAction = StoreAction::Store;
  renderPass_.colorAttachments.back().clearColor = {1.0f, 0.0f, 0.0f, 1.0f};
#if TINY_TEST_USE_DEPTH_BUFFER
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass_.depthAttachment.storeAction =
      StoreAction::Store; // save it so we can display it via ImGui
  renderPass_.depthAttachment.clearDepth = 1.0;
#else
  renderPass_.depthAttachment.loadAction = LoadAction::DontCare;
#endif // TINY_TEST_USE_DEPTH_BUFFER

  // initialize random rotation axes for all cubes
  for (uint32_t i = 0; i != kNumCubes; i++) {
    axis_[i] = glm::sphericalRand(1.0f);
  }
}

static void createRenderPipeline() {
  if (renderPipelineState_Mesh_) {
    return;
  }

  IGL_DEBUG_ASSERT(framebuffer_);

  RenderPipelineDesc desc;

  desc.targetDesc.colorAttachments.resize(1);
  desc.targetDesc.colorAttachments[0].textureFormat =
      framebuffer_->getColorAttachment(0)->getFormat();

  if (framebuffer_->getDepthAttachment()) {
    desc.targetDesc.depthAttachmentFormat = framebuffer_->getDepthAttachment()->getFormat();
  }

  desc.vertexInputState = vertexInput0_;
  desc.shaderStages = ShaderStagesCreator::fromModuleStringInput(
      *device_, codeVS, "main", "", codeFS, "main", "", nullptr);

#if !TINY_TEST_USE_DEPTH_BUFFER
  desc.cullMode = igl::CullMode::Back;
#endif // TINY_TEST_USE_DEPTH_BUFFER

  desc.frontFaceWinding = igl::WindingMode::Clockwise;
  desc.debugName = igl::genNameHandle("Pipeline: mesh");
  renderPipelineState_Mesh_ = device_->createRenderPipeline(desc, nullptr);
}

static std::shared_ptr<ITexture> getVulkanNativeDrawable() {
  const auto& vkPlatformDevice = device_->getPlatformDevice<igl::vulkan::PlatformDevice>();

  IGL_DEBUG_ASSERT(vkPlatformDevice != nullptr);

  Result ret;
  std::shared_ptr<ITexture> drawable = vkPlatformDevice->createTextureFromNativeDrawable(&ret);

  IGL_DEBUG_ASSERT(ret.isOk());
  return drawable;
}

static std::shared_ptr<ITexture> getVulkanNativeDepth() {
  const auto& vkPlatformDevice = device_->getPlatformDevice<igl::vulkan::PlatformDevice>();

  IGL_DEBUG_ASSERT(vkPlatformDevice != nullptr);

  Result ret;
  std::shared_ptr<ITexture> drawable =
      vkPlatformDevice->createTextureFromNativeDepth(width_, height_, &ret);

  IGL_DEBUG_ASSERT(ret.isOk());
  return drawable;
}

static void createFramebuffer(const std::shared_ptr<ITexture>& nativeDrawable) {
  framebufferDesc_.colorAttachments[0].texture = nativeDrawable;

#if TINY_TEST_USE_DEPTH_BUFFER
  framebufferDesc_.depthAttachment.texture = getVulkanNativeDepth();
#endif // TINY_TEST_USE_DEPTH_BUFFER

  framebuffer_ = device_->createFramebuffer(framebufferDesc_, nullptr);
  IGL_DEBUG_ASSERT(framebuffer_);
}

static void render(const std::shared_ptr<ITexture>& nativeDrawable, uint32_t frameIndex) {
  IGL_PROFILER_FUNCTION();

  if (!nativeDrawable) {
    return;
  }

#if IGL_WITH_IGLU
  imguiSession_->beginFrame(framebufferDesc_, 1.0f);

  ImGui::Begin("Texture Viewer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Image(ImTextureID(texture1_.get()), ImVec2(512, 512));
  ImGui::End();

  inputDispatcher_.processEvents();
#endif // IGL_WITH_IGLU

  const auto size = framebuffer_->getColorAttachment(0)->getSize();
  if (size.width != width_ || size.height != height_) {
    createFramebuffer(nativeDrawable);
  } else {
    framebuffer_->updateDrawable(nativeDrawable);
  }

  // from igl/shell/renderSessions/Textured3DCubeSession.cpp
  const float fov = float(45.0f * (M_PI / 180.0f));
  const float aspectRatio = (float)width_ / (float)height_;
  perFrame.proj = glm::perspectiveLH(fov, aspectRatio, 0.1f, 500.0f);
  // place a "camera" behind the cubes, the distance depends on the total number of cubes
  perFrame.view =
      glm::translate(mat4(1.0f), vec3(0.0f, 0.0f, sqrtf(kNumCubes / 16) * 20.0f * half));
  ubPerFrame_[frameIndex]->upload(&perFrame, igl::BufferRange(sizeof(perFrame)));

  // rotate cubes around random axes
  for (uint32_t i = 0; i != kNumCubes; i++) {
    const float direction = powf(-1, (float)(i + 1));
    const uint32_t cubesInLine = (uint32_t)sqrt(kNumCubes);
    const vec3 offset = vec3(-1.5f * sqrt(kNumCubes) + 4.0f * (i % cubesInLine),
                             -1.5f * sqrt(kNumCubes) + 4.0f * (i / cubesInLine),
                             0);
    perObject[i].model =
        glm::rotate(glm::translate(mat4(1.0f), offset), direction * (float)glfwGetTime(), axis_[i]);
  }

  ubPerObject_[frameIndex]->upload(&perObject, igl::BufferRange(sizeof(perObject)));

  // Command buffers (1-N per thread): create, submit and forget
  CommandBufferDesc cbDesc;
  std::shared_ptr<ICommandBuffer> buffer = commandQueue_->createCommandBuffer(cbDesc, nullptr);

  const igl::Viewport viewport = {0.0f, 0.0f, (float)width_, (float)height_, 0.0f, +1.0f};
  const igl::ScissorRect scissor = {0, 0, (uint32_t)width_, (uint32_t)height_};

  // This will clear the framebuffer
  auto commands = buffer->createRenderCommandEncoder(renderPass_, framebuffer_);

  commands->bindRenderPipelineState(renderPipelineState_Mesh_);
  commands->bindViewport(viewport);
  commands->bindScissorRect(scissor);
  commands->pushDebugGroupLabel("Render Mesh", igl::Color(1, 0, 0));
  commands->bindVertexBuffer(0, *vb0_);
  commands->bindDepthStencilState(depthStencilState_);
  commands->bindBuffer(0, ubPerFrame_[frameIndex].get());
  commands->bindTexture(0, igl::BindTarget::kFragment, texture0_.get());
  commands->bindTexture(1, igl::BindTarget::kFragment, texture1_.get());
  commands->bindSamplerState(0, igl::BindTarget::kFragment, sampler_.get());
  commands->bindSamplerState(1, igl::BindTarget::kFragment, sampler_.get());
  // Draw 2 cubes: we use uniform buffer to update matrices
  commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16);
  for (uint32_t i = 0; i != kNumCubes; i++) {
    commands->bindBuffer(1, ubPerObject_[frameIndex].get(), i * sizeof(UniformsPerObject));
    commands->drawIndexed(3u * 6u * 2u);
  }
  commands->popDebugGroupLabel();
#if IGL_WITH_IGLU
  imguiSession_->drawFPS(fps_.getAverageFPS());
  imguiSession_->endFrame(*device_.get(), *commands);
#endif // IGL_WITH_IGLU
  commands->endEncoding();

  buffer->present(nativeDrawable);

  commandQueue_->submit(*buffer);
}

int main(int argc, char* argv[]) {
  initWindow(&window_);
  initIGL();

  createFramebuffer(getVulkanNativeDrawable());
  createRenderPipeline();

#if IGL_WITH_IGLU
  imguiSession_ = std::make_unique<iglu::imgui::Session>(*device_.get(), inputDispatcher_);
#endif // IGL_WITH_IGLU

  double prevTime = glfwGetTime();

  uint32_t frameIndex = 0;

  // Main loop
  while (!glfwWindowShouldClose(window_)) {
    const double newTime = glfwGetTime();
    fps_.updateFPS(newTime - prevTime);
    prevTime = newTime;
    render(getVulkanNativeDrawable(), frameIndex);
    glfwPollEvents();
    frameIndex = (frameIndex + 1) % kNumBufferedFrames;
  }

#if IGL_WITH_IGLU
  imguiSession_ = nullptr;
#endif // IGL_WITH_IGLU

  // destroy all the Vulkan stuff before closing the window
  vb0_ = nullptr;
  ib0_ = nullptr;
  ubPerFrame_.clear();
  ubPerObject_.clear();
  renderPipelineState_Mesh_ = nullptr;
  texture0_ = nullptr;
  texture1_ = nullptr;
  sampler_ = nullptr;
  framebufferDesc_ = {};
  framebuffer_ = nullptr;
  device_.reset(nullptr);

  glfwDestroyWindow(window_);
  glfwTerminate();

  return 0;
}
