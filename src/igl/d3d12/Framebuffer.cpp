/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <chrono>
#include <igl/d3d12/Framebuffer.h>
#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/CommandQueue.h>
#include <igl/d3d12/Device.h>
#include <igl/d3d12/Texture.h>
#include <cstring>

namespace igl::d3d12 {

Framebuffer::Framebuffer(const FramebufferDesc& desc) : desc_(desc) {}

Framebuffer::~Framebuffer() {
  for (auto& cache : readbackCache_) {
    if (cache.fenceEvent) {
      CloseHandle(cache.fenceEvent);
      cache.fenceEvent = nullptr;
    }
  }
}

std::vector<size_t> Framebuffer::getColorAttachmentIndices() const {
  std::vector<size_t> indices;
  for (size_t i = 0; i < IGL_COLOR_ATTACHMENTS_MAX; ++i) {
    if (desc_.colorAttachments[i].texture) {
      indices.push_back(i);
    }
  }
  return indices;
}

std::shared_ptr<ITexture> Framebuffer::getColorAttachment(size_t index) const {
  if (index < IGL_COLOR_ATTACHMENTS_MAX) {
    return desc_.colorAttachments[index].texture;
  }
  return nullptr;
}

std::shared_ptr<ITexture> Framebuffer::getResolveColorAttachment(size_t index) const {
  if (index < IGL_COLOR_ATTACHMENTS_MAX) {
    return desc_.colorAttachments[index].resolveTexture;
  }
  return nullptr;
}

std::shared_ptr<ITexture> Framebuffer::getDepthAttachment() const {
  return desc_.depthAttachment.texture;
}

std::shared_ptr<ITexture> Framebuffer::getResolveDepthAttachment() const {
  return desc_.depthAttachment.resolveTexture;
}

std::shared_ptr<ITexture> Framebuffer::getStencilAttachment() const {
  return desc_.stencilAttachment.texture;
}

FramebufferMode Framebuffer::getMode() const {
  return desc_.mode;
}

bool Framebuffer::isSwapchainBound() const {
  return false;
}

void Framebuffer::copyBytesColorAttachment(ICommandQueue& cmdQueue,
                                           size_t index,
                                           void* pixelBytes,
                                           const TextureRangeDesc& range,
                                           size_t bytesPerRow) const {
  if (!pixelBytes || index >= IGL_COLOR_ATTACHMENTS_MAX) {
    return;
  }

  auto* d3dQueueWrapper = dynamic_cast<CommandQueue*>(&cmdQueue);
  if (!d3dQueueWrapper) {
    return;
  }

  auto& ctx = d3dQueueWrapper->getDevice().getD3D12Context();
  auto* device = ctx.getDevice();
  auto* d3dQueue = ctx.getCommandQueue();
  if (!device || !d3dQueue) {
    return;
  }

  auto srcTex = std::static_pointer_cast<Texture>(desc_.colorAttachments[index].texture);
  if (!srcTex) {
    return;
  }

  ID3D12Resource* srcRes = srcTex->getResource();
  if (!srcRes) {
    return;
  }

  const uint32_t mipLevel = range.mipLevel;
  const uint32_t copyLayer = (srcTex->getType() == TextureType::Cube) ? range.face : range.layer;
  const uint32_t subresourceIndex = srcTex->calcSubresourceIndex(mipLevel, copyLayer);

  const auto texDims = srcTex->getDimensions();
  const uint32_t mipWidth = std::max<uint32_t>(1u, texDims.width >> mipLevel);
  const uint32_t mipHeight = std::max<uint32_t>(1u, texDims.height >> mipLevel);

  const UINT64 frameFenceValue = ctx.getFenceValue();

  auto& cache = readbackCache_[index];

  const auto fmtProps = TextureFormatProperties::fromTextureFormat(srcTex->getFormat());
  const size_t bytesPerPixel = std::max<size_t>(fmtProps.bytesPerBlock, 1);
  const size_t fullRowBytes = static_cast<size_t>(mipWidth) * bytesPerPixel;

  bool cacheUpToDate = cache.cacheValid &&
                       cache.cachedFrameFenceValue == frameFenceValue &&
                       cache.cachedMipLevel == mipLevel &&
                       cache.cachedLayer == copyLayer &&
                       cache.cachedWidth == mipWidth &&
                       cache.cachedHeight == mipHeight &&
                       cache.cachedBytesPerPixel == bytesPerPixel;

  if (!cacheUpToDate) {
    const auto refreshStart = std::chrono::high_resolution_clock::now();
    D3D12_RESOURCE_DESC srcDesc = srcRes->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT numRows = 0;
    UINT64 rowSizeInBytes = 0;
    UINT64 totalBytes = 0;
    device->GetCopyableFootprints(
        &srcDesc, subresourceIndex, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

    if (totalBytes == 0) {
      return;
    }

    if (cache.allocator.Get() == nullptr) {
      if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                IID_PPV_ARGS(cache.allocator.GetAddressOf())))) {
        return;
      }
    }

    if (cache.commandList.Get() == nullptr) {
      if (FAILED(device->CreateCommandList(0,
                                           D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           cache.allocator.Get(),
                                           nullptr,
                                           IID_PPV_ARGS(cache.commandList.GetAddressOf())))) {
        return;
      }
      if (FAILED(cache.commandList->Close())) {
        cache.commandList.Reset();
        return;
      }
    }

    if (cache.fence.Get() == nullptr) {
      if (FAILED(device->CreateFence(0,
                                     D3D12_FENCE_FLAG_NONE,
                                     IID_PPV_ARGS(cache.fence.GetAddressOf())))) {
        return;
      }
    }

    if (!cache.fenceEvent) {
      cache.fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
      if (!cache.fenceEvent) {
        return;
      }
    }

    if (cache.readbackBuffer.Get() == nullptr || cache.readbackBufferSize < totalBytes) {
      D3D12_HEAP_PROPERTIES readbackHeap{};
      readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
      D3D12_RESOURCE_DESC readbackDesc{};
      readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      readbackDesc.Width = totalBytes;
      readbackDesc.Height = 1;
      readbackDesc.DepthOrArraySize = 1;
      readbackDesc.MipLevels = 1;
      readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
      readbackDesc.SampleDesc.Count = 1;
      readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

      Microsoft::WRL::ComPtr<ID3D12Resource> newReadback;
      if (FAILED(device->CreateCommittedResource(&readbackHeap,
                                                 D3D12_HEAP_FLAG_NONE,
                                                 &readbackDesc,
                                                 D3D12_RESOURCE_STATE_COPY_DEST,
                                                 nullptr,
                                                 IID_PPV_ARGS(newReadback.GetAddressOf())))) {
        return;
      }
      cache.readbackBuffer = std::move(newReadback);
      cache.readbackBufferSize = totalBytes;
    }

    if (FAILED(cache.allocator->Reset()) ||
        FAILED(cache.commandList->Reset(cache.allocator.Get(), nullptr))) {
      return;
    }

    const auto previousState = srcTex->getSubresourceState(mipLevel, copyLayer);
    srcTex->transitionTo(
        cache.commandList.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, mipLevel, copyLayer);

    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
    dstLoc.pResource = cache.readbackBuffer.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dstLoc.PlacedFootprint = footprint;

    D3D12_TEXTURE_COPY_LOCATION srcLoc{};
    srcLoc.pResource = srcRes;
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex = subresourceIndex;

    D3D12_BOX srcBox{};
    srcBox.left = 0;
    srcBox.top = 0;
    srcBox.front = 0;
    srcBox.right = mipWidth;
    srcBox.bottom = mipHeight;
    srcBox.back = 1;
    cache.commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

    srcTex->transitionTo(cache.commandList.Get(), previousState, mipLevel, copyLayer);

    if (FAILED(cache.commandList->Close())) {
      return;
    }

    ID3D12CommandList* lists[] = {cache.commandList.Get()};
    d3dQueue->ExecuteCommandLists(1, lists);

    const uint64_t fenceValue = ++cache.lastFenceValue;
    if (FAILED(d3dQueue->Signal(cache.fence.Get(), fenceValue))) {
      return;
    }
    if (FAILED(cache.fence->SetEventOnCompletion(fenceValue, cache.fenceEvent))) {
      return;
    }
    WaitForSingleObject(cache.fenceEvent, INFINITE);

    void* mapped = nullptr;
    if (FAILED(cache.readbackBuffer->Map(0, nullptr, &mapped))) {
      cache.cacheValid = false;
      return;
    }

    const uint8_t* srcPtr = static_cast<const uint8_t*>(mapped) + footprint.Offset;
    const size_t srcRowPitch = footprint.Footprint.RowPitch;
    const size_t copyRowBytes = fullRowBytes;
    const bool needsSwap = (srcTex->getFormat() == igl::TextureFormat::RGBA_UNorm8 ||
                            srcTex->getFormat() == igl::TextureFormat::RGBA_SRGB);

    cache.cachedRowPitch = static_cast<uint64_t>(fullRowBytes);
    cache.cachedData.resize(static_cast<size_t>(cache.cachedRowPitch) *
                            static_cast<size_t>(mipHeight));

    for (uint32_t row = 0; row < mipHeight; ++row) {
      const uint8_t* s = srcPtr + static_cast<size_t>(row) * srcRowPitch;
      uint8_t* d =
          cache.cachedData.data() +
          static_cast<size_t>(mipHeight - 1 - row) * static_cast<size_t>(cache.cachedRowPitch);

      if (needsSwap && bytesPerPixel == 4) {
        for (uint32_t col = 0; col < mipWidth; ++col) {
          const uint32_t idx = col * 4;
          d[idx + 0] = s[idx + 2];
          d[idx + 1] = s[idx + 1];
          d[idx + 2] = s[idx + 0];
          d[idx + 3] = s[idx + 3];
        }
      } else {
        std::memcpy(d, s, copyRowBytes);
      }
    }

    cache.readbackBuffer->Unmap(0, nullptr);

    cache.cachedWidth = mipWidth;
    cache.cachedHeight = mipHeight;
    cache.cachedBytesPerPixel = bytesPerPixel;
    cache.cachedMipLevel = mipLevel;
    cache.cachedLayer = copyLayer;
    cache.cachedFrameFenceValue = frameFenceValue;
    cache.cacheValid = true;

    const auto refreshEnd = std::chrono::high_resolution_clock::now();
    const double refreshMs =
        std::chrono::duration<double, std::milli>(refreshEnd - refreshStart).count();
    IGL_LOG_INFO("copyBytesColorAttachment: refreshed subresource (mip=%u, layer=%u) in %.2f ms (%ux%u)\n",
                 mipLevel,
                 copyLayer,
                 refreshMs,
                 mipWidth,
                 mipHeight);
  }

  if (!cache.cacheValid) {
    return;
  }

  if (range.width == 0 || range.height == 0 ||
      range.x + range.width > cache.cachedWidth ||
      range.y + range.height > cache.cachedHeight) {
    return;
  }

  const size_t copyRowBytes =
      static_cast<size_t>(range.width) * cache.cachedBytesPerPixel;
  const size_t dstRowPitch = bytesPerRow ? bytesPerRow : copyRowBytes;
  uint8_t* dstPtr = static_cast<uint8_t*>(pixelBytes);

  for (uint32_t destRow = 0; destRow < range.height; ++destRow) {
    const uint32_t gpuRow = range.y + (range.height - 1 - destRow);
    if (gpuRow >= cache.cachedHeight) {
      return;
    }
    const uint32_t cachedRow = cache.cachedHeight - 1 - gpuRow;
    const uint8_t* src =
        cache.cachedData.data() +
        static_cast<size_t>(cachedRow) * static_cast<size_t>(cache.cachedRowPitch) +
        static_cast<size_t>(range.x) * cache.cachedBytesPerPixel;
    std::memcpy(dstPtr + static_cast<size_t>(destRow) * dstRowPitch, src, copyRowBytes);
  }
}

void Framebuffer::copyBytesDepthAttachment(ICommandQueue& /*cmdQueue*/,
                                           void* /*pixelBytes*/,
                                           const TextureRangeDesc& /*range*/,
                                           size_t /*bytesPerRow*/) const {
  // TODO: Implement for D3D12
}

void Framebuffer::copyBytesStencilAttachment(ICommandQueue& /*cmdQueue*/,
                                             void* /*pixelBytes*/,
                                             const TextureRangeDesc& /*range*/,
                                             size_t /*bytesPerRow*/) const {
  // TODO: Implement for D3D12
}

void Framebuffer::copyTextureColorAttachment(ICommandQueue& cmdQueue,
                                             size_t index,
                                             std::shared_ptr<ITexture> destTexture,
                                             const TextureRangeDesc& range) const {
  // Create a transient command buffer to access the D3D12 context
  Result r;
  auto cmdBuf = cmdQueue.createCommandBuffer({}, &r);
  if (!cmdBuf || !r.isOk()) {
    return;
  }
  auto* d3dCmdBuf = dynamic_cast<igl::d3d12::CommandBuffer*>(cmdBuf.get());
  auto& ctx = d3dCmdBuf->getContext();
  auto* device = ctx.getDevice();
  auto* d3dQueue = ctx.getCommandQueue();
  if (!device || !d3dQueue) {
    return;
  }

  auto srcTex = std::static_pointer_cast<igl::d3d12::Texture>(desc_.colorAttachments[index].texture);
  auto dstTex = std::static_pointer_cast<igl::d3d12::Texture>(destTexture);
  if (!srcTex || !dstTex) {
    return;
  }
  ID3D12Resource* srcRes = srcTex->getResource();
  ID3D12Resource* dstRes = dstTex->getResource();
  if (!srcRes || !dstRes) {
    return;
  }

  // Command list/allocator
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
  if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            IID_PPV_ARGS(allocator.GetAddressOf()))) ||
      FAILED(device->CreateCommandList(0,
                                       D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       allocator.Get(),
                                       nullptr,
                                       IID_PPV_ARGS(cmdList.GetAddressOf())))) {
    return;
  }

  const uint32_t mipLevel = range.mipLevel;
  const uint32_t layer = range.layer;
  const auto srcPrevState = srcTex->getSubresourceState(mipLevel, layer);
  srcTex->transitionTo(cmdList.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, mipLevel, layer);
  dstTex->transitionTo(cmdList.Get(), D3D12_RESOURCE_STATE_COPY_DEST, mipLevel, layer);

  // Calculate proper subresource indices for array textures and cubemaps
  // D3D12CalcSubresource(MipSlice, ArraySlice, PlaneSlice, MipLevels, ArraySize)
  const UINT srcMipLevels = srcTex->getNumMipLevels();
  const UINT dstMipLevels = dstTex->getNumMipLevels();
  const UINT srcArraySize = srcTex->getNumLayers();
  const UINT dstArraySize = dstTex->getNumLayers();

  D3D12_TEXTURE_COPY_LOCATION dstLoc{};
  dstLoc.pResource = dstRes;
  dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dstLoc.SubresourceIndex = D3D12CalcSubresource(mipLevel, layer, 0, dstMipLevels, dstArraySize);

  D3D12_TEXTURE_COPY_LOCATION srcLoc{};
  srcLoc.pResource = srcRes;
  srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  srcLoc.SubresourceIndex = D3D12CalcSubresource(mipLevel, layer, 0, srcMipLevels, srcArraySize);

  D3D12_BOX srcBox{};
  srcBox.left = range.x;
  srcBox.top = range.y;
  srcBox.front = 0;
  srcBox.right = range.x + range.width;
  srcBox.bottom = range.y + range.height;
  srcBox.back = 1;
  cmdList->CopyTextureRegion(&dstLoc, range.x, range.y, 0, &srcLoc, &srcBox);

  // Transition dest to shader resource for sampling. Source back to its previous state.
  srcTex->transitionTo(cmdList.Get(), srcPrevState, mipLevel, layer);
  dstTex->transitionTo(cmdList.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mipLevel, layer);

  cmdList->Close();
  ID3D12CommandList* lists[] = {cmdList.Get()};
  d3dQueue->ExecuteCommandLists(1, lists);

  // Sync (simple fence)
  Microsoft::WRL::ComPtr<ID3D12Fence> fence;
  if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())))) {
    return;
  }
  HANDLE evt = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!evt) {
    return;
  }
  d3dQueue->Signal(fence.Get(), 1);
  fence->SetEventOnCompletion(1, evt);
  WaitForSingleObject(evt, INFINITE);
  CloseHandle(evt);
}

void Framebuffer::updateDrawable(std::shared_ptr<ITexture> texture) {
  desc_.colorAttachments[0].texture = std::move(texture);
}

void Framebuffer::updateDrawable(SurfaceTextures surfaceTextures) {
  desc_.colorAttachments[0].texture = std::move(surfaceTextures.color);
  desc_.depthAttachment.texture = surfaceTextures.depth;
  // Depth and stencil typically share the same texture
  desc_.stencilAttachment.texture = std::move(surfaceTextures.depth);
}

void Framebuffer::updateResolveAttachment(std::shared_ptr<ITexture> texture) {
  desc_.colorAttachments[0].resolveTexture = std::move(texture);
}

} // namespace igl::d3d12
