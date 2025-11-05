/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "BufferBindGroupSession.h"
#include <igl/ShaderCreator.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace igl::shell {

// Interleaved vertex data (position + color)
struct Vertex {
  float position[3];
  float color[4];
};

static const Vertex kVertices[] = {
    {{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},   // Top - Red
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}}, // Bottom-left - Green
    {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},  // Bottom-right - Blue
};

// Uniform buffer 0: Transform matrix
struct UniformTransform {
  glm::mat4 transform;
};

// Uniform buffer 1: Color tint
struct UniformColorTint {
  glm::vec4 tint;
};

// Uniform buffer 2: Position offset
struct UniformPositionOffset {
  glm::vec3 offset;
  float padding;
};

// Uniform buffer 3: Scale
struct UniformScale {
  float scale;
  float padding[3];
};

// D3D12 HLSL shaders
static const char* getD3D12VertexShaderSource() {
  return R"(
    cbuffer TransformBuffer : register(b3) {
      float4x4 transform;
    };

    cbuffer ColorTintBuffer : register(b4) {
      float4 tint;
    };

    cbuffer PositionOffsetBuffer : register(b5) {
      float3 positionOffset;
      float padding0;
    };

    cbuffer ScaleBuffer : register(b6) {
      float scale;
      float3 padding1;
    };

    struct VSInput {
      float3 position : POSITION;
      float4 color : COLOR0;
    };

    struct VSOutput {
      float4 position : SV_POSITION;
      float4 color : COLOR0;
    };

    VSOutput main(VSInput input) {
      VSOutput output;
      // Apply scale, then offset, then transform
      float3 pos = (input.position * scale) + positionOffset;
      output.position = mul(transform, float4(pos, 1.0));
      output.color = input.color * tint;
      return output;
    }
  )";
}

static const char* getD3D12FragmentShaderSource() {
  return R"(
    struct PSInput {
      float4 position : SV_POSITION;
      float4 color : COLOR0;
    };

    float4 main(PSInput input) : SV_Target {
      return input.color;
    }
  )";
}

// OpenGL GLSL shaders
static const char* getOpenGLVertexShaderSource() {
  return R"(
    #version 330 core

    layout(location = 0) in vec3 position;
    layout(location = 1) in vec4 color;

    uniform mat4 transform;
    uniform vec4 tint;
    uniform vec3 positionOffset;
    uniform float scale;

    out vec4 fragColor;

    void main() {
      // Apply scale, then offset, then transform
      vec3 pos = (position * scale) + positionOffset;
      gl_Position = transform * vec4(pos, 1.0);
      fragColor = color * tint;
    }
  )";
}

static const char* getOpenGLFragmentShaderSource() {
  return R"(
    #version 330 core

    in vec4 fragColor;
    out vec4 outColor;

    void main() {
      outColor = fragColor;
    }
  )";
}

BufferBindGroupSession::BufferBindGroupSession(std::shared_ptr<Platform> platform)
    : RenderSession(std::move(platform)) {
  device_ = &getPlatform().getDevice();
  imguiSession_ = std::make_unique<iglu::imgui::Session>(getPlatform().getDevice(),
                                                         getPlatform().getInputDispatcher());
}

void BufferBindGroupSession::initialize() noexcept {
  // Create interleaved vertex buffer
  BufferDesc vbDesc;
  vbDesc.type = BufferDesc::BufferTypeBits::Vertex;
  vbDesc.data = kVertices;
  vbDesc.length = sizeof(kVertices);
  vbDesc.storage = ResourceStorage::Shared;

  vertexBuffer_ = device_->createBuffer(vbDesc, nullptr);
  if (!vertexBuffer_) {
    IGL_LOG_ERROR("BufferBindGroupSession: Failed to create vertex buffer\n");
    return;
  }

  // Create uniform buffer 0 (transform matrix) - 256-byte aligned for CBV
  BufferDesc uniformDesc0;
  uniformDesc0.type = BufferDesc::BufferTypeBits::Uniform;
  uniformDesc0.length = 256;  // CBVs require 256-byte alignment
  uniformDesc0.storage = ResourceStorage::Shared;

  uniformBuffer0_ = device_->createBuffer(uniformDesc0, nullptr);
  if (!uniformBuffer0_) {
    IGL_LOG_ERROR("BufferBindGroupSession: Failed to create uniform buffer 0\n");
    return;
  }

  // Create uniform buffer 1 (color tint) - 256-byte aligned for CBV
  BufferDesc uniformDesc1;
  uniformDesc1.type = BufferDesc::BufferTypeBits::Uniform;
  uniformDesc1.length = 256;  // CBVs require 256-byte alignment
  uniformDesc1.storage = ResourceStorage::Shared;

  uniformBuffer1_ = device_->createBuffer(uniformDesc1, nullptr);
  if (!uniformBuffer1_) {
    IGL_LOG_ERROR("BufferBindGroupSession: Failed to create uniform buffer 1\n");
    return;
  }

  // Create uniform buffer 2 (position offset) - 256-byte aligned for CBV
  BufferDesc uniformDesc2;
  uniformDesc2.type = BufferDesc::BufferTypeBits::Uniform;
  uniformDesc2.length = 256;  // CBVs require 256-byte alignment
  uniformDesc2.storage = ResourceStorage::Shared;

  uniformBuffer2_ = device_->createBuffer(uniformDesc2, nullptr);
  if (!uniformBuffer2_) {
    IGL_LOG_ERROR("BufferBindGroupSession: Failed to create uniform buffer 2\n");
    return;
  }

  // Create uniform buffer 3 (scale) - 256-byte aligned for CBV
  BufferDesc uniformDesc3;
  uniformDesc3.type = BufferDesc::BufferTypeBits::Uniform;
  uniformDesc3.length = 256;  // CBVs require 256-byte alignment
  uniformDesc3.storage = ResourceStorage::Shared;

  uniformBuffer3_ = device_->createBuffer(uniformDesc3, nullptr);
  if (!uniformBuffer3_) {
    IGL_LOG_ERROR("BufferBindGroupSession: Failed to create uniform buffer 3\n");
    return;
  }

  // Create buffer bind group with all 4 uniform buffers
  // Note: Slots 3-6 (b3-b6) because b0/b1 are root CBVs, b2 for push constants
  BindGroupBufferDesc bindGroupDesc;
  bindGroupDesc.buffers[3] = uniformBuffer0_;
  bindGroupDesc.offset[3] = 0;
  bindGroupDesc.size[3] = sizeof(UniformTransform);

  bindGroupDesc.buffers[4] = uniformBuffer1_;
  bindGroupDesc.offset[4] = 0;
  bindGroupDesc.size[4] = sizeof(UniformColorTint);

  bindGroupDesc.buffers[5] = uniformBuffer2_;
  bindGroupDesc.offset[5] = 0;
  bindGroupDesc.size[5] = sizeof(UniformPositionOffset);

  bindGroupDesc.buffers[6] = uniformBuffer3_;
  bindGroupDesc.offset[6] = 0;
  bindGroupDesc.size[6] = sizeof(UniformScale);

  bindGroupDesc.debugName = "Uniform Buffer Bind Group (4 buffers at slots 3-6)";

  bufferBindGroup_ = device_->createBindGroup(bindGroupDesc, nullptr);
  if (bufferBindGroup_.empty()) {
    IGL_LOG_ERROR("BufferBindGroupSession: Failed to create buffer bind group\n");
    return;
  }

  IGL_LOG_INFO("BufferBindGroupSession: Created buffer bind group with 4 uniform buffers (slots 3-6, b3-b6)\n");

  // Create shaders based on backend
  std::shared_ptr<IShaderStages> shaderStages;
  const auto backendType = device_->getBackendType();

  if (backendType == BackendType::D3D12) {
    shaderStages = ShaderStagesCreator::fromModuleStringInput(
        *device_,
        getD3D12VertexShaderSource(),
        "main",
        "",
        getD3D12FragmentShaderSource(),
        "main",
        "",
        nullptr);
  } else {
    // OpenGL/Vulkan/Metal
    shaderStages = ShaderStagesCreator::fromModuleStringInput(
        *device_,
        getOpenGLVertexShaderSource(),
        "main",
        "",
        getOpenGLFragmentShaderSource(),
        "main",
        "",
        nullptr);
  }

  if (!shaderStages) {
    IGL_LOG_ERROR("BufferBindGroupSession: Failed to create shader stages\n");
    return;
  }

  // Create vertex input state
  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0] = {
      .bufferIndex = 0,
      .format = VertexAttributeFormat::Float3,
      .offset = offsetof(Vertex, position),
      .name = "position",
      .location = 0
  };
  inputDesc.attributes[1] = {
      .bufferIndex = 0,
      .format = VertexAttributeFormat::Float4,
      .offset = offsetof(Vertex, color),
      .name = "color",
      .location = 1
  };
  inputDesc.numInputBindings = 1;
  inputDesc.inputBindings[0] = {.stride = sizeof(Vertex)};

  auto vertexInput = device_->createVertexInputState(inputDesc, nullptr);
  if (!vertexInput) {
    IGL_LOG_ERROR("BufferBindGroupSession: Failed to create vertex input state\n");
    return;
  }

  // Create pipeline state
  RenderPipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = std::move(shaderStages);
  pipelineDesc.vertexInputState = vertexInput;
  pipelineDesc.targetDesc.colorAttachments.resize(1);
  pipelineDesc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_SRGB;
  pipelineDesc.cullMode = igl::CullMode::Disabled;
  pipelineDesc.frontFaceWinding = igl::WindingMode::CounterClockwise;

  pipelineState_ = device_->createRenderPipeline(pipelineDesc, nullptr);
  if (!pipelineState_) {
    IGL_LOG_ERROR("BufferBindGroupSession: Failed to create render pipeline\n");
    return;
  }

  IGL_LOG_INFO("BufferBindGroupSession: Initialized successfully - using BindGroupBufferDesc with 4 uniform buffers (slots 3-6, b3-b6)\n");
}

void BufferBindGroupSession::update(SurfaceTextures surfaceTextures) noexcept {
  if (!surfaceTextures.color || !pipelineState_ || !vertexBuffer_ || bufferBindGroup_.empty()) {
    return;
  }

  // Update FPS counter
  const float deltaSeconds = 1.0f / 60.0f;  // Assuming 60 FPS target
  fps_.updateFPS(deltaSeconds);

  // Animate rotation
  rotation_ += 0.01f;

  // Update uniform buffer 0 (transform)
  UniformTransform transformData;
  transformData.transform = glm::rotate(glm::mat4(1.0f), rotation_, glm::vec3(0.0f, 0.0f, 1.0f));
  uniformBuffer0_->upload(&transformData, igl::BufferRange(sizeof(UniformTransform), 0));

  // Update uniform buffer 1 (color tint) - pulse between white and half brightness
  UniformColorTint tintData;
  float brightness = 0.75f + 0.25f * std::sin(rotation_ * 2.0f);
  tintData.tint = glm::vec4(brightness, brightness, brightness, 1.0f);
  uniformBuffer1_->upload(&tintData, igl::BufferRange(sizeof(UniformColorTint), 0));

  // Update uniform buffer 2 (position offset) - move in a circle
  UniformPositionOffset offsetData;
  offsetData.offset = glm::vec3(
      0.2f * std::cos(rotation_),
      0.2f * std::sin(rotation_),
      0.0f
  );
  offsetData.padding = 0.0f;
  uniformBuffer2_->upload(&offsetData, igl::BufferRange(sizeof(UniformPositionOffset), 0));

  // Update uniform buffer 3 (scale) - pulsate
  UniformScale scaleData;
  scaleData.scale = 0.8f + 0.2f * std::sin(rotation_ * 3.0f);
  scaleData.padding[0] = scaleData.padding[1] = scaleData.padding[2] = 0.0f;
  uniformBuffer3_->upload(&scaleData, igl::BufferRange(sizeof(UniformScale), 0));

  // Create render pass
  RenderPassDesc renderPass;
  renderPass.colorAttachments.resize(1);
  renderPass.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass.colorAttachments[0].clearColor = {0.1f, 0.1f, 0.15f, 1.0f};

  // Create framebuffer
  FramebufferDesc framebufferDesc;
  framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;

  auto framebuffer = device_->createFramebuffer(framebufferDesc, nullptr);
  if (!framebuffer) {
    IGL_LOG_ERROR("BufferBindGroupSession: Failed to create framebuffer\n");
    return;
  }

  // Create command buffer
  CommandBufferDesc cbDesc;
  auto commandQueue = device_->createCommandQueue({}, nullptr);
  auto commandBuffer = commandQueue->createCommandBuffer(cbDesc, nullptr);

  // Create render encoder
  auto encoder = commandBuffer->createRenderCommandEncoder(renderPass, framebuffer, {}, nullptr);

  // Set viewport
  Viewport viewport;
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(surfaceTextures.color->getDimensions().width);
  viewport.height = static_cast<float>(surfaceTextures.color->getDimensions().height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  encoder->bindViewport(viewport);

  // Set scissor rect
  ScissorRect scissor;
  scissor.x = 0;
  scissor.y = 0;
  scissor.width = surfaceTextures.color->getDimensions().width;
  scissor.height = surfaceTextures.color->getDimensions().height;
  encoder->bindScissorRect(scissor);

  // Bind pipeline, vertex buffer, and buffer bind group
  encoder->bindRenderPipelineState(pipelineState_);
  encoder->bindVertexBuffer(0, *vertexBuffer_, 0);
  encoder->bindBindGroup(bufferBindGroup_);  // Bind all 4 uniform buffers at once!
  encoder->draw(3); // 3 vertices

  // Render ImGui FPS overlay
  if (imguiSession_) {
    imguiSession_->beginFrame(framebufferDesc, 1.0f);
    imguiSession_->drawFPS(fps_.getAverageFPS());
    imguiSession_->endFrame(*device_, *encoder);
  }

  encoder->endEncoding();

  commandBuffer->present(surfaceTextures.color);
  commandQueue->submit(*commandBuffer);
}

} // namespace igl::shell
