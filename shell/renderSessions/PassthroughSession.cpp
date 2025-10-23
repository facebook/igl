/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/renderSessions/PassthroughSession.h>

#include <shell/shared/renderSession/ShellParams.h>
#include <igl/NameHandle.h>
#include <igl/ShaderCreator.h>
#include <igl/CommandBuffer.h>
#include <igl/RenderCommandEncoder.h>
#include <igl/Framebuffer.h>
#include <igl/RenderPass.h>
#include <igl/RenderPipelineState.h>
#include <igl/VertexInputState.h>
#include <igl/SamplerState.h>

namespace igl::shell {

namespace {

// Full-screen quad vertices in NDC space (-1 to 1)
float vertexData[] = {
    -1.0f,  1.0f, 0.0f, 1.0f,  // Top-left
     1.0f,  1.0f, 0.0f, 1.0f,  // Top-right
    -1.0f, -1.0f, 0.0f, 1.0f,  // Bottom-left
     1.0f, -1.0f, 0.0f, 1.0f,  // Bottom-right
};

float uvData[] = {
    0.0f, 1.0f,  // Top-left
    1.0f, 1.0f,  // Top-right
    0.0f, 0.0f,  // Bottom-left
    1.0f, 0.0f,  // Bottom-right
};

uint16_t indexData[] = {0, 1, 2, 2, 1, 3};

// 2x2 texture data: {0x11223344, 0x11111111, 0x22222222, 0x33333333}
// This is the same test data from TextureTest.Passthrough
uint32_t textureData[] = {
    0x11223344,
    0x11111111,
    0x22222222,
    0x33333333,
};

std::string getD3D12VertexShaderSource() {
  return R"(
struct VSIn { float4 position_in : POSITION; float2 uv_in : TEXCOORD0; };
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
PSIn main(VSIn i) { PSIn o; o.position = i.position_in; o.uv = i.uv_in; return o; }
)";
}

std::string getD3D12FragmentShaderSource() {
  return R"(
Texture2D inputImage : register(t0);
SamplerState samp0 : register(s0);
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
float4 main(PSIn i) : SV_TARGET { return inputImage.Sample(samp0, i.uv); }
)";
}

std::string getOpenGLVertexShaderSource() {
  return R"(#version 100
precision highp float;
attribute vec4 position_in;
attribute vec2 uv_in;
varying vec2 uv;
void main() {
  gl_Position = position_in;
  uv = uv_in;
})";
}

std::string getOpenGLFragmentShaderSource() {
  return R"(#version 100
precision highp float;
varying vec2 uv;
uniform sampler2D inputImage;
void main() {
  gl_FragColor = texture2D(inputImage, uv);
})";
}

std::string getVulkanVertexShaderSource() {
  return R"(
layout(location = 0) in vec4 position_in;
layout(location = 1) in vec2 uv_in;
layout(location = 0) out vec2 uv;
void main() {
  gl_Position = position_in;
  uv = uv_in;
}
)";
}

std::string getVulkanFragmentShaderSource() {
  return R"(
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_FragColor;
layout(set = 0, binding = 0) uniform sampler2D inputImage;
void main() {
  out_FragColor = texture(inputImage, uv);
}
)";
}

std::string getMetalShaderSource() {
  return R"(
using namespace metal;

typedef struct {
  float4 position [[attribute(0)]];
  float2 uv [[attribute(1)]];
} VertexIn;

typedef struct {
  float4 position [[position]];
  float2 uv;
} VertexOut;

vertex VertexOut vertexShader(uint vid [[vertex_id]], constant VertexIn * vertices [[buffer(1)]]) {
  VertexOut out;
  out.position = vertices[vid].position;
  out.uv = vertices[vid].uv;
  return out;
}

fragment float4 fragmentShader(VertexOut IN [[stage_in]], texture2d<float> inputImage [[texture(0)]], sampler samp0 [[sampler(0)]]) {
  return inputImage.sample(samp0, IN.uv);
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
  case igl::BackendType::D3D12:
    return igl::ShaderStagesCreator::fromModuleStringInput(device,
                                                           getD3D12VertexShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           getD3D12FragmentShaderSource().c_str(),
                                                           "main",
                                                           "",
                                                           nullptr);
  }
  IGL_UNREACHABLE_RETURN(nullptr)
}
} // namespace

void PassthroughSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();
  Result ret;

  IGL_LOG_INFO("PassthroughSession::initialize() - Creating buffers and textures\n");

  // Create vertex buffer
  const BufferDesc vbDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, vertexData, sizeof(vertexData));
  vb0_ = device.createBuffer(vbDesc, &ret);
  IGL_DEBUG_ASSERT(vb0_ != nullptr);
  IGL_LOG_INFO("PassthroughSession: Created vertex buffer\n");

  // Create UV buffer
  const BufferDesc uvDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Vertex, uvData, sizeof(uvData));
  uv0_ = device.createBuffer(uvDesc, &ret);
  IGL_DEBUG_ASSERT(uv0_ != nullptr);
  IGL_LOG_INFO("PassthroughSession: Created UV buffer\n");

  // Create index buffer
  const BufferDesc ibDesc =
      BufferDesc(BufferDesc::BufferTypeBits::Index, indexData, sizeof(indexData));
  ib0_ = device.createBuffer(ibDesc, &ret);
  IGL_DEBUG_ASSERT(ib0_ != nullptr);
  IGL_LOG_INFO("PassthroughSession: Created index buffer\n");

  // Create input texture (2x2 with test pattern)
  const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                 2,
                                                 2,
                                                 TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = device.createTexture(texDesc, &ret);
  IGL_DEBUG_ASSERT(inputTexture_ != nullptr);
  IGL_LOG_INFO("PassthroughSession: Created input texture (2x2)\n");

  // Upload texture data
  auto rangeDesc = TextureRangeDesc::new2D(0, 0, 2, 2);
  inputTexture_->upload(rangeDesc, textureData);
  IGL_LOG_INFO("PassthroughSession: Uploaded texture data (0x%08x at [0,0])\n", textureData[0]);

  // Create sampler state
  const SamplerStateDesc samplerDesc;
  sampler_ = device.createSamplerState(samplerDesc, &ret);
  IGL_DEBUG_ASSERT(sampler_ != nullptr);
  IGL_LOG_INFO("PassthroughSession: Created sampler state\n");

  // Create vertex input state
  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float4;
  inputDesc.attributes[0].offset = 0;
  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].name = "position_in";
  inputDesc.attributes[0].location = 0;
  inputDesc.inputBindings[0].stride = sizeof(float) * 4;

  inputDesc.attributes[1].format = VertexAttributeFormat::Float2;
  inputDesc.attributes[1].offset = 0;
  inputDesc.attributes[1].bufferIndex = 1;
  inputDesc.attributes[1].name = "uv_in";
  inputDesc.attributes[1].location = 1;
  inputDesc.inputBindings[1].stride = sizeof(float) * 2;

  inputDesc.numInputBindings = 2;

  vertexInput0_ = device.createVertexInputState(inputDesc, &ret);
  IGL_DEBUG_ASSERT(vertexInput0_ != nullptr);
  IGL_LOG_INFO("PassthroughSession: Created vertex input state\n");

  // Create shader stages
  shaderStages_ = getShaderStagesForBackend(device);
  IGL_DEBUG_ASSERT(shaderStages_ != nullptr);
  IGL_LOG_INFO("PassthroughSession: Created shader stages\n");

  // Create command queue
  const CommandQueueDesc desc{};
  commandQueue_ = device.createCommandQueue(desc, &ret);
  IGL_DEBUG_ASSERT(commandQueue_ != nullptr);
  IGL_LOG_INFO("PassthroughSession: Created command queue\n");

  // Setup render pass (clear to black like the test)
  renderPass_.colorAttachments.resize(1);
  renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass_.colorAttachments[0].clearColor = {0.0, 0.0, 0.0, 1.0};

  IGL_LOG_INFO("PassthroughSession::initialize() - Complete!\n");
}

void PassthroughSession::update(SurfaceTextures surfaceTextures) noexcept {
  Result ret;

  // Create or update framebuffer
  if (framebuffer_ == nullptr) {
    FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;
    // Add depth attachment if available (like HelloWorldSession)
    if (surfaceTextures.depth) {
      framebufferDesc.depthAttachment.texture = surfaceTextures.depth;
      if (surfaceTextures.depth->getProperties().hasStencil()) {
        framebufferDesc.stencilAttachment.texture = surfaceTextures.depth;
      }
    }
    framebuffer_ = getPlatform().getDevice().createFramebuffer(framebufferDesc, &ret);
    if (!ret.isOk()) {
      IGL_LOG_ERROR("PassthroughSession: Failed to create framebuffer: %s\n", ret.message.c_str());
      return;
    }
    IGL_LOG_INFO("PassthroughSession: Created framebuffer\n");
  } else {
    framebuffer_->updateDrawable(surfaceTextures);
  }

  // Create graphics pipeline
  if (pipelineState_ == nullptr) {
    RenderPipelineDesc graphicsDesc;
    graphicsDesc.vertexInputState = vertexInput0_;
    graphicsDesc.shaderStages = shaderStages_;
    graphicsDesc.targetDesc.colorAttachments.resize(1);
    graphicsDesc.targetDesc.colorAttachments[0].textureFormat =
        framebuffer_->getColorAttachment(0)->getFormat();
    // Set depth format if available
    if (framebuffer_->getDepthAttachment()) {
      graphicsDesc.targetDesc.depthAttachmentFormat = framebuffer_->getDepthAttachment()->getFormat();
    }
    if (framebuffer_->getStencilAttachment()) {
      graphicsDesc.targetDesc.stencilAttachmentFormat = framebuffer_->getStencilAttachment()->getFormat();
    }
    graphicsDesc.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE("inputImage");
    graphicsDesc.cullMode = igl::CullMode::Disabled;

    pipelineState_ = getPlatform().getDevice().createRenderPipeline(graphicsDesc, &ret);
    if (!ret.isOk()) {
      IGL_LOG_ERROR("PassthroughSession: Failed to create pipeline: %s\n", ret.message.c_str());
      return;
    }
    IGL_LOG_INFO("PassthroughSession: Created pipeline state\n");
  }

  // Create command buffer
  IGL_LOG_INFO("PassthroughSession: Creating command buffer...\n");
  const CommandBufferDesc cbDesc;
  auto buffer = commandQueue_->createCommandBuffer(cbDesc, &ret);
  if (!buffer || !ret.isOk()) {
    IGL_LOG_ERROR("PassthroughSession: Failed to create command buffer: %s\n", ret.message.c_str());
    return;
  }

  // Submit rendering commands
  IGL_LOG_INFO("PassthroughSession: Creating render encoder...\n");
  const std::shared_ptr<IRenderCommandEncoder> commands =
      buffer->createRenderCommandEncoder(renderPass_, framebuffer_);
  if (!commands) {
    IGL_LOG_ERROR("PassthroughSession: Failed to create render command encoder\n");
    return;
  }
  IGL_LOG_INFO("PassthroughSession: Render encoder created successfully\n");

  commands->bindVertexBuffer(0, *vb0_);
  commands->bindVertexBuffer(1, *uv0_);
  commands->bindRenderPipelineState(pipelineState_);
  commands->bindTexture(0, BindTarget::kFragment, inputTexture_.get());
  commands->bindSamplerState(0, BindTarget::kFragment, sampler_.get());
  commands->bindIndexBuffer(*ib0_, IndexFormat::UInt16);
  IGL_LOG_INFO("PassthroughSession: Drawing...\n");
  commands->drawIndexed(6);

  commands->endEncoding();
  IGL_LOG_INFO("PassthroughSession: Encoding complete\n");

  auto drawableSurface = framebuffer_->getColorAttachment(0);
  if (shellParams().shouldPresent) {
    buffer->present(drawableSurface);
  }

  IGL_LOG_INFO("PassthroughSession: Submitting command buffer...\n");
  commandQueue_->submit(*buffer);
  IGL_LOG_INFO("PassthroughSession: Frame complete!\n");
  RenderSession::update(surfaceTextures);
}

} // namespace igl::shell
