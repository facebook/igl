/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/renderSessions/ComputeSession.h>

#include <igl/CommandBuffer.h>
#include <igl/ComputeCommandEncoder.h>
#include <igl/ComputePipelineState.h>
#include <igl/RenderPass.h>
#include <igl/RenderPipelineState.h>
#include <igl/ShaderCreator.h>

namespace igl::shell {

namespace {

// Simple compute shader that fills a buffer with incremental values
std::string getD3D12ComputeShaderSource() {
  return R"(
    // Output buffer - UAV at u0
    RWByteAddressBuffer outputBuffer : register(u0);

    [numthreads(64, 1, 1)]
    void main(uint3 threadID : SV_DispatchThreadID) {
      uint index = threadID.x;
      // Write thread index as value (creates gradient)
      outputBuffer.Store(index * 4, index);
    }
  )";
}

std::string getVulkanComputeShaderSource() {
  return R"(
    #version 450

    layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

    layout(set = 0, binding = 0) buffer OutputBuffer {
      uint data[];
    } outputBuffer;

    void main() {
      uint index = gl_GlobalInvocationID.x;
      outputBuffer.data[index] = index;
    }
  )";
}

// Vertex shader for visualization - renders a fullscreen quad
std::string getD3D12VertexShaderSource() {
  return R"(
    struct VSOutput {
      float4 position : SV_POSITION;
      float2 texCoord : TEXCOORD0;
    };

    VSOutput main(uint vertexID : SV_VertexID) {
      VSOutput output;
      // Generate fullscreen triangle
      float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
      output.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
      output.texCoord = uv;
      return output;
    }
  )";
}

// Fragment shader for visualization - reads compute buffer and visualizes as gradient
std::string getD3D12FragmentShaderSource() {
  return R"(
    // Read-only buffer containing compute results
    ByteAddressBuffer computeResults : register(t0);

    struct PSInput {
      float4 position : SV_POSITION;
      float2 texCoord : TEXCOORD0;
    };

    float4 main(PSInput input) : SV_TARGET {
      // Sample the compute buffer based on UV coordinates
      // We have 256 values, visualize them as a horizontal gradient
      uint index = uint(input.texCoord.x * 255.0);
      uint value = computeResults.Load(index * 4);

      // Normalize value to 0-1 range (values are 0-255)
      float normalizedValue = float(value) / 255.0;

      // Create a color gradient: blue -> cyan -> green -> yellow -> red
      float3 color;
      if (normalizedValue < 0.25) {
        // Blue to Cyan
        float t = normalizedValue * 4.0;
        color = lerp(float3(0, 0, 1), float3(0, 1, 1), t);
      } else if (normalizedValue < 0.5) {
        // Cyan to Green
        float t = (normalizedValue - 0.25) * 4.0;
        color = lerp(float3(0, 1, 1), float3(0, 1, 0), t);
      } else if (normalizedValue < 0.75) {
        // Green to Yellow
        float t = (normalizedValue - 0.5) * 4.0;
        color = lerp(float3(0, 1, 0), float3(1, 1, 0), t);
      } else {
        // Yellow to Red
        float t = (normalizedValue - 0.75) * 4.0;
        color = lerp(float3(1, 1, 0), float3(1, 0, 0), t);
      }

      return float4(color, 1.0);
    }
  )";
}

} // namespace

void ComputeSession::initialize() noexcept {
  auto& device = getPlatform().getDevice();

  IGL_LOG_INFO("ComputeSession::initialize() START\n");

  // Create compute shader stages
  std::unique_ptr<IShaderStages> shaderStages;

  switch (device.getBackendType()) {
  case BackendType::D3D12:
    shaderStages = ShaderStagesCreator::fromModuleStringInput(
        device,
        getD3D12ComputeShaderSource().c_str(),
        "main",
        "",
        nullptr);
    break;
  case BackendType::Vulkan:
    shaderStages = ShaderStagesCreator::fromModuleStringInput(
        device,
        getVulkanComputeShaderSource().c_str(),
        "main",
        "",
        nullptr);
    break;
  default:
    IGL_LOG_ERROR("ComputeSession: Backend not supported for compute test\n");
    return;
  }

  if (!shaderStages) {
    IGL_LOG_ERROR("ComputeSession: Failed to create compute shader stages\n");
    return;
  }

  // Create compute pipeline
  ComputePipelineDesc pipelineDesc;
  pipelineDesc.shaderStages = std::move(shaderStages);
  pipelineDesc.debugName = "ComputeTestPipeline";

  Result result;
  computePipeline_ = device.createComputePipeline(pipelineDesc, &result);

  if (!result.isOk() || !computePipeline_) {
    IGL_LOG_ERROR("ComputeSession: Failed to create compute pipeline: %s\n", result.message.c_str());
    return;
  }

  IGL_LOG_INFO("ComputeSession: Compute pipeline created successfully\n");

  // Create output buffer (256 elements * 4 bytes = 1KB)
  BufferDesc bufferDesc;
  bufferDesc.type = BufferDesc::BufferTypeBits::Storage;
  bufferDesc.data = nullptr;
  bufferDesc.length = 256 * sizeof(uint32_t);
  bufferDesc.storage = ResourceStorage::Private;

  outputBuffer_ = device.createBuffer(bufferDesc, &result);

  if (!result.isOk() || !outputBuffer_) {
    IGL_LOG_ERROR("ComputeSession: Failed to create output buffer: %s\n", result.message.c_str());
    return;
  }

  IGL_LOG_INFO("ComputeSession: Output buffer created successfully (%zu bytes)\n", bufferDesc.length);

  // Create render pipeline for visualization (D3D12 only for now)
  if (device.getBackendType() == BackendType::D3D12) {
    IGL_LOG_INFO("ComputeSession: Creating visualization render pipeline...\n");

    // Create shader stages from string input
    auto renderShaderStages = ShaderStagesCreator::fromModuleStringInput(
        device,
        getD3D12VertexShaderSource().c_str(),
        "main",
        "",
        getD3D12FragmentShaderSource().c_str(),
        "main",
        "",
        &result);

    if (!result.isOk() || !renderShaderStages) {
      IGL_LOG_ERROR("ComputeSession: Failed to create render shader stages: %s\n", result.message.c_str());
      return;
    }

    // Create render pipeline state
    RenderPipelineDesc renderPipelineDesc;
    renderPipelineDesc.shaderStages = std::move(renderShaderStages);
    renderPipelineDesc.targetDesc.colorAttachments.resize(1);
    renderPipelineDesc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;
    renderPipelineDesc.cullMode = igl::CullMode::Disabled;
    renderPipelineDesc.frontFaceWinding = igl::WindingMode::Clockwise;

    renderPipeline_ = device.createRenderPipeline(renderPipelineDesc, &result);

    if (!result.isOk() || !renderPipeline_) {
      IGL_LOG_ERROR("ComputeSession: Failed to create render pipeline: %s\n", result.message.c_str());
      return;
    }

    IGL_LOG_INFO("ComputeSession: Visualization render pipeline created successfully\n");
  }

  initialized_ = true;
  IGL_LOG_INFO("ComputeSession::initialize() COMPLETE\n");
}

void ComputeSession::update(SurfaceTextures surfaceTextures) noexcept {
  if (!initialized_) {
    IGL_LOG_ERROR("ComputeSession: Not initialized\n");
    return;
  }

  if (surfaceTextures.color == nullptr) {
    return;
  }

  auto& device = getPlatform().getDevice();

  // Create command buffer
  CommandBufferDesc cbDesc;
  auto commandQueue = device.createCommandQueue({}, nullptr);
  auto commandBuffer = commandQueue->createCommandBuffer(cbDesc, nullptr);

  // Create compute encoder
  auto computeEncoder = commandBuffer->createComputeCommandEncoder();

  IGL_LOG_INFO("ComputeSession::update() - binding resources\n");

  // Bind compute pipeline
  computeEncoder->bindComputePipelineState(computePipeline_);

  // Bind output buffer as UAV at index 0 (u0)
  computeEncoder->bindBuffer(0, outputBuffer_.get(), 0, outputBuffer_->getSizeInBytes());

  IGL_LOG_INFO("ComputeSession::update() - dispatching compute\n");

  // Dispatch compute work: 4 thread groups of 64 threads = 256 threads total
  Dimensions threadGroups(4, 1, 1);
  Dimensions threadGroupSize(64, 1, 1);
  computeEncoder->dispatchThreadGroups(threadGroups, threadGroupSize);

  IGL_LOG_INFO("ComputeSession::update() - ending compute encoder\n");

  // End compute encoding
  computeEncoder->endEncoding();

  // Render visualization if render pipeline exists
  if (renderPipeline_) {
    IGL_LOG_INFO("ComputeSession::update() - creating render pass for visualization\n");

    RenderPassDesc renderPass;
    renderPass.colorAttachments.resize(1);
    renderPass.colorAttachments[0].loadAction = LoadAction::Clear;
    renderPass.colorAttachments[0].storeAction = StoreAction::Store;
    renderPass.colorAttachments[0].clearColor = {0.0f, 0.0f, 0.0f, 1.0f}; // Black background

    FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;

    auto framebuffer = device.createFramebuffer(framebufferDesc, nullptr);
    auto renderEncoder = commandBuffer->createRenderCommandEncoder(renderPass, framebuffer, {}, nullptr);

    // Bind render pipeline
    renderEncoder->bindRenderPipelineState(renderPipeline_);

    // Bind compute output buffer as shader resource (t0) for reading
    renderEncoder->bindBuffer(0, outputBuffer_.get(), 0);

    // Draw fullscreen triangle (3 vertices)
    renderEncoder->draw(3);

    renderEncoder->endEncoding();

    IGL_LOG_INFO("ComputeSession::update() - visualization rendered\n");
  } else {
    IGL_LOG_INFO("ComputeSession::update() - creating render pass for clear\n");

    // Fallback: just clear the screen to green
    RenderPassDesc renderPass;
    renderPass.colorAttachments.resize(1);
    renderPass.colorAttachments[0].loadAction = LoadAction::Clear;
    renderPass.colorAttachments[0].storeAction = StoreAction::Store;
    renderPass.colorAttachments[0].clearColor = {0.0f, 0.8f, 0.0f, 1.0f}; // Green = success

    FramebufferDesc framebufferDesc;
    framebufferDesc.colorAttachments[0].texture = surfaceTextures.color;

    auto framebuffer = device.createFramebuffer(framebufferDesc, nullptr);
    auto renderEncoder = commandBuffer->createRenderCommandEncoder(renderPass, framebuffer, {}, nullptr);
    renderEncoder->endEncoding();
  }

  IGL_LOG_INFO("ComputeSession::update() - submitting command buffer\n");

  // Submit command buffer
  commandBuffer->present(surfaceTextures.color);
  commandQueue->submit(*commandBuffer);

  IGL_LOG_INFO("ComputeSession::update() - COMPLETE\n");
}

} // namespace igl::shell
