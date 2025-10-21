/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/RenderCommandEncoder.h>
#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/Buffer.h>
#include <igl/d3d12/RenderPipelineState.h>
#include <igl/d3d12/Texture.h>
#include <igl/RenderPass.h>

namespace igl::d3d12 {

RenderCommandEncoder::RenderCommandEncoder(CommandBuffer& commandBuffer,
                                           const RenderPassDesc& renderPass)
    : IRenderCommandEncoder(nullptr),
      commandBuffer_(commandBuffer),
      commandList_(commandBuffer.getCommandList()) {
  auto& context = commandBuffer_.getContext();

  // Set descriptor heaps for this command list
  ID3D12DescriptorHeap* heaps[] = {
    context.getCbvSrvUavHeap(),
    context.getSamplerHeap()
  };
  commandList_->SetDescriptorHeaps(2, heaps);

  // Transition render target to RENDER_TARGET state
  auto* backBuffer = context.getCurrentBackBuffer();
  if (!backBuffer) {
    IGL_LOG_ERROR("RenderCommandEncoder: No back buffer available\n");
    return;
  }

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = backBuffer;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

  commandList_->ResourceBarrier(1, &barrier);

  // Clear render target if requested
  if (!renderPass.colorAttachments.empty() &&
      renderPass.colorAttachments[0].loadAction == LoadAction::Clear) {
    const auto& clearColor = renderPass.colorAttachments[0].clearColor;
    const float color[] = {clearColor.r, clearColor.g, clearColor.b, clearColor.a};

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = context.getCurrentRTV();
    commandList_->ClearRenderTargetView(rtv, color, 0, nullptr);
  }

  // Set render target
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = context.getCurrentRTV();
  commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
}

void RenderCommandEncoder::endEncoding() {
  // Transition render target back to PRESENT state
  auto& context = commandBuffer_.getContext();
  auto* backBuffer = context.getCurrentBackBuffer();

  if (!backBuffer) {
    IGL_LOG_ERROR("RenderCommandEncoder::endEncoding: No back buffer available\n");
    return;
  }

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = backBuffer;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

  commandList_->ResourceBarrier(1, &barrier);

  // Close the command buffer
  commandBuffer_.end();
}

void RenderCommandEncoder::bindViewport(const Viewport& viewport) {
  static int callCount = 0;
  if (callCount < 3) {
    IGL_LOG_INFO("bindViewport called #%d: x=%.1f, y=%.1f, w=%.1f, h=%.1f\n",
                 ++callCount, viewport.x, viewport.y, viewport.width, viewport.height);
  }
  D3D12_VIEWPORT vp = {};
  vp.TopLeftX = viewport.x;
  vp.TopLeftY = viewport.y;
  vp.Width = viewport.width;
  vp.Height = viewport.height;
  vp.MinDepth = viewport.minDepth;
  vp.MaxDepth = viewport.maxDepth;
  commandList_->RSSetViewports(1, &vp);
}

void RenderCommandEncoder::bindScissorRect(const ScissorRect& rect) {
  D3D12_RECT scissor = {};
  scissor.left = static_cast<LONG>(rect.x);
  scissor.top = static_cast<LONG>(rect.y);
  scissor.right = static_cast<LONG>(rect.x + rect.width);
  scissor.bottom = static_cast<LONG>(rect.y + rect.height);
  commandList_->RSSetScissorRects(1, &scissor);
}

void RenderCommandEncoder::bindRenderPipelineState(
    const std::shared_ptr<IRenderPipelineState>& pipelineState) {
  if (!pipelineState) {
    IGL_LOG_ERROR("bindRenderPipelineState: pipelineState is null!\n");
    return;
  }

  auto* d3dPipelineState = static_cast<const RenderPipelineState*>(pipelineState.get());

  // Validate pointers before calling D3D12
  auto* pso = d3dPipelineState->getPipelineState();
  auto* rootSig = d3dPipelineState->getRootSignature();

  if (!pso) {
    IGL_LOG_ERROR("bindRenderPipelineState: PSO is null!\n");
    return;
  }
  if (!rootSig) {
    IGL_LOG_ERROR("bindRenderPipelineState: Root signature is null!\n");
    return;
  }

  IGL_LOG_INFO("bindRenderPipelineState: PSO=%p, RootSig=%p\n", pso, rootSig);

  commandList_->SetPipelineState(pso);
  commandList_->SetGraphicsRootSignature(rootSig);
  commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void RenderCommandEncoder::bindDepthStencilState(
    const std::shared_ptr<IDepthStencilState>& /*depthStencilState*/) {}


void RenderCommandEncoder::bindVertexBuffer(uint32_t index,
                                            IBuffer& buffer,
                                            size_t bufferOffset) {
  static int callCount = 0;
  if (callCount < 3) {
    IGL_LOG_INFO("bindVertexBuffer called #%d: index=%u\n", ++callCount, index);
  }
  auto* d3dBuffer = static_cast<Buffer*>(&buffer);

  D3D12_VERTEX_BUFFER_VIEW vbView = {};
  vbView.BufferLocation = d3dBuffer->gpuAddress(bufferOffset);
  vbView.SizeInBytes = static_cast<UINT>(d3dBuffer->getSizeInBytes() - bufferOffset);
  // TODO: Get stride from vertex input state
  // For now, hardcode for simple triangle (float3 position + float4 color = 28 bytes)
  vbView.StrideInBytes = 28;

  commandList_->IASetVertexBuffers(index, 1, &vbView);
}

void RenderCommandEncoder::bindIndexBuffer(IBuffer& buffer,
                                           IndexFormat format,
                                           size_t bufferOffset) {
  static int callCount = 0;
  if (callCount < 3) {
    IGL_LOG_INFO("bindIndexBuffer called #%d\n", ++callCount);
  }
  auto* d3dBuffer = static_cast<Buffer*>(&buffer);

  D3D12_INDEX_BUFFER_VIEW ibView = {};
  ibView.BufferLocation = d3dBuffer->gpuAddress(bufferOffset);
  ibView.SizeInBytes = static_cast<UINT>(d3dBuffer->getSizeInBytes() - bufferOffset);
  ibView.Format = (format == IndexFormat::UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

  commandList_->IASetIndexBuffer(&ibView);
}

void RenderCommandEncoder::bindBytes(size_t /*index*/,
                                     uint8_t /*target*/,
                                     const void* /*data*/,
                                     size_t /*length*/) {}
void RenderCommandEncoder::bindPushConstants(const void* /*data*/,
                                             size_t /*length*/,
                                             size_t /*offset*/) {}
void RenderCommandEncoder::bindSamplerState(size_t index,
                                            uint8_t /*target*/,
                                            ISamplerState* samplerState) {
  if (!samplerState || index >= 2) {
    return;  // Only support 2 sampler slots (s0, s1) for now
  }

  auto& context = commandBuffer_.getContext();
  auto* device = context.getDevice();

  // Create sampler descriptor
  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getSamplerHeap()->GetCPUDescriptorHandleForHeapStart();
  UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

  // Allocate a descriptor for this sampler
  UINT descriptorIndex = nextSamplerDescriptor_ + static_cast<UINT>(index);
  cpuHandle.ptr += descriptorIndex * descriptorSize;

  // Create sampler  descriptor
  D3D12_SAMPLER_DESC samplerDesc = {};
  samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  samplerDesc.MipLODBias = 0.0f;
  samplerDesc.MaxAnisotropy = 1;
  samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  samplerDesc.BorderColor[0] = 0.0f;
  samplerDesc.BorderColor[1] = 0.0f;
  samplerDesc.BorderColor[2] = 0.0f;
  samplerDesc.BorderColor[3] = 0.0f;
  samplerDesc.MinLOD = 0.0f;
  samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

  device->CreateSampler(&samplerDesc, cpuHandle);

  // Bind the sampler descriptor table (root parameter 3)
  D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getSamplerHeap()->GetGPUDescriptorHandleForHeapStart();
  gpuHandle.ptr += nextSamplerDescriptor_ * descriptorSize;
  commandList_->SetGraphicsRootDescriptorTable(3, gpuHandle);
}
void RenderCommandEncoder::bindTexture(size_t index,
                                       uint8_t /*target*/,
                                       ITexture* texture) {
  // Delegate to single-argument version
  bindTexture(index, texture);
}

void RenderCommandEncoder::bindTexture(size_t index, ITexture* texture) {
  static int callCount = 0;
  if (callCount < 5) {
    IGL_LOG_INFO("bindTexture called #%d: index=%zu, texture=%p\n", ++callCount, index, texture);
  }
  if (!texture || index >= 2) {
    IGL_LOG_INFO("bindTexture: early return (texture=%p, index=%zu)\n", texture, index);
    return;  // Only support 2 texture slots (t0, t1) for now
  }

  IGL_LOG_INFO("bindTexture: getting context...\n");
  auto& context = commandBuffer_.getContext();
  auto* device = context.getDevice();
  auto* d3dTexture = static_cast<Texture*>(texture);

  // Get or create SRV for this texture
  // For simplicity, we create SRVs on the fly at the next available slot in the heap
  // A real implementation would cache these

  IGL_LOG_INFO("bindTexture: getting heap handles...\n");
  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavHeap()->GetCPUDescriptorHandleForHeapStart();
  UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  // Allocate a descriptor for this texture (simple bump allocator)
  UINT descriptorIndex = nextCbvSrvUavDescriptor_ + static_cast<UINT>(index);
  cpuHandle.ptr += descriptorIndex * descriptorSize;

  IGL_LOG_INFO("bindTexture: creating SRV desc...\n");
  // Create SRV
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = textureFormatToDXGIFormat(d3dTexture->getFormat());
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Texture2D.MipLevels = d3dTexture->getNumMipLevels();
  srvDesc.Texture2D.MostDetailedMip = 0;

  IGL_LOG_INFO("bindTexture: getting resource...\n");
  ID3D12Resource* resource = d3dTexture->getResource();
  if (resource) {
    IGL_LOG_INFO("bindTexture: creating SRV...\n");
    device->CreateShaderResourceView(resource, &srvDesc, cpuHandle);

    IGL_LOG_INFO("bindTexture: setting descriptor table...\n");
    // Bind the descriptor table (root parameter 2) to the start of our SRV range
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavHeap()->GetGPUDescriptorHandleForHeapStart();
    gpuHandle.ptr += nextCbvSrvUavDescriptor_ * descriptorSize;
    commandList_->SetGraphicsRootDescriptorTable(2, gpuHandle);
  }
  IGL_LOG_INFO("bindTexture: done\n");
}
void RenderCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {}

void RenderCommandEncoder::draw(size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t baseInstance) {
  commandList_->DrawInstanced(static_cast<UINT>(vertexCount),
                              instanceCount,
                              firstVertex,
                              baseInstance);
}

void RenderCommandEncoder::drawIndexed(size_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t vertexOffset,
                                       uint32_t baseInstance) {
  static int drawCallCount = 0;
  if (drawCallCount < 3) {
    IGL_LOG_INFO("DrawIndexed called: indexCount=%zu, instanceCount=%u\n", indexCount, instanceCount);
    drawCallCount++;
  }
  commandList_->DrawIndexedInstanced(static_cast<UINT>(indexCount),
                                     instanceCount,
                                     firstIndex,
                                     vertexOffset,
                                     baseInstance);
}
void RenderCommandEncoder::multiDrawIndirect(IBuffer& /*indirectBuffer*/,
                                             size_t /*indirectBufferOffset*/,
                                             uint32_t /*drawCount*/,
                                             uint32_t /*stride*/) {}
void RenderCommandEncoder::multiDrawIndexedIndirect(IBuffer& /*indirectBuffer*/,
                                                    size_t /*indirectBufferOffset*/,
                                                    uint32_t /*drawCount*/,
                                                    uint32_t /*stride*/) {}

void RenderCommandEncoder::setStencilReferenceValue(uint32_t /*value*/) {}
void RenderCommandEncoder::setBlendColor(const Color& /*color*/) {}
void RenderCommandEncoder::setDepthBias(float /*depthBias*/, float /*slopeScale*/, float /*clamp*/) {}

void RenderCommandEncoder::pushDebugGroupLabel(const char* /*label*/, const Color& /*color*/) const {}
void RenderCommandEncoder::insertDebugEventLabel(const char* /*label*/, const Color& /*color*/) const {}
void RenderCommandEncoder::popDebugGroupLabel() const {}

void RenderCommandEncoder::bindBuffer(uint32_t index,
                                       IBuffer* buffer,
                                       size_t offset,
                                       size_t /*bufferSize*/) {
  static int callCount = 0;
  if (callCount < 5) {
    IGL_LOG_INFO("bindBuffer START #%d: index=%u\n", callCount + 1, index);
  }
  if (!buffer) {
    IGL_LOG_INFO("bindBuffer: null buffer, returning\n");
    return;
  }

  IGL_LOG_INFO("bindBuffer: getting d3dBuffer...\n");
  auto* d3dBuffer = static_cast<Buffer*>(buffer);

  // D3D12 requires constant buffer addresses to be 256-byte aligned
  // Round offset UP to nearest 256 bytes
  const size_t alignedOffset = (offset + 255) & ~255;
  if (offset != alignedOffset) {
    IGL_LOG_INFO("bindBuffer: WARNING - offset %zu not 256-byte aligned, using %zu instead\n",
                 offset, alignedOffset);
  }

  IGL_LOG_INFO("bindBuffer: getting gpuAddress(offset=%zu)...\n", alignedOffset);
  D3D12_GPU_VIRTUAL_ADDRESS bufferAddress = d3dBuffer->gpuAddress(alignedOffset);
  IGL_LOG_INFO("bindBuffer: got address=0x%llx, aligned=%d\n", bufferAddress, (bufferAddress & 0xFF) == 0);

  // Bind to root parameter based on index
  // Root parameter 0 = b0 (UniformsPerFrame)
  // Root parameter 1 = b1 (UniformsPerObject)
  IGL_LOG_INFO("bindBuffer: commandList_ pointer = %p\n", commandList_);
  if (!commandList_) {
    IGL_LOG_ERROR("bindBuffer: commandList_ is NULL!\n");
    return;
  }

  if (index == 0) {
    IGL_LOG_INFO("bindBuffer: About to call SetGraphicsRootConstantBufferView(0, 0x%llx)...\n", bufferAddress);
    IGL_LOG_INFO("bindBuffer: Calling NOW...\n");
    fflush(stdout); // Force flush to see if this line prints before hang
    commandList_->SetGraphicsRootConstantBufferView(0, bufferAddress);
    IGL_LOG_INFO("bindBuffer: SetGraphicsRootConstantBufferView(0) COMPLETED\n");
  } else if (index == 1) {
    IGL_LOG_INFO("bindBuffer: About to call SetGraphicsRootConstantBufferView(1, 0x%llx)...\n", bufferAddress);
    commandList_->SetGraphicsRootConstantBufferView(1, bufferAddress);
    IGL_LOG_INFO("bindBuffer: SetGraphicsRootConstantBufferView(1) COMPLETED\n");
  }
  if (callCount < 5) {
    IGL_LOG_INFO("bindBuffer END #%d\n", ++callCount);
  }
}
void RenderCommandEncoder::bindBindGroup(BindGroupTextureHandle /*handle*/) {}
void RenderCommandEncoder::bindBindGroup(BindGroupBufferHandle /*handle*/,
                                          uint32_t /*numDynamicOffsets*/,
                                          const uint32_t* /*dynamicOffsets*/) {}

} // namespace igl::d3d12
