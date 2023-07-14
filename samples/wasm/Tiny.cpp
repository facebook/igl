/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <emscripten.h>
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>
#include <igl/FPSCounter.h>
#include <igl/IGL.h>
#include <igl/ShaderCreator.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/TextureBufferExternal.h>
#include <igl/opengl/webgl/Context.h>
#include <igl/opengl/webgl/Device.h>
#include <igl/opengl/webgl/PlatformDevice.h>

using namespace igl;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

constexpr uint32_t kNumCubes = 16;
constexpr uint32_t kNumBufferedFrames = 3;
constexpr const char* codeVS = R"(#version 300 es
precision mediump float;

layout (location=0) in vec3 pos;
layout (location=1) in vec3 col;
layout (location=2) in vec2 st;
out vec3 color;
out vec2 uv;

layout(std140) uniform perFrame {
  mat4 proj;
  mat4 view;
};

layout(std140) uniform perObject {
  mat4 model;
};

void main() {
  mat4 proj = proj;
  mat4 view = view;
  mat4 model = model;
  gl_Position = proj * view * model * vec4(pos, 1.0);
  color = col;
  uv = st;
}
)";

constexpr const char* codeFS = R"(#version 300 es
precision mediump float;

in vec3 color;
in vec2 uv;
out vec4 out_FragColor;

uniform sampler2D texture0;
uniform sampler2D texture1;

void main() {

  vec4 t0 = texture(texture0, 2.0*uv);
  vec4 t1 = texture(texture1, uv);
  out_FragColor = vec4(color * (t0.rgb + t1.rgb), 1.0);
}
)";

vec3 axis_[kNumCubes];
std::shared_ptr<igl::opengl::webgl::Device> device_;
const char* canvas = "#canvas";
int width_ = 1024;
int height_ = 768;
igl::FPSCounter fps_;

std::shared_ptr<igl::IFramebuffer> framebuffer_;
std::shared_ptr<ICommandQueue> commandQueue_;
RenderPassDesc renderPass_;
FramebufferDesc framebufferDesc_;
std::shared_ptr<IRenderPipelineState> renderPipelineState_Mesh_;
std::shared_ptr<IBuffer> vb0_, ib0_;
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
struct alignas(256) UniformsPerObject {
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

static std::shared_ptr<ITexture> getNativeDrawable() {
  Result ret;
  auto platformDevice = static_cast<igl::IDevice*>(device_.get())
                            ->getPlatformDevice<igl::opengl::webgl::PlatformDevice>();

  IGL_ASSERT(platformDevice != nullptr);

  std::shared_ptr<ITexture> drawable = platformDevice->createTextureFromNativeDrawable(&ret);

  IGL_ASSERT_MSG(ret.isOk(), ret.message.c_str());
  IGL_ASSERT(drawable != nullptr);

  return drawable;
}

static void createFramebuffer(const std::shared_ptr<ITexture>& nativeDrawable) {
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = nativeDrawable;
  framebuffer_ = device_->createFramebuffer(framebufferDesc, nullptr);

  IGL_ASSERT(framebuffer_);
}

bool initialize() {
  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.majorVersion = 3;
  attrs.minorVersion = 0;
  attrs.premultipliedAlpha = false;
  attrs.alpha = false;
  attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_DEFAULT;
  device_ = std::make_unique<igl::opengl::webgl::Device>(
      std::make_unique<::igl::opengl::webgl::Context>(attrs, canvas, width_, height_));

  auto webGLcontext =
      reinterpret_cast<igl::opengl::webgl::Context*>(&device_->getContext())->getWebGLContext();
  emscripten_webgl_get_drawing_buffer_size(webGLcontext, &width_, &height_);

  auto platformDevice = static_cast<igl::IDevice*>(device_.get())
                            ->getPlatformDevice<igl::opengl::webgl::PlatformDevice>();
  auto nativeDrawable = platformDevice->createTextureFromNativeDrawable(nullptr);

  igl::TextureDesc depthDesc =
      igl::TextureDesc::new2D(igl::TextureFormat::Z_UNorm24, width_, height_, 0);
  depthDesc.usage = igl::TextureDesc::TextureUsageBits::Attachment;
  depthDesc.storage = igl::ResourceStorage::Private;
  auto depthTexture = device_->createTexture(depthDesc, nullptr);

  createFramebuffer(getNativeDrawable());

  renderPass_.colorAttachments.resize(1);
  renderPass_.colorAttachments[0] = igl::RenderPassDesc::ColorAttachmentDesc{};
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = {1.0f, 0.0f, 1.0f, 1.0f};
  renderPass_.depthAttachment.clearDepth = 1.0;

  renderPass_.depthAttachment.loadAction = LoadAction::DontCare;

  CommandQueueDesc desc{CommandQueueType::Graphics};
  commandQueue_ = device_->createCommandQueue(desc, nullptr);

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
    ubPerFrame_.push_back(
        device_->createBuffer(BufferDesc(BufferDesc::BufferTypeBits::Uniform,
                                         &perFrame,
                                         sizeof(UniformsPerFrame),
                                         ResourceStorage::Shared,
                                         BufferDesc::BufferAPIHintBits::UniformBlock,
                                         "Buffer: uniforms (per frame)"),
                              nullptr));

    ubPerObject_.push_back(
        device_->createBuffer(BufferDesc(BufferDesc::BufferTypeBits::Uniform,
                                         perObject,
                                         kNumCubes * sizeof(UniformsPerObject),
                                         ResourceStorage::Shared,
                                         BufferDesc::BufferAPIHintBits::UniformBlock,
                                         "Buffer: uniforms (per object)"),
                              nullptr));
  }

  {
    VertexInputStateDesc desc;
    desc.numAttributes = 3;
    desc.attributes[0].format = VertexAttributeFormat::Float3;
    desc.attributes[0].offset = offsetof(VertexPosUvw, position);
    desc.attributes[0].bufferIndex = 0;
    desc.attributes[0].name = "pos";
    desc.attributes[0].location = 0;
    desc.attributes[1].format = VertexAttributeFormat::Float3;
    desc.attributes[1].offset = offsetof(VertexPosUvw, color);
    desc.attributes[1].bufferIndex = 0;
    desc.attributes[1].name = "col";
    desc.attributes[1].location = 1;
    desc.attributes[2].format = VertexAttributeFormat::Float2;
    desc.attributes[2].offset = offsetof(VertexPosUvw, uv);
    desc.attributes[2].bufferIndex = 0;
    desc.attributes[2].name = "st";
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
    const TextureDesc desc = TextureDesc::new2D(igl::TextureFormat::RGBA_UNorm8,
                                                texWidth,
                                                texHeight,
                                                TextureDesc::TextureUsageBits::Sampled,
                                                "XOR pattern 1");
    texture0_ = device_->createTexture(desc, nullptr);
    std::vector<uint32_t> pixels(texWidth * texHeight);
    for (uint32_t y = 0; y != texHeight; y++) {
      for (uint32_t x = 0; x != texWidth; x++) {
        // create a XOR pattern 1
        pixels[y * texWidth + x] = 0xFF000000 + ((x ^ y) << 16) + ((x ^ y) << 8) + (x ^ y);
      }
    }
    texture0_->upload(TextureRangeDesc::new2D(0, 0, texWidth, texHeight), pixels.data());
  }

  {
    const uint32_t texWidth = 256;
    const uint32_t texHeight = 256;
    const TextureDesc desc = TextureDesc::new2D(igl::TextureFormat::RGBA_UNorm8,
                                                texWidth,
                                                texHeight,
                                                TextureDesc::TextureUsageBits::Sampled,
                                                "XOR pattern 2");
    texture1_ = device_->createTexture(desc, nullptr);
    std::vector<uint32_t> pixels(texWidth * texHeight);
    for (uint32_t y = 0; y != texHeight; y++) {
      for (uint32_t x = 0; x != texWidth; x++) {
        // create a XOR pattern 2
        pixels[y * texWidth + x] = 0x00FF0000 + ((x ^ y) << 16) + ((x ^ y) << 8) + (x ^ y);
      }
    }
    texture1_->upload(TextureRangeDesc::new2D(0, 0, texWidth, texHeight), pixels.data());
  }

  {
    igl::SamplerStateDesc desc = igl::SamplerStateDesc::newLinear();
    desc.addressModeU = igl::SamplerAddressMode::Repeat;
    desc.addressModeV = igl::SamplerAddressMode::Repeat;
    desc.debugName = "Sampler: linear";
    sampler_ = device_->createSamplerState(desc, nullptr);
  }

  // initialize random rotation axes for all cubes
  for (uint32_t i = 0; i != kNumCubes; i++) {
    axis_[i] = glm::sphericalRand(1.0f);
  }

  return true;
}

static void createRenderPipeline() {
  if (renderPipelineState_Mesh_) {
    return;
  }

  IGL_ASSERT(framebuffer_);
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

  desc.frontFaceWinding = igl::WindingMode::Clockwise;
  desc.debugName = igl::genNameHandle("Pipeline: mesh");
  desc.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE("texture0");
  desc.fragmentUnitSamplerMap[1] = IGL_NAMEHANDLE("texture1");
  desc.uniformBlockBindingMap[0] = std::make_pair(IGL_NAMEHANDLE("perFrame"), igl::NameHandle{});
  desc.uniformBlockBindingMap[1] = std::make_pair(IGL_NAMEHANDLE("perObject"), igl::NameHandle{});

  renderPipelineState_Mesh_ = device_->createRenderPipeline(desc, nullptr);
}

void onDraw(void*) {
  static uint32_t frameIndex = 0;
  static float time_ = 0.0f;
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
        glm::rotate(glm::translate(mat4(1.0f), offset), direction * time_, axis_[i]);
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
  commands->bindBuffer(0, BindTarget::kVertex, vb0_, 0);
  commands->bindDepthStencilState(depthStencilState_);
  commands->bindBuffer(0, BindTarget::kAllGraphics, ubPerFrame_[frameIndex], 0);
  commands->bindTexture(0, igl::BindTarget::kFragment, texture0_);
  commands->bindTexture(1, igl::BindTarget::kFragment, texture1_);
  commands->bindSamplerState(0, igl::BindTarget::kFragment, sampler_);
  // Draw 2 cubes: we use uniform buffer to update matrices
  for (uint32_t i = 0; i != kNumCubes; i++) {
    commands->bindBuffer(
        1, BindTarget::kAllGraphics, ubPerObject_[frameIndex], i * sizeof(UniformsPerObject));
    commands->drawIndexed(PrimitiveType::Triangle, 3 * 6 * 2, IndexFormat::UInt16, *ib0_.get(), 0);
  }
  commands->popDebugGroupLabel();
  commands->endEncoding();

  buffer->present(getNativeDrawable());

  commandQueue_->submit(*buffer);

  frameIndex = (frameIndex + 1) % kNumBufferedFrames;
  time_ += 0.001;
}

int main(int argc, char* argv[]) {
  if (initialize()) {
    createRenderPipeline();
    emscripten_set_canvas_element_size(canvas, width_, height_);
    emscripten_set_main_loop_arg(onDraw, 0, 0, 1);
    while (1) {
      onDraw(0);
    }
  }
  return EXIT_FAILURE; // not reached
}
