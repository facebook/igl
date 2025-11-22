/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/RenderCommandEncoder.h>
#include <cstdlib>
#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/Buffer.h>
#include <igl/d3d12/RenderPipelineState.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/Texture.h>
#include <igl/RenderPass.h>
#include <igl/d3d12/HeadlessContext.h>
#include <igl/d3d12/DescriptorHeapManager.h>
#include <igl/d3d12/SamplerState.h>

namespace igl::d3d12 {

RenderCommandEncoder::RenderCommandEncoder(CommandBuffer& commandBuffer,
                                           const std::shared_ptr<IFramebuffer>& framebuffer)
    : IRenderCommandEncoder(nullptr),
      commandBuffer_(commandBuffer),
      commandList_(commandBuffer.getCommandList()),
      resourcesBinder_(commandBuffer, false /* isCompute */),
      framebuffer_(framebuffer) {
  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder::RenderCommandEncoder() - Lightweight initialization\n");
}

void RenderCommandEncoder::begin(const RenderPassDesc& renderPass) {
  // Enforce single-call semantics: begin() allocates descriptors and cannot be safely called twice.
  IGL_DEBUG_ASSERT(!hasBegun_, "begin() called multiple times - this will cause resource leaks");
  hasBegun_ = true;

  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder::begin() - START\n");
  auto& context = commandBuffer_.getContext();
  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Got context\n");

  // Set descriptor heaps for this command list.
  // Must use per-frame heaps from D3D12Context, not DescriptorHeapManager.
  // Per-frame heaps are isolated per frame to prevent descriptor conflicts.
  DescriptorHeapManager* heapMgr = context.getDescriptorHeapManager();

  // Use active heap from frame context, not the legacy accessor.
  // This ensures we bind the currently active page, not hardcoded page 0.
  auto& frameCtx = context.getFrameContexts()[context.getCurrentFrameIndex()];
  cbvSrvUavHeap_ = frameCtx.activeCbvSrvUavHeap.Get();
  samplerHeap_ = frameCtx.samplerHeap.Get();

  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Using active per-frame heap from FrameContext\n");

  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: CBV/SRV/UAV heap (active) = %p\n", cbvSrvUavHeap_);
  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Sampler heap = %p\n", samplerHeap_);

  // Bind active heap (may be page 0 or a later page).
  ID3D12DescriptorHeap* heaps[] = {cbvSrvUavHeap_, samplerHeap_};
  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Setting descriptor heaps...\n");
  commandList_->SetDescriptorHeaps(2, heaps);
  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Descriptor heaps set\n");

  // Create RTV from framebuffer if provided; otherwise fallback to swapchain RTV
  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Setting up RTV...\n");
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = {};
  std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
  rtvIndices_.clear();
  bool usedOffscreenRTV = false;
  // Note: heapMgr already retrieved above for setting descriptor heaps
  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: DescriptorHeapManager = %p\n", heapMgr);

  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Checking framebuffer_=%p\n", framebuffer_.get());
  // Only create offscreen RTV if we have DescriptorHeapManager AND it's not a swapchain texture
  // Swapchain textures should use context.getCurrentRTV() directly
  if (framebuffer_ && framebuffer_->getColorAttachment(0) && heapMgr) {
    IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Has framebuffer with color attachment AND DescriptorHeapManager\n");
    ID3D12Device* device = context.getDevice();
    if (device) {
      // Create RTVs for each color attachment
      const size_t count = std::min<size_t>(framebuffer_->getColorAttachmentIndices().size(), D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
      IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: MRT count = %zu (indices.size=%zu)\n", count, framebuffer_->getColorAttachmentIndices().size());
      for (size_t i = 0; i < count; ++i) {
        auto tex = std::static_pointer_cast<Texture>(framebuffer_->getColorAttachment(i));
        IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: MRT loop i=%zu, tex=%p, resource=%p\n", i, tex.get(), tex ? tex->getResource() : nullptr);
        if (!tex || !tex->getResource()) {
          IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: MRT loop i=%zu SKIPPED (null tex or resource)\n", i);
          continue;
        }
        const bool hasAttachmentDesc = (i < renderPass.colorAttachments.size());
        // CRITICAL: Extract values before using in expressions to avoid MSVC debug iterator checks
        const uint32_t mipLevel = hasAttachmentDesc ? renderPass.colorAttachments[i].mipLevel : 0;
        const uint32_t attachmentLayer = hasAttachmentDesc ? renderPass.colorAttachments[i].layer : 0;
        const uint32_t attachmentFace = hasAttachmentDesc ? renderPass.colorAttachments[i].face : 0;
        // Allocate RTV
        uint32_t rtvIdx = heapMgr->allocateRTV();
        if (rtvIdx == UINT32_MAX) {
          IGL_LOG_ERROR("RenderCommandEncoder: Failed to allocate RTV descriptor (heap exhausted)\n");
          continue;
        }
  // Check return value from getHandle.
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
        if (!heapMgr->getRTVHandle(rtvIdx, &rtvHandle)) {
          IGL_LOG_ERROR("RenderCommandEncoder: Failed to get RTV handle for index %u\n", rtvIdx);
          heapMgr->freeRTV(rtvIdx);
          continue;
        }
        rtvIndices_.push_back(rtvIdx);
        // Create RTV view - use the resource's actual format to avoid SRGB/UNORM mismatches
        D3D12_RESOURCE_DESC resourceDesc = tex->getResource()->GetDesc();
        D3D12_RENDER_TARGET_VIEW_DESC rdesc = {};
        rdesc.Format = resourceDesc.Format;  // Use actual D3D12 resource format, not IGL format

        // Determine if this is a texture array or texture view.
        // Cube textures are stored as 2D array resources (6 slices per cube).
        const bool isView = tex->isView();
        const bool isCubeTexture = (tex->getType() == TextureType::Cube);
        const uint32_t arraySliceOffset = isView ? tex->getArraySliceOffset() : 0;
        const uint32_t totalArraySlices =
            isView ? tex->getNumArraySlicesInView() : resourceDesc.DepthOrArraySize;
        const bool isArrayTexture = !isCubeTexture &&
                                     ((isView && tex->getNumArraySlicesInView() > 0) ||
                                      (!isView && resourceDesc.DepthOrArraySize > 1));
        uint32_t targetArraySlice = attachmentLayer;
        if (isCubeTexture) {
          // Cube textures map faces onto 2D array slices. See Texture Subresources (D3D12).
          const uint32_t clampedFace = std::min<uint32_t>(attachmentFace, 5u);
          const uint32_t cubesInView = (totalArraySlices + 5u) / 6u;
          const uint32_t clampedCubeIndex =
              std::min<uint32_t>(attachmentLayer, (cubesInView == 0u) ? 0u : (cubesInView - 1u));
          const uint32_t baseSlice = arraySliceOffset + clampedCubeIndex * 6u;
          const uint32_t maxSlice =
              (totalArraySlices > 0u) ? (arraySliceOffset + totalArraySlices - 1u)
                                      : arraySliceOffset;
          targetArraySlice = std::min<uint32_t>(baseSlice + clampedFace, maxSlice);
        }

        // Set view dimension based on sample count (MSAA support) and array type
        if (resourceDesc.SampleDesc.Count > 1) {
          // MSAA texture
          if (isCubeTexture) {
            rdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
            rdesc.Texture2DMSArray.FirstArraySlice = targetArraySlice;
            rdesc.Texture2DMSArray.ArraySize = 1;
            IGL_D3D12_LOG_VERBOSE(
                "RenderCommandEncoder: Creating MSAA cube RTV with %u samples, face %u, cube index %u (array slice %u)\n",
                resourceDesc.SampleDesc.Count,
                attachmentFace,
                attachmentLayer,
                rdesc.Texture2DMSArray.FirstArraySlice);
          } else if (isArrayTexture) {
            // MSAA texture array - use TEXTURE2DMSARRAY view dimension
            rdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
            if (isView) {
              rdesc.Texture2DMSArray.FirstArraySlice = tex->getArraySliceOffset();
              rdesc.Texture2DMSArray.ArraySize = tex->getNumArraySlicesInView();
            } else {
              rdesc.Texture2DMSArray.FirstArraySlice = attachmentLayer;
              rdesc.Texture2DMSArray.ArraySize = 1;  // Render to single layer
            }
            IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Creating MSAA array RTV with %u samples, layer %u\n",
                         resourceDesc.SampleDesc.Count, rdesc.Texture2DMSArray.FirstArraySlice);
          } else {
            // MSAA non-array texture - use TEXTURE2DMS view dimension
            rdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Creating MSAA RTV with %u samples\n", resourceDesc.SampleDesc.Count);
          }
        } else {
          // Non-MSAA texture
          if (isCubeTexture) {
            rdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rdesc.Texture2DArray.MipSlice = mipLevel;
            rdesc.Texture2DArray.PlaneSlice = 0;
            rdesc.Texture2DArray.FirstArraySlice = targetArraySlice;
            rdesc.Texture2DArray.ArraySize = 1;
            IGL_D3D12_LOG_VERBOSE(
                "RenderCommandEncoder: Creating cube RTV, mip %u, face %u, cube index %u (array slice %u)\n",
                mipLevel,
                attachmentFace,
                attachmentLayer,
                rdesc.Texture2DArray.FirstArraySlice);
          } else if (isArrayTexture) {
            // Texture array - use TEXTURE2DARRAY view dimension
            rdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            // CRITICAL: Extract value before assignment to avoid MSVC debug iterator bounds check
            const uint32_t mipSliceArray = (i < renderPass.colorAttachments.size()) ? renderPass.colorAttachments[i].mipLevel : 0;
            rdesc.Texture2DArray.MipSlice = mipSliceArray;
            rdesc.Texture2DArray.PlaneSlice = 0;
            if (isView) {
              rdesc.Texture2DArray.FirstArraySlice = tex->getArraySliceOffset();
              rdesc.Texture2DArray.ArraySize = tex->getNumArraySlicesInView();
            } else {
              rdesc.Texture2DArray.FirstArraySlice = attachmentLayer;
              rdesc.Texture2DArray.ArraySize = 1;  // Render to single layer
            }
            IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Creating array RTV, mip %u, layer %u\n",
                         rdesc.Texture2DArray.MipSlice, rdesc.Texture2DArray.FirstArraySlice);
          } else {
            // Non-array texture - use standard TEXTURE2D view dimension
            rdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            // CRITICAL: Extract value before assignment to avoid MSVC debug iterator bounds check
            const uint32_t mipSlice2D = (i < renderPass.colorAttachments.size()) ? renderPass.colorAttachments[i].mipLevel : 0;
            rdesc.Texture2D.MipSlice = mipSlice2D;
            rdesc.Texture2D.PlaneSlice = 0;
            IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Creating RTV, mip %u\n", rdesc.Texture2D.MipSlice);
          }
        }
        // Pre-creation validation.
        IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateRenderTargetView");
        IGL_DEBUG_ASSERT(tex->getResource() != nullptr, "Texture resource is null before CreateRenderTargetView");
        IGL_DEBUG_ASSERT(rtvHandle.ptr != 0, "RTV descriptor handle is invalid");

        device->CreateRenderTargetView(tex->getResource(), &rdesc, rtvHandle);

        // Transition to RENDER_TARGET
        // IMPORTANT: For multi-frame rendering, offscreen targets may have been transitioned to
        // PIXEL_SHADER_RESOURCE in the previous frame's endEncoding(). We MUST transition them
        // back to RENDER_TARGET at the start of each render pass.
        // The transitionTo() function checks current state and only transitions if needed.
        const uint32_t transitionSlice =
            isCubeTexture ? targetArraySlice : attachmentLayer;
        tex->transitionTo(
            commandList_, D3D12_RESOURCE_STATE_RENDER_TARGET, mipLevel, transitionSlice);

        // Clear if requested
        if (hasAttachmentDesc && renderPass.colorAttachments[i].loadAction == LoadAction::Clear) {
          const auto& clearColor = renderPass.colorAttachments[i].clearColor;
          const float color[] = {clearColor.r, clearColor.g, clearColor.b, clearColor.a};
          IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Clearing MRT attachment %zu with color (%.2f, %.2f, %.2f, %.2f)\n",
                       i, color[0], color[1], color[2], color[3]);
          commandList_->ClearRenderTargetView(rtvHandle, color, 0, nullptr);
        } else {
          // CRITICAL: Must extract value completely outside ternary to avoid MSVC debug iterator check
          int loadActionDbg = -1;
          if (i < renderPass.colorAttachments.size()) {
            loadActionDbg = (int)renderPass.colorAttachments[i].loadAction;
          }
          IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: NOT clearing MRT attachment %zu (loadAction=%d, hasAttachment=%d)\n",
                       i, loadActionDbg, i < renderPass.colorAttachments.size());
        }
        rtvs.push_back(rtvHandle);
        IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: MRT Created RTV #%zu, total RTVs now=%zu\n", i, rtvs.size());
      }
      IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: MRT Total RTVs created: %zu\n", rtvs.size());
      if (!rtvs.empty()) {
        rtv = rtvs[0];
        usedOffscreenRTV = true;
      }
    }
  }
  if (!usedOffscreenRTV) {
    IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Using swapchain back buffer\n");
    auto* backBuffer = context.getCurrentBackBuffer();
    IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Got back buffer=%p\n", backBuffer);
    if (!backBuffer) {
      IGL_LOG_ERROR("RenderCommandEncoder: No back buffer available\n");
      return;
    }
    IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Transitioning back buffer to RENDER_TARGET\n");
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);
    IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Resource barrier executed\n");

    if (!renderPass.colorAttachments.empty() &&
        renderPass.colorAttachments[0].loadAction == LoadAction::Clear) {
      IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Clearing render target\n");
      const auto& cc = renderPass.colorAttachments[0].clearColor;
      const float col[] = {cc.r, cc.g, cc.b, cc.a};
      D3D12_CPU_DESCRIPTOR_HANDLE swapRtv = context.getCurrentRTV();
      commandList_->ClearRenderTargetView(swapRtv, col, 0, nullptr);
      IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Clear complete\n");
    }
    rtv = context.getCurrentRTV();
    IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Got RTV handle\n");
  }

  // Create/Bind depth-stencil view if we have a framebuffer with a depth attachment
  const bool hasDepth = (framebuffer_ && framebuffer_->getDepthAttachment());
  if (hasDepth) {
    auto depthTex = std::static_pointer_cast<igl::d3d12::Texture>(framebuffer_->getDepthAttachment());
    ID3D12Device* device = context.getDevice();
    if (device && depthTex && depthTex->getResource()) {
      if (heapMgr) {
        dsvIndex_ = heapMgr->allocateDSV();
  // Check return value from getHandle.
        if (!heapMgr->getDSVHandle(dsvIndex_, &dsvHandle_)) {
          IGL_LOG_ERROR("RenderCommandEncoder: Failed to get DSV handle for index %u\n", dsvIndex_);
          heapMgr->freeDSV(dsvIndex_);
          dsvIndex_ = UINT32_MAX;
          return;
        }
      } else {
        // Fallback: transient heap
        igl::d3d12::ComPtr<ID3D12DescriptorHeap> tmpHeap;
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
      dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

      // Set view dimension based on sample count (MSAA support)
      D3D12_RESOURCE_DESC depthResourceDesc = depthTex->getResource()->GetDesc();
      if (depthResourceDesc.SampleDesc.Count > 1) {
        // MSAA depth texture - use TEXTURE2DMS view dimension
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
        IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Creating MSAA DSV with %u samples\n", depthResourceDesc.SampleDesc.Count);
      } else {
        // Non-MSAA depth texture - use standard TEXTURE2D view dimension
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = renderPass.depthAttachment.mipLevel;
      }

      const uint32_t depthMip = renderPass.depthAttachment.mipLevel;
      const uint32_t depthLayer = renderPass.depthAttachment.layer;
      depthTex->transitionTo(commandList_, D3D12_RESOURCE_STATE_DEPTH_WRITE, depthMip, depthLayer);

      // Pre-creation validation.
      IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateDepthStencilView");
      IGL_DEBUG_ASSERT(depthTex->getResource() != nullptr, "Depth texture resource is null");
      IGL_DEBUG_ASSERT(dsvHandle_.ptr != 0, "DSV descriptor handle is invalid");

      device->CreateDepthStencilView(depthTex->getResource(), &dsvDesc, dsvHandle_);

      // Clear depth and/or stencil if requested
      const bool clearDepth = (renderPass.depthAttachment.loadAction == LoadAction::Clear);
      const bool clearStencil = (renderPass.stencilAttachment.loadAction == LoadAction::Clear);

      if (clearDepth || clearStencil) {
        D3D12_CLEAR_FLAGS clearFlags = static_cast<D3D12_CLEAR_FLAGS>(0);
        if (clearDepth) {
          clearFlags = static_cast<D3D12_CLEAR_FLAGS>(clearFlags | D3D12_CLEAR_FLAG_DEPTH);
        }
        if (clearStencil) {
          clearFlags = static_cast<D3D12_CLEAR_FLAGS>(clearFlags | D3D12_CLEAR_FLAG_STENCIL);
        }

        const float depthClearValue = renderPass.depthAttachment.clearDepth;
        const UINT8 stencilClearValue = static_cast<UINT8>(renderPass.stencilAttachment.clearStencil);

        commandList_->ClearDepthStencilView(dsvHandle_, clearFlags, depthClearValue, stencilClearValue, 0, nullptr);

        IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Cleared depth-stencil (depth=%d, stencil=%d, depthVal=%.2f, stencilVal=%u)\n",
                     clearDepth, clearStencil, depthClearValue, stencilClearValue);
      }

      // Bind RTV + DSV (or DSV-only for depth-only rendering)
      if (!rtvs.empty()) {
        // Multi-render target or offscreen rendering with color+depth
        IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: OMSetRenderTargets with %zu RTVs + DSV\n", rtvs.size());
        commandList_->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), FALSE, &dsvHandle_);
      } else if (usedOffscreenRTV) {
        // Single offscreen render target with depth
        IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: OMSetRenderTargets with 1 RTV + DSV\n");
        commandList_->OMSetRenderTargets(1, &rtv, FALSE, &dsvHandle_);
      } else if (!framebuffer_->getColorAttachment(0)) {
        // Depth-only rendering (no color attachments) - shadow mapping scenario
        IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Depth-only rendering - OMSetRenderTargets with 0 RTVs + DSV\n");
        commandList_->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle_);
      } else {
        // Swapchain backbuffer with depth
        IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: OMSetRenderTargets with swapchain RTV + DSV\n");
        commandList_->OMSetRenderTargets(1, &rtv, FALSE, &dsvHandle_);
      }
    } else {
      IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Binding RTV without DSV (no resource)\n");
      if (!rtvs.empty()) {
        IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: OMSetRenderTargets with %zu RTVs, no DSV\n", rtvs.size());
        commandList_->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), FALSE, nullptr);
      } else {
        IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: OMSetRenderTargets with 1 RTV, no DSV\n");
        commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
      }
    }
  } else {
    IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Binding RTV without DSV (no hasDepth)\n");
    if (!rtvs.empty()) {
      IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: OMSetRenderTargets with %zu RTVs, no DSV (no hasDepth)\n", rtvs.size());
      commandList_->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), FALSE, nullptr);
    } else {
      IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: OMSetRenderTargets with 1 RTV, no DSV (no hasDepth)\n");
      commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    }
  }

  // Set a default full-screen viewport/scissor if caller forgets. Prefer framebuffer attachment size.
  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Setting default viewport...\n");
  if (framebuffer_ && framebuffer_->getColorAttachment(0)) {
    IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Using framebuffer color attachment\n");
    auto colorTex = std::static_pointer_cast<Texture>(framebuffer_->getColorAttachment(0));
    if (colorTex && colorTex->getResource()) {
      auto dims = colorTex->getDimensions();
      IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Framebuffer dimensions: %ux%u\n", dims.width, dims.height);
      D3D12_VIEWPORT vp{}; vp.TopLeftX=0; vp.TopLeftY=0; vp.Width=(float)dims.width; vp.Height=(float)dims.height; vp.MinDepth=0; vp.MaxDepth=1;
      commandList_->RSSetViewports(1, &vp);
      D3D12_RECT sc{}; sc.left=0; sc.top=0; sc.right=(LONG)dims.width; sc.bottom=(LONG)dims.height; commandList_->RSSetScissorRects(1, &sc);
      IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Set default viewport/scissor to %ux%u\n", dims.width, dims.height);
    } else {
      IGL_LOG_ERROR("RenderCommandEncoder: Color texture is null or has no resource!\n");
    }
  } else {
    IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Using back buffer\n");
    auto* backBufferRes = context.getCurrentBackBuffer();
    if (backBufferRes) {
      D3D12_RESOURCE_DESC bbDesc = backBufferRes->GetDesc();
      IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Back buffer dimensions: %llux%u\n", bbDesc.Width, bbDesc.Height);
      D3D12_VIEWPORT vp = {}; vp.TopLeftX=0; vp.TopLeftY=0; vp.Width=(float)bbDesc.Width; vp.Height=(float)bbDesc.Height; vp.MinDepth=0; vp.MaxDepth=1;
      commandList_->RSSetViewports(1, &vp);
      D3D12_RECT scissor = {}; scissor.left=0; scissor.top=0; scissor.right=(LONG)bbDesc.Width; scissor.bottom=(LONG)bbDesc.Height; commandList_->RSSetScissorRects(1, &scissor);
      IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Set default viewport/scissor to back buffer %llux%u\n", bbDesc.Width, bbDesc.Height);
    } else {
      IGL_LOG_ERROR("RenderCommandEncoder: No back buffer available!\n");
    }
  }
  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder::begin() - Complete!\n");
}

void RenderCommandEncoder::endEncoding() {
  auto& context2 = commandBuffer_.getContext();

  // ========== MSAA RESOLVE OPERATION ==========
  // Resolve MSAA textures to non-MSAA textures before transitioning resources
  // This must happen AFTER rendering but BEFORE the final state transitions
  if (framebuffer_) {
    // Resolve color attachments
    const auto indices = framebuffer_->getColorAttachmentIndices();
    for (size_t i : indices) {
      auto msaaAttachment = std::static_pointer_cast<Texture>(framebuffer_->getColorAttachment(i));
      auto resolveAttachment = std::static_pointer_cast<Texture>(framebuffer_->getResolveColorAttachment(i));

      // Check if both MSAA source and resolve target exist
      if (msaaAttachment && resolveAttachment &&
          msaaAttachment->getResource() && resolveAttachment->getResource()) {

        // Verify MSAA source has samples > 1 and resolve target has samples == 1
        D3D12_RESOURCE_DESC msaaDesc = msaaAttachment->getResource()->GetDesc();
        D3D12_RESOURCE_DESC resolveDesc = resolveAttachment->getResource()->GetDesc();

        if (msaaDesc.SampleDesc.Count > 1 && resolveDesc.SampleDesc.Count == 1) {
          IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder::endEncoding - Resolving MSAA color attachment %zu (%u samples -> 1 sample)\n",
                       i, msaaDesc.SampleDesc.Count);

          // Transition MSAA texture to RESOLVE_SOURCE state
          msaaAttachment->transitionAll(commandList_, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);

          // Transition resolve texture to RESOLVE_DEST state
          resolveAttachment->transitionAll(commandList_, D3D12_RESOURCE_STATE_RESOLVE_DEST);

          // Perform resolve operation: converts multi-sample texture to single-sample
          // This averages all samples in the MSAA texture and writes to the resolve texture
          commandList_->ResolveSubresource(
              resolveAttachment->getResource(),  // pDstResource (non-MSAA)
              0,                                  // DstSubresource (mip 0, layer 0)
              msaaAttachment->getResource(),      // pSrcResource (MSAA)
              0,                                  // SrcSubresource (mip 0, layer 0)
              msaaDesc.Format                     // Format (must be compatible)
          );

          IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder::endEncoding - MSAA color resolve completed for attachment %zu\n", i);

          // Transition resolve texture to PIXEL_SHADER_RESOURCE for subsequent use
          resolveAttachment->transitionAll(commandList_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
      }
    }

    // Resolve depth attachment if present
    auto msaaDepth = std::static_pointer_cast<Texture>(framebuffer_->getDepthAttachment());
    auto resolveDepth = std::static_pointer_cast<Texture>(framebuffer_->getResolveDepthAttachment());

    if (msaaDepth && resolveDepth &&
        msaaDepth->getResource() && resolveDepth->getResource()) {

      D3D12_RESOURCE_DESC msaaDesc = msaaDepth->getResource()->GetDesc();
      D3D12_RESOURCE_DESC resolveDesc = resolveDepth->getResource()->GetDesc();

      if (msaaDesc.SampleDesc.Count > 1 && resolveDesc.SampleDesc.Count == 1) {
        IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder::endEncoding - Resolving MSAA depth attachment (%u samples -> 1 sample)\n",
                     msaaDesc.SampleDesc.Count);

        // Transition depth textures to appropriate resolve states
        msaaDepth->transitionAll(commandList_, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        resolveDepth->transitionAll(commandList_, D3D12_RESOURCE_STATE_RESOLVE_DEST);

        // Resolve depth buffer
        commandList_->ResolveSubresource(
            resolveDepth->getResource(),
            0,
            msaaDepth->getResource(),
            0,
            msaaDesc.Format
        );

        IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder::endEncoding - MSAA depth resolve completed\n");

        // Transition resolved depth to shader resource for sampling
        resolveDepth->transitionAll(commandList_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      }
    }
  }
  // ========== END MSAA RESOLVE OPERATION ==========

  // For offscreen framebuffers (MRT targets), transition all attachments to PIXEL_SHADER_RESOURCE
  // so they can be sampled in subsequent passes
  if (framebuffer_ && framebuffer_->getColorAttachment(0)) {
    auto swapColor = std::static_pointer_cast<Texture>(framebuffer_->getColorAttachment(0));

    // Check if this is the swapchain backbuffer
    const bool isSwapchainTarget = (swapColor && swapColor->getResource() == context2.getCurrentBackBuffer());

    if (isSwapchainTarget) {
      // Swapchain framebuffer: transition to PRESENT
      swapColor->transitionAll(commandList_, D3D12_RESOURCE_STATE_PRESENT);
    } else {
      // Offscreen framebuffer (e.g., MRT targets): transition all color attachments to PIXEL_SHADER_RESOURCE
      // This allows the render targets to be sampled in subsequent rendering passes (multi-frame support)
      const auto indices = framebuffer_->getColorAttachmentIndices();
      for (size_t i : indices) {
        auto attachment = std::static_pointer_cast<Texture>(framebuffer_->getColorAttachment(i));
        if (attachment && attachment->getResource()) {
          attachment->transitionAll(commandList_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
      }
    }
  } else {
    // No framebuffer provided - using swapchain directly
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

  // G-001: Flush any remaining barriers before ending encoding
  flushBarriers();

  // Return RTV/DSV indices to the descriptor heap manager if used
  if (auto* mgr = context2.getDescriptorHeapManager()) {
    if (!rtvIndices_.empty()) {
      for (auto idx : rtvIndices_) {
        mgr->freeRTV(idx);
      }
      rtvIndices_.clear();
    }
    if (dsvIndex_ != UINT32_MAX) {
      mgr->freeDSV(dsvIndex_);
      dsvIndex_ = UINT32_MAX;
    }
  }
}

void RenderCommandEncoder::bindViewport(const Viewport& viewport) {
  IGL_D3D12_LOG_VERBOSE("bindViewport called: x=%.1f, y=%.1f, w=%.1f, h=%.1f\n",
               viewport.x, viewport.y, viewport.width, viewport.height);
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

  IGL_D3D12_LOG_VERBOSE("bindRenderPipelineState: PSO=%p, RootSig=%p\n", pso, rootSig);

  commandList_->SetPipelineState(pso);
  commandList_->SetGraphicsRootSignature(rootSig);

  // Set primitive topology from the pipeline state
  D3D_PRIMITIVE_TOPOLOGY topology = d3dPipelineState->getPrimitiveTopology();
  IGL_D3D12_LOG_VERBOSE("bindRenderPipelineState: Setting topology=%d\n", (int)topology);
  commandList_->IASetPrimitiveTopology(topology);

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
  IGL_D3D12_LOG_VERBOSE("bindVertexBuffer called: index=%u\n", index);
  if (index >= IGL_BUFFER_BINDINGS_MAX) {
    IGL_LOG_ERROR("bindVertexBuffer: index %u exceeds max %u\n", index, IGL_BUFFER_BINDINGS_MAX);
    return;
  }

  auto* d3dBuffer = static_cast<Buffer*>(&buffer);
  cachedVertexBuffers_[index].bufferLocation = d3dBuffer->gpuAddress(bufferOffset);
  cachedVertexBuffers_[index].sizeInBytes = static_cast<UINT>(d3dBuffer->getSizeInBytes() - bufferOffset);
  cachedVertexBuffers_[index].bound = true;
}

void RenderCommandEncoder::bindIndexBuffer(IBuffer& buffer,
                                           IndexFormat format,
                                           size_t bufferOffset) {
  IGL_D3D12_LOG_VERBOSE("bindIndexBuffer called\n");
  auto* d3dBuffer = static_cast<Buffer*>(&buffer);
  cachedIndexBuffer_.bufferLocation = d3dBuffer->gpuAddress(bufferOffset);
  cachedIndexBuffer_.sizeInBytes = static_cast<UINT>(d3dBuffer->getSizeInBytes() - bufferOffset);
  // D3D12 only supports 16-bit and 32-bit index formats (not 8-bit)
  cachedIndexBuffer_.format = (format == IndexFormat::UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
  cachedIndexBuffer_.bound = true;
}

void RenderCommandEncoder::bindBytes(size_t /*index*/,
                                     uint8_t /*target*/,
                                     const void* /*data*/,
                                     size_t /*length*/) {
  // D3D12 backend does not support bindBytes
  // Applications should use uniform buffers (bindBuffer) instead
  // This is a no-op to maintain compatibility with cross-platform code
  IGL_DEBUG_ASSERT_NOT_IMPLEMENTED();
  IGL_LOG_INFO_ONCE("bindBytes is not supported in D3D12 backend. Use bindBuffer with uniform buffers instead.\n");
}
void RenderCommandEncoder::bindPushConstants(const void* data,
                                             size_t length,
                                             size_t offset) {
  if (!commandList_ || !data || length == 0) {
    IGL_LOG_ERROR("bindPushConstants: Invalid parameters (list=%p, data=%p, len=%zu)\n",
                  commandList_, data, length);
    return;
  }

  // Root signature parameter 0 is declared as D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS (b2)
  // with 16 DWORDs (64 bytes) capacity
  constexpr size_t kMaxPushConstantBytes = 64;

  if (length + offset > kMaxPushConstantBytes) {
    IGL_LOG_ERROR("bindPushConstants: size %zu + offset %zu exceeds maximum %zu bytes\n",
                  length, offset, kMaxPushConstantBytes);
    return;
  }

  // Calculate number of 32-bit values and offset in DWORDs
  const uint32_t num32BitValues = static_cast<uint32_t>((length + 3) / 4);  // Round up to DWORDs
  const uint32_t destOffsetIn32BitValues = static_cast<uint32_t>(offset / 4);

  // Use SetGraphicsRoot32BitConstants to directly write data to root constants
  // Root parameter 0 = b2 (Push Constants), as declared in graphics root signature
  commandList_->SetGraphicsRoot32BitConstants(
      0,                          // Root parameter index (push constants at parameter 0)
      num32BitValues,             // Number of 32-bit values to set
      data,                       // Source data
      destOffsetIn32BitValues);   // Destination offset in 32-bit values

  IGL_D3D12_LOG_VERBOSE("bindPushConstants: Set %u DWORDs (%zu bytes) at offset %zu to root parameter 0 (b2)\n",
               num32BitValues, length, offset);
}
void RenderCommandEncoder::bindSamplerState(size_t index,
                                            uint8_t /*target*/,
                                            ISamplerState* samplerState) {
  // Delegate to D3D12ResourcesBinder for centralized descriptor management.
  resourcesBinder_.bindSamplerState(static_cast<uint32_t>(index), samplerState);

  // Clear bindBindGroup cache to switch from bindBindGroup path to bindSamplerState path
  // This ensures draw() will call resourcesBinder_.updateBindings() instead of using cached handles
  cachedTextureCount_ = 0;
  cachedSamplerCount_ = 0;
  usedBindGroup_ = false;
}
void RenderCommandEncoder::bindTexture(size_t index,
                                       uint8_t /*target*/,
                                       ITexture* texture) {
  // Delegate to single-argument version
  bindTexture(index, texture);
}

void RenderCommandEncoder::bindTexture(size_t index, ITexture* texture) {
  // Delegate to D3D12ResourcesBinder for centralized descriptor management.
  resourcesBinder_.bindTexture(static_cast<uint32_t>(index), texture);

  // Clear bindBindGroup cache to switch from bindBindGroup path to bindTexture path
  // This ensures draw() will call resourcesBinder_.updateBindings() instead of using cached handles
  cachedTextureCount_ = 0;
  cachedSamplerCount_ = 0;
  usedBindGroup_ = false;
}
void RenderCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {}

void RenderCommandEncoder::draw(size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t baseInstance) {
  // G-001: Flush any pending barriers before draw call
  flushBarriers();

  // Apply all resource bindings (textures, samplers, buffers) before draw.
  // Skip ResourcesBinder if bindBindGroup was explicitly used
  // Note: cachedTextureCount_/cachedSamplerCount_ may also be set by storage buffer SRV path,
  // so we use dedicated usedBindGroup_ flag to distinguish bindBindGroup from other paths
  if (!usedBindGroup_) {
    Result bindResult;
    if (!resourcesBinder_.updateBindings(&bindResult)) {
      IGL_LOG_ERROR("draw: Failed to update resource bindings: %s\n", bindResult.message.c_str());
      return;
    }
  }

  // D3D12 requires ALL root parameters to be bound before drawing
  // Hybrid root signature layout:
  // - Root parameter 0: Push constants at b2
  // - Root parameter 1: Root CBV for b0 (legacy bindBuffer)
  // - Root parameter 2: Root CBV for b1 (legacy bindBuffer)
  // - Root parameter 3: CBV descriptor table for b3-b15 (bindBindGroup)
  // - Root parameter 4: SRV descriptor table for t0-tN
  // - Root parameter 5: Sampler descriptor table for s0-sN

  // Bind root CBVs for b0 and b1 (legacy bindBuffer support)
  if (constantBufferBound_[0]) {
    commandList_->SetGraphicsRootConstantBufferView(1, cachedConstantBuffers_[0]);  // Root param 1: b0
    IGL_D3D12_LOG_VERBOSE("draw: binding root CBV for b0 (address 0x%llx)\n", cachedConstantBuffers_[0]);
  }
  if (constantBufferBound_[1]) {
    commandList_->SetGraphicsRootConstantBufferView(2, cachedConstantBuffers_[1]);  // Root param 2: b1
    IGL_D3D12_LOG_VERBOSE("draw: binding root CBV for b1 (address 0x%llx)\n", cachedConstantBuffers_[1]);
  }

  // Bind CBV descriptor table for b3-b15 (bindBindGroup support)
  if (cbvTableCount_ > 0 && cachedCbvTableGpuHandles_[0].ptr != 0) {
    commandList_->SetGraphicsRootDescriptorTable(3, cachedCbvTableGpuHandles_[0]);  // Root param 3: CBV table (b3-b15)
    IGL_D3D12_LOG_VERBOSE("draw: binding CBV descriptor table with %zu descriptors (GPU handle 0x%llx)\n",
                 cbvTableCount_, cachedCbvTableGpuHandles_[0].ptr);
  }

  // Bind SRV and sampler descriptor tables (bindBindGroup support)
  // Add debug validation to catch sparse binding (non-zero count with zero base handle).
  // Note: Storage buffer SRVs bound via bindBindGroup(buffer) may have already set root parameter 4.
  // This will rebind it with texture SRVs. The last binding wins for each draw call.
  if (cachedTextureCount_ > 0) {
    IGL_DEBUG_ASSERT(cachedTextureGpuHandles_[0].ptr != 0,
                     "Texture count > 0 but base handle is null - did you bind only higher slots?");
    if (cachedTextureGpuHandles_[0].ptr != 0) {
      commandList_->SetGraphicsRootDescriptorTable(4, cachedTextureGpuHandles_[0]);  // Root param 4: SRV table
      IGL_D3D12_LOG_VERBOSE("draw: binding SRV descriptor table with %zu descriptors (GPU handle 0x%llx)\n",
                   cachedTextureCount_, cachedTextureGpuHandles_[0].ptr);
    }
  }
  if (cachedSamplerCount_ > 0) {
    IGL_DEBUG_ASSERT(cachedSamplerGpuHandles_[0].ptr != 0,
                     "Sampler count > 0 but base handle is null - did you bind only higher slots?");
    if (cachedSamplerGpuHandles_[0].ptr != 0) {
      commandList_->SetGraphicsRootDescriptorTable(5, cachedSamplerGpuHandles_[0]);  // Root param 5: Sampler table
      IGL_D3D12_LOG_VERBOSE("draw: binding Sampler descriptor table with %zu descriptors (GPU handle 0x%llx)\n",
                   cachedSamplerCount_, cachedSamplerGpuHandles_[0].ptr);
    }
  }

  // Apply vertex buffers
  for (uint32_t i = 0; i < IGL_BUFFER_BINDINGS_MAX; ++i) {
    if (cachedVertexBuffers_[i].bound) {
      UINT stride = vertexStrides_[i];
      if (stride == 0) {
        // Assert in debug builds to catch misconfigured pipelines.
        // Check both per-slot stride and fallback currentVertexStride_
        IGL_DEBUG_ASSERT(vertexStrides_[i] != 0 || currentVertexStride_ != 0,
                         "Vertex buffer bound but no stride from pipeline for this slot - check vertex input layout");
        if (vertexStrides_[i] == 0 && currentVertexStride_ == 0) {
          IGL_LOG_INFO_ONCE("Vertex buffer bound to slot %u but no stride from pipeline - using fallback stride of 32 bytes\n", i);
        }
        stride = currentVertexStride_ ? currentVertexStride_ : 32;
      }
      D3D12_VERTEX_BUFFER_VIEW vbView = {};
      vbView.BufferLocation = cachedVertexBuffers_[i].bufferLocation;
      vbView.SizeInBytes = cachedVertexBuffers_[i].sizeInBytes;
      vbView.StrideInBytes = stride;
      IGL_D3D12_LOG_VERBOSE("draw: VB[%u] = GPU 0x%llx, size=%u, stride=%u\n", i, vbView.BufferLocation, vbView.SizeInBytes, vbView.StrideInBytes);
      commandList_->IASetVertexBuffers(i, 1, &vbView);
    }
  }

  commandBuffer_.incrementDrawCount();

  IGL_D3D12_LOG_VERBOSE("draw: DrawInstanced(vertexCount=%zu, instanceCount=%u, firstVertex=%u, baseInstance=%u)\n", vertexCount, instanceCount, firstVertex, baseInstance);
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
  // G-001: Flush any pending barriers before draw call
  flushBarriers();

  // Apply all resource bindings (textures, samplers, buffers) before draw.
  // Skip ResourcesBinder if bindBindGroup was explicitly used
  // Note: cachedTextureCount_/cachedSamplerCount_ may also be set by storage buffer SRV path,
  // so we use dedicated usedBindGroup_ flag to distinguish bindBindGroup from other paths
  if (!usedBindGroup_) {
    Result bindResult;
    if (!resourcesBinder_.updateBindings(&bindResult)) {
      IGL_LOG_ERROR("drawIndexed: Failed to update resource bindings: %s\n", bindResult.message.c_str());
      return;
    }
  }

  // D3D12 requires ALL root parameters to be bound before drawing
  // Hybrid root signature layout:
  // - Root parameter 0: Push constants at b2
  // - Root parameter 1: Root CBV for b0 (legacy bindBuffer)
  // - Root parameter 2: Root CBV for b1 (legacy bindBuffer)
  // - Root parameter 3: CBV descriptor table for b3-b15 (bindBindGroup)
  // - Root parameter 4: SRV descriptor table for t0-tN
  // - Root parameter 5: Sampler descriptor table for s0-sN

  // Bind root CBVs for b0 and b1 (legacy bindBuffer support)
  if (constantBufferBound_[0]) {
    commandList_->SetGraphicsRootConstantBufferView(1, cachedConstantBuffers_[0]);  // Root param 1: b0
  }
  if (constantBufferBound_[1]) {
    commandList_->SetGraphicsRootConstantBufferView(2, cachedConstantBuffers_[1]);  // Root param 2: b1
  }

  // Bind CBV descriptor table for b3-b15 (bindBindGroup support)
  if (cbvTableCount_ > 0 && cachedCbvTableGpuHandles_[0].ptr != 0) {
    commandList_->SetGraphicsRootDescriptorTable(3, cachedCbvTableGpuHandles_[0]);  // Root param 3: CBV table (b3-b15)
  }

  // Bind SRV and sampler descriptor tables (bindBindGroup support)
  // Add debug validation to catch sparse binding (non-zero count with zero base handle).
  // Note: Storage buffer SRVs bound via bindBindGroup(buffer) may have already set root parameter 4.
  // This will rebind it with texture SRVs. The last binding wins for each draw call.
  if (cachedTextureCount_ > 0) {
    IGL_DEBUG_ASSERT(cachedTextureGpuHandles_[0].ptr != 0,
                     "Texture count > 0 but base handle is null - did you bind only higher slots?");
    if (cachedTextureGpuHandles_[0].ptr != 0) {
      commandList_->SetGraphicsRootDescriptorTable(4, cachedTextureGpuHandles_[0]);  // Root param 4: SRV table
    }
  }
  if (cachedSamplerCount_ > 0) {
    IGL_DEBUG_ASSERT(cachedSamplerGpuHandles_[0].ptr != 0,
                     "Sampler count > 0 but base handle is null - did you bind only higher slots?");
    if (cachedSamplerGpuHandles_[0].ptr != 0) {
      commandList_->SetGraphicsRootDescriptorTable(5, cachedSamplerGpuHandles_[0]);  // Root param 5: Sampler table
    }
  }

  // Apply cached vertex buffer bindings now that pipeline state is bound
  for (uint32_t i = 0; i < IGL_BUFFER_BINDINGS_MAX; ++i) {
    if (cachedVertexBuffers_[i].bound) {
      UINT stride = vertexStrides_[i];
      if (stride == 0) {
        // Assert in debug builds to catch misconfigured pipelines.
        // Check both per-slot stride and fallback currentVertexStride_
        IGL_DEBUG_ASSERT(vertexStrides_[i] != 0 || currentVertexStride_ != 0,
                         "Vertex buffer bound but no stride from pipeline for this slot - check vertex input layout");
        if (vertexStrides_[i] == 0 && currentVertexStride_ == 0) {
          IGL_LOG_INFO_ONCE("Vertex buffer bound to slot %u but no stride from pipeline - using fallback stride of 32 bytes\n", i);
        }
        stride = currentVertexStride_ ? currentVertexStride_ : 32;
      }
      D3D12_VERTEX_BUFFER_VIEW vbView = {};
      vbView.BufferLocation = cachedVertexBuffers_[i].bufferLocation;
      vbView.SizeInBytes = cachedVertexBuffers_[i].sizeInBytes;
      vbView.StrideInBytes = stride;
      commandList_->IASetVertexBuffers(i, 1, &vbView);
    }
  }

    // Apply cached index buffer binding
    if (cachedIndexBuffer_.bound) {
      D3D12_INDEX_BUFFER_VIEW ibView = {};
      ibView.BufferLocation = cachedIndexBuffer_.bufferLocation;
      ibView.SizeInBytes = cachedIndexBuffer_.sizeInBytes;
      ibView.Format = cachedIndexBuffer_.format;
      commandList_->IASetIndexBuffer(&ibView);
    }

    // Track per-command-buffer draw count; CommandQueue aggregates into device on submit
    commandBuffer_.incrementDrawCount();

  commandList_->DrawIndexedInstanced(static_cast<UINT>(indexCount),
                                     instanceCount,
                                     firstIndex,
                                     vertexOffset,
                                     baseInstance);

#if IGL_DEBUG
  static const bool kLogDrawErrors = []() {
    const char* env = std::getenv("IGL_D3D12_LOG_DRAW_ERRORS");
    return env && (env[0] == '1');
  }();
  if (kLogDrawErrors) {
    auto* device = commandBuffer_.getContext().getDevice();
    if (device) {
      igl::d3d12::ComPtr<ID3D12InfoQueue> infoQueue;
      if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
        const UINT64 messageCount = infoQueue->GetNumStoredMessages();
        for (UINT64 i = 0; i < messageCount; ++i) {
          SIZE_T length = 0;
          if (FAILED(infoQueue->GetMessage(i, nullptr, &length)) || length == 0) {
            continue;
          }
          auto* message = static_cast<D3D12_MESSAGE*>(malloc(length));
          if (message && SUCCEEDED(infoQueue->GetMessage(i, message, &length))) {
            IGL_LOG_ERROR("[D3D12 Debug] %s\n", message->pDescription ? message->pDescription : "<no description>");
          }
          free(message);
        }
        infoQueue->ClearStoredMessages();
      }
    }
  }
#endif
}
void RenderCommandEncoder::multiDrawIndirect(IBuffer& indirectBuffer,
                                             size_t indirectBufferOffset,
                                             uint32_t drawCount,
                                             uint32_t stride) {
  if (!commandList_) {
    IGL_LOG_ERROR("RenderCommandEncoder::multiDrawIndirect: commandList_ is null\n");
    return;
  }

  // Get D3D12 buffer resource
  auto* d3dBuffer = static_cast<Buffer*>(&indirectBuffer);
  if (!d3dBuffer) {
    IGL_LOG_ERROR("RenderCommandEncoder::multiDrawIndirect: indirectBuffer is null\n");
    return;
  }

  ID3D12Resource* argBuffer = d3dBuffer->getResource();
  if (!argBuffer) {
    IGL_LOG_ERROR("RenderCommandEncoder::multiDrawIndirect: argBuffer resource is null\n");
    return;
  }

  // Get command signature from D3D12Context
  auto& ctx = commandBuffer_.getContext();
  ID3D12CommandSignature* signature = ctx.getDrawIndirectSignature();
  if (!signature) {
    IGL_LOG_ERROR("RenderCommandEncoder::multiDrawIndirect: command signature is null\n");
    return;
  }

  // Use default stride if not provided (sizeof D3D12_DRAW_ARGUMENTS = 16 bytes)
  const UINT actualStride = stride ? stride : sizeof(D3D12_DRAW_ARGUMENTS);

  // ExecuteIndirect for multi-draw
  // Parameters: signature, maxCommandCount, argumentBuffer, argumentBufferOffset, countBuffer, countBufferOffset
  commandList_->ExecuteIndirect(
      signature,
      drawCount,
      argBuffer,
      static_cast<UINT64>(indirectBufferOffset),
      nullptr,  // No count buffer (exact draw count specified)
      0);

  // Track draw call count
  commandBuffer_.incrementDrawCount(drawCount);

  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder::multiDrawIndirect: Executed %u indirect draws (stride: %u)\n",
               drawCount, actualStride);
}
void RenderCommandEncoder::multiDrawIndexedIndirect(IBuffer& indirectBuffer,
                                                    size_t indirectBufferOffset,
                                                    uint32_t drawCount,
                                                    uint32_t stride) {
  if (!commandList_) {
    IGL_LOG_ERROR("RenderCommandEncoder::multiDrawIndexedIndirect: commandList_ is null\n");
    return;
  }

  // Get D3D12 buffer resource
  auto* d3dBuffer = static_cast<Buffer*>(&indirectBuffer);
  if (!d3dBuffer) {
    IGL_LOG_ERROR("RenderCommandEncoder::multiDrawIndexedIndirect: indirectBuffer is null\n");
    return;
  }

  ID3D12Resource* argBuffer = d3dBuffer->getResource();
  if (!argBuffer) {
    IGL_LOG_ERROR("RenderCommandEncoder::multiDrawIndexedIndirect: argBuffer resource is null\n");
    return;
  }

  // Get command signature from D3D12Context
  auto& ctx = commandBuffer_.getContext();
  ID3D12CommandSignature* signature = ctx.getDrawIndexedIndirectSignature();
  if (!signature) {
    IGL_LOG_ERROR("RenderCommandEncoder::multiDrawIndexedIndirect: command signature is null\n");
    return;
  }

  // Use default stride if not provided (sizeof D3D12_DRAW_INDEXED_ARGUMENTS = 20 bytes)
  const UINT actualStride = stride ? stride : sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);

  // ExecuteIndirect for multi-draw indexed
  // Parameters: signature, maxCommandCount, argumentBuffer, argumentBufferOffset, countBuffer, countBufferOffset
  commandList_->ExecuteIndirect(
      signature,
      drawCount,
      argBuffer,
      static_cast<UINT64>(indirectBufferOffset),
      nullptr,  // No count buffer (exact draw count specified)
      0);

  // Track draw call count
  commandBuffer_.incrementDrawCount(drawCount);

  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder::multiDrawIndexedIndirect: Executed %u indirect indexed draws (stride: %u)\n",
               drawCount, actualStride);
}

void RenderCommandEncoder::setStencilReferenceValue(uint32_t value) {
  if (!commandList_) {
    return;
  }
  // Set stencil reference value for stencil testing
  commandList_->OMSetStencilRef(value);
  IGL_D3D12_LOG_VERBOSE("setStencilReferenceValue: Set stencil ref to %u\n", value);
}

void RenderCommandEncoder::setBlendColor(const Color& color) {
  if (!commandList_) {
    return;
  }
  // Set blend factor constants for BlendFactor::BlendColor operations
  // D3D12 uses RGBA float array, matching IGL Color structure
  const float blendFactor[4] = {color.r, color.g, color.b, color.a};
  commandList_->OMSetBlendFactor(blendFactor);
  IGL_D3D12_LOG_VERBOSE("setBlendColor: Set blend factor to (%.2f, %.2f, %.2f, %.2f)\n",
               color.r, color.g, color.b, color.a);
}

void RenderCommandEncoder::setDepthBias(float /*depthBias*/, float /*slopeScale*/, float /*clamp*/) {
  // Note: Depth bias is configured in the pipeline state (RasterizerState)
  // D3D12 does not support dynamic depth bias changes during rendering
  // This would require rebuilding the PSO with different depth bias values
}

void RenderCommandEncoder::pushDebugGroupLabel(const char* label, const Color& /*color*/) const {
  if (commandList_ && label) {
    const size_t len = strlen(label);
    std::wstring wlabel(len, L' ');
    std::mbstowcs(&wlabel[0], label, len);
    commandList_->BeginEvent(0, wlabel.c_str(), static_cast<UINT>((wlabel.length() + 1) * sizeof(wchar_t)));
  }
}

void RenderCommandEncoder::insertDebugEventLabel(const char* label, const Color& /*color*/) const {
  if (commandList_ && label) {
    const size_t len = strlen(label);
    std::wstring wlabel(len, L' ');
    std::mbstowcs(&wlabel[0], label, len);
    commandList_->SetMarker(0, wlabel.c_str(), static_cast<UINT>((wlabel.length() + 1) * sizeof(wchar_t)));
  }
}

void RenderCommandEncoder::popDebugGroupLabel() const {
  if (commandList_) {
    commandList_->EndEvent();
  }
}

void RenderCommandEncoder::bindBuffer(uint32_t index,
                                       IBuffer* buffer,
                                       size_t offset,
                                       size_t /*bufferSize*/) {
  IGL_D3D12_LOG_VERBOSE("bindBuffer START: index=%u\n", index);
  if (!buffer) {
    IGL_D3D12_LOG_VERBOSE("bindBuffer: null buffer, returning\n");
    return;
  }

  auto* d3dBuffer = static_cast<Buffer*>(buffer);

  // Check if this is a storage buffer - needs SRV binding for shader reads
  const bool isStorageBuffer = (d3dBuffer->getBufferType() & BufferDesc::BufferTypeBits::Storage) != 0;

  if (isStorageBuffer) {
    // Storage buffer - create SRV for ByteAddressBuffer reads in pixel shader
    IGL_D3D12_LOG_VERBOSE("bindBuffer: Storage buffer detected at index %u - creating SRV for pixel shader read\n", index);

    // For raw (ByteAddressBuffer) SRVs we treat the buffer as a sequence of 4-byte units.
    // This matches HLSL ByteAddressBuffer / RWByteAddressBuffer semantics.
    if ((offset & 3) != 0) {
      IGL_LOG_ERROR("bindBuffer: Storage buffer offset %zu is not 4-byte aligned (required for DXGI_FORMAT_R32_TYPELESS). "
                    "Raw buffer SRV FirstElement will be rounded down, which may cause incorrect data access.\n", offset);
      // Continue but log warning - FirstElement below uses integer division
    }

    auto& context = commandBuffer_.getContext();
    auto* device = context.getDevice();
    if (!device || cbvSrvUavHeap_ == nullptr) {
      IGL_LOG_ERROR("bindBuffer: Missing device or per-frame CBV/SRV/UAV heap\n");
      return;
    }

    // Allocate descriptor slot from command buffer's shared counter
    // Uses Result-based allocation with dynamic heap growth.
    uint32_t descriptorIndex = 0;
    Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
    if (!allocResult.isOk()) {
      IGL_LOG_ERROR("bindBuffer: Failed to allocate descriptor: %s\n", allocResult.message.c_str());
      return;
    }
    IGL_D3D12_LOG_VERBOSE("bindBuffer: Allocated SRV descriptor slot %u for buffer at t%u\n", descriptorIndex, index);

    // Create SRV descriptor for ByteAddressBuffer (raw view)
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;  // Raw buffer (ByteAddressBuffer)
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    // FirstElement/NumElements expressed in 32-bit units (4 bytes)
    srvDesc.Buffer.FirstElement = static_cast<UINT64>(offset) / 4;  // Offset in 32-bit elements
    // NumElements must be (totalSize - offset) to avoid exceeding buffer bounds
    srvDesc.Buffer.NumElements =
        static_cast<UINT>((buffer->getSizeInBytes() - offset) / 4);  // Size in 32-bit elements
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;  // Raw buffer access

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);
    // Pre-creation validation.
    IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateShaderResourceView");
    IGL_DEBUG_ASSERT(d3dBuffer->getResource() != nullptr, "Buffer resource is null");
    IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "SRV descriptor handle is invalid");

    device->CreateShaderResourceView(d3dBuffer->getResource(), &srvDesc, cpuHandle);

    IGL_D3D12_LOG_VERBOSE("bindBuffer: Created SRV at descriptor slot %u (FirstElement=%llu, NumElements=%u)\n",
                 descriptorIndex, srvDesc.Buffer.FirstElement, srvDesc.Buffer.NumElements);

    // Cache GPU handle for descriptor table binding in draw calls
    // SRVs are bound to root parameter 3 (render root signature)
    cachedTextureGpuHandles_[index] = gpuHandle;
    cachedTextureCount_ = std::max(cachedTextureCount_, static_cast<size_t>(index + 1));

    IGL_D3D12_LOG_VERBOSE("bindBuffer: Storage buffer SRV binding complete\n");

    // CRITICAL: Track the Buffer OBJECT (not just resource) to keep it alive until GPU finishes
    // This prevents the Buffer destructor from releasing the resource while GPU commands reference it
    // Use weak_from_this().lock() instead of shared_from_this() to avoid exception
    std::shared_ptr<IBuffer> sharedBuffer = d3dBuffer->weak_from_this().lock();
    if (sharedBuffer) {
      static_cast<CommandBuffer&>(commandBuffer_).trackTransientBuffer(std::move(sharedBuffer));
      IGL_D3D12_LOG_VERBOSE("bindBuffer: Tracking Buffer object (shared_ptr) for lifetime management\n");
    } else {
      // Buffer not managed by shared_ptr (e.g., persistent buffer from member variable)
      // Fall back to tracking just the resource (AddRef on ID3D12Resource)
      static_cast<CommandBuffer&>(commandBuffer_).trackTransientResource(d3dBuffer->getResource());
      IGL_D3D12_LOG_VERBOSE("bindBuffer: Buffer not shared_ptr-managed, tracking resource only\n");
    }
  } else {
    // Constant buffer - use CBV binding (existing path)
    IGL_D3D12_LOG_VERBOSE("bindBuffer: Constant buffer at index %u\n", index);

    // D3D12 requires constant buffer addresses to be 256-byte aligned
    // (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
    if ((offset & 255) != 0) {
      IGL_LOG_ERROR("bindBuffer: ERROR - CBV offset %zu is not 256-byte aligned (required by D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT). "
                    "Constant buffers must be created at aligned offsets. Ignoring bind request.\n", offset);
      return;
    }

    D3D12_GPU_VIRTUAL_ADDRESS bufferAddress = d3dBuffer->gpuAddress(offset);
    IGL_D3D12_LOG_VERBOSE("bindBuffer: CBV address=0x%llx\n", bufferAddress);

    if (!commandList_) {
      IGL_LOG_ERROR("bindBuffer: commandList_ is NULL!\n");
      return;
    }

    // CRITICAL: Track the Buffer OBJECT (not just resource) to keep it alive until GPU finishes
    // This prevents the Buffer destructor from releasing the resource while GPU commands reference it
    // Use weak_from_this().lock() instead of shared_from_this() to avoid exception
    std::shared_ptr<IBuffer> sharedBuffer = d3dBuffer->weak_from_this().lock();
    if (sharedBuffer) {
      static_cast<CommandBuffer&>(commandBuffer_).trackTransientBuffer(std::move(sharedBuffer));
      IGL_D3D12_LOG_VERBOSE("bindBuffer: Tracking Buffer object (shared_ptr) for lifetime management\n");
    } else {
      // Buffer not managed by shared_ptr (e.g., persistent buffer from member variable)
      // Fall back to tracking just the resource (AddRef on ID3D12Resource)
      static_cast<CommandBuffer&>(commandBuffer_).trackTransientResource(d3dBuffer->getResource());
      IGL_D3D12_LOG_VERBOSE("bindBuffer: Buffer not shared_ptr-managed, tracking resource only\n");
    }

    // Cache CBV for draw calls (b0, b1) - bind to root CBVs
    if (index <= 1) {
      cachedConstantBuffers_[index] = bufferAddress;
      constantBufferBound_[index] = true;
      IGL_D3D12_LOG_VERBOSE("bindBuffer: Cached CBV at index %u with address 0x%llx\n", index, bufferAddress);
    } else if (index == 2) {
      // b2 maps to push constants (root parameter 0)
      void* bufferData = nullptr;
      HRESULT hr = d3dBuffer->getResource()->Map(0, nullptr, &bufferData);
      if (SUCCEEDED(hr) && bufferData) {
        const size_t bufferSize = d3dBuffer->getSizeInBytes();
        const size_t clampedSize = std::min(bufferSize, static_cast<size_t>(256));

        const uint32_t num32BitValues = static_cast<uint32_t>((clampedSize + 3) / 4);
        commandList_->SetGraphicsRoot32BitConstants(
            0,  // Root parameter index (push constants at b2)
            num32BitValues,
            static_cast<const uint8_t*>(bufferData) + offset,
            0);

        d3dBuffer->getResource()->Unmap(0, nullptr);
        IGL_D3D12_LOG_VERBOSE("bindBuffer: Bound index 2 (b2) as push constants (%zu bytes)\n", clampedSize);
      } else {
        IGL_LOG_ERROR("bindBuffer: Failed to map buffer for index 2 (b2) push constants\n");
      }
    }
  }

  IGL_D3D12_LOG_VERBOSE("bindBuffer END\n");
}
void RenderCommandEncoder::bindBindGroup(BindGroupTextureHandle handle) {
  IGL_D3D12_LOG_VERBOSE("bindBindGroup(texture): handle valid=%d\n", !handle.empty());

  // Get the bind group descriptor from the device
  auto& device = commandBuffer_.getDevice();
  const auto* desc = device.getBindGroupTextureDesc(handle);
  if (!desc) {
    IGL_LOG_ERROR("bindBindGroup(texture): Invalid handle or descriptor not found\n");
    return;
  }

  auto& context = commandBuffer_.getContext();
  auto* d3dDevice = context.getDevice();
  auto* cmd = commandList_;
  if (!d3dDevice || !cmd) {
    IGL_LOG_ERROR("bindBindGroup(texture): missing device or command list\n");
    return;
  }

  // Compute dense counts for textures and samplers
  uint32_t texCount = 0;
  for (; texCount < IGL_TEXTURE_SAMPLERS_MAX; ++texCount) {
    if (!desc->textures[texCount]) break;
  }
  uint32_t smpCount = 0;
  for (; smpCount < IGL_TEXTURE_SAMPLERS_MAX; ++smpCount) {
    if (!desc->samplers[smpCount]) break;
  }

  // Allocate contiguous slices from per-frame descriptor heaps
  // Allocate descriptors one at a time (they may span pages).
  const uint32_t srvBaseIndex = 0;  // Will use first allocated index
  std::vector<uint32_t> srvIndices;
  for (uint32_t i = 0; i < texCount; ++i) {
    uint32_t descriptorIndex = 0;
    Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
    if (!allocResult.isOk()) {
      IGL_LOG_ERROR("bindBindGroup: Failed to allocate SRV descriptor %u: %s\n", i, allocResult.message.c_str());
      return;
    }
    srvIndices.push_back(descriptorIndex);
  }
  uint32_t& nextSmp = commandBuffer_.getNextSamplerDescriptor();
  const uint32_t smpBaseIndex = nextSmp;
  nextSmp += smpCount;

  // Create SRVs into the allocated SRV slice
  for (uint32_t i = 0; i < texCount; ++i) {
    auto* tex = static_cast<Texture*>(desc->textures[i].get());
    if (!tex || !tex->getResource()) {
      IGL_LOG_ERROR("bindBindGroup(texture): null texture at index %u\n", i);
      continue;
    }
    tex->transitionAll(cmd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // Build SRV description for the texture/view
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureFormatToDXGIShaderResourceViewFormat(tex->getFormat());
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    auto resourceDesc = tex->getResource()->GetDesc();
    const bool isView = tex->isView();
    const uint32_t mostDetailedMip = isView ? tex->getMipLevelOffset() : 0;
    const uint32_t mipLevels = isView ? tex->getNumMipLevelsInView() : tex->getNumMipLevels();

    if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
      if (tex->getType() == TextureType::TwoDArray) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip = mostDetailedMip;
        srvDesc.Texture2DArray.MipLevels = mipLevels;
        srvDesc.Texture2DArray.FirstArraySlice = isView ? tex->getArraySliceOffset() : 0;
        srvDesc.Texture2DArray.ArraySize = isView ? tex->getNumArraySlicesInView() : resourceDesc.DepthOrArraySize;
        srvDesc.Texture2DArray.PlaneSlice = 0;
        srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
      } else {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = mostDetailedMip;
        srvDesc.Texture2D.MipLevels = mipLevels;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
      }
    } else if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
      srvDesc.Texture3D.MostDetailedMip = mostDetailedMip;
      srvDesc.Texture3D.MipLevels = mipLevels;
      srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;
    } else if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D) {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
      srvDesc.Texture1D.MostDetailedMip = mostDetailedMip;
      srvDesc.Texture1D.MipLevels = mipLevels;
      srvDesc.Texture1D.ResourceMinLODClamp = 0.0f;
    } else {
      // Fallback
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MostDetailedMip = mostDetailedMip;
      srvDesc.Texture2D.MipLevels = mipLevels;
      srvDesc.Texture2D.PlaneSlice = 0;
      srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    }

    // Use individually allocated descriptor indices (may span multiple pages).
    const uint32_t dstIndex = srvIndices[i];
    D3D12_CPU_DESCRIPTOR_HANDLE dstCpu = context.getCbvSrvUavCpuHandle(dstIndex);
    // Pre-creation validation.
    IGL_DEBUG_ASSERT(d3dDevice != nullptr, "Device is null before CreateShaderResourceView");
    IGL_DEBUG_ASSERT(tex->getResource() != nullptr, "Texture resource is null");
    IGL_DEBUG_ASSERT(dstCpu.ptr != 0, "SRV descriptor handle is invalid");

    d3dDevice->CreateShaderResourceView(tex->getResource(), &srvDesc, dstCpu);
    D3D12Context::trackResourceCreation("SRV", 0);
  }

  // Create samplers into the allocated sampler slice
  for (uint32_t i = 0; i < smpCount; ++i) {
    auto* smp = static_cast<SamplerState*>(desc->samplers[i].get());
    D3D12_SAMPLER_DESC samplerDesc = smp ? smp->getDesc() : D3D12_SAMPLER_DESC{};
    D3D12_CPU_DESCRIPTOR_HANDLE dstCpu = context.getSamplerCpuHandle(smpBaseIndex + i);
    // Pre-creation validation.
    IGL_DEBUG_ASSERT(d3dDevice != nullptr, "Device is null before CreateSampler");
    IGL_DEBUG_ASSERT(dstCpu.ptr != 0, "Sampler descriptor handle is invalid");

    d3dDevice->CreateSampler(&samplerDesc, dstCpu);
    D3D12Context::trackResourceCreation("Sampler", 0);
  }

  // Cache base GPU handles so draw() binds once per table.
  // Use first allocated descriptor index (descriptors may span pages).
  if (texCount > 0 && !srvIndices.empty()) {
    cachedTextureGpuHandles_[0] = context.getCbvSrvUavGpuHandle(srvIndices[0]);
    cachedTextureCount_ = texCount;
  }
  if (smpCount > 0) {
    cachedSamplerGpuHandles_[0] = context.getSamplerGpuHandle(smpBaseIndex);
    cachedSamplerCount_ = smpCount;
  }

  // Mark that bindBindGroup was used (vs storage buffer SRV or binder paths)
  usedBindGroup_ = true;
}

void RenderCommandEncoder::bindBindGroup(BindGroupBufferHandle handle,
                                          uint32_t numDynamicOffsets,
                                          const uint32_t* dynamicOffsets) {
  IGL_D3D12_LOG_VERBOSE("bindBindGroup(buffer): handle valid=%d, dynCount=%u\n", !handle.empty(), numDynamicOffsets);

  auto& device = commandBuffer_.getDevice();
  const auto* desc = device.getBindGroupBufferDesc(handle);
  if (!desc) {
    IGL_LOG_ERROR("bindBindGroup(buffer): Invalid handle or descriptor not found\n");
    return;
  }

  auto* cmd = commandList_;
  if (!cmd) {
    IGL_LOG_ERROR("bindBindGroup(buffer): null command list\n");
    return;
  }

  // Emulate Vulkan dynamic offsets for uniform buffers by binding to root CBVs (b0,b1) when possible.
  // Note: Current RS exposes two root CBVs (params 1 and 2). Additional CBVs and storage buffers should
  // be promoted to SRV/UAV tables in a future RS update.
  uint32_t dynIdx = 0;

  for (uint32_t slot = 0; slot < IGL_UNIFORM_BLOCKS_BINDING_MAX; ++slot) {
    if (!desc->buffers[slot]) continue; // Skip empty slots

    auto* buf = static_cast<Buffer*>(desc->buffers[slot].get());
    const bool isUniform = (buf->getBufferType() & BufferDesc::BufferTypeBits::Uniform) != 0;
    const bool isStorage = (buf->getBufferType() & BufferDesc::BufferTypeBits::Storage) != 0;

    // Track buffer resource to prevent it from being deleted while GPU address is cached
    commandBuffer_.trackTransientResource(buf->getResource());

    size_t baseOffset = desc->offset[slot];
    if ((desc->isDynamicBufferMask & (1u << slot)) != 0) {
      if (dynIdx < numDynamicOffsets && dynamicOffsets) {
        baseOffset = dynamicOffsets[dynIdx++];
      }
    }

    if (isUniform) {
      // 256B alignment required for CBVs
      const size_t aligned = (baseOffset + 255) & ~size_t(255);
      D3D12_GPU_VIRTUAL_ADDRESS addr = buf->gpuAddress(aligned);

      // Map BindGroupBufferDesc slot index to descriptor table index
      // BindGroupBufferDesc slots 0,1,2,... map to descriptor table b3,b4,b5,...
      // This allows cross-platform bind group code to use indices starting from 0
      const uint32_t tableIndex = slot;  // slot 0 -> b3, slot 1 -> b4, etc.

      if (tableIndex < IGL_BUFFER_BINDINGS_MAX) {
        // Allocate descriptor from per-frame heap
        // Uses Result-based allocation with dynamic heap growth.
        uint32_t descriptorIndex = 0;
        Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
        if (!allocResult.isOk()) {
          IGL_LOG_ERROR("bindBindGroup(buffer): Failed to allocate CBV descriptor: %s\n", allocResult.message.c_str());
          continue;
        }

        // Get context and device
        auto& context = commandBuffer_.getContext();
        auto* device = context.getDevice();

        // Get CPU/GPU descriptor handles for this slot
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);

        // Respect requested buffer size and enforce the 64 KB limit.
        // If size[slot] is 0, use remaining buffer size from offset
        size_t requestedSize = desc->size[slot];
        if (requestedSize == 0) {
          requestedSize = buf->getSizeInBytes() - aligned;
        }

        // D3D12 spec: Constant buffers must be ≤ 64 KB
        constexpr size_t kMaxCBVSize = 65536;  // 64 KB
        if (requestedSize > kMaxCBVSize) {
          IGL_LOG_ERROR("bindBindGroup(buffer): Constant buffer size (%zu bytes) exceeds D3D12 64 KB limit at slot %u\n",
                        requestedSize, slot);
          continue;  // Skip this binding
        }

        // Create CBV descriptor
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = addr;
        cbvDesc.SizeInBytes = static_cast<UINT>((requestedSize + 255) & ~255);  // Must be 256-byte aligned

        // Pre-creation validation.
        IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateConstantBufferView");
        IGL_DEBUG_ASSERT(addr != 0, "Buffer GPU address is null");
        IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "CBV descriptor handle is invalid");
        IGL_DEBUG_ASSERT(cbvDesc.SizeInBytes <= kMaxCBVSize, "CBV size exceeds 64 KB after alignment");

        device->CreateConstantBufferView(&cbvDesc, cpuHandle);

        // Cache the GPU handle for the descriptor table
        // BindGroupBufferDesc index 0 -> table index 0 (b3), index 1 -> table index 1 (b4), etc.
        cachedCbvTableGpuHandles_[tableIndex] = gpuHandle;
        cbvTableBound_[tableIndex] = true;
        cbvTableCount_ = std::max(cbvTableCount_, static_cast<size_t>(tableIndex + 1));

        IGL_D3D12_LOG_VERBOSE("bindBindGroup(buffer): bound uniform buffer at BindGroupBufferDesc[%u] to b%u (descriptor table index %u, GPU handle 0x%llx)\n",
                     slot, slot + 3, tableIndex, gpuHandle.ptr);
      } else {
        IGL_LOG_ERROR("bindBindGroup(buffer): BindGroupBufferDesc slot %u exceeds maximum (%u)\n", slot, IGL_BUFFER_BINDINGS_MAX);
      }
    } else if (isStorage) {
      // Implement storage buffer binding via UAV/SRV descriptors.
      auto& context = commandBuffer_.getContext();
      auto* device = context.getDevice();
      ID3D12Resource* resource = buf->getResource();

      // Determine if buffer is read-write (UAV) or read-only (SRV)
      // D3D12 storage buffers with UAV flag are read-write by default
      // Private/Shared storage indicates read-write access, Managed indicates read-only
      const bool isReadWrite = (buf->storage() == ResourceStorage::Private ||
                                buf->storage() == ResourceStorage::Shared);

      if (isReadWrite) {
        // Create UAV for read-write storage buffer
        // Uses Result-based allocation with dynamic heap growth.
        uint32_t descriptorIndex = 0;
        Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
        if (!allocResult.isOk()) {
          IGL_LOG_ERROR("bindBindGroup(buffer): Failed to allocate UAV descriptor: %s\n", allocResult.message.c_str());
          continue;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);

        // Create UAV descriptor for structured buffer
        // Use the storage stride from BufferDesc when available; default to 4 bytes otherwise.
        size_t elementStride = buf->getStorageElementStride();
        if (elementStride == 0) {
          elementStride = 4;
        }

        // Validate baseOffset doesn't exceed buffer size
        const size_t bufferSizeBytes = buf->getSizeInBytes();
        if (baseOffset > bufferSizeBytes) {
          IGL_LOG_ERROR("bindBindGroup(buffer): baseOffset %zu exceeds buffer size %zu; skipping UAV binding\n",
                        baseOffset, bufferSizeBytes);
          continue;
        }

        if (baseOffset % elementStride != 0) {
          IGL_LOG_ERROR("bindBindGroup(buffer): Storage buffer baseOffset %zu is not aligned to "
                        "element stride (%zu bytes). UAV FirstElement will be truncated (offset/stride).\n",
                        baseOffset, elementStride);
        }

        const size_t remaining = bufferSizeBytes - baseOffset;

        // Check for undersized buffer (would create empty or partial view)
        if (remaining < elementStride) {
          IGL_LOG_ERROR("bindBindGroup(buffer): Remaining buffer size %zu is less than element stride %zu; "
                        "UAV will have NumElements=0 (empty view). Check buffer size and offset.\n",
                        remaining, elementStride);
          // Continue to create the descriptor, but it will be empty (NumElements=0)
        }

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = static_cast<UINT64>(baseOffset / elementStride);
        // CRITICAL: NumElements must be (size - offset) / stride, not total size / stride
        uavDesc.Buffer.NumElements = static_cast<UINT>(remaining / elementStride);
        uavDesc.Buffer.StructureByteStride = static_cast<UINT>(elementStride);
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        // Pre-creation validation.
        IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateUnorderedAccessView");
        IGL_DEBUG_ASSERT(resource != nullptr, "Buffer resource is null");
        IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "UAV descriptor handle is invalid");

        device->CreateUnorderedAccessView(resource, nullptr, &uavDesc, cpuHandle);

        // Bind UAV descriptor table (Parameter 6 in graphics root signature)
        commandList_->SetGraphicsRootDescriptorTable(6, gpuHandle);

        IGL_D3D12_LOG_VERBOSE("bindBindGroup(buffer): bound read-write storage buffer at slot %u (UAV u%u, GPU handle 0x%llx)\n",
                     slot, slot, gpuHandle.ptr);
      } else {
        // Create SRV for read-only storage buffer
        // Uses Result-based allocation with dynamic heap growth.
        uint32_t descriptorIndex = 0;
        Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
        if (!allocResult.isOk()) {
          IGL_LOG_ERROR("bindBindGroup(buffer): Failed to allocate SRV descriptor: %s\n", allocResult.message.c_str());
          continue;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);

        // Create SRV descriptor for structured buffer
        size_t elementStride = buf->getStorageElementStride();
        if (elementStride == 0) {
          elementStride = 4;
        }

        // Validate baseOffset doesn't exceed buffer size
        const size_t bufferSizeBytes = buf->getSizeInBytes();
        if (baseOffset > bufferSizeBytes) {
          IGL_LOG_ERROR("bindBindGroup(buffer): baseOffset %zu exceeds buffer size %zu; skipping SRV binding\n",
                        baseOffset, bufferSizeBytes);
          continue;
        }

        if (baseOffset % elementStride != 0) {
          IGL_LOG_ERROR("bindBindGroup(buffer): Storage buffer baseOffset %zu is not aligned to "
                        "element stride (%zu bytes). SRV FirstElement will be truncated (offset/stride).\n",
                        baseOffset, elementStride);
        }

        const size_t remaining = bufferSizeBytes - baseOffset;

        // Check for undersized buffer (would create empty or partial view)
        if (remaining < elementStride) {
          IGL_LOG_ERROR("bindBindGroup(buffer): Remaining buffer size %zu is less than element stride %zu; "
                        "SRV will have NumElements=0 (empty view). Check buffer size and offset.\n",
                        remaining, elementStride);
          // Continue to create the descriptor, but it will be empty (NumElements=0)
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = static_cast<UINT64>(baseOffset / elementStride);
        // CRITICAL: NumElements must be (size - offset) / stride, not total size / stride
        srvDesc.Buffer.NumElements = static_cast<UINT>(remaining / elementStride);
        srvDesc.Buffer.StructureByteStride = static_cast<UINT>(elementStride);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        // Pre-creation validation.
        IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateShaderResourceView");
        IGL_DEBUG_ASSERT(resource != nullptr, "Buffer resource is null");
        IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "SRV descriptor handle is invalid");

        device->CreateShaderResourceView(resource, &srvDesc, cpuHandle);

        // Bind SRV descriptor table (Parameter 4 in graphics root signature)
        // Note: This shares the texture SRV table, storage buffers and textures will be bound together
        // PRECEDENCE: Storage buffer SRVs bound here will override any previous texture SRVs bound via
        // the binder or bindBindGroup(texture) for root parameter 4. The last SetGraphicsRootDescriptorTable
        // call wins. This is intentional - storage buffer bindings via bindBindGroup(buffer) take precedence
        // over texture bindings. If both storage buffers and textures are needed, they should be coordinated
        // at the application level to ensure correct binding order.
        commandList_->SetGraphicsRootDescriptorTable(4, gpuHandle);

        IGL_D3D12_LOG_VERBOSE("bindBindGroup(buffer): bound read-only storage buffer at slot %u (SRV t%u, GPU handle 0x%llx)\n",
                     slot, slot, gpuHandle.ptr);
      }
    }
  }

  // Mark that bindBindGroup was used (vs storage buffer SRV or binder paths).
  usedBindGroup_ = true;
}

// G-001: Barrier batching implementation
void RenderCommandEncoder::flushBarriers() {
  if (pendingBarriers_.empty()) {
    return;
  }

  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Flushing %zu batched resource barriers\n",
               pendingBarriers_.size());

  // Submit all pending barriers in a single API call
  commandList_->ResourceBarrier(static_cast<UINT>(pendingBarriers_.size()),
                                 pendingBarriers_.data());

  // Clear the pending barrier queue
  pendingBarriers_.clear();
}

void RenderCommandEncoder::queueBarrier(const D3D12_RESOURCE_BARRIER& barrier) {
  pendingBarriers_.push_back(barrier);
  IGL_D3D12_LOG_VERBOSE("RenderCommandEncoder: Queued barrier (total pending: %zu)\n",
               pendingBarriers_.size());
}

} // namespace igl::d3d12
