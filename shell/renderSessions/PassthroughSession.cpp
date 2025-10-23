/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/renderSessions/PassthroughSession.h>
#include <shell/shared/renderSession/ShellParams.h>
#include <igl/ShaderCreator.h>
#include <igl/d3d12/HeadlessContext.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/D3D12Headers.h>

namespace igl::shell {

// Full-screen quad vertices in NDC space (-1 to 1)
static const float kVertexData[] = {
    -1.0f,  1.0f, 0.0f, 1.0f,  // Top-left
     1.0f,  1.0f, 0.0f, 1.0f,  // Top-right
    -1.0f, -1.0f, 0.0f, 1.0f,  // Bottom-left
     1.0f, -1.0f, 0.0f, 1.0f,  // Bottom-right
};

static const float kUVData[] = {
    0.0f, 1.0f,  // Top-left
    1.0f, 1.0f,  // Top-right
    0.0f, 0.0f,  // Bottom-left
    1.0f, 0.0f,  // Bottom-right
};

static const uint16_t kIndexData[] = {0, 1, 2, 2, 1, 3};

// 2x2 texture data: {0x11223344, 0x11111111, 0x22222222, 0x33333333}
// This is the same test data from TextureTest.Passthrough
static const uint32_t kTextureData[] = {
    0x11223344,
    0x11111111,
    0x22222222,
    0x33333333,
};

// D3D12 HLSL shaders
static const char* kVertexShaderHLSL = R"(
struct VSIn { float4 position : POSITION; float2 uv : TEXCOORD0; };
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
PSIn main(VSIn input) {
  PSIn output;
  output.position = input.position;
  output.uv = input.uv;
  return output;
}
)";

static const char* kFragmentShaderHLSL = R"(
Texture2D inputImage : register(t0);
SamplerState samp0 : register(s0);
struct PSIn { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
float4 main(PSIn input) : SV_TARGET {
  return inputImage.Sample(samp0, input.uv);
}
)";

PassthroughSession::PassthroughSession(std::shared_ptr<Platform> platform)
    : RenderSession(std::move(platform)) {
  device_ = &getPlatform().getDevice();
}

void PassthroughSession::initialize() noexcept {
  IGL_LOG_INFO("PassthroughSession::initialize() - START\n");

  // Create vertex buffer
  BufferDesc vbDesc;
  vbDesc.type = BufferDesc::BufferTypeBits::Vertex;
  vbDesc.data = kVertexData;
  vbDesc.length = sizeof(kVertexData);
  vbDesc.storage = ResourceStorage::Shared;
  vertexBuffer_ = device_->createBuffer(vbDesc, nullptr);
  if (!vertexBuffer_) {
    IGL_LOG_ERROR("Failed to create vertex buffer\n");
    return;
  }

  // Create UV buffer
  BufferDesc uvDesc;
  uvDesc.type = BufferDesc::BufferTypeBits::Vertex;
  uvDesc.data = kUVData;
  uvDesc.length = sizeof(kUVData);
  uvDesc.storage = ResourceStorage::Shared;
  uvBuffer_ = device_->createBuffer(uvDesc, nullptr);
  if (!uvBuffer_) {
    IGL_LOG_ERROR("Failed to create UV buffer\n");
    return;
  }

  // Create index buffer
  BufferDesc ibDesc;
  ibDesc.type = BufferDesc::BufferTypeBits::Index;
  ibDesc.data = kIndexData;
  ibDesc.length = sizeof(kIndexData);
  ibDesc.storage = ResourceStorage::Shared;
  indexBuffer_ = device_->createBuffer(ibDesc, nullptr);
  if (!indexBuffer_) {
    IGL_LOG_ERROR("Failed to create index buffer\n");
    return;
  }

  // Create input texture (2x2 with test pattern)
  TextureDesc texDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8, 2, 2, TextureDesc::TextureUsageBits::Sampled);
  inputTexture_ = device_->createTexture(texDesc, nullptr);
  if (!inputTexture_) {
    IGL_LOG_ERROR("Failed to create input texture\n");
    return;
  }

  // Upload texture data
  auto rangeDesc = TextureRangeDesc::new2D(0, 0, 2, 2);
  Result uploadResult = inputTexture_->upload(rangeDesc, kTextureData);
  if (!uploadResult.isOk()) {
    IGL_LOG_ERROR("Failed to upload texture data: %s\n", uploadResult.message.c_str());
    return;
  }
  IGL_LOG_INFO("PassthroughSession: Uploaded texture data (0x%08x at [0,0])\n", kTextureData[0]);

  // Create offscreen texture for validation (same size as input texture, matches test)
  const TextureDesc offscreenTexDesc = TextureDesc::new2D(
      TextureFormat::RGBA_UNorm8,
      2, 2,
      TextureDesc::TextureUsageBits::Sampled | TextureDesc::TextureUsageBits::Attachment);
  offscreenTexture_ = device_->createTexture(offscreenTexDesc, nullptr);
  if (!offscreenTexture_) {
    IGL_LOG_ERROR("PassthroughSession: Failed to create offscreen texture\n");
    return;
  }
  IGL_LOG_INFO("PassthroughSession: Created offscreen texture for validation (2x2)\n");

  // Create sampler state
  SamplerStateDesc samplerDesc;
  sampler_ = device_->createSamplerState(samplerDesc, nullptr);
  if (!sampler_) {
    IGL_LOG_ERROR("Failed to create sampler state\n");
    return;
  }

  // Create shader stages using ShaderStagesCreator (same as tests)
  std::unique_ptr<IShaderStages> shaderStages =
      ShaderStagesCreator::fromModuleStringInput(*device_,
                                                 kVertexShaderHLSL,
                                                 "main",
                                                 "",
                                                 kFragmentShaderHLSL,
                                                 "main",
                                                 "",
                                                 nullptr);
  if (!shaderStages) {
    IGL_LOG_ERROR("Failed to create shader stages\n");
    return;
  }

  // Create vertex input state
  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = 2;
  inputDesc.attributes[0].format = VertexAttributeFormat::Float4;
  inputDesc.attributes[0].offset = 0;
  inputDesc.attributes[0].bufferIndex = 0;
  inputDesc.attributes[0].name = "position";
  inputDesc.attributes[0].location = 0;
  inputDesc.inputBindings[0].stride = sizeof(float) * 4;

  inputDesc.attributes[1].format = VertexAttributeFormat::Float2;
  inputDesc.attributes[1].offset = 0;
  inputDesc.attributes[1].bufferIndex = 1;
  inputDesc.attributes[1].name = "uv";
  inputDesc.attributes[1].location = 1;
  inputDesc.inputBindings[1].stride = sizeof(float) * 2;

  inputDesc.numInputBindings = 2;
  vertexInputState_ = device_->createVertexInputState(inputDesc, nullptr);
  if (!vertexInputState_) {
    IGL_LOG_ERROR("Failed to create vertex input state\n");
    return;
  }

  // Create pipeline state
  RenderPipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = std::move(shaderStages);
  pipelineDesc.vertexInputState = vertexInputState_;
  pipelineDesc.targetDesc.colorAttachments.resize(1);
  pipelineDesc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_SRGB;
  pipelineDesc.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE("inputImage");
  pipelineDesc.cullMode = igl::CullMode::Disabled;
  pipelineDesc.frontFaceWinding = igl::WindingMode::CounterClockwise;

  pipelineState_ = device_->createRenderPipeline(pipelineDesc, nullptr);
  if (!pipelineState_) {
    IGL_LOG_ERROR("Failed to create render pipeline\n");
    return;
  }

  IGL_LOG_INFO("PassthroughSession::initialize() - Windowed setup complete!\n");

  // ===========================
  //  Initialize headless context for comparison
  // ===========================
  IGL_LOG_INFO("\n====== INITIALIZING HEADLESS CONTEXT ======\n");

  headlessContext_ = std::make_unique<igl::d3d12::HeadlessD3D12Context>();
  Result headlessResult = headlessContext_->initializeHeadless(2, 2);
  if (!headlessResult.isOk()) {
    IGL_LOG_ERROR("Failed to initialize headless context: %s\n", headlessResult.message.c_str());
    return;
  }

  headlessDevice_ = std::make_unique<igl::d3d12::Device>(std::move(headlessContext_));
  headlessCommandQueue_ = headlessDevice_->createCommandQueue({}, nullptr);

  // Create headless buffers (identical to windowed)
  headlessVertexBuffer_ = headlessDevice_->createBuffer(vbDesc, nullptr);
  headlessUvBuffer_ = headlessDevice_->createBuffer(uvDesc, nullptr);
  headlessIndexBuffer_ = headlessDevice_->createBuffer(ibDesc, nullptr);

  // Create headless input texture
  headlessInputTexture_ = headlessDevice_->createTexture(texDesc, nullptr);
  uploadResult = headlessInputTexture_->upload(rangeDesc, kTextureData);
  if (!uploadResult.isOk()) {
    IGL_LOG_ERROR("HEADLESS: Failed to upload texture data: %s\n", uploadResult.message.c_str());
    return;
  }
  IGL_LOG_INFO("HEADLESS: Uploaded texture data (0x%08x at [0,0])\n", kTextureData[0]);

  // Create headless offscreen texture
  headlessOffscreenTexture_ = headlessDevice_->createTexture(offscreenTexDesc, nullptr);
  IGL_LOG_INFO("HEADLESS: Created offscreen texture for validation (2x2)\n");

  // Create headless sampler
  headlessSampler_ = headlessDevice_->createSamplerState(samplerDesc, nullptr);

  // Create headless shader stages
  std::unique_ptr<IShaderStages> headlessShaderStages =
      ShaderStagesCreator::fromModuleStringInput(*headlessDevice_,
                                                 kVertexShaderHLSL,
                                                 "main",
                                                 "",
                                                 kFragmentShaderHLSL,
                                                 "main",
                                                 "",
                                                 nullptr);

  // Create headless vertex input state
  headlessVertexInputState_ = headlessDevice_->createVertexInputState(inputDesc, nullptr);

  // Create headless pipeline state
  RenderPipelineDesc headlessPipelineDesc;
  headlessPipelineDesc.shaderStages = std::move(headlessShaderStages);
  headlessPipelineDesc.vertexInputState = headlessVertexInputState_;
  headlessPipelineDesc.targetDesc.colorAttachments.resize(1);
  headlessPipelineDesc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;
  headlessPipelineDesc.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE("inputImage");
  headlessPipelineDesc.cullMode = igl::CullMode::Disabled;
  headlessPipelineDesc.frontFaceWinding = igl::WindingMode::CounterClockwise;

  headlessPipelineState_ = headlessDevice_->createRenderPipeline(headlessPipelineDesc, nullptr);

  IGL_LOG_INFO("====== HEADLESS CONTEXT INITIALIZED ======\n\n");
  IGL_LOG_INFO("PassthroughSession::initialize() - Complete!\n");
}

void PassthroughSession::update(SurfaceTextures surfaceTextures) noexcept {
  if (!surfaceTextures.color || !pipelineState_ || !vertexBuffer_ || !inputTexture_ || !offscreenTexture_) {
    return;
  }

  // Render with HEADLESS context first (for PIX comparison)
  if (headlessDevice_ && !headlessValidationDone_) {
    renderHeadless();
  }

  // Then render with WINDOWED context
  renderWindowed(surfaceTextures);
}

void PassthroughSession::renderWindowed(SurfaceTextures surfaceTextures) {
  // On first frame, render to offscreen texture and validate (like the test)
  if (!validationDone_) {
    IGL_LOG_INFO("\n=== PassthroughSession: VALIDATION PASS ===\n");

    // Create render pass for offscreen rendering
    RenderPassDesc offscreenRenderPass;
    offscreenRenderPass.colorAttachments.resize(1);
    offscreenRenderPass.colorAttachments[0].loadAction = LoadAction::Clear;
    offscreenRenderPass.colorAttachments[0].storeAction = StoreAction::Store;
    offscreenRenderPass.colorAttachments[0].clearColor = {0.0f, 0.0f, 0.0f, 1.0f};  // Black like test

    // Create offscreen framebuffer
    FramebufferDesc offscreenFbDesc;
    offscreenFbDesc.colorAttachments[0].texture = offscreenTexture_;
    auto offscreenFramebuffer = device_->createFramebuffer(offscreenFbDesc, nullptr);
    if (!offscreenFramebuffer) {
      IGL_LOG_ERROR("Failed to create offscreen framebuffer\n");
      return;
    }

    // Render to offscreen texture
    CommandBufferDesc cbDesc;
    auto commandQueue = device_->createCommandQueue({}, nullptr);
    auto commandBuffer = commandQueue->createCommandBuffer(cbDesc, nullptr);
    auto encoder = commandBuffer->createRenderCommandEncoder(offscreenRenderPass, offscreenFramebuffer, {}, nullptr);

    if (!encoder) {
      IGL_LOG_ERROR("Failed to create render encoder for validation\n");
      return;
    }

    // NOTE: PIX event markers would go here if we had access to the command list
    // The log messages will help identify this as WINDOWED rendering in PIX
    IGL_LOG_INFO("==> PIX MARKER: WINDOWED_TEXTURE_SAMPLING <==\n");

    // Set viewport for 2x2 offscreen texture
    Viewport offscreenViewport;
    offscreenViewport.x = 0.0f;
    offscreenViewport.y = 0.0f;
    offscreenViewport.width = 2.0f;
    offscreenViewport.height = 2.0f;
    offscreenViewport.minDepth = 0.0f;
    offscreenViewport.maxDepth = 1.0f;
    encoder->bindViewport(offscreenViewport);

    ScissorRect offscreenScissor;
    offscreenScissor.x = 0;
    offscreenScissor.y = 0;
    offscreenScissor.width = 2;
    offscreenScissor.height = 2;
    encoder->bindScissorRect(offscreenScissor);

    // Bind and draw (same as test)
    encoder->bindRenderPipelineState(pipelineState_);
    encoder->bindVertexBuffer(0, *vertexBuffer_, 0);
    encoder->bindVertexBuffer(1, *uvBuffer_, 0);
    encoder->bindTexture(0, BindTarget::kFragment, inputTexture_.get());
    encoder->bindSamplerState(0, BindTarget::kFragment, sampler_.get());
    encoder->bindIndexBuffer(*indexBuffer_, IndexFormat::UInt16);
    encoder->drawIndexed(6);

    encoder->endEncoding();

    commandQueue->submit(*commandBuffer);

    // Wait for GPU to finish
    commandBuffer->waitUntilCompleted();

    // Read back and validate (using same method as test)
    IGL_LOG_INFO("PassthroughSession: Reading back offscreen texture...\n");
    uint32_t readbackData[4] = {0};
    auto readbackRange = TextureRangeDesc::new2D(0, 0, 2, 2);

    // Use copyBytesColorAttachment like the test does
    offscreenFramebuffer->copyBytesColorAttachment(*commandQueue, 0, readbackData, readbackRange);

    IGL_LOG_INFO("PassthroughSession: Readback successful!\n");
    IGL_LOG_INFO("  Expected: [0]=0x11223344, [1]=0x11111111, [2]=0x22222222, [3]=0x33333333\n");
    IGL_LOG_INFO("  Actual:   [0]=0x%08x, [1]=0x%08x, [2]=0x%08x, [3]=0x%08x\n",
                 readbackData[0], readbackData[1], readbackData[2], readbackData[3]);

    // Check if data matches
    bool allMatch = (readbackData[0] == kTextureData[0]) &&
                    (readbackData[1] == kTextureData[1]) &&
                    (readbackData[2] == kTextureData[2]) &&
                    (readbackData[3] == kTextureData[3]);

    if (allMatch) {
      IGL_LOG_INFO("PassthroughSession: ✓ VALIDATION PASSED! Texture data matches!\n");
    } else {
      IGL_LOG_ERROR("PassthroughSession: ✗ VALIDATION FAILED! Texture data mismatch!\n");
    }

    IGL_LOG_INFO("=== PassthroughSession: VALIDATION COMPLETE ===\n\n");
    validationDone_ = true;
  }

  // Normal rendering to swapchain for display
  // Create render pass
  RenderPassDesc renderPass;
  renderPass.colorAttachments.resize(1);
  renderPass.colorAttachments[0].loadAction = LoadAction::Clear;
  renderPass.colorAttachments[0].storeAction = StoreAction::Store;
  renderPass.colorAttachments[0].clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

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
  if (!encoder) {
    IGL_LOG_ERROR("Failed to create render encoder\n");
    return;
  }

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
  encoder->bindVertexBuffer(0, *vertexBuffer_, 0);
  encoder->bindVertexBuffer(1, *uvBuffer_, 0);
  encoder->bindTexture(0, BindTarget::kFragment, inputTexture_.get());
  encoder->bindSamplerState(0, BindTarget::kFragment, sampler_.get());
  encoder->bindIndexBuffer(*indexBuffer_, IndexFormat::UInt16);

  // Draw the quad
  encoder->drawIndexed(6);

  encoder->endEncoding();

  commandBuffer->present(surfaceTextures.color);
  commandQueue->submit(*commandBuffer);
}

void PassthroughSession::renderHeadless() {
  IGL_LOG_INFO("\n====== HEADLESS RENDERING PASS ======\n");

  // Create render pass for headless offscreen rendering
  RenderPassDesc headlessRenderPass;
  headlessRenderPass.colorAttachments.resize(1);
  headlessRenderPass.colorAttachments[0].loadAction = LoadAction::Clear;
  headlessRenderPass.colorAttachments[0].storeAction = StoreAction::Store;
  headlessRenderPass.colorAttachments[0].clearColor = {0.0f, 0.0f, 0.0f, 1.0f};  // Black like test

  // Create headless framebuffer
  FramebufferDesc headlessFbDesc;
  headlessFbDesc.colorAttachments[0].texture = headlessOffscreenTexture_;
  auto headlessFramebuffer = headlessDevice_->createFramebuffer(headlessFbDesc, nullptr);

  // Create command buffer and encoder
  CommandBufferDesc cbDesc;
  auto commandBuffer = headlessCommandQueue_->createCommandBuffer(cbDesc, nullptr);
  auto encoder = commandBuffer->createRenderCommandEncoder(headlessRenderPass, headlessFramebuffer, {}, nullptr);

  // NOTE: PIX event markers would go here if we had access to the command list
  // The log messages will help identify this as HEADLESS rendering in PIX
  IGL_LOG_INFO("==> PIX MARKER: HEADLESS_TEXTURE_SAMPLING <==\n");

  // Set viewport for 2x2 offscreen texture
  Viewport headlessViewport;
  headlessViewport.x = 0.0f;
  headlessViewport.y = 0.0f;
  headlessViewport.width = 2.0f;
  headlessViewport.height = 2.0f;
  headlessViewport.minDepth = 0.0f;
  headlessViewport.maxDepth = 1.0f;
  encoder->bindViewport(headlessViewport);

  ScissorRect headlessScissor;
  headlessScissor.x = 0;
  headlessScissor.y = 0;
  headlessScissor.width = 2;
  headlessScissor.height = 2;
  encoder->bindScissorRect(headlessScissor);

  // Bind pipeline and resources
  encoder->bindRenderPipelineState(headlessPipelineState_);
  encoder->bindVertexBuffer(0, *headlessVertexBuffer_, 0);
  encoder->bindVertexBuffer(1, *headlessUvBuffer_, 0);
  encoder->bindTexture(0, BindTarget::kFragment, headlessInputTexture_.get());
  encoder->bindSamplerState(0, BindTarget::kFragment, headlessSampler_.get());
  encoder->bindIndexBuffer(*headlessIndexBuffer_, IndexFormat::UInt16);

  // Draw the quad
  encoder->drawIndexed(6);

  encoder->endEncoding();
  headlessCommandQueue_->submit(*commandBuffer);

  // Read back and validate
  auto readbackRange = TextureRangeDesc::new2D(0, 0, 2, 2);
  uint32_t readbackData[4] = {};
  headlessFramebuffer->copyBytesColorAttachment(*headlessCommandQueue_, 0, readbackData, readbackRange);

  IGL_LOG_INFO("HEADLESS: Reading back offscreen texture...\n");
  IGL_LOG_INFO("  Expected: [0]=0x%08x, [1]=0x%08x, [2]=0x%08x, [3]=0x%08x\n",
               kTextureData[0], kTextureData[1], kTextureData[2], kTextureData[3]);
  IGL_LOG_INFO("  Actual:   [0]=0x%08x, [1]=0x%08x, [2]=0x%08x, [3]=0x%08x\n",
               readbackData[0], readbackData[1], readbackData[2], readbackData[3]);

  bool allMatch = (readbackData[0] == kTextureData[0]) &&
                  (readbackData[1] == kTextureData[1]) &&
                  (readbackData[2] == kTextureData[2]) &&
                  (readbackData[3] == kTextureData[3]);

  if (allMatch) {
    IGL_LOG_INFO("HEADLESS: ✓ VALIDATION PASSED! Texture data matches!\n");
  } else {
    IGL_LOG_ERROR("HEADLESS: ✗ VALIDATION FAILED! Texture data mismatch!\n");
  }

  headlessValidationDone_ = true;
  IGL_LOG_INFO("====== HEADLESS RENDERING COMPLETE ======\n\n");
}

} // namespace igl::shell
