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
  auto& context = commandBuffer_.getContext();

  // Set descriptor heaps for this command list
  ID3D12DescriptorHeap* heaps[] = {
    context.getCbvSrvUavHeap(),
    context.getSamplerHeap()
  };
  commandList_->SetDescriptorHeaps(2, heaps);

  // Create RTV from framebuffer if provided; otherwise fallback to swapchain RTV
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = {};
  bool usedOffscreenRTV = false;
  // Try to get descriptor heap manager (available in headless context)
  DescriptorHeapManager* heapMgr = context.getDescriptorHeapManager();

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
      commandList_->OMSetRenderTargets(1, &rtv, FALSE, &dsvHandle_);
    } else {
      commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    }
  } else {
    commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
  }

  // Set a default full-screen viewport/scissor if caller forgets. Prefer framebuffer attachment size.
  if (framebuffer_ && framebuffer_->getColorAttachment(0)) {
    auto colorTex = std::static_pointer_cast<Texture>(framebuffer_->getColorAttachment(0));
    if (colorTex) {
      auto dims = colorTex->getDimensions();
      D3D12_VIEWPORT vp{}; vp.TopLeftX=0; vp.TopLeftY=0; vp.Width=(float)dims.width; vp.Height=(float)dims.height; vp.MinDepth=0; vp.MaxDepth=1;
      commandList_->RSSetViewports(1, &vp);
      D3D12_RECT sc{}; sc.left=0; sc.top=0; sc.right=(LONG)dims.width; sc.bottom=(LONG)dims.height; commandList_->RSSetScissorRects(1, &sc);
    }
  } else {
    auto* backBufferRes = context.getCurrentBackBuffer();
    if (backBufferRes) {
      D3D12_RESOURCE_DESC bbDesc = backBufferRes->GetDesc();
      D3D12_VIEWPORT vp = {}; vp.TopLeftX=0; vp.TopLeftY=0; vp.Width=(float)bbDesc.Width; vp.Height=(float)bbDesc.Height; vp.MinDepth=0; vp.MaxDepth=1;
      commandList_->RSSetViewports(1, &vp);
      D3D12_RECT scissor = {}; scissor.left=0; scissor.top=0; scissor.right=(LONG)bbDesc.Width; scissor.bottom=(LONG)bbDesc.Height; commandList_->RSSetScissorRects(1, &scissor);
    }
  }
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
