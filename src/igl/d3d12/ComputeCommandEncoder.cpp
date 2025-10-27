/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/ComputeCommandEncoder.h>
#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/ComputePipelineState.h>
#include <igl/d3d12/Buffer.h>
#include <igl/d3d12/Texture.h>

namespace igl::d3d12 {

ComputeCommandEncoder::ComputeCommandEncoder(CommandBuffer& commandBuffer) :
  commandBuffer_(commandBuffer), isEncoding_(true) {
  IGL_LOG_INFO("ComputeCommandEncoder created\n");
}

void ComputeCommandEncoder::endEncoding() {
  if (!isEncoding_) {
    return;
  }

  IGL_LOG_INFO("ComputeCommandEncoder::endEncoding()\n");
  isEncoding_ = false;
}

void ComputeCommandEncoder::bindComputePipelineState(
    const std::shared_ptr<IComputePipelineState>& pipelineState) {
  if (!pipelineState) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindComputePipelineState - null pipeline state\n");
    return;
  }

  currentPipeline_ = static_cast<const ComputePipelineState*>(pipelineState.get());

  auto* commandList = commandBuffer_.getCommandList();
  if (!commandList) {
    IGL_LOG_ERROR("ComputeCommandEncoder::bindComputePipelineState - null command list\n");
    return;
  }

  // Set compute root signature and pipeline state
  commandList->SetComputeRootSignature(currentPipeline_->getRootSignature());
  commandList->SetPipelineState(currentPipeline_->getPipelineState());

  IGL_LOG_INFO("ComputeCommandEncoder::bindComputePipelineState - PSO and root signature set\n");
}

void ComputeCommandEncoder::dispatchThreadGroups(const Dimensions& threadgroupCount,
                                                 const Dimensions& /*threadgroupSize*/,
                                                 const Dependencies& /*dependencies*/) {
  if (!currentPipeline_) {
    IGL_LOG_ERROR("ComputeCommandEncoder::dispatchThreadGroups - no pipeline state bound\n");
    return;
  }

  auto* commandList = commandBuffer_.getCommandList();
  if (!commandList) {
    IGL_LOG_ERROR("ComputeCommandEncoder::dispatchThreadGroups - null command list\n");
    return;
  }

  IGL_LOG_INFO("ComputeCommandEncoder::dispatchThreadGroups(%u, %u, %u)\n",
               threadgroupCount.width, threadgroupCount.height, threadgroupCount.depth);

  // Dispatch compute work
  // Note: threadgroupSize is embedded in the compute shader ([numthreads(...)])
  commandList->Dispatch(threadgroupCount.width, threadgroupCount.height, threadgroupCount.depth);

  // Insert UAV barrier to ensure compute writes are visible to subsequent operations
  D3D12_RESOURCE_BARRIER uavBarrier = {};
  uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  uavBarrier.UAV.pResource = nullptr;  // Global UAV barrier
  commandList->ResourceBarrier(1, &uavBarrier);

  IGL_LOG_INFO("ComputeCommandEncoder::dispatchThreadGroups - dispatch complete, UAV barrier inserted\n");
}

void ComputeCommandEncoder::bindPushConstants(const void* /*data*/,
                                              size_t /*length*/,
                                              size_t /*offset*/) {
  // Push constants not yet implemented for D3D12
  // Requires proper root signature design and backward compatibility considerations
  IGL_LOG_INFO("ComputeCommandEncoder::bindPushConstants - not yet implemented\n");
}

void ComputeCommandEncoder::bindTexture(uint32_t index, ITexture* texture) {
  // Bind SRV texture
  // In a full implementation, this would:
  // 1. Get the SRV descriptor handle for the texture
  // 2. Set the descriptor table via SetComputeRootDescriptorTable
  // For now, log a stub message
  IGL_LOG_INFO("ComputeCommandEncoder::bindTexture(%u, %p) - stub\n", index, texture);
  (void)index;
  (void)texture;
}

void ComputeCommandEncoder::bindBuffer(uint32_t index, IBuffer* buffer, size_t offset, size_t /*bufferSize*/) {
  // Bind buffer (UAV or SRV or CBV depending on usage)
  // In a full implementation, this would:
  // 1. Determine if it's a UAV, SRV, or CBV based on buffer type
  // 2. Get the appropriate descriptor handle
  // 3. Set the descriptor table or root descriptor
  // For now, log a stub message
  IGL_LOG_INFO("ComputeCommandEncoder::bindBuffer(%u, %p, %zu) - stub\n", index, buffer, offset);
  (void)index;
  (void)buffer;
  (void)offset;
}

void ComputeCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {
  // Single uniform binding not supported in D3D12
  // Use uniform buffers (CBVs) instead
  IGL_LOG_INFO("ComputeCommandEncoder::bindUniform - not supported, use uniform buffers\n");
}

void ComputeCommandEncoder::bindBytes(uint32_t /*index*/, const void* /*data*/, size_t /*length*/) {
  // bindBytes not yet implemented
  // Would need to create a temporary upload buffer and copy data
  IGL_LOG_INFO("ComputeCommandEncoder::bindBytes - not yet implemented\n");
}

void ComputeCommandEncoder::bindImageTexture(uint32_t index, ITexture* texture, TextureFormat /*format*/) {
  // Bind UAV texture for read/write access
  // In a full implementation, this would:
  // 1. Get the UAV descriptor handle for the texture
  // 2. Set the descriptor table via SetComputeRootDescriptorTable
  // For now, log a stub message
  IGL_LOG_INFO("ComputeCommandEncoder::bindImageTexture(%u, %p) - stub\n", index, texture);
  (void)index;
  (void)texture;
}

void ComputeCommandEncoder::bindSamplerState(uint32_t index, ISamplerState* /*samplerState*/) {
  // Bind sampler
  // In a full implementation, this would:
  // 1. Get the sampler descriptor handle
  // 2. Set the sampler descriptor table
  // For now, log a stub message
  IGL_LOG_INFO("ComputeCommandEncoder::bindSamplerState(%u) - stub\n", index);
  (void)index;
}

void ComputeCommandEncoder::pushDebugGroupLabel(const char* label, const Color& /*color*/) const {
  auto* commandList = commandBuffer_.getCommandList();
  if (commandList) {
    // PIX debug markers
    const size_t len = strlen(label);
    std::wstring wlabel(len, L' ');
    std::mbstowcs(&wlabel[0], label, len);
    commandList->BeginEvent(0, wlabel.c_str(), static_cast<UINT>((wlabel.length() + 1) * sizeof(wchar_t)));
  }
}

void ComputeCommandEncoder::insertDebugEventLabel(const char* label, const Color& /*color*/) const {
  auto* commandList = commandBuffer_.getCommandList();
  if (commandList) {
    const size_t len = strlen(label);
    std::wstring wlabel(len, L' ');
    std::mbstowcs(&wlabel[0], label, len);
    commandList->SetMarker(0, wlabel.c_str(), static_cast<UINT>((wlabel.length() + 1) * sizeof(wchar_t)));
  }
}

void ComputeCommandEncoder::popDebugGroupLabel() const {
  auto* commandList = commandBuffer_.getCommandList();
  if (commandList) {
    commandList->EndEvent();
  }
}

} // namespace igl::d3d12
