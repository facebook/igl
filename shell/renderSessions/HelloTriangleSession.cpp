/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "HelloTriangleSession.h"

// Embedded shader bytecode (FXC compiled)
#include "simple_vs_fxc.h"
#include "simple_ps_fxc.h"

namespace igl::shell {

struct Vertex {
  float position[3];
  float color[4];
};

// Triangle vertices in NDC coordinates (-1 to 1)
static const Vertex kVertices[] = {
    {{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},   // Top - Red
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}}, // Bottom-left - Green
    {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},  // Bottom-right - Blue
};

HelloTriangleSession::HelloTriangleSession(std::shared_ptr<Platform> platform)
    : RenderSession(std::move(platform)) {
  device_ = &getPlatform().getDevice();
}

void HelloTriangleSession::initialize() noexcept {
  // Create vertex buffer
  BufferDesc vbDesc;
  vbDesc.type = BufferDesc::BufferTypeBits::Vertex;
  vbDesc.data = kVertices;
  vbDesc.length = sizeof(kVertices);
  vbDesc.storage = ResourceStorage::Shared;

  vertexBuffer_ = device_->createBuffer(vbDesc, nullptr);
  if (!vertexBuffer_) {
    IGL_LOG_ERROR("Failed to create vertex buffer\n");
    return;
  }

  // Create shader modules
  ShaderModuleDesc vsDesc = ShaderModuleDesc::fromBinaryInput(
      simple_vs_fxc_cso,
      simple_vs_fxc_cso_len,
      {ShaderStage::Vertex, "main"},
      "VertexShader");

  vertexShader_ = device_->createShaderModule(vsDesc, nullptr);
  if (!vertexShader_) {
    IGL_LOG_ERROR("Failed to create vertex shader\n");
    return;
  }

  ShaderModuleDesc psDesc = ShaderModuleDesc::fromBinaryInput(
      simple_ps_fxc_cso,
      simple_ps_fxc_cso_len,
      {ShaderStage::Fragment, "main"},
      "PixelShader");

  fragmentShader_ = device_->createShaderModule(psDesc, nullptr);
  if (!fragmentShader_) {
    IGL_LOG_ERROR("Failed to create fragment shader\n");
    return;
  }

  // Create shader stages
  auto shaderStagesDesc = ShaderStagesDesc::fromRenderModules(vertexShader_, fragmentShader_);
  auto shaderStages = device_->createShaderStages(shaderStagesDesc, nullptr);
  if (!shaderStages) {
    IGL_LOG_ERROR("Failed to create shader stages\n");
    return;
  }

  // Create pipeline state
  RenderPipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = std::move(shaderStages);
  pipelineDesc.targetDesc.colorAttachments.resize(1);
  pipelineDesc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_SRGB;
  pipelineDesc.cullMode = igl::CullMode::Disabled;
  pipelineDesc.frontFaceWinding = igl::WindingMode::CounterClockwise;

  pipelineState_ = device_->createRenderPipeline(pipelineDesc, nullptr);
  if (!pipelineState_) {
    IGL_LOG_ERROR("Failed to create render pipeline\n");
    return;
  }

  IGL_LOG_INFO("HelloTriangleSession initialized successfully\n");
}

void HelloTriangleSession::update(SurfaceTextures surfaceTextures) noexcept {
  if (!surfaceTextures.color || !pipelineState_ || !vertexBuffer_) {
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
    IGL_LOG_ERROR("Failed to create framebuffer\n");
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

  // Bind pipeline and draw
  encoder->bindRenderPipelineState(pipelineState_);
  encoder->bindVertexBuffer(0, *vertexBuffer_, 0);
  encoder->draw(3); // 3 vertices

  encoder->endEncoding();

  commandBuffer->present(surfaceTextures.color);
  commandQueue->submit(*commandBuffer);
}

} // namespace igl::shell
