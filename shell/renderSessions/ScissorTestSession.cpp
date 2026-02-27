/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/renderSessions/ScissorTestSession.h>

#include <shell/shared/renderSession/ShellParams.h>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>

namespace igl::shell {

namespace {
struct VertexPosColor {
  iglu::simdtypes::float3 position;
  iglu::simdtypes::float4 color;
};

// Full-screen quad covering the entire NDC range [-1, 1].
// We draw this quad 4 times, each with a different solid color and scissor rect.
// The scissor rectangles will clip each draw to a different quadrant.

// RED quad (top-left quadrant)
VertexPosColor redQuadData[] = {
    {.position = {-1.0f, -1.0f, 0.0f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
    {.position = {1.0f, -1.0f, 0.0f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
    {.position = {1.0f, 1.0f, 0.0f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
    {.position = {-1.0f, 1.0f, 0.0f}, .color = {1.0f, 0.0f, 0.0f, 1.0f}},
};

// GREEN quad (top-right quadrant)
VertexPosColor greenQuadData[] = {
    {.position = {-1.0f, -1.0f, 0.0f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},
    {.position = {1.0f, -1.0f, 0.0f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},
    {.position = {1.0f, 1.0f, 0.0f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},
    {.position = {-1.0f, 1.0f, 0.0f}, .color = {0.0f, 1.0f, 0.0f, 1.0f}},
};

// BLUE quad (bottom-left quadrant)
VertexPosColor blueQuadData[] = {
    {.position = {-1.0f, -1.0f, 0.0f}, .color = {0.0f, 0.0f, 1.0f, 1.0f}},
    {.position = {1.0f, -1.0f, 0.0f}, .color = {0.0f, 0.0f, 1.0f, 1.0f}},
    {.position = {1.0f, 1.0f, 0.0f}, .color = {0.0f, 0.0f, 1.0f, 1.0f}},
    {.position = {-1.0f, 1.0f, 0.0f}, .color = {0.0f, 0.0f, 1.0f, 1.0f}},
};

// YELLOW quad (bottom-right quadrant)
VertexPosColor yellowQuadData[] = {
    {.position = {-1.0f, -1.0f, 0.0f}, .color = {1.0f, 1.0f, 0.0f, 1.0f}},
    {.position = {1.0f, -1.0f, 0.0f}, .color = {1.0f, 1.0f, 0.0f, 1.0f}},
    {.position = {1.0f, 1.0f, 0.0f}, .color = {1.0f, 1.0f, 0.0f, 1.0f}},
    {.position = {-1.0f, 1.0f, 0.0f}, .color = {1.0f, 1.0f, 0.0f, 1.0f}},
};

// Indices for a quad (two triangles)
uint16_t quadIndices[] = {
    0,
    1,
    2,
    0,
    2,
    3,
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

std::unique_ptr<IShaderStages> getShaderStagesForBackend(IDevice& device) {
  switch (device.getBackendType()) {
  case igl::BackendType::Invalid:
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
  case igl::BackendType::D3D12: {
    static const char* kVS = R"(
      struct VSIn { float3 position : POSITION; float4 color : COLOR; };
      struct VSOut { float4 position : SV_POSITION; float4 color : COLOR; };
      VSOut main(VSIn v) {
        VSOut o; o.position = float4(v.position, 1.0); o.color = v.color; return o; }
    )";
    static const char* kPS = R"(
      struct PSIn { float4 position : SV_POSITION; float4 color : COLOR; };
      float4 main(PSIn i) : SV_TARGET { return i.color; }
    )";
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device, kVS, "main", "", kPS, "main", "", nullptr);
  }
  }
  IGL_UNREACHABLE_RETURN(nullptr)
}
} // namespace

void ScissorTestSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();

  // Create 4 vertex buffers, one for each colored quad
  redVertexBuffer_ = device.createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, redQuadData, sizeof(redQuadData)), nullptr);
  greenVertexBuffer_ = device.createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, greenQuadData, sizeof(greenQuadData)),
      nullptr);
  blueVertexBuffer_ = device.createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, blueQuadData, sizeof(blueQuadData)), nullptr);
  yellowVertexBuffer_ = device.createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, yellowQuadData, sizeof(yellowQuadData)),
      nullptr);

  // Shared index buffer for all quads
  indexBuffer_ = device.createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Index, quadIndices, sizeof(quadIndices)), nullptr);
  IGL_DEBUG_ASSERT(indexBuffer_ != nullptr);

  vertexInputState_ = device.createVertexInputState(
      VertexInputStateDesc{
          .numAttributes = 2,
          .attributes =
              {
                  VertexAttribute{.bufferIndex = 1,
                                  .format = VertexAttributeFormat::Float3,
                                  .offset = offsetof(VertexPosColor, position),
                                  .name = "position",
                                  .location = 0},
                  VertexAttribute{.bufferIndex = 1,
                                  .format = VertexAttributeFormat::Float4,
                                  .offset = offsetof(VertexPosColor, color),
                                  .name = "color_in",
                                  .location = 1},
              },
          .numInputBindings = 1,
          .inputBindings = {{}, {.stride = sizeof(VertexPosColor)}},
      },
      nullptr);
  IGL_DEBUG_ASSERT(vertexInputState_ != nullptr);

  shaderStages_ = getShaderStagesForBackend(device);
  IGL_DEBUG_ASSERT(shaderStages_ != nullptr);

  // Command queue
  commandQueue_ = device.createCommandQueue(CommandQueueDesc{}, nullptr);
  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);

  renderPass_ = {
      .colorAttachments =
          {
              {
                  .loadAction = LoadAction::Clear,
                  .storeAction = StoreAction::Store,
                  .clearColor = getPreferredClearColor(),
              },
          },
      .depthAttachment =
          {
              .loadAction = LoadAction::Clear,
              .clearDepth = 1.0,
          },
  };
}

void ScissorTestSession::update(SurfaceTextures textures) noexcept {
  Result ret;
  if (framebuffer_ == nullptr) {
    framebuffer_ = getPlatform().getDevice().createFramebuffer(
        FramebufferDesc{
            .colorAttachments = {{.texture = textures.color}},
            .depthAttachment = {.texture = textures.depth},
            .stencilAttachment = textures.depth && textures.depth->getProperties().hasStencil()
                                     ? FramebufferDesc::AttachmentDesc{.texture = textures.depth}
                                     : FramebufferDesc::AttachmentDesc{},
        },
        &ret);
    IGL_DEBUG_ASSERT(ret.isOk());
    IGL_DEBUG_ASSERT(framebuffer_ != nullptr);
  } else {
    framebuffer_->updateDrawable(textures);
  }

  // Graphics pipeline
  if (pipelineState_ == nullptr) {
    pipelineState_ = getPlatform().getDevice().createRenderPipeline(
        RenderPipelineDesc{
            .vertexInputState = vertexInputState_,
            .shaderStages = shaderStages_,
            .targetDesc =
                {
                    .colorAttachments = {{.textureFormat =
                                              framebuffer_->getColorAttachment(0)->getFormat()}},
                    .depthAttachmentFormat = framebuffer_->getDepthAttachment()->getFormat(),
                    .stencilAttachmentFormat =
                        framebuffer_->getStencilAttachment()
                            ? framebuffer_->getStencilAttachment()->getFormat()
                            : igl::TextureFormat::Invalid,
                },
            .cullMode = igl::CullMode::Disabled,
            .frontFaceWinding = igl::WindingMode::CounterClockwise,
        },
        nullptr);
    IGL_DEBUG_ASSERT(pipelineState_ != nullptr);
  }

  // Command Buffers
  auto buffer = commandQueue_->createCommandBuffer(CommandBufferDesc{}, nullptr);
  IGL_DEBUG_ASSERT(buffer != nullptr);
  auto drawableSurface = framebuffer_->getColorAttachment(0);

  // Get framebuffer dimensions
  const uint32_t width = drawableSurface->getDimensions().width;
  const uint32_t height = drawableSurface->getDimensions().height;
  const uint32_t halfWidth = width / 2;
  const uint32_t halfHeight = height / 2;

  // Submit commands
  const std::shared_ptr<IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  IGL_DEBUG_ASSERT(commands != nullptr);
  if (commands) {
    commands->bindRenderPipelineState(pipelineState_);
    commands->bindIndexBuffer(*indexBuffer_, IndexFormat::UInt16);

    // Full-screen viewport
    commands->bindViewport(Viewport{.x = 0.0f,
                                    .y = 0.0f,
                                    .width = static_cast<float>(width),
                                    .height = static_cast<float>(height),
                                    .minDepth = 0.0f,
                                    .maxDepth = 1.0f});

    // Draw 4 full-screen quads, each clipped by a different scissor rectangle
    // to show only in its designated quadrant.

    // 1. RED quad - TOP-LEFT quadrant
    commands->bindScissorRect(
        ScissorRect{.x = 0, .y = 0, .width = halfWidth, .height = halfHeight});
    commands->bindVertexBuffer(1, *redVertexBuffer_);
    commands->drawIndexed(6);

    // 2. GREEN quad - TOP-RIGHT quadrant
    commands->bindScissorRect(
        ScissorRect{.x = halfWidth, .y = 0, .width = halfWidth, .height = halfHeight});
    commands->bindVertexBuffer(1, *greenVertexBuffer_);
    commands->drawIndexed(6);

    // 3. BLUE quad - BOTTOM-LEFT quadrant
    commands->bindScissorRect(
        ScissorRect{.x = 0, .y = halfHeight, .width = halfWidth, .height = halfHeight});
    commands->bindVertexBuffer(1, *blueVertexBuffer_);
    commands->drawIndexed(6);

    // 4. YELLOW quad - BOTTOM-RIGHT quadrant
    commands->bindScissorRect(
        ScissorRect{.x = halfWidth, .y = halfHeight, .width = halfWidth, .height = halfHeight});
    commands->bindVertexBuffer(1, *yellowVertexBuffer_);
    commands->drawIndexed(6);

    commands->endEncoding();
  }

  IGL_DEBUG_ASSERT(buffer != nullptr);
  if (shellParams().shouldPresent) {
    buffer->present(drawableSurface);
  }

  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);
  commandQueue_->submit(*buffer);
  frameCount_++;
  RenderSession::update(textures);
}

} // namespace igl::shell
