/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "MultiBufferSession.h"
#include <igl/ShaderCreator.h>

namespace igl::shell {

// Separate buffers for vertex attributes
static const float kPositions[] = {
    0.0f, 0.5f, 0.0f,    // Top
    -0.5f, -0.5f, 0.0f,  // Bottom-left
    0.5f, -0.5f, 0.0f,   // Bottom-right
};

static const float kColors[] = {
    1.0f, 0.0f, 0.0f, 1.0f,  // Top - Red
    0.0f, 1.0f, 0.0f, 1.0f,  // Bottom-left - Green
    0.0f, 0.0f, 1.0f, 1.0f,  // Bottom-right - Blue
};

static const float kUVs[] = {
    0.5f, 0.0f,  // Top - center top
    0.0f, 1.0f,  // Bottom-left
    1.0f, 1.0f,  // Bottom-right
};

// D3D12 HLSL shaders
static const char* getD3D12VertexShaderSource() {
  return R"(
    struct VSInput {
      float3 position : POSITION;
      float4 color : COLOR0;
      float2 uv : TEXCOORD0;
    };

    struct VSOutput {
      float4 position : SV_POSITION;
      float4 color : COLOR0;
      float2 uv : TEXCOORD0;
    };

    VSOutput main(VSInput input) {
      VSOutput output;
      output.position = float4(input.position, 1.0);
      output.color = input.color;
      output.uv = input.uv;
      return output;
    }
  )";
}

static const char* getD3D12FragmentShaderSource() {
  return R"(
    Texture2D colorTexture : register(t0);
    SamplerState colorSampler : register(s0);

    struct PSInput {
      float4 position : SV_POSITION;
      float4 color : COLOR0;
      float2 uv : TEXCOORD0;
    };

    float4 main(PSInput input) : SV_Target {
      float4 texColor = colorTexture.Sample(colorSampler, input.uv);
      return input.color * texColor;
    }
  )";
}

// OpenGL GLSL shaders
static const char* getOpenGLVertexShaderSource() {
  return R"(
    #version 330 core

    layout(location = 0) in vec3 position;
    layout(location = 1) in vec4 color;
    layout(location = 2) in vec2 uv;

    out vec4 fragColor;
    out vec2 fragUV;

    void main() {
      gl_Position = vec4(position, 1.0);
      fragColor = color;
      fragUV = uv;
    }
  )";
}

static const char* getOpenGLFragmentShaderSource() {
  return R"(
    #version 330 core

    in vec4 fragColor;
    in vec2 fragUV;
    out vec4 outColor;

    uniform sampler2D colorTexture;

    void main() {
      vec4 texColor = texture(colorTexture, fragUV);
      outColor = fragColor * texColor;
    }
  )";
}

MultiBufferSession::MultiBufferSession(std::shared_ptr<Platform> platform)
    : RenderSession(std::move(platform)) {
  device_ = &getPlatform().getDevice();
}

void MultiBufferSession::initialize() noexcept {
  // Create position buffer (slot 0)
  BufferDesc posDesc;
  posDesc.type = BufferDesc::BufferTypeBits::Vertex;
  posDesc.data = kPositions;
  posDesc.length = sizeof(kPositions);
  posDesc.storage = ResourceStorage::Shared;

  positionBuffer_ = device_->createBuffer(posDesc, nullptr);
  if (!positionBuffer_) {
    IGL_LOG_ERROR("MultiBufferSession: Failed to create position buffer\n");
    return;
  }

  // Create color buffer (slot 1)
  BufferDesc colorDesc;
  colorDesc.type = BufferDesc::BufferTypeBits::Vertex;
  colorDesc.data = kColors;
  colorDesc.length = sizeof(kColors);
  colorDesc.storage = ResourceStorage::Shared;

  colorBuffer_ = device_->createBuffer(colorDesc, nullptr);
  if (!colorBuffer_) {
    IGL_LOG_ERROR("MultiBufferSession: Failed to create color buffer\n");
    return;
  }

  // Create UV buffer (slot 2)
  BufferDesc uvDesc;
  uvDesc.type = BufferDesc::BufferTypeBits::Vertex;
  uvDesc.data = kUVs;
  uvDesc.length = sizeof(kUVs);
  uvDesc.storage = ResourceStorage::Shared;

  uvBuffer_ = device_->createBuffer(uvDesc, nullptr);
  if (!uvBuffer_) {
    IGL_LOG_ERROR("MultiBufferSession: Failed to create UV buffer\n");
    return;
  }

  // Create a simple procedural texture (checkerboard pattern)
  const uint32_t texWidth = 256;
  const uint32_t texHeight = 256;
  std::vector<uint32_t> texData(texWidth * texHeight);
  for (uint32_t y = 0; y < texHeight; y++) {
    for (uint32_t x = 0; x < texWidth; x++) {
      // Checkerboard pattern
      bool isWhite = ((x / 32) + (y / 32)) % 2 == 0;
      texData[y * texWidth + x] = isWhite ? 0xFFFFFFFF : 0xFF808080;
    }
  }

  TextureDesc texDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8,
      texWidth,
      texHeight,
      TextureDesc::TextureUsageBits::Sampled);
  texDesc.debugName = "MultiBuffer Checkerboard";

  texture_ = device_->createTexture(texDesc, nullptr);
  if (!texture_) {
    IGL_LOG_ERROR("MultiBufferSession: Failed to create texture\n");
    return;
  }
  texture_->upload(texture_->getFullRange(), texData.data());

  // Create sampler
  SamplerStateDesc samplerDesc;
  samplerDesc.minFilter = SamplerMinMagFilter::Linear;
  samplerDesc.magFilter = SamplerMinMagFilter::Linear;
  samplerDesc.addressModeU = SamplerAddressMode::Repeat;
  samplerDesc.addressModeV = SamplerAddressMode::Repeat;

  sampler_ = device_->createSamplerState(samplerDesc, nullptr);
  if (!sampler_) {
    IGL_LOG_ERROR("MultiBufferSession: Failed to create sampler\n");
    return;
  }

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
    IGL_LOG_ERROR("MultiBufferSession: Failed to create shader stages\n");
    return;
  }

  // Create vertex input state - three separate buffer bindings
  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 3;
  inputDesc.attributes[0] = {
      .bufferIndex = 0,
      .format = VertexAttributeFormat::Float3,
      .offset = 0,
      .name = "position",
      .location = 0
  };
  inputDesc.attributes[1] = {
      .bufferIndex = 1,
      .format = VertexAttributeFormat::Float4,
      .offset = 0,
      .name = "color",
      .location = 1
  };
  inputDesc.attributes[2] = {
      .bufferIndex = 2,
      .format = VertexAttributeFormat::Float2,
      .offset = 0,
      .name = "uv",
      .location = 2
  };
  inputDesc.numInputBindings = 3;
  inputDesc.inputBindings[0] = {.stride = sizeof(float) * 3};  // 3 floats per position
  inputDesc.inputBindings[1] = {.stride = sizeof(float) * 4};  // 4 floats per color
  inputDesc.inputBindings[2] = {.stride = sizeof(float) * 2};  // 2 floats per UV

  auto vertexInput = device_->createVertexInputState(inputDesc, nullptr);
  if (!vertexInput) {
    IGL_LOG_ERROR("MultiBufferSession: Failed to create vertex input state\n");
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
    IGL_LOG_ERROR("MultiBufferSession: Failed to create render pipeline\n");
    return;
  }

  IGL_LOG_INFO("MultiBufferSession: Initialized successfully - using 3 separate vertex buffers (position, color, UV) with texture\n");
}

void MultiBufferSession::update(SurfaceTextures surfaceTextures) noexcept {
  if (!surfaceTextures.color || !pipelineState_ || !positionBuffer_ || !colorBuffer_ || !uvBuffer_ || !texture_ || !sampler_) {
    return;
  }

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
    IGL_LOG_ERROR("MultiBufferSession: Failed to create framebuffer\n");
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

  // Bind pipeline and resources
  encoder->bindRenderPipelineState(pipelineState_);
  encoder->bindVertexBuffer(0, *positionBuffer_, 0);           // Bind position buffer to slot 0
  encoder->bindVertexBuffer(1, *colorBuffer_, 0);              // Bind color buffer to slot 1
  encoder->bindVertexBuffer(2, *uvBuffer_, 0);                 // Bind UV buffer to slot 2
  encoder->bindTexture(0, texture_.get());                     // Bind texture to slot 0
  encoder->bindSamplerState(0, BindTarget::kFragment, sampler_.get());  // Bind sampler to slot 0
  encoder->draw(3); // 3 vertices

  encoder->endEncoding();

  commandBuffer->present(surfaceTextures.color);
  commandQueue->submit(*commandBuffer);
}

} // namespace igl::shell
