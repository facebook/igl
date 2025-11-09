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
  // CRITICAL: Must use per-frame heaps from D3D12Context, NOT DescriptorHeapManager!
  // DescriptorHeapManager has a shared heap (old broken approach) which causes race conditions.
  // Per-frame heaps are isolated per frame to prevent descriptor conflicts.
  DescriptorHeapManager* heapMgr = context.getDescriptorHeapManager();

  // ALWAYS use per-frame heaps from D3D12Context (NOT DescriptorHeapManager)
  // Store in member variables so bindTexture/bindSamplerState can use them
  cbvSrvUavHeap_ = context.getCbvSrvUavHeap();
  samplerHeap_ = context.getSamplerHeap();

  IGL_LOG_INFO("RenderCommandEncoder: Using per-frame heaps from D3D12Context\n");

  IGL_LOG_INFO("RenderCommandEncoder: CBV/SRV/UAV heap = %p\n", cbvSrvUavHeap_);
  IGL_LOG_INFO("RenderCommandEncoder: Sampler heap = %p\n", samplerHeap_);

  ID3D12DescriptorHeap* heaps[] = {cbvSrvUavHeap_, samplerHeap_};
  IGL_LOG_INFO("RenderCommandEncoder: Setting descriptor heaps...\n");
  commandList_->SetDescriptorHeaps(2, heaps);
  IGL_LOG_INFO("RenderCommandEncoder: Descriptor heaps set\n");

  // Create RTV from framebuffer if provided; otherwise fallback to swapchain RTV
  IGL_LOG_INFO("RenderCommandEncoder: Setting up RTV...\n");
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = {};
  std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
  rtvIndices_.clear();
  bool usedOffscreenRTV = false;
  // Note: heapMgr already retrieved above for setting descriptor heaps
  IGL_LOG_INFO("RenderCommandEncoder: DescriptorHeapManager = %p\n", heapMgr);

  IGL_LOG_INFO("RenderCommandEncoder: Checking framebuffer_=%p\n", framebuffer_.get());
  // Only create offscreen RTV if we have DescriptorHeapManager AND it's not a swapchain texture
  // Swapchain textures should use context.getCurrentRTV() directly
  if (framebuffer_ && framebuffer_->getColorAttachment(0) && heapMgr) {
    IGL_LOG_INFO("RenderCommandEncoder: Has framebuffer with color attachment AND DescriptorHeapManager\n");
    ID3D12Device* device = context.getDevice();
    if (device) {
      // Create RTVs for each color attachment
      const size_t count = std::min<size_t>(framebuffer_->getColorAttachmentIndices().size(), D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
      IGL_LOG_INFO("RenderCommandEncoder: MRT count = %zu (indices.size=%zu)\n", count, framebuffer_->getColorAttachmentIndices().size());
      for (size_t i = 0; i < count; ++i) {
        auto tex = std::static_pointer_cast<Texture>(framebuffer_->getColorAttachment(i));
        IGL_LOG_INFO("RenderCommandEncoder: MRT loop i=%zu, tex=%p, resource=%p\n", i, tex.get(), tex ? tex->getResource() : nullptr);
        if (!tex || !tex->getResource()) {
          IGL_LOG_INFO("RenderCommandEncoder: MRT loop i=%zu SKIPPED (null tex or resource)\n", i);
          continue;
        }
        const bool hasAttachmentDesc = (i < renderPass.colorAttachments.size());
        const uint32_t mipLevel =
            hasAttachmentDesc ? renderPass.colorAttachments[i].mipLevel : 0;
        const uint32_t attachmentLayer =
            hasAttachmentDesc ? renderPass.colorAttachments[i].layer : 0;
        const uint32_t attachmentFace =
            hasAttachmentDesc ? renderPass.colorAttachments[i].face : 0;
        // Allocate RTV
        uint32_t rtvIdx = heapMgr->allocateRTV();
        if (rtvIdx == UINT32_MAX) {
          IGL_LOG_ERROR("RenderCommandEncoder: Failed to allocate RTV descriptor (heap exhausted)\n");
          continue;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = heapMgr->getRTVHandle(rtvIdx);
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
            IGL_LOG_INFO(
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
            IGL_LOG_INFO("RenderCommandEncoder: Creating MSAA array RTV with %u samples, layer %u\n",
                         resourceDesc.SampleDesc.Count, rdesc.Texture2DMSArray.FirstArraySlice);
          } else {
            // MSAA non-array texture - use TEXTURE2DMS view dimension
            rdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            IGL_LOG_INFO("RenderCommandEncoder: Creating MSAA RTV with %u samples\n", resourceDesc.SampleDesc.Count);
          }
        } else {
          // Non-MSAA texture
          if (isCubeTexture) {
            rdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rdesc.Texture2DArray.MipSlice = mipLevel;
            rdesc.Texture2DArray.PlaneSlice = 0;
            rdesc.Texture2DArray.FirstArraySlice = targetArraySlice;
            rdesc.Texture2DArray.ArraySize = 1;
            IGL_LOG_INFO(
                "RenderCommandEncoder: Creating cube RTV, mip %u, face %u, cube index %u (array slice %u)\n",
                mipLevel,
                attachmentFace,
                attachmentLayer,
                rdesc.Texture2DArray.FirstArraySlice);
          } else if (isArrayTexture) {
            // Texture array - use TEXTURE2DARRAY view dimension
            rdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rdesc.Texture2DArray.MipSlice = (i < renderPass.colorAttachments.size()) ? renderPass.colorAttachments[i].mipLevel : 0;
            rdesc.Texture2DArray.PlaneSlice = 0;
            if (isView) {
              rdesc.Texture2DArray.FirstArraySlice = tex->getArraySliceOffset();
              rdesc.Texture2DArray.ArraySize = tex->getNumArraySlicesInView();
            } else {
              rdesc.Texture2DArray.FirstArraySlice = attachmentLayer;
              rdesc.Texture2DArray.ArraySize = 1;  // Render to single layer
            }
            IGL_LOG_INFO("RenderCommandEncoder: Creating array RTV, mip %u, layer %u\n",
                         rdesc.Texture2DArray.MipSlice, rdesc.Texture2DArray.FirstArraySlice);
          } else {
            // Non-array texture - use standard TEXTURE2D view dimension
            rdesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rdesc.Texture2D.MipSlice = (i < renderPass.colorAttachments.size()) ? renderPass.colorAttachments[i].mipLevel : 0;
            rdesc.Texture2D.PlaneSlice = 0;
            IGL_LOG_INFO("RenderCommandEncoder: Creating RTV, mip %u\n", rdesc.Texture2D.MipSlice);
          }
        }
        // Pre-creation validation (TASK_P0_DX12-004)
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
          IGL_LOG_INFO("RenderCommandEncoder: Clearing MRT attachment %zu with color (%.2f, %.2f, %.2f, %.2f)\n",
                       i, color[0], color[1], color[2], color[3]);
          commandList_->ClearRenderTargetView(rtvHandle, color, 0, nullptr);
        } else {
          IGL_LOG_INFO("RenderCommandEncoder: NOT clearing MRT attachment %zu (loadAction=%d, hasAttachment=%d)\n",
                       i, renderPass.colorAttachments.size() > i ? (int)renderPass.colorAttachments[i].loadAction : -1,
                       i < renderPass.colorAttachments.size());
        }
        rtvs.push_back(rtvHandle);
        IGL_LOG_INFO("RenderCommandEncoder: MRT Created RTV #%zu, total RTVs now=%zu\n", i, rtvs.size());
      }
      IGL_LOG_INFO("RenderCommandEncoder: MRT Total RTVs created: %zu\n", rtvs.size());
      if (!rtvs.empty()) {
        rtv = rtvs[0];
        usedOffscreenRTV = true;
      }
    }
  }
  if (!usedOffscreenRTV) {
    IGL_LOG_INFO("RenderCommandEncoder: Using swapchain back buffer\n");
    auto* backBuffer = context.getCurrentBackBuffer();
    IGL_LOG_INFO("RenderCommandEncoder: Got back buffer=%p\n", backBuffer);
    if (!backBuffer) {
      IGL_LOG_ERROR("RenderCommandEncoder: No back buffer available\n");
      return;
    }
    IGL_LOG_INFO("RenderCommandEncoder: Transitioning back buffer to RENDER_TARGET\n");
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);
    IGL_LOG_INFO("RenderCommandEncoder: Resource barrier executed\n");

    if (!renderPass.colorAttachments.empty() &&
        renderPass.colorAttachments[0].loadAction == LoadAction::Clear) {
      IGL_LOG_INFO("RenderCommandEncoder: Clearing render target\n");
      const auto& cc = renderPass.colorAttachments[0].clearColor;
      const float col[] = {cc.r, cc.g, cc.b, cc.a};
      D3D12_CPU_DESCRIPTOR_HANDLE swapRtv = context.getCurrentRTV();
      commandList_->ClearRenderTargetView(swapRtv, col, 0, nullptr);
      IGL_LOG_INFO("RenderCommandEncoder: Clear complete\n");
    }
    rtv = context.getCurrentRTV();
    IGL_LOG_INFO("RenderCommandEncoder: Got RTV handle\n");
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
      dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

      // Set view dimension based on sample count (MSAA support)
      D3D12_RESOURCE_DESC depthResourceDesc = depthTex->getResource()->GetDesc();
      if (depthResourceDesc.SampleDesc.Count > 1) {
        // MSAA depth texture - use TEXTURE2DMS view dimension
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
        IGL_LOG_INFO("RenderCommandEncoder: Creating MSAA DSV with %u samples\n", depthResourceDesc.SampleDesc.Count);
      } else {
        // Non-MSAA depth texture - use standard TEXTURE2D view dimension
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = renderPass.depthAttachment.mipLevel;
      }

      const uint32_t depthMip = renderPass.depthAttachment.mipLevel;
      const uint32_t depthLayer = renderPass.depthAttachment.layer;
      depthTex->transitionTo(commandList_, D3D12_RESOURCE_STATE_DEPTH_WRITE, depthMip, depthLayer);

      // Pre-creation validation (TASK_P0_DX12-004)
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

        IGL_LOG_INFO("RenderCommandEncoder: Cleared depth-stencil (depth=%d, stencil=%d, depthVal=%.2f, stencilVal=%u)\n",
                     clearDepth, clearStencil, depthClearValue, stencilClearValue);
      }

      // Bind RTV + DSV (or DSV-only for depth-only rendering)
      if (!rtvs.empty()) {
        // Multi-render target or offscreen rendering with color+depth
        IGL_LOG_INFO("RenderCommandEncoder: OMSetRenderTargets with %zu RTVs + DSV\n", rtvs.size());
        commandList_->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), FALSE, &dsvHandle_);
      } else if (usedOffscreenRTV) {
        // Single offscreen render target with depth
        IGL_LOG_INFO("RenderCommandEncoder: OMSetRenderTargets with 1 RTV + DSV\n");
        commandList_->OMSetRenderTargets(1, &rtv, FALSE, &dsvHandle_);
      } else if (!framebuffer_->getColorAttachment(0)) {
        // Depth-only rendering (no color attachments) - shadow mapping scenario
        IGL_LOG_INFO("RenderCommandEncoder: Depth-only rendering - OMSetRenderTargets with 0 RTVs + DSV\n");
        commandList_->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle_);
      } else {
        // Swapchain backbuffer with depth
        IGL_LOG_INFO("RenderCommandEncoder: OMSetRenderTargets with swapchain RTV + DSV\n");
        commandList_->OMSetRenderTargets(1, &rtv, FALSE, &dsvHandle_);
      }
    } else {
      IGL_LOG_INFO("RenderCommandEncoder: Binding RTV without DSV (no resource)\n");
      if (!rtvs.empty()) {
        IGL_LOG_INFO("RenderCommandEncoder: OMSetRenderTargets with %zu RTVs, no DSV\n", rtvs.size());
        commandList_->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), FALSE, nullptr);
      } else {
        IGL_LOG_INFO("RenderCommandEncoder: OMSetRenderTargets with 1 RTV, no DSV\n");
        commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
      }
    }
  } else {
    IGL_LOG_INFO("RenderCommandEncoder: Binding RTV without DSV (no hasDepth)\n");
    if (!rtvs.empty()) {
      IGL_LOG_INFO("RenderCommandEncoder: OMSetRenderTargets with %zu RTVs, no DSV (no hasDepth)\n", rtvs.size());
      commandList_->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), FALSE, nullptr);
    } else {
      IGL_LOG_INFO("RenderCommandEncoder: OMSetRenderTargets with 1 RTV, no DSV (no hasDepth)\n");
      commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    }
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
          IGL_LOG_INFO("RenderCommandEncoder::endEncoding - Resolving MSAA color attachment %zu (%u samples -> 1 sample)\n",
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

          IGL_LOG_INFO("RenderCommandEncoder::endEncoding - MSAA color resolve completed for attachment %zu\n", i);

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
        IGL_LOG_INFO("RenderCommandEncoder::endEncoding - Resolving MSAA depth attachment (%u samples -> 1 sample)\n",
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

        IGL_LOG_INFO("RenderCommandEncoder::endEncoding - MSAA depth resolve completed\n");

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

  // Set primitive topology from the pipeline state
  D3D_PRIMITIVE_TOPOLOGY topology = d3dPipelineState->getPrimitiveTopology();
  IGL_LOG_INFO("bindRenderPipelineState: Setting topology=%d\n", (int)topology);
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
  static int callCount = 0;
  if (callCount < 3) {
    IGL_LOG_INFO("bindVertexBuffer called #%d: index=%u\n", ++callCount, index);
  }
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
  static int callCount = 0;
  if (callCount < 3) {
    IGL_LOG_INFO("bindIndexBuffer called #%d\n", ++callCount);
  }
  auto* d3dBuffer = static_cast<Buffer*>(&buffer);
  cachedIndexBuffer_.bufferLocation = d3dBuffer->gpuAddress(bufferOffset);
  cachedIndexBuffer_.sizeInBytes = static_cast<UINT>(d3dBuffer->getSizeInBytes() - bufferOffset);
  // D3D12 only supports 16-bit and 32-bit index formats (not 8-bit)
  cachedIndexBuffer_.format = (format == IndexFormat::UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
  cachedIndexBuffer_.bound = true;
}

void RenderCommandEncoder::bindBytes(size_t index,
                                     uint8_t /*target*/,
                                     const void* data,
                                     size_t length) {
  if (!commandList_ || !data || length == 0) {
    IGL_LOG_ERROR("bindBytes: Invalid parameters (list=%p, data=%p, len=%zu)\n",
                  commandList_, data, length);
    return;
  }

  // bindBytes binds small constant data as a constant buffer
  // We use a dynamic CBV approach: allocate temp buffer from upload heap and bind as root CBV

  // Validate index range - we support binding to b0 and b1 via root parameters 1 and 2
  if (index >= 2) {
    IGL_LOG_ERROR("bindBytes: index %zu exceeds supported range [0,1] for root CBV binding\n", index);
    return;
  }

  // D3D12 requires constant buffer addresses to be 256-byte aligned
  constexpr size_t kCBVAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT; // 256 bytes
  const size_t alignedSize = (length + kCBVAlignment - 1) & ~(kCBVAlignment - 1);

  // Create transient upload buffer for this data
  // The buffer will be tracked by the frame context and kept alive until GPU finishes
  auto& context = commandBuffer_.getContext();
  auto* device = context.getDevice();
  if (!device) {
    IGL_LOG_ERROR("bindBytes: D3D12 device is null\n");
    return;
  }

  // Create upload heap buffer
  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  D3D12_RESOURCE_DESC bufferDesc = {};
  bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  bufferDesc.Alignment = 0;
  bufferDesc.Width = alignedSize;
  bufferDesc.Height = 1;
  bufferDesc.DepthOrArraySize = 1;
  bufferDesc.MipLevels = 1;
  bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
  bufferDesc.SampleDesc.Count = 1;
  bufferDesc.SampleDesc.Quality = 0;
  bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
  HRESULT hr = device->CreateCommittedResource(
      &heapProps,
      D3D12_HEAP_FLAG_NONE,
      &bufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(uploadBuffer.GetAddressOf())
  );

  if (FAILED(hr)) {
    IGL_LOG_ERROR("bindBytes: Failed to create upload buffer: HRESULT = 0x%08X\n", static_cast<unsigned>(hr));
    return;
  }

  // Map and copy data
  void* mappedPtr = nullptr;
  hr = uploadBuffer->Map(0, nullptr, &mappedPtr);
  if (FAILED(hr) || !mappedPtr) {
    IGL_LOG_ERROR("bindBytes: Failed to map upload buffer: HRESULT = 0x%08X\n", static_cast<unsigned>(hr));
    return;
  }

  memcpy(mappedPtr, data, length);
  uploadBuffer->Unmap(0, nullptr);

  // Get GPU virtual address
  D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = uploadBuffer->GetGPUVirtualAddress();

  // Bind as root CBV
  // Root parameter 1 = b0, Root parameter 2 = b1
  const uint32_t rootParameterIndex = static_cast<uint32_t>(index + 1);
  commandList_->SetGraphicsRootConstantBufferView(rootParameterIndex, gpuAddress);

  // Track in cached state for draw calls
  cachedConstantBuffers_[index] = gpuAddress;
  constantBufferBound_[index] = true;

  // Track transient resource to keep it alive until GPU finishes
  commandBuffer_.trackTransientResource(uploadBuffer.Get());

  IGL_LOG_INFO("bindBytes: Bound %zu bytes (aligned to %zu) to b%zu (root param %u, GPU address 0x%llx)\n",
               length, alignedSize, index, rootParameterIndex, gpuAddress);
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

  IGL_LOG_INFO("bindPushConstants: Set %u DWORDs (%zu bytes) at offset %zu to root parameter 0 (b2)\n",
               num32BitValues, length, offset);
}
void RenderCommandEncoder::bindSamplerState(size_t index,
                                            uint8_t /*target*/,
                                            ISamplerState* samplerState) {
  static int callCount = 0;
  if (callCount < 3) {
    IGL_LOG_INFO("bindSamplerState called #%d: index=%zu, sampler=%p\n", ++callCount, index, samplerState);
  }
  // Validate index range against IGL_TEXTURE_SAMPLERS_MAX
  if (!samplerState || index >= IGL_TEXTURE_SAMPLERS_MAX) {
    if (index >= IGL_TEXTURE_SAMPLERS_MAX) {
      IGL_LOG_ERROR("bindSamplerState: index %zu exceeds maximum %u\n", index, IGL_TEXTURE_SAMPLERS_MAX);
    }
    return;
  }

  auto& context = commandBuffer_.getContext();
  auto* device = context.getDevice();
  if (!device || samplerHeap_ == nullptr) {
    IGL_LOG_ERROR("bindSamplerState: missing device or per-frame sampler heap\n");
    return;
  }

  // Allocate a unique descriptor slot from the command buffer's shared counter
  // This prevents descriptor conflicts between render passes (e.g., cubes pass + ImGui pass)
  const uint32_t descriptorIndex = commandBuffer_.getNextSamplerDescriptor()++;
  IGL_LOG_INFO("bindSamplerState: allocated descriptor slot %u for sampler index s%zu\n", descriptorIndex, index);

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
  // ALWAYS use per-frame heaps from D3D12Context
  // CRITICAL: Do NOT use heapMgr handles - they point to shared heap, not per-frame heap
  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getSamplerCpuHandle(descriptorIndex);
  D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getSamplerGpuHandle(descriptorIndex);

  IGL_LOG_INFO("bindSamplerState: creating sampler at slot %u, CPU handle 0x%llx\n", descriptorIndex, cpuHandle.ptr);
  // Pre-creation validation (TASK_P0_DX12-004)
  IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateSampler");
  IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "Sampler descriptor handle is invalid");

  device->CreateSampler(&samplerDesc, cpuHandle);
  D3D12Context::trackResourceCreation("Sampler", 0);

  // Cache sampler GPU handle for this index - store in array indexed by sampler unit
  cachedSamplerGpuHandles_[index] = gpuHandle;
  constexpr size_t kMaxCachedSamplers =
      sizeof(cachedSamplerGpuHandles_) / sizeof(cachedSamplerGpuHandles_[0]);
  for (size_t i = index + 1; i < kMaxCachedSamplers; ++i) {
    cachedSamplerGpuHandles_[i] = {};
  }
  cachedSamplerCount_ = index + 1;
  cachedSamplerGpuHandle_ =
      (cachedSamplerCount_ > 0) ? cachedSamplerGpuHandles_[0] : D3D12_GPU_DESCRIPTOR_HANDLE{};
  IGL_LOG_INFO("bindSamplerState: cached sampler[%zu] GPU handle 0x%llx\n", index, cachedSamplerGpuHandles_[index].ptr);
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
  // Validate index range against IGL_TEXTURE_SAMPLERS_MAX
  if (!texture || index >= IGL_TEXTURE_SAMPLERS_MAX) {
    if (index >= IGL_TEXTURE_SAMPLERS_MAX) {
      IGL_LOG_ERROR("bindTexture: index %zu exceeds maximum %u\n", index, IGL_TEXTURE_SAMPLERS_MAX);
    }
    return;
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

  // Ensure resource is in PIXEL_SHADER_RESOURCE state for sampling
  d3dTexture->transitionAll(commandList_, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

  // Allocate descriptor from per-frame heap (descriptors are recreated each frame)
  // CRITICAL: Do NOT cache descriptor indices globally - per-frame heaps reset counters to 0 each frame
  // C-001: Now uses Result-based allocation with dynamic heap growth
  uint32_t descriptorIndex = 0;
  Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
  if (!allocResult.isOk()) {
    IGL_LOG_ERROR("bindTexture: Failed to allocate descriptor: %s\n", allocResult.message.c_str());
    return;
  }
  IGL_LOG_INFO("bindTexture: allocated descriptor slot %u for texture index t%zu\n", descriptorIndex, index);

  // Create SRV descriptor at the allocated slot
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Format =
      textureFormatToDXGIShaderResourceViewFormat(d3dTexture->getFormat());
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

  // Set view dimension based on texture type
  // CRITICAL: Must query actual resource dimension to avoid CreateShaderResourceView errors
  auto resourceDesc = resource->GetDesc();
  IGL_LOG_INFO("bindTexture: resource dimension=%d, texture type=%d\n", resourceDesc.Dimension, (int)d3dTexture->getType());

  // Check if this is a texture view and use view parameters for SRV
  const bool isView = d3dTexture->isView();
  const uint32_t mostDetailedMip = isView ? d3dTexture->getMipLevelOffset() : 0;
  const uint32_t mipLevels = isView ? d3dTexture->getNumMipLevelsInView() : d3dTexture->getNumMipLevels();

  if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    srvDesc.Texture3D.MipLevels = mipLevels;
    srvDesc.Texture3D.MostDetailedMip = mostDetailedMip;
    IGL_LOG_INFO("bindTexture: using TEXTURE3D view dimension (mip offset=%u, mip count=%u)\n", mostDetailedMip, mipLevels);
  } else if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
    const auto textureType = d3dTexture->getType();
    // Check if this is a texture array (either a view or a regular array texture)
    const bool isArrayTexture = (isView && d3dTexture->getNumArraySlicesInView() > 0) ||
                                 (!isView && resourceDesc.DepthOrArraySize > 1);

    if (textureType == TextureType::Cube) {
      // Cube textures are stored as 2D array resources with six faces per cube.
      // References: Texture Subresources (D3D12), D3D12_TEXCUBE_SRV, D3D12_TEX2D_ARRAY_SRV docs.
      const uint32_t firstArraySlice = isView ? d3dTexture->getArraySliceOffset() : 0;
      const uint32_t facesInView =
          isView ? d3dTexture->getNumArraySlicesInView() : resourceDesc.DepthOrArraySize;
      const bool isCubeArray = (firstArraySlice != 0) || (facesInView > 6);

      if (isCubeArray) {
        const uint32_t numCubes = std::max(1u, facesInView / 6);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
        srvDesc.TextureCubeArray.MostDetailedMip = mostDetailedMip;
        srvDesc.TextureCubeArray.MipLevels = mipLevels;
        srvDesc.TextureCubeArray.First2DArrayFace = firstArraySlice;
        srvDesc.TextureCubeArray.NumCubes = numCubes;
        srvDesc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
        IGL_LOG_INFO(
            "bindTexture: using TEXTURECUBE_ARRAY view dimension (mip offset=%u, mip count=%u, first face=%u, cube count=%u)\n",
            mostDetailedMip,
            mipLevels,
            srvDesc.TextureCubeArray.First2DArrayFace,
            srvDesc.TextureCubeArray.NumCubes);
      } else {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = mostDetailedMip;
        srvDesc.TextureCube.MipLevels = mipLevels;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        IGL_LOG_INFO(
            "bindTexture: using TEXTURECUBE view dimension (mip offset=%u, mip count=%u)\n",
            mostDetailedMip,
            mipLevels);
      }
    } else if (isArrayTexture) {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
      srvDesc.Texture2DArray.MipLevels = mipLevels;
      srvDesc.Texture2DArray.MostDetailedMip = mostDetailedMip;
      srvDesc.Texture2DArray.PlaneSlice = 0;
      srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;

      if (isView) {
        // For texture views, use view-specific array parameters
        srvDesc.Texture2DArray.FirstArraySlice = d3dTexture->getArraySliceOffset();
        srvDesc.Texture2DArray.ArraySize = d3dTexture->getNumArraySlicesInView();
      } else {
        // For regular array textures, use full array size
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.ArraySize = resourceDesc.DepthOrArraySize;
      }

      IGL_LOG_INFO(
          "bindTexture: using TEXTURE2DARRAY view dimension (mip offset=%u, mip count=%u, array offset=%u, array count=%u)\n",
          mostDetailedMip,
          mipLevels,
          srvDesc.Texture2DArray.FirstArraySlice,
          srvDesc.Texture2DArray.ArraySize);
    } else {
      // Default to 2D textures
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MipLevels = mipLevels;
      srvDesc.Texture2D.MostDetailedMip = mostDetailedMip;
      IGL_LOG_INFO("bindTexture: using TEXTURE2D view dimension (mip offset=%u, mip count=%u)\n",
                   mostDetailedMip,
                   mipLevels);
    }
  } else {
    IGL_LOG_ERROR("bindTexture: unsupported resource dimension %d\n", resourceDesc.Dimension);
    return;
  }

  // Verify we're using the right heap - MUST use per-frame heap from constructor
  auto* heap = cbvSrvUavHeap_;  // Use per-frame heap, NOT heapMgr->getCbvSrvUavHeap()
  D3D12_CPU_DESCRIPTOR_HANDLE heapStartCpu = heap->GetCPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE heapStartGpu = heap->GetGPUDescriptorHandleForHeapStart();

  // Get descriptor handles: ALWAYS use per-frame heaps from D3D12Context
  // CRITICAL: Do NOT use heapMgr handles - they point to shared heap, not per-frame heap
  D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
  D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);

  IGL_LOG_INFO("bindTexture: heap=%p, heapStart CPU=0x%llx, GPU=0x%llx\n", heap, heapStartCpu.ptr, heapStartGpu.ptr);

  // Always create descriptor (per-frame heaps require recreation each frame)
  IGL_LOG_INFO("bindTexture: creating SRV at slot %u, CPU handle 0x%llx\n", descriptorIndex, cpuHandle.ptr);
  // Pre-creation validation (TASK_P0_DX12-004)
  IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateShaderResourceView");
  IGL_DEBUG_ASSERT(resource != nullptr, "Texture resource is null before CreateShaderResourceView");
  IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "SRV descriptor handle is invalid");

  device->CreateShaderResourceView(resource, &srvDesc, cpuHandle);
  D3D12Context::trackResourceCreation("SRV", 0);

  // Cache texture GPU handle for this index - store in array indexed by texture unit
  cachedTextureGpuHandles_[index] = gpuHandle;
  constexpr size_t kMaxCachedTextures =
      sizeof(cachedTextureGpuHandles_) / sizeof(cachedTextureGpuHandles_[0]);
  for (size_t i = index + 1; i < kMaxCachedTextures; ++i) {
    cachedTextureGpuHandles_[i] = {};
  }
  cachedTextureCount_ = index + 1;
  cachedTextureGpuHandle_ =
      (cachedTextureCount_ > 0) ? cachedTextureGpuHandles_[0] : D3D12_GPU_DESCRIPTOR_HANDLE{};
  IGL_LOG_INFO("bindTexture: cached texture[%zu] GPU handle 0x%llx (offset from heap start: %lld)\n",
               index, cachedTextureGpuHandles_[index].ptr,
               (int64_t)(cachedTextureGpuHandles_[index].ptr - heapStartGpu.ptr));

  IGL_LOG_INFO("bindTexture: done\n");
}
void RenderCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {}

void RenderCommandEncoder::draw(size_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t baseInstance) {
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
    IGL_LOG_INFO("draw: binding root CBV for b0 (address 0x%llx)\n", cachedConstantBuffers_[0]);
  }
  if (constantBufferBound_[1]) {
    commandList_->SetGraphicsRootConstantBufferView(2, cachedConstantBuffers_[1]);  // Root param 2: b1
    IGL_LOG_INFO("draw: binding root CBV for b1 (address 0x%llx)\n", cachedConstantBuffers_[1]);
  }

  // Bind CBV descriptor table for b3-b15 (bindBindGroup support)
  if (cbvTableCount_ > 0 && cachedCbvTableGpuHandles_[0].ptr != 0) {
    commandList_->SetGraphicsRootDescriptorTable(3, cachedCbvTableGpuHandles_[0]);  // Root param 3: CBV table (b3-b15)
    IGL_LOG_INFO("draw: binding CBV descriptor table with %zu descriptors (GPU handle 0x%llx)\n",
                 cbvTableCount_, cachedCbvTableGpuHandles_[0].ptr);
  }

  // Bind SRV and sampler descriptor tables
  if (cachedTextureCount_ > 0 && cachedTextureGpuHandles_[0].ptr != 0) {
    commandList_->SetGraphicsRootDescriptorTable(4, cachedTextureGpuHandles_[0]);  // Root param 4: SRV table
  }
  if (cachedSamplerCount_ > 0 && cachedSamplerGpuHandles_[0].ptr != 0) {
    commandList_->SetGraphicsRootDescriptorTable(5, cachedSamplerGpuHandles_[0]);  // Root param 5: Sampler table
  }

  // Apply vertex buffers
  for (uint32_t i = 0; i < IGL_BUFFER_BINDINGS_MAX; ++i) {
    if (cachedVertexBuffers_[i].bound) {
      UINT stride = vertexStrides_[i];
      if (stride == 0) stride = currentVertexStride_ ? currentVertexStride_ : 32;
      D3D12_VERTEX_BUFFER_VIEW vbView = {};
      vbView.BufferLocation = cachedVertexBuffers_[i].bufferLocation;
      vbView.SizeInBytes = cachedVertexBuffers_[i].sizeInBytes;
      vbView.StrideInBytes = stride;
      IGL_LOG_INFO("draw: VB[%u] = GPU 0x%llx, size=%u, stride=%u\n", i, vbView.BufferLocation, vbView.SizeInBytes, vbView.StrideInBytes);
      commandList_->IASetVertexBuffers(i, 1, &vbView);
    }
  }

  commandBuffer_.incrementDrawCount();

  IGL_LOG_INFO("draw: DrawInstanced(vertexCount=%zu, instanceCount=%u, firstVertex=%u, baseInstance=%u)\n", vertexCount, instanceCount, firstVertex, baseInstance);
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

  // Bind SRV and sampler descriptor tables
  if (cachedTextureCount_ > 0 && cachedTextureGpuHandles_[0].ptr != 0) {
    commandList_->SetGraphicsRootDescriptorTable(4, cachedTextureGpuHandles_[0]);  // Root param 4: SRV table
  }
  if (cachedSamplerCount_ > 0 && cachedSamplerGpuHandles_[0].ptr != 0) {
    commandList_->SetGraphicsRootDescriptorTable(5, cachedSamplerGpuHandles_[0]);  // Root param 5: Sampler table
  }

  // Apply cached vertex buffer bindings now that pipeline state is bound
  for (uint32_t i = 0; i < IGL_BUFFER_BINDINGS_MAX; ++i) {
    if (cachedVertexBuffers_[i].bound) {
      UINT stride = vertexStrides_[i];
      if (stride == 0) stride = currentVertexStride_ ? currentVertexStride_ : 32;
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
      Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
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

  IGL_LOG_INFO("RenderCommandEncoder::multiDrawIndirect: Executed %u indirect draws (stride: %u)\n",
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

  IGL_LOG_INFO("RenderCommandEncoder::multiDrawIndexedIndirect: Executed %u indirect indexed draws (stride: %u)\n",
               drawCount, actualStride);
}

void RenderCommandEncoder::setStencilReferenceValue(uint32_t value) {
  if (!commandList_) {
    return;
  }
  // Set stencil reference value for stencil testing
  commandList_->OMSetStencilRef(value);
  IGL_LOG_INFO("setStencilReferenceValue: Set stencil ref to %u\n", value);
}

void RenderCommandEncoder::setBlendColor(const Color& color) {
  if (!commandList_) {
    return;
  }
  // Set blend factor constants for BlendFactor::BlendColor operations
  // D3D12 uses RGBA float array, matching IGL Color structure
  const float blendFactor[4] = {color.r, color.g, color.b, color.a};
  commandList_->OMSetBlendFactor(blendFactor);
  IGL_LOG_INFO("setBlendColor: Set blend factor to (%.2f, %.2f, %.2f, %.2f)\n",
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
  static int callCount = 0;
  if (callCount < 5) {
    IGL_LOG_INFO("bindBuffer START #%d: index=%u\n", callCount + 1, index);
  }
  if (!buffer) {
    IGL_LOG_INFO("bindBuffer: null buffer, returning\n");
    return;
  }

  auto* d3dBuffer = static_cast<Buffer*>(buffer);

  // Check if this is a storage buffer - needs SRV binding for shader reads
  const bool isStorageBuffer = (d3dBuffer->getBufferType() & BufferDesc::BufferTypeBits::Storage) != 0;

  if (isStorageBuffer) {
    // Storage buffer - create SRV for ByteAddressBuffer reads in pixel shader
    IGL_LOG_INFO("bindBuffer: Storage buffer detected at index %u - creating SRV for pixel shader read\n", index);

    auto& context = commandBuffer_.getContext();
    auto* device = context.getDevice();
    if (!device || cbvSrvUavHeap_ == nullptr) {
      IGL_LOG_ERROR("bindBuffer: Missing device or per-frame CBV/SRV/UAV heap\n");
      return;
    }

    // Allocate descriptor slot from command buffer's shared counter
    // C-001: Now uses Result-based allocation with dynamic heap growth
    uint32_t descriptorIndex = 0;
    Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
    if (!allocResult.isOk()) {
      IGL_LOG_ERROR("bindBuffer: Failed to allocate descriptor: %s\n", allocResult.message.c_str());
      return;
    }
    IGL_LOG_INFO("bindBuffer: Allocated SRV descriptor slot %u for buffer at t%u\n", descriptorIndex, index);

    // Create SRV descriptor for ByteAddressBuffer
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;  // Raw buffer (ByteAddressBuffer)
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = static_cast<UINT64>(offset) / 4;  // Offset in 32-bit elements
    srvDesc.Buffer.NumElements = static_cast<UINT>(buffer->getSizeInBytes() / 4);  // Size in 32-bit elements
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;  // Raw buffer access

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);
    // Pre-creation validation (TASK_P0_DX12-004)
    IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateShaderResourceView");
    IGL_DEBUG_ASSERT(d3dBuffer->getResource() != nullptr, "Buffer resource is null");
    IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "SRV descriptor handle is invalid");

    device->CreateShaderResourceView(d3dBuffer->getResource(), &srvDesc, cpuHandle);

    IGL_LOG_INFO("bindBuffer: Created SRV at descriptor slot %u (FirstElement=%llu, NumElements=%u)\n",
                 descriptorIndex, srvDesc.Buffer.FirstElement, srvDesc.Buffer.NumElements);

    // Cache GPU handle for descriptor table binding in draw calls
    // SRVs are bound to root parameter 3 (render root signature)
    cachedTextureGpuHandles_[index] = gpuHandle;
    cachedTextureCount_ = std::max(cachedTextureCount_, static_cast<size_t>(index + 1));

    IGL_LOG_INFO("bindBuffer: Storage buffer SRV binding complete\n");

    // CRITICAL: Track the Buffer OBJECT (not just resource) to keep it alive until GPU finishes
    // This prevents the Buffer destructor from releasing the resource while GPU commands reference it
    try {
      std::shared_ptr<IBuffer> sharedBuffer = d3dBuffer->shared_from_this();
      static_cast<CommandBuffer&>(commandBuffer_).trackTransientBuffer(std::move(sharedBuffer));
      IGL_LOG_INFO("bindBuffer: Tracking Buffer object (shared_ptr) for lifetime management\n");
    } catch (const std::bad_weak_ptr&) {
      // Buffer not managed by shared_ptr (e.g., persistent buffer from member variable)
      // Fall back to tracking just the resource (AddRef on ID3D12Resource)
      static_cast<CommandBuffer&>(commandBuffer_).trackTransientResource(d3dBuffer->getResource());
      IGL_LOG_INFO("bindBuffer: Buffer not shared_ptr-managed, tracking resource only\n");
    }
  } else {
    // Constant buffer - use CBV binding (existing path)
    IGL_LOG_INFO("bindBuffer: Constant buffer at index %u\n", index);

    // D3D12 requires constant buffer addresses to be 256-byte aligned
    // (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
    if ((offset & 255) != 0) {
      IGL_LOG_ERROR("bindBuffer: ERROR - CBV offset %zu is not 256-byte aligned (required by D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT). "
                    "Constant buffers must be created at aligned offsets. Ignoring bind request.\n", offset);
      return;
    }

    D3D12_GPU_VIRTUAL_ADDRESS bufferAddress = d3dBuffer->gpuAddress(offset);
    IGL_LOG_INFO("bindBuffer: CBV address=0x%llx\n", bufferAddress);

    if (!commandList_) {
      IGL_LOG_ERROR("bindBuffer: commandList_ is NULL!\n");
      return;
    }

    // CRITICAL: Track the Buffer OBJECT (not just resource) to keep it alive until GPU finishes
    // This prevents the Buffer destructor from releasing the resource while GPU commands reference it
    try {
      std::shared_ptr<IBuffer> sharedBuffer = d3dBuffer->shared_from_this();
      static_cast<CommandBuffer&>(commandBuffer_).trackTransientBuffer(std::move(sharedBuffer));
      IGL_LOG_INFO("bindBuffer: Tracking Buffer object (shared_ptr) for lifetime management\n");
    } catch (const std::bad_weak_ptr&) {
      // Buffer not managed by shared_ptr (e.g., persistent buffer from member variable)
      // Fall back to tracking just the resource (AddRef on ID3D12Resource)
      static_cast<CommandBuffer&>(commandBuffer_).trackTransientResource(d3dBuffer->getResource());
      IGL_LOG_INFO("bindBuffer: Buffer not shared_ptr-managed, tracking resource only\n");
    }

    // Cache CBV for draw calls (b0, b1) - bind to root CBVs
    if (index <= 1) {
      cachedConstantBuffers_[index] = bufferAddress;
      constantBufferBound_[index] = true;
      IGL_LOG_INFO("bindBuffer: Cached CBV at index %u with address 0x%llx\n", index, bufferAddress);
    }
  }

  if (callCount < 5) {
    IGL_LOG_INFO("bindBuffer END #%d\n", ++callCount);
  }
}
void RenderCommandEncoder::bindBindGroup(BindGroupTextureHandle handle) {
  IGL_LOG_INFO("bindBindGroup(texture): handle valid=%d\n", !handle.empty());

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
  // C-001: Allocate descriptors one at a time (they may span pages)
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

    // C-001: Use individually allocated descriptor indices (may span multiple pages)
    const uint32_t dstIndex = srvIndices[i];
    D3D12_CPU_DESCRIPTOR_HANDLE dstCpu = context.getCbvSrvUavCpuHandle(dstIndex);
    // Pre-creation validation (TASK_P0_DX12-004)
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
    // Pre-creation validation (TASK_P0_DX12-004)
    IGL_DEBUG_ASSERT(d3dDevice != nullptr, "Device is null before CreateSampler");
    IGL_DEBUG_ASSERT(dstCpu.ptr != 0, "Sampler descriptor handle is invalid");

    d3dDevice->CreateSampler(&samplerDesc, dstCpu);
    D3D12Context::trackResourceCreation("Sampler", 0);
  }

  // Cache base GPU handles so draw() binds once per table
  // C-001: Use first allocated descriptor index (descriptors may span pages)
  if (texCount > 0 && !srvIndices.empty()) {
    cachedTextureGpuHandles_[0] = context.getCbvSrvUavGpuHandle(srvIndices[0]);
    cachedTextureCount_ = texCount;
  }
  if (smpCount > 0) {
    cachedSamplerGpuHandles_[0] = context.getSamplerGpuHandle(smpBaseIndex);
    cachedSamplerCount_ = smpCount;
  }
}

void RenderCommandEncoder::bindBindGroup(BindGroupBufferHandle handle,
                                          uint32_t numDynamicOffsets,
                                          const uint32_t* dynamicOffsets) {
  IGL_LOG_INFO("bindBindGroup(buffer): handle valid=%d, dynCount=%u\n", !handle.empty(), numDynamicOffsets);

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
        // C-001: Now uses Result-based allocation with dynamic heap growth
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

        // Create CBV descriptor
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = addr;
        cbvDesc.SizeInBytes = static_cast<UINT>((buf->getSizeInBytes() + 255) & ~255);  // Must be 256-byte aligned

        // Pre-creation validation (TASK_P0_DX12-004)
        IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateConstantBufferView");
        IGL_DEBUG_ASSERT(addr != 0, "Buffer GPU address is null");
        IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "CBV descriptor handle is invalid");

        device->CreateConstantBufferView(&cbvDesc, cpuHandle);

        // Cache the GPU handle for the descriptor table
        // BindGroupBufferDesc index 0 -> table index 0 (b3), index 1 -> table index 1 (b4), etc.
        cachedCbvTableGpuHandles_[tableIndex] = gpuHandle;
        cbvTableBound_[tableIndex] = true;
        cbvTableCount_ = std::max(cbvTableCount_, static_cast<size_t>(tableIndex + 1));

        IGL_LOG_INFO("bindBindGroup(buffer): bound uniform buffer at BindGroupBufferDesc[%u] to b%u (descriptor table index %u, GPU handle 0x%llx)\n",
                     slot, slot + 3, tableIndex, gpuHandle.ptr);
      } else {
        IGL_LOG_ERROR("bindBindGroup(buffer): BindGroupBufferDesc slot %u exceeds maximum (%u)\n", slot, IGL_BUFFER_BINDINGS_MAX);
      }
    } else if (isStorage) {
      // P1_DX12-FIND-05: Implement storage buffer binding via UAV/SRV descriptors
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
        // C-001: Now uses Result-based allocation with dynamic heap growth
        uint32_t descriptorIndex = 0;
        Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
        if (!allocResult.isOk()) {
          IGL_LOG_ERROR("bindBindGroup(buffer): Failed to allocate UAV descriptor: %s\n", allocResult.message.c_str());
          continue;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);

        // Create UAV descriptor for structured buffer
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = static_cast<UINT64>(baseOffset / 4);  // Assume 4-byte stride for now
        uavDesc.Buffer.NumElements = static_cast<UINT>(buf->getSizeInBytes() / 4);
        uavDesc.Buffer.StructureByteStride = 4;  // TODO: Get from buffer desc if available
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        // Pre-creation validation (TASK_P0_DX12-004)
        IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateUnorderedAccessView");
        IGL_DEBUG_ASSERT(resource != nullptr, "Buffer resource is null");
        IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "UAV descriptor handle is invalid");

        device->CreateUnorderedAccessView(resource, nullptr, &uavDesc, cpuHandle);

        // Bind UAV descriptor table (Parameter 6 in graphics root signature)
        commandList_->SetGraphicsRootDescriptorTable(6, gpuHandle);

        IGL_LOG_INFO("bindBindGroup(buffer): bound read-write storage buffer at slot %u (UAV u%u, GPU handle 0x%llx)\n",
                     slot, slot, gpuHandle.ptr);
      } else {
        // Create SRV for read-only storage buffer
        // C-001: Now uses Result-based allocation with dynamic heap growth
        uint32_t descriptorIndex = 0;
        Result allocResult = commandBuffer_.getNextCbvSrvUavDescriptor(&descriptorIndex);
        if (!allocResult.isOk()) {
          IGL_LOG_ERROR("bindBindGroup(buffer): Failed to allocate SRV descriptor: %s\n", allocResult.message.c_str());
          continue;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = context.getCbvSrvUavCpuHandle(descriptorIndex);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = context.getCbvSrvUavGpuHandle(descriptorIndex);

        // Create SRV descriptor for structured buffer
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = static_cast<UINT64>(baseOffset / 4);  // Assume 4-byte stride for now
        srvDesc.Buffer.NumElements = static_cast<UINT>(buf->getSizeInBytes() / 4);
        srvDesc.Buffer.StructureByteStride = 4;  // TODO: Get from buffer desc if available
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        // Pre-creation validation (TASK_P0_DX12-004)
        IGL_DEBUG_ASSERT(device != nullptr, "Device is null before CreateShaderResourceView");
        IGL_DEBUG_ASSERT(resource != nullptr, "Buffer resource is null");
        IGL_DEBUG_ASSERT(cpuHandle.ptr != 0, "SRV descriptor handle is invalid");

        device->CreateShaderResourceView(resource, &srvDesc, cpuHandle);

        // Bind SRV descriptor table (Parameter 4 in graphics root signature)
        // Note: This shares the texture SRV table, storage buffers and textures will be bound together
        commandList_->SetGraphicsRootDescriptorTable(4, gpuHandle);

        IGL_LOG_INFO("bindBindGroup(buffer): bound read-only storage buffer at slot %u (SRV t%u, GPU handle 0x%llx)\n",
                     slot, slot, gpuHandle.ptr);
      }
    }
  }
}

} // namespace igl::d3d12
