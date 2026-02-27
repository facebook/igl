/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <shell/renderSessions/WireframeSession.h>

#include <shell/shared/renderSession/ShellParams.h>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>

namespace igl::shell {

namespace {
struct VertexPosColor {
  iglu::simdtypes::float3 position;
  iglu::simdtypes::float4 color;
};

// A hexagon composed of 6 triangles sharing the center vertex.
// This provides enough faces to clearly see the wireframe overlay.
// clang-format off
VertexPosColor vertexData[] = {
    // Center
    {.position = {0.0f, 0.0f, 0.0f}, .color = {0.2f, 0.2f, 0.6f, 1.0f}},
    // Outer vertices (hexagon)
    {.position = {0.0f, 0.6f, 0.0f}, .color = {0.4f, 0.1f, 0.5f, 1.0f}},
    {.position = {0.52f, 0.3f, 0.0f}, .color = {0.5f, 0.2f, 0.4f, 1.0f}},
    {.position = {0.52f, -0.3f, 0.0f}, .color = {0.3f, 0.3f, 0.6f, 1.0f}},
    {.position = {0.0f, -0.6f, 0.0f}, .color = {0.4f, 0.1f, 0.5f, 1.0f}},
    {.position = {-0.52f, -0.3f, 0.0f}, .color = {0.5f, 0.2f, 0.4f, 1.0f}},
    {.position = {-0.52f, 0.3f, 0.0f}, .color = {0.3f, 0.3f, 0.6f, 1.0f}},
};
// clang-format on

// 6 triangles, each sharing center vertex 0
uint16_t indexData[] = {
    0,
    1,
    2, // triangle 0
    0,
    2,
    3, // triangle 1
    0,
    3,
    4, // triangle 2
    0,
    4,
    5, // triangle 3
    0,
    5,
    6, // triangle 4
    0,
    6,
    1, // triangle 5
};

const uint32_t kNumIndices = sizeof(indexData) / sizeof(indexData[0]);

std::string getVersion() {
  return {"#version 100"};
}

// ---------------------------------------------------------------------------
// Solid shaders: output the per-vertex color
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Wireframe shaders: output a bright green color for wireframe edges
// ---------------------------------------------------------------------------

std::string getWireframeMetalShaderSource() {
  return R"(
              using namespace metal;

              typedef struct {
                float3 position [[attribute(0)]];
                float4 color [[attribute(1)]];
              } VertexIn;

              typedef struct {
                float4 position [[position]];
              } VertexOut;

              vertex VertexOut vertexShaderWireframe(
                  uint vid [[vertex_id]], constant VertexIn * vertices [[buffer(1)]]) {
                VertexOut out;
                out.position = float4(vertices[vid].position, 1.0);
                return out;
              }

              fragment float4 fragmentShaderWireframe(
                  VertexOut IN [[stage_in]]) {
                  return float4(0.0, 1.0, 0.2, 1.0);
              }
    )";
}

std::string getWireframeOpenGLVertexShaderSource() {
  return getVersion() + R"(
                precision highp float;
                attribute vec3 position;
                attribute vec4 color_in;

                void main() {
                  gl_Position = vec4(position, 1.0);
                })";
}

std::string getWireframeOpenGLFragmentShaderSource() {
  return getVersion() + std::string(R"(
                precision highp float;

                void main() {
                  gl_FragColor = vec4(0.0, 1.0, 0.2, 1.0);
                })");
}

std::string getWireframeVulkanVertexShaderSource() {
  return R"(
                layout(location = 0) in vec3 position;
                layout(location = 1) in vec4 color_in;

                void main() {
                  gl_Position = vec4(position, 1.0);
                }
                )";
}

std::string getWireframeVulkanFragmentShaderSource() {
  return R"(
                layout(location = 0) out vec4 out_FragColor;

                void main() {
                  out_FragColor = vec4(0.0, 1.0, 0.2, 1.0);
                }
                )";
}

// ---------------------------------------------------------------------------
// Shader stage creation helpers
// ---------------------------------------------------------------------------

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

std::unique_ptr<IShaderStages> getWireframeShaderStagesForBackend(IDevice& device) {
  switch (device.getBackendType()) {
  case igl::BackendType::Invalid:
  case igl::BackendType::Custom:
    IGL_DEBUG_ASSERT_NOT_REACHED();
    return nullptr;
  case igl::BackendType::Vulkan:
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device,
        getWireframeVulkanVertexShaderSource().c_str(),
        "main",
        "",
        getWireframeVulkanFragmentShaderSource().c_str(),
        "main",
        "",
        nullptr);
  // @fb-only
    // @fb-only
    // @fb-only
  case igl::BackendType::Metal:
    return igl::ShaderStagesCreator::fromLibraryStringInput(device,
                                                            getWireframeMetalShaderSource().c_str(),
                                                            "vertexShaderWireframe",
                                                            "fragmentShaderWireframe",
                                                            "",
                                                            nullptr);
  case igl::BackendType::OpenGL:
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device,
        getWireframeOpenGLVertexShaderSource().c_str(),
        "main",
        "",
        getWireframeOpenGLFragmentShaderSource().c_str(),
        "main",
        "",
        nullptr);
  case igl::BackendType::D3D12: {
    static const char* kVS = R"(
      struct VSIn { float3 position : POSITION; float4 color : COLOR; };
      struct VSOut { float4 position : SV_POSITION; };
      VSOut main(VSIn v) {
        VSOut o; o.position = float4(v.position, 1.0); return o; }
    )";
    static const char* kPS = R"(
      struct PSIn { float4 position : SV_POSITION; };
      float4 main(PSIn i) : SV_TARGET { return float4(0.0, 1.0, 0.2, 1.0); }
    )";
    return igl::ShaderStagesCreator::fromModuleStringInput(
        device, kVS, "main", "", kPS, "main", "", nullptr);
  }
  }
  IGL_UNREACHABLE_RETURN(nullptr)
}
} // namespace

void WireframeSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();

  // Vertex & Index buffer
  vertexBuffer_ = device.createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, vertexData, sizeof(vertexData)), nullptr);
  IGL_DEBUG_ASSERT(vertexBuffer_ != nullptr);
  indexBuffer_ = device.createBuffer(
      BufferDesc(BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData)), nullptr);
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

  // Solid shaders (per-vertex color output)
  shaderStages_ = getShaderStagesForBackend(device);
  IGL_DEBUG_ASSERT(shaderStages_ != nullptr);

  // Wireframe shaders (bright green output)
  wireframeShaderStages_ = getWireframeShaderStagesForBackend(device);
  IGL_DEBUG_ASSERT(wireframeShaderStages_ != nullptr);

  // Command queue
  commandQueue_ = device.createCommandQueue(CommandQueueDesc{}, nullptr);
  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);

  // Depth stencil state
  depthStencilState_ = device.createDepthStencilState(
      DepthStencilStateDesc{
          .compareFunction = igl::CompareFunction::LessEqual,
          .isDepthWriteEnabled = true,
      },
      nullptr);

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

void WireframeSession::update(SurfaceTextures textures) noexcept {
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

  // Solid pipeline: PolygonFillMode::Fill (default)
  if (solidPipelineState_ == nullptr) {
    solidPipelineState_ = getPlatform().getDevice().createRenderPipeline(
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
            .polygonFillMode = igl::PolygonFillMode::Fill,
        },
        nullptr);
    IGL_DEBUG_ASSERT(solidPipelineState_ != nullptr);
  }

  // Wireframe pipeline: PolygonFillMode::Line
  if (wireframePipelineState_ == nullptr) {
    wireframePipelineState_ = getPlatform().getDevice().createRenderPipeline(
        RenderPipelineDesc{
            .vertexInputState = vertexInputState_,
            .shaderStages = wireframeShaderStages_,
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
            .polygonFillMode = igl::PolygonFillMode::Line,
        },
        nullptr);
    IGL_DEBUG_ASSERT(wireframePipelineState_ != nullptr);
  }

  // Command Buffers
  auto buffer = commandQueue_->createCommandBuffer(CommandBufferDesc{}, nullptr);
  IGL_DEBUG_ASSERT(buffer != nullptr);
  auto drawableSurface = framebuffer_->getColorAttachment(0);

  // Submit commands
  const std::shared_ptr<IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  IGL_DEBUG_ASSERT(commands != nullptr);
  if (commands) {
    commands->bindVertexBuffer(1, *vertexBuffer_);
    commands->bindIndexBuffer(*indexBuffer_, IndexFormat::UInt16);
    commands->bindDepthStencilState(depthStencilState_);

    // Draw 1: Solid fill -- renders the hexagon with per-vertex colors
    commands->bindRenderPipelineState(solidPipelineState_);
    commands->drawIndexed(kNumIndices);

    // Draw 2: Wireframe overlay -- renders bright green edges on top
    commands->bindRenderPipelineState(wireframePipelineState_);
    commands->drawIndexed(kNumIndices);

    commands->endEncoding();
  }

  IGL_DEBUG_ASSERT(buffer != nullptr);
  if (shellParams().shouldPresent) {
    buffer->present(drawableSurface);
  }

  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);
  commandQueue_->submit(*buffer);
  RenderSession::update(textures);
}

} // namespace igl::shell
