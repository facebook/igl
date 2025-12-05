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
#include <igl/d3d12/D3D12FenceWaiter.h>
#include <igl/d3d12/D3D12ImmediateCommands.h>
#include <igl/d3d12/D3D12StagingDevice.h>
#include <cstring>

namespace igl::d3d12 {

namespace {
// Import ComPtr for readability
template<typename T>
using ComPtr = igl::d3d12::ComPtr<T>;
} // namespace

Framebuffer::Framebuffer(const FramebufferDesc& desc) : desc_(desc) {}

Framebuffer::~Framebuffer() {
  // FenceWaiter RAII handles event cleanup automatically
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

  auto& iglDevice = d3dQueueWrapper->getDevice();
  auto& ctx = iglDevice.getD3D12Context();
  auto* device = ctx.getDevice();
  if (!device) {
    return;
  }

  // Get shared infrastructure used for readback.
  auto* immediateCommands = iglDevice.getImmediateCommands();
  auto* stagingDevice = iglDevice.getStagingDevice();
  if (!immediateCommands || !stagingDevice) {
    IGL_LOG_ERROR("Framebuffer::copyBytesColorAttachment - Shared infrastructure not available\n");
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

    // Use D3D12StagingDevice for readback buffer allocation.
    auto stagingBuffer = stagingDevice->allocateReadback(totalBytes);
    if (!stagingBuffer.valid || !stagingBuffer.buffer.Get()) {
      IGL_LOG_ERROR("Framebuffer::copyBytesColorAttachment - Failed to allocate readback buffer\n");
      cache.cacheValid = false;
      return;
    }

    // Use D3D12ImmediateCommands for the copy operation.
    Result result;
    ID3D12GraphicsCommandList* cmdList = immediateCommands->begin(&result);
    if (!cmdList || !result.isOk()) {
      IGL_LOG_ERROR("Framebuffer::copyBytesColorAttachment - Failed to begin command list: %s\n",
                    result.message.c_str());
      stagingDevice->free(stagingBuffer, 0);
      cache.cacheValid = false;
      return;
    }

    const auto previousState = srcTex->getSubresourceState(mipLevel, copyLayer);
    srcTex->transitionTo(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE, mipLevel, copyLayer);

    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
    dstLoc.pResource = stagingBuffer.buffer.Get();
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
    cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

    srcTex->transitionTo(cmdList, previousState, mipLevel, copyLayer);

    // Submit and wait using the shared fence.
    uint64_t fenceValue = immediateCommands->submit(true, &result);
    if (fenceValue == 0 || !result.isOk()) {
      IGL_LOG_ERROR("Framebuffer::copyBytesColorAttachment - Failed to submit command list: %s\n",
                    result.message.c_str());
      stagingDevice->free(stagingBuffer, 0);
      cache.cacheValid = false;
      return;
    }

    // Map and read the readback buffer
    void* mapped = nullptr;
    D3D12_RANGE readRange{0, totalBytes};
    if (FAILED(stagingBuffer.buffer->Map(0, &readRange, &mapped))) {
      IGL_LOG_ERROR("Framebuffer::copyBytesColorAttachment - Failed to map readback buffer\n");
      stagingDevice->free(stagingBuffer, fenceValue);
      cache.cacheValid = false;
      return;
    }

    const uint8_t* srcPtr = static_cast<const uint8_t*>(mapped) + footprint.Offset;
    const size_t srcRowPitch = footprint.Footprint.RowPitch;
    const size_t copyRowBytes = fullRowBytes;

    cache.cachedRowPitch = static_cast<uint64_t>(fullRowBytes);
    cache.cachedData.resize(static_cast<size_t>(cache.cachedRowPitch) *
                            static_cast<size_t>(mipHeight));

    // Direct copy with vertical flip only; no channel swap needed.
    // DXGI_FORMAT_R8G8B8A8_UNORM has R,G,B,A byte order matching IGL expectations.
    for (uint32_t row = 0; row < mipHeight; ++row) {
      const uint8_t* s = srcPtr + static_cast<size_t>(row) * srcRowPitch;
      uint8_t* d =
          cache.cachedData.data() +
          static_cast<size_t>(mipHeight - 1 - row) * static_cast<size_t>(cache.cachedRowPitch);

      std::memcpy(d, s, copyRowBytes);
    }

    stagingBuffer.buffer->Unmap(0, nullptr);

    // Free the staging buffer back to the pool.
    stagingDevice->free(stagingBuffer, fenceValue);

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
    IGL_D3D12_LOG_VERBOSE("copyBytesColorAttachment: refreshed subresource (mip=%u, layer=%u) in %.2f ms (%ux%u)\n",
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

void Framebuffer::copyBytesDepthAttachment(ICommandQueue& cmdQueue,
                                           void* pixelBytes,
                                           const TextureRangeDesc& range,
                                           size_t bytesPerRow) const {
  // Depth attachment readback.
  if (!pixelBytes) {
    return;
  }

  auto* d3dQueueWrapper = dynamic_cast<CommandQueue*>(&cmdQueue);
  if (!d3dQueueWrapper) {
    return;
  }

  auto& iglDevice = d3dQueueWrapper->getDevice();
  auto& ctx = iglDevice.getD3D12Context();
  auto* device = ctx.getDevice();
  if (!device) {
    return;
  }

  // Get shared staging infrastructure.
  auto* immediateCommands = iglDevice.getImmediateCommands();
  auto* stagingDevice = iglDevice.getStagingDevice();
  if (!immediateCommands || !stagingDevice) {
    IGL_LOG_ERROR("Framebuffer::copyBytesDepthAttachment - Shared infrastructure not available\n");
    return;
  }

  auto depthTex = std::static_pointer_cast<Texture>(desc_.depthAttachment.texture);
  if (!depthTex) {
    return;
  }

  ID3D12Resource* depthRes = depthTex->getResource();
  if (!depthRes) {
    return;
  }

  const uint32_t mipLevel = range.mipLevel;
  const uint32_t copyLayer = (depthTex->getType() == TextureType::Cube) ? range.face : range.layer;
  const uint32_t subresourceIndex = depthTex->calcSubresourceIndex(mipLevel, copyLayer);

  const auto texDims = depthTex->getDimensions();
  const uint32_t mipWidth = std::max<uint32_t>(1u, texDims.width >> mipLevel);
  const uint32_t mipHeight = std::max<uint32_t>(1u, texDims.height >> mipLevel);

  // Get footprint for the depth resource
  D3D12_RESOURCE_DESC depthDesc = depthRes->GetDesc();

  // Validate and log depth format to clarify raw-bits vs converted-float behavior.
  const DXGI_FORMAT depthFormat = depthDesc.Format;
  const bool isD32Float = (depthFormat == DXGI_FORMAT_D32_FLOAT ||
                           depthFormat == DXGI_FORMAT_D32_FLOAT_S8X24_UINT);

  if (!isD32Float) {
    IGL_D3D12_LOG_VERBOSE("Framebuffer::copyBytesDepthAttachment - Format 0x%X is not D32_FLOAT; "
                          "returning raw GPU bits (not normalized [0,1] floats). "
                          "For UNORM formats, caller must convert manually.\n",
                          static_cast<unsigned int>(depthFormat));
  }

  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  UINT numRows = 0;
  UINT64 rowSizeInBytes = 0;
  UINT64 totalBytes = 0;
  device->GetCopyableFootprints(
      &depthDesc, subresourceIndex, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

  if (totalBytes == 0) {
    return;
  }

  // Allocate readback buffer from the staging device.
  auto stagingBuffer = stagingDevice->allocateReadback(totalBytes);
  if (!stagingBuffer.valid || !stagingBuffer.buffer.Get()) {
    IGL_LOG_ERROR("Framebuffer::copyBytesDepthAttachment - Failed to allocate readback buffer\n");
    return;
  }

  // Begin immediate command recording.
  Result result;
  ID3D12GraphicsCommandList* cmdList = immediateCommands->begin(&result);
  if (!cmdList || !result.isOk()) {
    IGL_LOG_ERROR("Framebuffer::copyBytesDepthAttachment - Failed to begin command list: %s\n",
                  result.message.c_str());
    stagingDevice->free(stagingBuffer, 0);
    return;
  }

  // Transition depth texture to copy source
  const auto previousState = depthTex->getSubresourceState(mipLevel, copyLayer);
  depthTex->transitionTo(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE, mipLevel, copyLayer);

  // Set up copy locations
  D3D12_TEXTURE_COPY_LOCATION dstLoc{};
  dstLoc.pResource = stagingBuffer.buffer.Get();
  dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dstLoc.PlacedFootprint = footprint;

  D3D12_TEXTURE_COPY_LOCATION srcLoc{};
  srcLoc.pResource = depthRes;
  srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  srcLoc.SubresourceIndex = subresourceIndex;

  D3D12_BOX srcBox{};
  srcBox.left = 0;
  srcBox.top = 0;
  srcBox.front = 0;
  srcBox.right = mipWidth;
  srcBox.bottom = mipHeight;
  srcBox.back = 1;

  // Copy depth data
  cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

  // Transition back to previous state
  depthTex->transitionTo(cmdList, previousState, mipLevel, copyLayer);

  // Submit and wait using the shared fence.
  uint64_t fenceValue = immediateCommands->submit(true, &result);
  if (fenceValue == 0 || !result.isOk()) {
    IGL_LOG_ERROR("Framebuffer::copyBytesDepthAttachment - Failed to submit command list: %s\n",
                  result.message.c_str());
    stagingDevice->free(stagingBuffer, 0);
    return;
  }

  // Map readback buffer and copy data
  void* mapped = nullptr;
  D3D12_RANGE readRange{0, totalBytes};
  if (FAILED(stagingBuffer.buffer->Map(0, &readRange, &mapped))) {
    IGL_LOG_ERROR("Framebuffer::copyBytesDepthAttachment - Failed to map readback buffer\n");
    stagingDevice->free(stagingBuffer, fenceValue);
    return;
  }

  // Validate range bounds before copying.
  if (range.width == 0 || range.height == 0 ||
      range.x + range.width > mipWidth ||
      range.y + range.height > mipHeight) {
    IGL_LOG_ERROR("Framebuffer::copyBytesDepthAttachment - Invalid range: [%u,%u %ux%u] exceeds mip size %ux%u\n",
                  range.x, range.y, range.width, range.height, mipWidth, mipHeight);
    stagingBuffer.buffer->Unmap(0, nullptr);
    stagingDevice->free(stagingBuffer, fenceValue);
    return;
  }

  const uint8_t* srcPtr = static_cast<const uint8_t*>(mapped) + footprint.Offset;
  const size_t srcRowPitch = footprint.Footprint.RowPitch;

  // Depth readback contract: callers (tests) provide a float-per-pixel buffer.
  // Use 4 bytes per destination pixel regardless of the underlying DXGI format,
  // and only copy that many bytes from the GPU data to avoid overrunning the
  // caller's buffer (e.g., for combined depth-stencil formats like D32_S8).
  //
  // LIMITATION: This implementation assumes a "raw bits" contract - it copies
  // the native GPU representation without format conversion. This is correct for
  // D32_FLOAT (which is already IEEE 754 float), but for normalized integer depth
  // formats (D16_UNORM, D24_UNORM_S8_UINT), the copied data is raw bits, not
  // converted [0,1] floats. Callers expecting normalized depth values from non-float
  // formats will receive unconverted data. Future work should add format detection
  // and explicit UNORM-to-float conversion for broader compatibility.
  constexpr size_t kDstBytesPerPixel = sizeof(float);

  // Derive the native bytes-per-pixel for the copied subresource using the
  // rowSizeInBytes returned by GetCopyableFootprints when possible.
  size_t nativeBytesPerPixel = 0;
  if (mipWidth > 0 && rowSizeInBytes > 0) {
    nativeBytesPerPixel = static_cast<size_t>(rowSizeInBytes) / static_cast<size_t>(mipWidth);
  }

  const size_t copyRowBytes = static_cast<size_t>(range.width) * kDstBytesPerPixel;
  const size_t dstRowPitch = bytesPerRow ? bytesPerRow : copyRowBytes;
  uint8_t* dstPtr = static_cast<uint8_t*>(pixelBytes);

  for (uint32_t destRow = 0; destRow < range.height; ++destRow) {
    const uint32_t gpuRow = range.y + (range.height - 1 - destRow);
    if (gpuRow >= mipHeight) {
      break;
    }
    const uint32_t srcRow = mipHeight - 1 - gpuRow;
    const uint8_t* src =
        srcPtr + static_cast<size_t>(srcRow) * srcRowPitch +
        static_cast<size_t>(range.x) * (nativeBytesPerPixel > 0 ? nativeBytesPerPixel : kDstBytesPerPixel);

    std::memcpy(dstPtr + static_cast<size_t>(destRow) * dstRowPitch, src, copyRowBytes);
  }

  stagingBuffer.buffer->Unmap(0, nullptr);

  // Free staging buffer back to the pool.
  stagingDevice->free(stagingBuffer, fenceValue);
}

void Framebuffer::copyBytesStencilAttachment(ICommandQueue& cmdQueue,
                                             void* pixelBytes,
                                             const TextureRangeDesc& range,
                                             size_t bytesPerRow) const {
  // Stencil attachment readback.
  if (!pixelBytes) {
    return;
  }

  auto* d3dQueueWrapper = dynamic_cast<CommandQueue*>(&cmdQueue);
  if (!d3dQueueWrapper) {
    return;
  }

  auto& iglDevice = d3dQueueWrapper->getDevice();
  auto& ctx = iglDevice.getD3D12Context();
  auto* device = ctx.getDevice();
  if (!device) {
    return;
  }

  // Get shared infrastructure.
  auto* immediateCommands = iglDevice.getImmediateCommands();
  auto* stagingDevice = iglDevice.getStagingDevice();
  if (!immediateCommands || !stagingDevice) {
    IGL_LOG_ERROR("Framebuffer::copyBytesStencilAttachment - Shared infrastructure not available\n");
    return;
  }

  auto stencilTex = std::static_pointer_cast<Texture>(desc_.stencilAttachment.texture);
  if (!stencilTex) {
    return;
  }

  ID3D12Resource* stencilRes = stencilTex->getResource();
  if (!stencilRes) {
    return;
  }

  const uint32_t mipLevel = range.mipLevel;
  const uint32_t copyLayer = (stencilTex->getType() == TextureType::Cube) ? range.face : range.layer;

  // Detect stencil format and select the appropriate plane slice.
  D3D12_RESOURCE_DESC stencilDesc = stencilRes->GetDesc();
  const DXGI_FORMAT stencilFormat = stencilDesc.Format;

  // Determine plane slice based on format:
  // - Planar depth-stencil formats: stencil is in plane 1
  // - Pure stencil formats: plane 0
  UINT planeSlice = 0; // Default for non-planar

  if (stencilFormat == DXGI_FORMAT_D24_UNORM_S8_UINT ||
      stencilFormat == DXGI_FORMAT_D32_FLOAT_S8X24_UINT ||
      stencilFormat == DXGI_FORMAT_R24G8_TYPELESS ||
      stencilFormat == DXGI_FORMAT_R32G8X24_TYPELESS) {
    // Planar depth-stencil: Plane 0 = depth, Plane 1 = stencil
    planeSlice = 1;
  } else if (stencilFormat == DXGI_FORMAT_R8_TYPELESS) {
    // Pure stencil formats: Plane 0
    planeSlice = 0;
  } else {
    IGL_LOG_ERROR("Framebuffer::copyBytesStencilAttachment - Unsupported stencil format 0x%X; "
                  "assuming plane 0. May fail for planar formats.\n",
                  static_cast<unsigned int>(stencilFormat));
    IGL_DEBUG_ASSERT(false, "Unsupported stencil format - add to known format list");
    planeSlice = 0;
  }

  const UINT numMipLevels = stencilTex->getNumMipLevels();
  const UINT numLayers = stencilTex->getNumLayers();
  const uint32_t subresourceIndex = D3D12CalcSubresource(mipLevel, copyLayer, planeSlice, numMipLevels, numLayers);

  const auto texDims = stencilTex->getDimensions();
  const uint32_t mipWidth = std::max<uint32_t>(1u, texDims.width >> mipLevel);
  const uint32_t mipHeight = std::max<uint32_t>(1u, texDims.height >> mipLevel);

  // Get footprint for the stencil plane (reuse stencilDesc from above)
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  UINT numRows = 0;
  UINT64 rowSizeInBytes = 0;
  UINT64 totalBytes = 0;
  device->GetCopyableFootprints(
      &stencilDesc, subresourceIndex, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

  if (totalBytes == 0) {
    return;
  }

  // Allocate readback buffer from the staging device.
  auto stagingBuffer = stagingDevice->allocateReadback(totalBytes);
  if (!stagingBuffer.valid || !stagingBuffer.buffer.Get()) {
    IGL_LOG_ERROR("Framebuffer::copyBytesStencilAttachment - Failed to allocate readback buffer\n");
    return;
  }

  // Begin immediate command recording.
  Result result;
  ID3D12GraphicsCommandList* cmdList = immediateCommands->begin(&result);
  if (!cmdList || !result.isOk()) {
    IGL_LOG_ERROR("Framebuffer::copyBytesStencilAttachment - Failed to begin command list: %s\n",
                  result.message.c_str());
    stagingDevice->free(stagingBuffer, 0);
    return;
  }

  // Transition stencil texture to copy source
  const auto previousState = stencilTex->getSubresourceState(mipLevel, copyLayer);
  stencilTex->transitionTo(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE, mipLevel, copyLayer);

  // Set up copy locations for stencil plane
  D3D12_TEXTURE_COPY_LOCATION dstLoc{};
  dstLoc.pResource = stagingBuffer.buffer.Get();
  dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dstLoc.PlacedFootprint = footprint;

  D3D12_TEXTURE_COPY_LOCATION srcLoc{};
  srcLoc.pResource = stencilRes;
  srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  srcLoc.SubresourceIndex = subresourceIndex;

  D3D12_BOX srcBox{};
  srcBox.left = 0;
  srcBox.top = 0;
  srcBox.front = 0;
  srcBox.right = mipWidth;
  srcBox.bottom = mipHeight;
  srcBox.back = 1;

  // Copy stencil data
  cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

  // Transition back to previous state
  stencilTex->transitionTo(cmdList, previousState, mipLevel, copyLayer);

  // Submit and wait using the shared fence.
  uint64_t fenceValue = immediateCommands->submit(true, &result);
  if (fenceValue == 0 || !result.isOk()) {
    IGL_LOG_ERROR("Framebuffer::copyBytesStencilAttachment - Failed to submit command list: %s\n",
                  result.message.c_str());
    stagingDevice->free(stagingBuffer, 0);
    return;
  }

  // Map readback buffer and copy data
  void* mapped = nullptr;
  D3D12_RANGE readRange{0, totalBytes};
  if (FAILED(stagingBuffer.buffer->Map(0, &readRange, &mapped))) {
    IGL_LOG_ERROR("Framebuffer::copyBytesStencilAttachment - Failed to map readback buffer\n");
    stagingDevice->free(stagingBuffer, fenceValue);
    return;
  }

  // Validate range bounds before copying.
  if (range.width == 0 || range.height == 0 ||
      range.x + range.width > mipWidth ||
      range.y + range.height > mipHeight) {
    IGL_LOG_ERROR("Framebuffer::copyBytesStencilAttachment - Invalid range: [%u,%u %ux%u] exceeds mip size %ux%u\n",
                  range.x, range.y, range.width, range.height, mipWidth, mipHeight);
    stagingBuffer.buffer->Unmap(0, nullptr);
    stagingDevice->free(stagingBuffer, fenceValue);
    return;
  }

  const uint8_t* srcPtr = static_cast<const uint8_t*>(mapped) + footprint.Offset;
  const size_t srcRowPitch = footprint.Footprint.RowPitch;

  // Stencil is always 8-bit (1 byte per pixel)
  const size_t bytesPerPixel = 1;

  // Copy with vertical flip (D3D12 textures are top-down, IGL expects bottom-up)
  const size_t copyRowBytes = static_cast<size_t>(range.width) * bytesPerPixel;
  const size_t dstRowPitch = bytesPerRow ? bytesPerRow : copyRowBytes;
  uint8_t* dstPtr = static_cast<uint8_t*>(pixelBytes);

  for (uint32_t destRow = 0; destRow < range.height; ++destRow) {
    const uint32_t gpuRow = range.y + (range.height - 1 - destRow);
    if (gpuRow >= mipHeight) {
      break;
    }
    const uint32_t srcRow = mipHeight - 1 - gpuRow;
    const uint8_t* src = srcPtr + static_cast<size_t>(srcRow) * srcRowPitch +
                         static_cast<size_t>(range.x) * bytesPerPixel;
    std::memcpy(dstPtr + static_cast<size_t>(destRow) * dstRowPitch, src, copyRowBytes);
  }

  stagingBuffer.buffer->Unmap(0, nullptr);

  // Free staging buffer back to the pool.
  stagingDevice->free(stagingBuffer, fenceValue);
}

void Framebuffer::copyTextureColorAttachment(ICommandQueue& cmdQueue,
                                             size_t index,
                                             std::shared_ptr<ITexture> destTexture,
                                             const TextureRangeDesc& range) const {
  // Bounds check for index parameter
  if (index >= IGL_COLOR_ATTACHMENTS_MAX) {
    IGL_LOG_ERROR("Framebuffer::copyTextureColorAttachment: index %zu out of bounds (max %u)\n",
                  index, IGL_COLOR_ATTACHMENTS_MAX);
    return;
  }

  // Get device and shared infrastructure directly (avoid transient CommandBuffer).
  auto* d3dQueueWrapper = dynamic_cast<CommandQueue*>(&cmdQueue);
  if (!d3dQueueWrapper) {
    IGL_LOG_ERROR("Framebuffer::copyTextureColorAttachment - Invalid command queue\n");
    IGL_DEBUG_ASSERT(false, "D3D12 Framebuffer used with non-D3D12 command queue");
    return;
  }

  auto& iglDevice = d3dQueueWrapper->getDevice();
  auto* immediateCommands = iglDevice.getImmediateCommands();
  if (!immediateCommands) {
    IGL_LOG_ERROR("Framebuffer::copyTextureColorAttachment - Immediate commands not available\n");
    IGL_DEBUG_ASSERT(false, "D3D12ImmediateCommands not initialized");
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
  Result result;
  ID3D12GraphicsCommandList* cmdList = immediateCommands->begin(&result);
  if (!cmdList || !result.isOk()) {
    IGL_LOG_ERROR("Framebuffer::copyTextureColorAttachment - Failed to begin command list: %s\n",
                  result.message.c_str());
    return;
  }

  const uint32_t mipLevel = range.mipLevel;
  const uint32_t layer = range.layer;
  const auto srcPrevState = srcTex->getSubresourceState(mipLevel, layer);
  srcTex->transitionTo(cmdList, D3D12_RESOURCE_STATE_COPY_SOURCE, mipLevel, layer);
  dstTex->transitionTo(cmdList, D3D12_RESOURCE_STATE_COPY_DEST, mipLevel, layer);

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
  srcTex->transitionTo(cmdList, srcPrevState, mipLevel, layer);
  dstTex->transitionTo(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mipLevel, layer);

  // Submit and wait using the shared fence (replaces manual CreateEvent/WaitForSingleObject).
  uint64_t fenceValue = immediateCommands->submit(true, &result);
  if (fenceValue == 0 || !result.isOk()) {
    IGL_LOG_ERROR("Framebuffer::copyTextureColorAttachment - Failed to submit command list: %s\n",
                  result.message.c_str());
    return;
  }
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
