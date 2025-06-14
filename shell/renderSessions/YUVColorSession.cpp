/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <IGLU/simdtypes/SimdTypes.h>
#include <shell/renderSessions/YUVColorSession.h>
#include <shell/shared/fileLoader/FileLoader.h>
#include <shell/shared/platform/DisplayContext.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>

namespace igl::shell {

struct VertexPosUv {
  iglu::simdtypes::float3 position;
  iglu::simdtypes::float2 uv;
};

namespace {

constexpr VertexPosUv kVertexData[] = {
    {{-1.f, 1.f, 0.0}, {0.0, 0.0}},
    {{1.f, 1.f, 0.0}, {1.0, 0.0}},
    {{-1.f, -1.f, 0.0}, {0.0, 1.0}},
    {{1.f, -1.f, 0.0}, {1.0, 1.0}},
};
constexpr uint16_t kIndexData[] = {0, 1, 2, 1, 3, 2};

std::string getOpenGLVertexShaderSource() {
  return R"(
                #version 300 es
                precision highp float;
                in vec3 position;
                in vec2 uv_in;

                out vec2 uv;

                void main() {
                  gl_Position = vec4(position, 1.0);
                  uv = uv_in; // position.xy * 0.5 + 0.5;
                })";
}

std::string getOpenGLFragmentShaderSource() {
  return std::string(R"(
                #version 300 es
                #extension GL_EXT_YUV_target : require
                precision highp float;
                uniform __samplerExternal2DY2YEXT inputImage;

                in vec2 uv;
                layout (yuv) out vec4 outColor;

                void main() {
                  outColor = texture(inputImage, uv);
                })");
}

std::string getVulkanVertexShaderSource() {
  return R"(
                layout(location = 0) in vec3 position;
                layout(location = 1) in vec2 uv_in;
                layout(location = 0) out vec2 uv;

                void main() {
                  gl_Position = vec4(position, 1.0);
                  uv = uv_in;
                }
                )";
}

std::string getVulkanFragmentShaderSource() {
  return R"(
                layout(location = 0) in vec2 uv;
                layout(location = 0) out vec4 out_FragColor;

                layout(set = 0, binding = 0) uniform sampler2D in_texture;

                void main() {
                  out_FragColor = texture(in_texture, uv);
                }
                )";
}

std::unique_ptr<IShaderStages> getShaderStagesForBackend(IDevice& device) {
  switch (device.getBackendType()) {
  // @fb-only
  case igl::BackendType::Invalid:
  case igl::BackendType::Metal:
  case igl::BackendType::Custom:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    return nullptr;
  case igl::BackendType::Vulkan:
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           getVulkanVertexShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           getVulkanFragmentShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           nullptr);
  case igl::BackendType::OpenGL:
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           getOpenGLVertexShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           getOpenGLFragmentShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           nullptr);
  }
  IGL_UNREACHABLE_RETURN(nullptr)
}

} // namespace

YUVColorSession::YUVColorSession(std::shared_ptr<Platform> platform) :
  RenderSession(std::move(platform)) {
  listener_ = std::make_shared<Listener>(*this);
  getPlatform().getInputDispatcher().addKeyListener(listener_);
  getPlatform().getInputDispatcher().addMouseListener(listener_);
  imguiSession_ = std::make_unique<iglu::imgui::Session>(getPlatform().getDevice(),
                                                         getPlatform().getInputDispatcher());
}

// clang-tidy off
void YUVColorSession::initialize() noexcept {
  // clang-tidy on
  auto& device = getPlatform().getDevice();

  // Vertex & Index buffer
  vb0_ = device.createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, kVertexData, sizeof(kVertexData)), nullptr);
  IGL_DEBUG_ASSERT(vb0_ != nullptr);
  ib0_ = device.createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Index, kIndexData, sizeof(kIndexData)), nullptr);
  IGL_DEBUG_ASSERT(ib0_ != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0] = VertexAttribute{
      1, VertexAttributeFormat::Float3, offsetof(VertexPosUv, position), "position", 0};
  inputDesc.attributes[1] =
      VertexAttribute{1, VertexAttributeFormat::Float2, offsetof(VertexPosUv, uv), "uv_in", 1};
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[1].stride = sizeof(VertexPosUv);
  vertexInput0_ = device.createVertexInputState(inputDesc, nullptr);
  IGL_DEBUG_ASSERT(vertexInput0_ != nullptr);

  // Samplers & Textures

  auto createYUVDemo =
      [this](IDevice& device, const char* demoName, TextureFormat yuvFormat, const char* fileName) {
        constexpr uint32_t width = 1920;
        constexpr uint32_t height = 1080;

        auto sampler =
            device.createSamplerState(SamplerStateDesc::newYUV(yuvFormat, "YUVSampler"), nullptr);
        IGL_DEBUG_ASSERT(sampler != nullptr);

        auto& fileLoader = getPlatform().getFileLoader();
        const auto fileData = fileLoader.loadBinaryData(fileName);
        IGL_DEBUG_ASSERT(fileData.data && fileData.length, "Cannot load texture file");

        const igl::TextureDesc textureDesc = igl::TextureDesc::new2D(
            yuvFormat, width, height, TextureDesc::TextureUsageBits::Sampled, "YUV texture");
        IGL_DEBUG_ASSERT(width * height + width * height / 2 == fileData.length);
        auto texture = device.createTexture(textureDesc, nullptr);
        IGL_DEBUG_ASSERT(texture);
        texture->upload(TextureRangeDesc{0, 0, 0, width, height}, fileData.data.get());

        this->yuvFormatDemos_.push_back(YUVFormatDemo{demoName, sampler, texture, nullptr});
      };

  createYUVDemo(device, "YUV 420p", igl::TextureFormat::YUV_420p, "output_frame_900.420p.yuv");
  createYUVDemo(device, "YUV NV12", igl::TextureFormat::YUV_NV12, "output_frame_900.nv12.yuv");

  shaderStages_ = getShaderStagesForBackend(device);
  IGL_DEBUG_ASSERT(shaderStages_ != nullptr);

  // Command queue
  const CommandQueueDesc desc{};
  commandQueue_ = device.createCommandQueue(desc, nullptr);
  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);

  renderPass_.colorAttachments.resize(1);
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = getPreferredClearColor();
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass_.depthAttachment.clearDepth = 1.0;
}

void YUVColorSession::update(SurfaceTextures surfaceTextures) noexcept {
  Result ret;
  framebufferDesc_.colorAttachments[0].texture = surfaceTextures.color;
  if (framebuffer_ == nullptr) {
    IGL_DEBUG_ASSERT(ret.isOk());
    framebufferDesc_.depthAttachment.texture = surfaceTextures.depth;
    framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc_, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    IGL_DEBUG_ASSERT(framebuffer_ != nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures.color);
  }

  constexpr size_t kTextureUnit = 0;

  YUVFormatDemo& demo = yuvFormatDemos_[currentDemo_];

  if (!demo.pipelineState) {
    RenderPipelineDesc desc;
    desc.vertexInputState = vertexInput0_;
    desc.shaderStages = shaderStages_;
    desc.targetDesc.colorAttachments.resize(1);
    desc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_->getColorAttachment(0)->getProperties().format;
    desc.targetDesc.depthAttachmentFormat =
        framebuffer_->getDepthAttachment()->getProperties().format;
    desc.fragmentUnitSamplerMap[kTextureUnit] = IGL_NAMEHANDLE("inputImage");
    desc.cullMode = igl::CullMode::Back;
    desc.frontFaceWinding = igl::WindingMode::Clockwise;
    desc.immutableSamplers[kTextureUnit] = demo.sampler; // Ycbcr sampler

    demo.pipelineState = getPlatform().getDevice().createRenderPipeline(desc, nullptr);
    IGL_DEBUG_ASSERT(demo.pipelineState != nullptr);
  }

  // Command Buffers
  auto buffer = commandQueue_->createCommandBuffer({}, nullptr);
  IGL_DEBUG_ASSERT(buffer != nullptr);
  auto drawableSurface = framebuffer_->getColorAttachment(0);

  framebuffer_->updateDrawable(drawableSurface);

  // Submit commands
  const std::shared_ptr<IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  IGL_DEBUG_ASSERT(commands != nullptr);
  if (commands) {
    commands->bindVertexBuffer(0, *vb0_);
    commands->bindVertexBuffer(1, *vb0_);
    commands->bindRenderPipelineState(demo.pipelineState);
    commands->bindTexture(kTextureUnit, BindTarget::kFragment, demo.texture.get());
    commands->bindSamplerState(kTextureUnit, BindTarget::kFragment, demo.sampler.get());
    commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16);
    commands->drawIndexed(6);

    // draw the YUV format name using ImGui
    {
      imguiSession_->beginFrame(framebufferDesc_, getPlatform().getDisplayContext().pixelsPerPoint);
      constexpr ImGuiWindowFlags flags =
          ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
          ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
      ImGui::SetNextWindowPos({15.0f, 15.0f});
      ImGui::SetNextWindowBgAlpha(0.30f);
      ImGui::Begin("##FormatYUV", nullptr, flags);
      ImGui::Text("%s", demo.name);
      ImGui::Text("Press any key to change");
      ImGui::End();
      imguiSession_->endFrame(getPlatform().getDevice(), *commands);
    }

    commands->endEncoding();
  }

  IGL_DEBUG_ASSERT(buffer != nullptr);
  if (shellParams().shouldPresent) {
    buffer->present(drawableSurface);
  }

  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);
  commandQueue_->submit(*buffer);
  RenderSession::update(surfaceTextures);
}

void YUVColorSession::nextFormatDemo() {
  currentDemo_ = (currentDemo_ + 1) % yuvFormatDemos_.size();
}

bool YUVColorSession::Listener::process(const KeyEvent& event) {
  if (!event.isDown) {
    session.nextFormatDemo();
  }
  return true;
}

bool YUVColorSession::Listener::process(const MouseButtonEvent& event) {
  if (!event.isDown) {
    session.nextFormatDemo();
  }
  return true;
}

bool YUVColorSession::Listener::process(const MouseMotionEvent& /*event*/) {
  return false;
}

bool YUVColorSession::Listener::process(const MouseWheelEvent& /*event*/) {
  return false;
}

} // namespace igl::shell
