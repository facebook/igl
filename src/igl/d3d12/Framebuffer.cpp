/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/Framebuffer.h>
#include <igl/d3d12/CommandBuffer.h>
#include <igl/d3d12/Texture.h>
#include <cstring>

namespace igl::d3d12 {

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
  // Implement readback of color attachment texture into CPU memory (RGBA/UNORM/float).
  // Assumptions (Phase 2): 2D textures, single layer/face, sample count = 1.
  if (!pixelBytes) {
    return;
  }

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

  if (index >= IGL_COLOR_ATTACHMENTS_MAX) {
    return;
  }
  auto srcTex = std::static_pointer_cast<igl::d3d12::Texture>(desc_.colorAttachments[index].texture);
  if (!srcTex) {
    return;
  }

  ID3D12Resource* srcRes = srcTex->getResource();
  if (!srcRes) {
    return;
  }

  // Describe the region to read back
  const uint32_t mipLevel = range.mipLevel;
  D3D12_RESOURCE_DESC srcDesc = srcRes->GetDesc();

  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  UINT numRows = 0;
  UINT64 rowSizeInBytes = 0;
  UINT64 totalBytes = 0;
  device->GetCopyableFootprints(&srcDesc, mipLevel, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

  // Create readback buffer
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

  Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuf;
  if (FAILED(device->CreateCommittedResource(&readbackHeap,
                                             D3D12_HEAP_FLAG_NONE,
                                             &readbackDesc,
                                             D3D12_RESOURCE_STATE_COPY_DEST,
                                             nullptr,
                                             IID_PPV_ARGS(readbackBuf.GetAddressOf())))) {
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

  // Transition to COPY_SOURCE
  D3D12_RESOURCE_BARRIER toCopySrc{};
  toCopySrc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  toCopySrc.Transition.pResource = srcRes;
  toCopySrc.Transition.Subresource = mipLevel;
  toCopySrc.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
  toCopySrc.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  cmdList->ResourceBarrier(1, &toCopySrc);

  // Define copy locations
  D3D12_TEXTURE_COPY_LOCATION dstLoc{};
  dstLoc.pResource = readbackBuf.Get();
  dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dstLoc.PlacedFootprint = footprint;

  D3D12_TEXTURE_COPY_LOCATION srcLoc{};
  srcLoc.pResource = srcRes;
  srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  srcLoc.SubresourceIndex = mipLevel;

  // Copy the requested box region
  D3D12_BOX srcBox{};
  srcBox.left = range.x;
  srcBox.top = range.y;
  srcBox.front = 0;
  srcBox.right = range.x + range.width;
  srcBox.bottom = range.y + range.height;
  srcBox.back = 1;
  cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

  // Transition back to COMMON (best-effort)
  toCopySrc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
  toCopySrc.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
  cmdList->ResourceBarrier(1, &toCopySrc);

  cmdList->Close();
  ID3D12CommandList* lists[] = {cmdList.Get()};
  d3dQueue->ExecuteCommandLists(1, lists);

  // Sync
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

  // Map readback and copy into pixelBytes tightly packed (or bytesPerRow if given)
  void* mapped = nullptr;
  if (FAILED(readbackBuf->Map(0, nullptr, &mapped))) {
    return;
  }

  const uint8_t* srcPtr = static_cast<const uint8_t*>(mapped) + footprint.Offset;
  // Bytes per pixel from texture format
  const auto fmtProps = TextureFormatProperties::fromTextureFormat(srcTex->getFormat());
  const size_t bytesPerPixel = std::max<uint8_t>(fmtProps.bytesPerBlock, 1);
  const size_t dstRowPitch = bytesPerRow ? bytesPerRow : static_cast<size_t>(range.width) * bytesPerPixel;
  uint8_t* dstPtr = static_cast<uint8_t*>(pixelBytes);

  const size_t srcRowPitch = footprint.Footprint.RowPitch;
  const size_t copyRowBytes = static_cast<size_t>(range.width) * bytesPerPixel;

  // Always flip vertically to match Metal/Vulkan behavior
  // The validation helper will un-flip for non-render-target textures if needed
  for (uint32_t row = 0; row < range.height; ++row) {
    const uint8_t* s = srcPtr + row * srcRowPitch;
    uint8_t* d = dstPtr + (range.height - 1 - row) * dstRowPitch;  // Flip
    std::memcpy(d, s, copyRowBytes);
  }

  readbackBuf->Unmap(0, nullptr);
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

  // Barriers: src to COPY_SOURCE, dst to COPY_DEST
  D3D12_RESOURCE_BARRIER barriers[2] = {};
  barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barriers[0].Transition.pResource = srcRes;
  barriers[0].Transition.Subresource = range.mipLevel;
  barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
  barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barriers[1].Transition.pResource = dstRes;
  barriers[1].Transition.Subresource = range.mipLevel;
  barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
  barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
  cmdList->ResourceBarrier(2, barriers);

  D3D12_TEXTURE_COPY_LOCATION dstLoc{};
  dstLoc.pResource = dstRes;
  dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dstLoc.SubresourceIndex = range.mipLevel;
  D3D12_TEXTURE_COPY_LOCATION srcLoc{};
  srcLoc.pResource = srcRes;
  srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  srcLoc.SubresourceIndex = range.mipLevel;

  D3D12_BOX srcBox{};
  srcBox.left = range.x;
  srcBox.top = range.y;
  srcBox.front = 0;
  srcBox.right = range.x + range.width;
  srcBox.bottom = range.y + range.height;
  srcBox.back = 1;
  cmdList->CopyTextureRegion(&dstLoc, range.x, range.y, 0, &srcLoc, &srcBox);

  // Transition dest to shader resource for sampling. Source back to COMMON.
  barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
  barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  cmdList->ResourceBarrier(2, barriers);

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
