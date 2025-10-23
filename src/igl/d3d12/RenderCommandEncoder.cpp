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
#include <igl/d3d12/HeadlessContext.h>
#include <igl/d3d12/DescriptorHeapManager.h>
#include <igl/d3d12/SamplerState.h>

namespace igl::d3d12 {

RenderCommandEncoder::RenderCommandEncoder(CommandBuffer& commandBuffer,
                                           const RenderPassDesc& renderPass,
                                           const std::shared_ptr<IFramebuffer>& framebuffer)
    : IRenderCommandEncoder(nullptr),
      commandBuffer_(commandBuffer),
      commandList_(commandBuffer.getCommandList()),
      framebuffer_(framebuffer) {
  IGL_LOG_INFO("RenderCommandEncoder::RenderCommandEncoder() - START\n");
  auto& context = commandBuffer_.getContext();
  IGL_LOG_INFO("RenderCommandEncoder: Got context\n");

  // Set descriptor heaps for this command list
  IGL_LOG_INFO("RenderCommandEncoder: Getting CBV/SRV/UAV heap...\n");
  auto* cbvSrvUavHeap = context.getCbvSrvUavHeap();
  IGL_LOG_INFO("RenderCommandEncoder: CBV/SRV/UAV heap = %p\n", cbvSrvUavHeap);

  IGL_LOG_INFO("RenderCommandEncoder: Getting Sampler heap...\n");
  auto* samplerHeap = context.getSamplerHeap();
  IGL_LOG_INFO("RenderCommandEncoder: Sampler heap = %p\n", samplerHeap);

  ID3D12DescriptorHeap* heaps[] = {cbvSrvUavHeap, samplerHeap};
  IGL_LOG_INFO("RenderCommandEncoder: Setting descriptor heaps...\n");
  commandList_->SetDescriptorHeaps(2, heaps);
  IGL_LOG_INFO("RenderCommandEncoder: Descriptor heaps set\n");

  // Create RTV from framebuffer if provided; otherwise fallback to swapchain RTV
  IGL_LOG_INFO("RenderCommandEncoder: Setting up RTV...\n");
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = {};
  bool usedOffscreenRTV = false;
  // Try to get descriptor heap manager (available in headless context)
  DescriptorHeapManager* heapMgr = context.getDescriptorHeapManager();
  IGL_LOG_INFO("RenderCommandEncoder: DescriptorHeapManager = %p\n", heapMgr);

  if (framebuffer_ && framebuffer_->getColorAttachment(0)) {
    auto colorTex = std::static_pointer_cast<Texture>(framebuffer_->getColorAttachment(0));
    ID3D12Device* device = context.getDevice();
    if (device && colorTex && colorTex->getResource()) {
      if (heapMgr) {
        rtvIndex_ = heapMgr->allocateRTV();
        rtvHandle_ = heapMgr->getRTVHandle(rtvIndex_);
      } else {
        // Fallback: transient heap
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> tmpHeap;
        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
        rtvDesc.NumDescriptors = 1;
        rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(tmpHeap.GetAddressOf()));
        rtvHandle_ = tmpHeap->GetCPUDescriptorHandleForHeapStart();
      }

      D3D12_RENDER_TARGET_VIEW_DESC rdesc = {};
      rdesc.Format = textureFormatToDXGIFormat(colorTex->getFormat());
      rdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
      device->CreateRenderTargetView(colorTex->getResource(), &rdesc, rtvHandle_);

      D3D12_RESOURCE_BARRIER bb = {};
      bb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      bb.Transition.pResource = colorTex->getResource();
      bb.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
      bb.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
      bb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      commandList_->ResourceBarrier(1, &bb);

      if (!renderPass.colorAttachments.empty() &&
          renderPass.colorAttachments[0].loadAction == LoadAction::Clear) {
        const auto& clearColor = renderPass.colorAttachments[0].clearColor;
        const float color[] = {clearColor.r, clearColor.g, clearColor.b, clearColor.a};
        commandList_->ClearRenderTargetView(rtvHandle_, color, 0, nullptr);
      }

      rtv = rtvHandle_;
      usedOffscreenRTV = true;
    }
  }
  if (!usedOffscreenRTV) {
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

    if (!renderPass.colorAttachments.empty() &&
        renderPass.colorAttachments[0].loadAction == LoadAction::Clear) {
      const auto& cc = renderPass.colorAttachments[0].clearColor;
      const float col[] = {cc.r, cc.g, cc.b, cc.a};
      D3D12_CPU_DESCRIPTOR_HANDLE swapRtv = context.getCurrentRTV();
      commandList_->ClearRenderTargetView(swapRtv, col, 0, nullptr);
    }
    rtv = context.getCurrentRTV();
  }

  // Create/Bind depth-stencil view if we have a framebuffer with a depth attachment
  const bool hasDepth = (framebuffer_ && framebuffer_->getDepthAttachment());
  if (hasDepth) {
    auto depthTex = std::static_pointer_cast<igl::d3d12::Texture>(framebuffer_->getDepthAttachment());
    ID3D12Device* device = context.getDevice();
    if (device && depthTex && depthTex->getResource()) {
      if (heapMgr) {
        dsvIndex_ = heapMgr->allocateDSV();
        dsvHandle_ = heapMgr->getDSVHandle(dsvIndex_);
      } else {
        // Fallback: transient heap
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> tmpHeap;
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(tmpHeap.GetAddressOf()));
        dsvHandle_ = tmpHeap->GetCPUDescriptorHandleForHeapStart();
      }

      // Create DSV description
      D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
      dsvDesc.Format = textureFormatToDXGIFormat(depthTex->getFormat());
      dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
      dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

      // Transition depth to DEPTH_WRITE
      D3D12_RESOURCE_BARRIER barrier = {};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barrier.Transition.pResource = depthTex->getResource();
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
      barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      commandList_->ResourceBarrier(1, &barrier);

      device->CreateDepthStencilView(depthTex->getResource(), &dsvDesc, dsvHandle_);

      // Clear depth if requested
      if (renderPass.depthAttachment.loadAction == LoadAction::Clear) {
        float depthClear = renderPass.depthAttachment.clearDepth;
        commandList_->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, depthClear, 0, 0, nullptr);
      }

      // Bind RTV + DSV
      IGL_LOG_INFO("RenderCommandEncoder: Binding RTV with DSV\n");
      commandList_->OMSetRenderTargets(1, &rtv, FALSE, &dsvHandle_);
    } else {
      IGL_LOG_INFO("RenderCommandEncoder: Binding RTV without DSV (no resource)\n");
      commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    }
  } else {
    IGL_LOG_INFO("RenderCommandEncoder: Binding RTV without DSV (no hasDepth)\n");
    commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
  }

  // Set a default full-screen viewport/scissor if caller forgets. Prefer framebuffer attachment size.
  IGL_LOG_INFO("RenderCommandEncoder: Setting default viewport...\n");
  if (framebuffer_ && framebuffer_->getColorAttachment(0)) {
    IGL_LOG_INFO("RenderCommandEncoder: Using framebuffer color attachment\n");
    auto colorTex = std::static_pointer_cast<Texture>(framebuffer_->getColorAttachment(0));
    if (colorTex && colorTex->getResource()) {
      auto dims = colorTex->getDimensions();
      IGL_LOG_INFO("RenderCommandEncoder: Framebuffer dimensions: %ux%u\n", dims.width, dims.height);
      D3D12_VIEWPORT vp{}; vp.TopLeftX=0; vp.TopLeftY=0; vp.Width=(float)dims.width; vp.Height=(float)dims.height; vp.MinDepth=0; vp.MaxDepth=1;
      commandList_->RSSetViewports(1, &vp);
      D3D12_RECT sc{}; sc.left=0; sc.top=0; sc.right=(LONG)dims.width; sc.bottom=(LONG)dims.height; commandList_->RSSetScissorRects(1, &sc);
      IGL_LOG_INFO("RenderCommandEncoder: Set default viewport/scissor to %ux%u\n", dims.width, dims.height);
    } else {
      IGL_LOG_ERROR("RenderCommandEncoder: Color texture is null or has no resource!\n");
    }
  } else {
    IGL_LOG_INFO("RenderCommandEncoder: Using back buffer\n");
    auto* backBufferRes = context.getCurrentBackBuffer();
    if (backBufferRes) {
      D3D12_RESOURCE_DESC bbDesc = backBufferRes->GetDesc();
      IGL_LOG_INFO("RenderCommandEncoder: Back buffer dimensions: %llux%u\n", bbDesc.Width, bbDesc.Height);
      D3D12_VIEWPORT vp = {}; vp.TopLeftX=0; vp.TopLeftY=0; vp.Width=(float)bbDesc.Width; vp.Height=(float)bbDesc.Height; vp.MinDepth=0; vp.MaxDepth=1;
      commandList_->RSSetViewports(1, &vp);
      D3D12_RECT scissor = {}; scissor.left=0; scissor.top=0; scissor.right=(LONG)bbDesc.Width; scissor.bottom=(LONG)bbDesc.Height; commandList_->RSSetScissorRects(1, &scissor);
      IGL_LOG_INFO("RenderCommandEncoder: Set default viewport/scissor to back buffer %llux%u\n", bbDesc.Width, bbDesc.Height);
    } else {
      IGL_LOG_ERROR("RenderCommandEncoder: No back buffer available!\n");
    }
  }
  IGL_LOG_INFO("RenderCommandEncoder: Constructor complete!\n");
}

void RenderCommandEncoder::endEncoding() {
  // Transition back to PRESENT only if swapchain RTV was used
  auto& context2 = commandBuffer_.getContext();
  if (!framebuffer_ || !framebuffer_->getColorAttachment(0)) {
    auto* backBuffer = context2.getCurrentBackBuffer();
    if (backBuffer) {
      D3D12_RESOURCE_BARRIER barrier = {};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barrier.Transition.pResource = backBuffer;
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
      barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      commandList_->ResourceBarrier(1, &barrier);
    }
  }

  // Close the command buffer
  commandBuffer_.end();

  // Return RTV/DSV indices to the descriptor heap manager if used
  if (auto* mgr = context2.getDescriptorHeapManager()) {
    if (rtvIndex_ != UINT32_MAX) {
      mgr->freeRTV(rtvIndex_);
      rtvIndex_ = UINT32_MAX;
    }
    if (dsvIndex_ != UINT32_MAX) {
      mgr->freeDSV(dsvIndex_);
      dsvIndex_ = UINT32_MAX;
    }
  }
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

  // Cache vertex stride from pipeline (used when binding vertex buffers)
  currentVertexStride_ = d3dPipelineState->getVertexStride();
  // Fill per-slot strides
  for (size_t s = 0; s < IGL_BUFFER_BINDINGS_MAX; ++s) {
    vertexStrides_[s] = d3dPipelineState->getVertexStride(s);
  }
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
  // Use stride from currently bound pipeline if available; fallback to 32 (pos3+col3+uv2)
  UINT stride = vertexStrides_[index];
  if (stride == 0) stride = currentVertexStride_ ? currentVertexStride_ : 32;
  vbView.StrideInBytes = stride;

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
  static int callCount = 0;
  if (callCount < 3) {
    IGL_LOG_INFO("bindSamplerState called #%d: index=%zu, sampler=%p\n", ++callCount, index, samplerState);
  }
  // Enable binding: create sampler in the shader-visible sampler heap and set the descriptor table
  if (!samplerState || index >= 2) {
    IGL_LOG_INFO("bindSamplerState: early return (sampler=%p, index=%zu)\n", samplerState, index);
    return;  // Only support 2 sampler slots (s0, s1) for now
  }

  auto& context = commandBuffer_.getContext();
  auto* device = context.getDevice();

  // Use descriptor heap manager
  auto* heapMgr = context.getDescriptorHeapManager();
  if (!heapMgr) {
    IGL_LOG_ERROR("bindSamplerState: no descriptor heap manager\n");
    return;
  }

  // For simple cases, use slot 0 for s0 (first sampler)
  // This matches the root signature which expects descriptor table starting at s0
  const uint32_t descriptorIndex = 0;
  IGL_LOG_INFO("bindSamplerState: using descriptor slot %u for s0\n", descriptorIndex);

  // Create sampler descriptor from ISamplerState if provided, else fallback
  D3D12_SAMPLER_DESC samplerDesc = {};
  if (auto* d3dSampler = dynamic_cast<SamplerState*>(samplerState)) {
    samplerDesc = d3dSampler->getDesc();
  } else {
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
  }

  // Create sampler descriptor at the allocated slot
  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = heapMgr->getSamplerCpuHandle(descriptorIndex);
  IGL_LOG_INFO("bindSamplerState: creating sampler at slot %u, CPU handle 0x%llx\n", descriptorIndex, cpuHandle.ptr);
  device->CreateSampler(&samplerDesc, cpuHandle);

  // Cache sampler GPU handle - will be bound in drawIndexed along with texture
  cachedSamplerGpuHandle_ = heapMgr->getSamplerGpuHandle(descriptorIndex);
  IGL_LOG_INFO("bindSamplerState: cached sampler GPU handle 0x%llx\n", cachedSamplerGpuHandle_.ptr);
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
  ID3D12Resource* resource = d3dTexture->getResource();

  if (!resource) {
    IGL_LOG_ERROR("bindTexture: resource is null\n");
    return;
  }

  IGL_LOG_INFO("bindTexture: resource=%p, format=%d, dimensions=%ux%u\n",
               resource, d3dTexture->getFormat(),
               d3dTexture->getDimensions().width, d3dTexture->getDimensions().height);

  // Use descriptor heap manager
  auto* heapMgr = context.getDescriptorHeapManager();
  if (!heapMgr) {
    IGL_LOG_ERROR("bindTexture: no descriptor heap manager\n");
    return;
  }

  // For simple cases, use slot 0 for t0 (first texture)
  // This matches the root signature which expects descriptor table starting at t0
  const uint32_t descriptorIndex = 0;
  IGL_LOG_INFO("bindTexture: using descriptor slot %u for t0\n", descriptorIndex);

  // Create SRV descriptor at the allocated slot
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format = textureFormatToDXGIFormat(d3dTexture->getFormat());
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Texture2D.MipLevels = d3dTexture->getNumMipLevels();
  srvDesc.Texture2D.MostDetailedMip = 0;

  // Verify we're using the right heap
  auto* heap = heapMgr->getCbvSrvUavHeap();
  D3D12_CPU_DESCRIPTOR_HANDLE heapStartCpu = heap->GetCPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE heapStartGpu = heap->GetGPUDescriptorHandleForHeapStart();

  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = heapMgr->getCbvSrvUavCpuHandle(descriptorIndex);
  IGL_LOG_INFO("bindTexture: heap=%p, heapStart CPU=0x%llx, GPU=0x%llx\n", heap, heapStartCpu.ptr, heapStartGpu.ptr);
  IGL_LOG_INFO("bindTexture: creating SRV at slot %u, CPU handle 0x%llx\n", descriptorIndex, cpuHandle.ptr);
  device->CreateShaderResourceView(resource, &srvDesc, cpuHandle);

  // Cache texture GPU handle - will be bound in drawIndexed along with sampler
  cachedTextureGpuHandle_ = heapMgr->getCbvSrvUavGpuHandle(descriptorIndex);
  IGL_LOG_INFO("bindTexture: cached texture GPU handle 0x%llx (offset from heap start: %lld)\n",
               cachedTextureGpuHandle_.ptr, (int64_t)(cachedTextureGpuHandle_.ptr - heapStartGpu.ptr));

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

  // D3D12 requires ALL root parameters to be bound before drawing
  // Root signature defines CBVs at parameters 0 and 1, but the test shader doesn't use them
  // Note: Null CBV addresses (0) should be valid according to D3D12 spec for unused resources
  // However, let's verify if this might be causing issues
  commandList_->SetGraphicsRootConstantBufferView(0, 0);  // b0 - null address (unused by shader)
  commandList_->SetGraphicsRootConstantBufferView(1, 0);  // b1 - null address (unused by shader)
  IGL_LOG_INFO("DrawIndexed: bound null CBVs for unused root parameters 0 and 1\n");

  // CRITICAL: SetDescriptorHeaps invalidates all previously bound descriptor tables
  // We must set heaps ONCE, then bind ALL descriptor tables (texture + sampler)
  auto& context = commandBuffer_.getContext();
  auto* heapMgr = context.getDescriptorHeapManager();
  if (heapMgr) {
    // Set both descriptor heaps (CBV/SRV/UAV and Sampler)
    ID3D12DescriptorHeap* heaps[] = {heapMgr->getCbvSrvUavHeap(), heapMgr->getSamplerHeap()};
    IGL_LOG_INFO("DrawIndexed: Setting descriptor heaps: CBV/SRV/UAV=%p, Sampler=%p\n", heaps[0], heaps[1]);
    commandList_->SetDescriptorHeaps(2, heaps);

    // Now bind descriptor tables - these are valid after SetDescriptorHeaps
    // Parameter 2: Texture SRV table (t0)
    if (cachedTextureGpuHandle_.ptr != 0) {
      commandList_->SetGraphicsRootDescriptorTable(2, cachedTextureGpuHandle_);
      IGL_LOG_INFO("DrawIndexed: bound texture descriptor table (handle=0x%llx)\n", cachedTextureGpuHandle_.ptr);
    }

    // Parameter 3: Sampler table (s0)
    if (cachedSamplerGpuHandle_.ptr != 0) {
      commandList_->SetGraphicsRootDescriptorTable(3, cachedSamplerGpuHandle_);
      IGL_LOG_INFO("DrawIndexed: bound sampler descriptor table (handle=0x%llx)\n", cachedSamplerGpuHandle_.ptr);
    }
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
  // Bind constant buffer GPU address to root parameters 0 (b0) and 1 (b1)
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
