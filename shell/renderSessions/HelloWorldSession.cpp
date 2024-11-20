/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include "HelloWorldSession.h"
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>
#include <igl/opengl/GLIncludes.h>
#include <shell/shared/renderSession/ShellParams.h>

namespace igl::shell {

namespace {
struct VertexPosColor {
  iglu::simdtypes::float3 position;
  iglu::simdtypes::float4 color;
};
VertexPosColor vertexData[] = {
    {{-0.6f, -0.4f, 0.0}, {1.0, 0.0, 0.0, 1.0}},
    {{0.6f, -0.4f, 0.0}, {0.0, 1.0, 0.0, 1.0}},
    {{0.0f, 0.6f, 0.0}, {0.0, 0.0, 1.0, 1.0}},
};
uint16_t indexData[] = {
    2,
    1,
    0,
};

std::string getVersion() {
  return {"#version 100"};
}

std::string getMetalShaderSource() {
  return R"(
              using namespace metal;

              typedef struct {
                float3 position [[attribute(0)]];
                float4 color [[attribute(1)]];
              } VertexIn;

              typedef struct {
                float4 position [[position]];
                float4 color;
              } VertexOut;

              vertex VertexOut vertexShader(
                  uint vid [[vertex_id]], constant VertexIn * vertices [[buffer(1)]]) {
                VertexOut out;
                out.position = float4(vertices[vid].position, 1.0);
                out.color = vertices[vid].color;
                return out;
              }

              fragment float4 fragmentShader(
                  VertexOut IN [[stage_in]]) {
                  return IN.color;
              }
    )";
}

std::string getOpenGLVertexShaderSource() {
  return getVersion() + R"(
                precision highp float;
                attribute vec3 position;
                attribute vec4 color_in;

                varying vec4 vColor;

                void main() {
                  gl_Position = vec4(position, 1.0);
                  vColor = color_in;
                })";
}

std::string getOpenGLFragmentShaderSource() {
  return getVersion() + std::string(R"(
                precision highp float;

                varying vec4 vColor;

                void main() {
                  gl_FragColor = vColor;
                })");
}

std::string getVulkanVertexShaderSource() {
  return R"(
                layout(location = 0) in vec3 position;
                layout(location = 1) in vec4 color_in;
                layout(location = 0) out vec4 color;

                void main() {
                  gl_Position = vec4(position, 1.0);
                  color = color_in;
                }
                )";
}

std::string getVulkanFragmentShaderSource() {
  return R"(
                layout(location = 0) in vec4 color;
                layout(location = 0) out vec4 out_FragColor;

                void main() {
                  out_FragColor = color;
                }
                )";
}

std::unique_ptr<IShaderStages> getShaderStagesForBackend(igl::IDevice& device) {
  switch (device.getBackendType()) {
  case igl::BackendType::Invalid:
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
  // @fb-only
    // @fb-only
    // @fb-only
  case igl::BackendType::Metal:
    return igl::ShaderStagesCreator::fromLibraryStringInput(
        device, getMetalShaderSource().c_str(), "vertexShader", "fragmentShader", "", nullptr);
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

void HelloWorldSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();

  // Vertex & Index buffer
  const BufferDesc vbDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, vertexData, sizeof(vertexData));
  vb0_ = device.createBuffer(vbDesc, nullptr);
  IGL_DEBUG_ASSERT(vb0_ != nullptr);
  const BufferDesc ibDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData));
  ib0_ = device.createBuffer(ibDesc, nullptr);
  IGL_DEBUG_ASSERT(ib0_ != nullptr);

  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0] = VertexAttribute(
      1, VertexAttributeFormat::Float3, offsetof(VertexPosColor, position), "position", 0);
  inputDesc.attributes[1] = VertexAttribute(
      1, VertexAttributeFormat::Float4, offsetof(VertexPosColor, color), "color_in", 1);
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[1].stride = sizeof(VertexPosColor);
  vertexInput0_ = device.createVertexInputState(inputDesc, nullptr);
  IGL_DEBUG_ASSERT(vertexInput0_ != nullptr);

  shaderStages_ = getShaderStagesForBackend(device);
  IGL_DEBUG_ASSERT(shaderStages_ != nullptr);

  // Command queue
  const CommandQueueDesc desc{igl::CommandQueueType::Graphics};
  commandQueue_ = device.createCommandQueue(desc, nullptr);
  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);

  renderPass_.colorAttachments.resize(1);
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = getPreferredClearColor();
  renderPass_.depthAttachment.loadAction = LoadAction::Clear;
  renderPass_.depthAttachment.clearDepth = 1.0;
}

void HelloWorldSession::update(igl::SurfaceTextures surfaceTextures) noexcept {
  igl::Result ret;
  if (framebuffer_ == nullptr) {
    igl::FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    framebufferDesc.depthAttachment.texture = surfaceTextures.depth;
    if (surfaceTextures.depth && surfaceTextures.depth->getProperties().hasStencil()) {
      framebufferDesc.stencilAttachment.texture = surfaceTextures.depth;
    }
    IGL_DEBUG_ASSERT(ret.isOk());
    framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    IGL_DEBUG_ASSERT(framebuffer_ != nullptr);
  } else {
    framebuffer_->updateDrawable(surfaceTextures);
  }

  // Graphics pipeline
  if (pipelineState_ == nullptr) {
    RenderPipelineDesc graphicsDesc;
    graphicsDesc.vertexInputState = vertexInput0_;
    graphicsDesc.shaderStages = shaderStages_;
    graphicsDesc.targetDesc.colorAttachments.resize(1);
    graphicsDesc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_->getColorAttachment(0)->getFormat();
    graphicsDesc.targetDesc.depthAttachmentFormat = framebuffer_->getDepthAttachment()->getFormat();
    graphicsDesc.targetDesc.stencilAttachmentFormat =
        framebuffer_->getStencilAttachment() ? framebuffer_->getStencilAttachment()->getFormat()
                                             : igl::TextureFormat::Invalid;
    graphicsDesc.cullMode = igl::CullMode::Back;
    graphicsDesc.frontFaceWinding = igl::WindingMode::Clockwise;

    pipelineState_ = getPlatform().getDevice().createRenderPipeline(graphicsDesc, nullptr);
    IGL_DEBUG_ASSERT(pipelineState_ != nullptr);
  }

  // Command Buffers
  const CommandBufferDesc cbDesc;
  auto buffer = commandQueue_->createCommandBuffer(cbDesc, nullptr);
  IGL_DEBUG_ASSERT(buffer != nullptr);
  auto drawableSurface = framebuffer_->getColorAttachment(0);

  // Submit commands
  const std::shared_ptr<igl::IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  IGL_DEBUG_ASSERT(commands != nullptr);
  if (commands) {
    commands->bindVertexBuffer(1, *vb0_);
    commands->bindRenderPipelineState(pipelineState_);
    commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16);
    commands->drawIndexed(3);

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

} // namespace igl::shell
