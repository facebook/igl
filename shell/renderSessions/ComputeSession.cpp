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
      // Write thread index as value
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

  IGL_LOG_INFO("ComputeSession::update() - creating render pass for clear\n");

  // Clear the screen to indicate success (green color)
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

  IGL_LOG_INFO("ComputeSession::update() - submitting command buffer\n");

  // Submit command buffer
  commandBuffer->present(surfaceTextures.color);
  commandQueue->submit(*commandBuffer);

  IGL_LOG_INFO("ComputeSession::update() - COMPLETE\n");
}

} // namespace igl::shell
