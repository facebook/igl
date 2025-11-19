/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <igl/d3d12/Texture.h>
#include <igl/d3d12/Device.h>  // P0_DX12-005: For command allocator pool access
#include <igl/d3d12/DXCCompiler.h>
#include <igl/d3d12/UploadRingBuffer.h>  // P1_DX12-009: Upload ring buffer
#include <igl/d3d12/D3D12FenceWaiter.h>

// P1_DX12-FIND-06: Removed needsRGBAChannelSwap() function
// No channel swap needed - DXGI_FORMAT_R8G8B8A8_UNORM matches IGL TextureFormat::RGBA_UNorm8 byte order

namespace igl::d3d12 {

namespace {
// Import ComPtr for readability
template<typename T>
using ComPtr = igl::d3d12::ComPtr<T>;
} // namespace

std::shared_ptr<Texture> Texture::createFromResource(ID3D12Resource* resource,
                                                     TextureFormat format,
                                                     const TextureDesc& desc,
                                                     ID3D12Device* device,
                                                     ID3D12CommandQueue* queue,
                                                     D3D12_RESOURCE_STATES initialState,
                                                     Device* iglDevice) {
  if (!resource) {
    IGL_LOG_ERROR("Texture::createFromResource - resource is NULL!\n");
    return nullptr;
  }

  auto texture = std::make_shared<Texture>(format);

  // Attach the resource to ComPtr (takes ownership, AddRefs)
  resource->AddRef();
  texture->resource_.Attach(resource);

  texture->device_ = device;
  texture->queue_ = queue;
  texture->iglDevice_ = iglDevice;  // P0_DX12-005: Store igl Device for command allocator pool access
  texture->format_ = format;
  texture->dimensions_ = Dimensions{desc.width, desc.height, desc.depth};
  texture->type_ = desc.type;
  texture->numLayers_ = desc.numLayers;
  texture->numMipLevels_ = desc.numMipLevels;
  texture->samples_ = desc.numSamples;
  texture->usage_ = desc.usage;

  texture->initializeStateTracking(initialState);

  IGL_D3D12_LOG_VERBOSE("Texture::createFromResource - SUCCESS: %dx%d format=%d\n",
               desc.width, desc.height, (int)format);

  return texture;
}

std::shared_ptr<Texture> Texture::createTextureView(std::shared_ptr<Texture> parent,
                                                     const TextureViewDesc& desc) {
  if (!parent) {
    IGL_LOG_ERROR("Texture::createTextureView - parent is NULL!\n");
    return nullptr;
  }

  // Determine the format to use for the view
  TextureFormat viewFormat = (desc.format != TextureFormat::Invalid) ? desc.format : parent->format_;

  auto view = std::make_shared<Texture>(viewFormat);

  // Share the D3D12 resource (don't create new one)
  // ComPtr doesn't have copy assignment, so we need to use Attach() and AddRef()
  auto* parentResource = parent->resource_.Get();
  if (parentResource) {
    parentResource->AddRef();
    view->resource_.Attach(parentResource);
  }
  view->isView_ = true;
  view->parentTexture_ = parent;

  // Defensive check: parent and view must share the same underlying D3D12 resource
  IGL_DEBUG_ASSERT(parent->resource_.Get() == view->resource_.Get(),
                   "Parent and view must share the same D3D12 resource");

  // Store view parameters (cumulative offsets for nested views)
  view->mipLevelOffset_ = parent->mipLevelOffset_ + desc.mipLevel;
  view->numMipLevelsInView_ = desc.numMipLevels;
  view->arraySliceOffset_ = parent->arraySliceOffset_ + desc.layer;
  view->numArraySlicesInView_ = desc.numLayers;

  // Copy properties from parent
  view->device_ = parent->device_;
  view->queue_ = parent->queue_;
  view->iglDevice_ = parent->iglDevice_;  // P0_DX12-005: Propagate igl Device pointer
  view->format_ = viewFormat;
  view->type_ = desc.type;
  view->usage_ = parent->usage_;
  view->samples_ = parent->samples_;

  // Calculate view dimensions based on mip level
  const uint32_t mipDivisor = 1u << desc.mipLevel;
  view->dimensions_ = Dimensions{
      std::max(1u, parent->dimensions_.width >> desc.mipLevel),
      std::max(1u, parent->dimensions_.height >> desc.mipLevel),
      std::max(1u, parent->dimensions_.depth >> desc.mipLevel)
  };
  view->numLayers_ = desc.numLayers;
  view->numMipLevels_ = desc.numMipLevels;

  // T24: Views delegate state tracking to root texture - don't maintain separate state.
  // State is accessed via getStateOwner() which walks to root for views.
  // Views share the same D3D12 resource and subresourceStates_ tracking with their root.

  IGL_D3D12_LOG_VERBOSE("Texture::createTextureView - SUCCESS: view of %dx%d, mips %u-%u, layers %u-%u\n",
               view->dimensions_.width, view->dimensions_.height,
               desc.mipLevel, desc.mipLevel + desc.numMipLevels - 1,
               desc.layer, desc.layer + desc.numLayers - 1);

  return view;
}

// B-001: Explicit destructor to free descriptor heap slots
Texture::~Texture() {
  IGL_D3D12_LOG_VERBOSE("Texture::~Texture - START: Destroying texture %p\n", this);

  // B-001: Texture views share the parent's resource, so they don't own descriptors
  // Only free descriptors for non-view textures
  if (isView_) {
    IGL_D3D12_LOG_VERBOSE("Texture::~Texture - Texture is a view, skipping descriptor cleanup\n");
    return;
  }

  // B-001: Get descriptor heap manager from device
  // Note: In current architecture, descriptors are allocated/freed by RenderCommandEncoder,
  // not stored in Texture. This destructor is defensive - it will clean up any descriptors
  // if the architecture changes to store them in Texture in the future.
  if (!iglDevice_) {
    IGL_D3D12_LOG_VERBOSE("Texture::~Texture - No IGL device, skipping descriptor cleanup\n");
    return;
  }

  // B-001: For now, descriptors are managed by RenderCommandEncoder and freed when encoder
  // is destroyed. This destructor is here to ensure proper cleanup if we later add
  // per-texture descriptor allocation (as described in task B-001).
  // The rtvIndices_, dsvIndices_, and srvIndex_ members are currently unused but
  // reserved for future use.

  IGL_D3D12_LOG_VERBOSE("Texture::~Texture - Destruction complete\n");
}

Result Texture::upload(const TextureRangeDesc& range,
                      const void* data,
                      size_t bytesPerRow) const {
  IGL_D3D12_LOG_VERBOSE("Texture::upload() - START: %dx%d\n", range.width, range.height);

  if (!device_ || !queue_ || !resource_.Get()) {
    IGL_LOG_ERROR("Texture::upload() - FAILED: device, queue, or resource not available\n");
    return Result(Result::Code::RuntimeError, "Device, queue, or resource not available for upload");
  }

  if (!data) {
    IGL_LOG_ERROR("Texture::upload() - FAILED: data is null\n");
    return Result(Result::Code::ArgumentInvalid, "Upload data is null");
  }

  IGL_D3D12_LOG_VERBOSE("Texture::upload() - Proceeding with upload\n");

  // Calculate dimensions and data size
  const uint32_t width = range.width > 0 ? range.width : dimensions_.width;
  const uint32_t height = range.height > 0 ? range.height : dimensions_.height;
  const uint32_t depth = range.depth > 0 ? range.depth : dimensions_.depth;

  const auto props = TextureFormatProperties::fromTextureFormat(format_);
  // Calculate bytes per row if not provided
  if (bytesPerRow == 0) {
    const size_t bpp = std::max<uint8_t>(props.bytesPerBlock, 1);
    bytesPerRow = static_cast<size_t>(width) * bpp;
  }

  // Get the resource description to calculate required size
  D3D12_RESOURCE_DESC resourceDesc = resource_->GetDesc();

  // Determine how many layers/faces and mip levels we need to upload
  const uint32_t numSlicesToUpload = (type_ == TextureType::Cube) ? range.numFaces : range.numLayers;
  const uint32_t baseSlice = (type_ == TextureType::Cube) ? range.face : range.layer;
  const uint32_t numMipsToUpload = range.numMipLevels;
  const uint32_t baseMip = range.mipLevel;
  IGL_D3D12_LOG_VERBOSE("Texture::upload - type=%d, baseSlice=%u, numSlicesToUpload=%u, baseMip=%u, numMipsToUpload=%u\n",
               (int)type_, baseSlice, numSlicesToUpload, baseMip, numMipsToUpload);

  // Calculate total staging buffer size for ALL subresources
  UINT64 totalStagingSize = 0;
  std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts;
  std::vector<UINT> numRowsArray;
  std::vector<UINT64> rowSizesArray;

  for (uint32_t mipOffset = 0; mipOffset < numMipsToUpload; ++mipOffset) {
    for (uint32_t sliceOffset = 0; sliceOffset < numSlicesToUpload; ++sliceOffset) {
      const uint32_t subresource = calcSubresourceIndex(baseMip + mipOffset, baseSlice + sliceOffset);
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
      UINT numRows = 0;
      UINT64 rowSize = 0;
      UINT64 subresSize = 0;
      device_->GetCopyableFootprints(&resourceDesc, subresource, 1, totalStagingSize, &layout, &numRows, &rowSize, &subresSize);
      layouts.push_back(layout);
      numRowsArray.push_back(numRows);
      rowSizesArray.push_back(rowSize);
      totalStagingSize += subresSize;
    }
  }

  // P1_DX12-009: Try to allocate from upload ring buffer first
  UploadRingBuffer* ringBuffer = nullptr;
  UploadRingBuffer::Allocation ringAllocation;
  bool useRingBuffer = false;
  UINT64 uploadFenceValue = 0;

  if (iglDevice_) {
    // Reclaim completed upload buffers before allocating new ones.
    iglDevice_->processCompletedUploads();

    ringBuffer = iglDevice_->getUploadRingBuffer();
    // Get fence value that will signal when this upload completes
    uploadFenceValue = iglDevice_->getNextUploadFenceValue();

    if (ringBuffer) {
      // D3D12 requires 512-byte alignment for texture uploads (D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT)
      constexpr uint64_t kTextureUploadAlignment = 512;
      ringAllocation = ringBuffer->allocate(totalStagingSize, kTextureUploadAlignment, uploadFenceValue);

      if (ringAllocation.valid) {
        useRingBuffer = true;
      }
    }
  }

  // Fallback: Create temporary staging buffer if ring buffer allocation failed
  igl::d3d12::ComPtr<ID3D12Resource> stagingBuffer;
  void* mappedData = nullptr;
  uint64_t stagingBaseOffset = 0;
  HRESULT hr = S_OK;

  if (useRingBuffer) {
    // Use ring buffer allocation
    mappedData = ringAllocation.cpuAddress;
    stagingBaseOffset = ringAllocation.offset;
  } else {
    // Create temporary staging buffer
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeapProps.CreationNodeMask = 1;
    uploadHeapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC stagingDesc = {};
    stagingDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    stagingDesc.Width = totalStagingSize;
    stagingDesc.Height = 1;
    stagingDesc.DepthOrArraySize = 1;
    stagingDesc.MipLevels = 1;
    stagingDesc.Format = DXGI_FORMAT_UNKNOWN;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = device_->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &stagingDesc,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                   IID_PPV_ARGS(stagingBuffer.GetAddressOf()));
    if (FAILED(hr)) {
      return Result(Result::Code::RuntimeError, "Failed to create staging buffer");
    }

    // Map staging buffer once
    hr = stagingBuffer->Map(0, nullptr, &mappedData);
    if (FAILED(hr)) {
      return Result(Result::Code::RuntimeError, "Failed to map staging buffer");
    }
  }

  // Copy all subresource data to staging buffer
  // P1_DX12-FIND-06: Direct copy - no channel swap needed for RGBA formats
  // DXGI_FORMAT_R8G8B8A8_UNORM has R,G,B,A byte order matching IGL TextureFormat::RGBA_UNorm8
  size_t srcDataOffset = 0;
  size_t layoutIdx = 0;

  for (uint32_t mipOffset = 0; mipOffset < numMipsToUpload; ++mipOffset) {
    const uint32_t mipWidth = std::max(width >> (baseMip + mipOffset), 1u);
    const uint32_t mipHeight = std::max(height >> (baseMip + mipOffset), 1u);
    const uint32_t mipDepth = std::max(depth >> (baseMip + mipOffset), 1u);
    const size_t mipBytesPerRow = (bytesPerRow * mipWidth) / width;
    const size_t srcLayerSize = mipBytesPerRow * mipHeight * mipDepth;

    for (uint32_t sliceOffset = 0; sliceOffset < numSlicesToUpload; ++sliceOffset) {
      const auto& layout = layouts[layoutIdx];
      const UINT numRows = numRowsArray[layoutIdx];
      const UINT64 rowSize = rowSizesArray[layoutIdx];
      layoutIdx++;

      const uint8_t* srcData = static_cast<const uint8_t*>(data) + srcDataOffset;
      uint8_t* dstData = static_cast<uint8_t*>(mappedData) + layout.Offset;
      const size_t copyBytes = std::min(static_cast<size_t>(rowSize), mipBytesPerRow);
      const size_t srcDepthPitch = mipBytesPerRow * mipHeight;
      const size_t dstDepthPitch = layout.Footprint.RowPitch * layout.Footprint.Height;

      for (UINT z = 0; z < mipDepth; ++z) {
        const uint8_t* srcSlice = srcData + z * srcDepthPitch;
        uint8_t* dstSlice = dstData + z * dstDepthPitch;
        for (UINT row = 0; row < std::min(mipHeight, numRows); ++row) {
          const uint8_t* srcRow = srcSlice + row * mipBytesPerRow;
          uint8_t* dstRow = dstSlice + row * layout.Footprint.RowPitch;
          memcpy(dstRow, srcRow, copyBytes);
        }
      }
      srcDataOffset += srcLayerSize;
    }
  }

  // Unmap temporary staging buffer (ring buffer stays persistently mapped)
  if (!useRingBuffer && stagingBuffer.Get()) {
    stagingBuffer->Unmap(0, nullptr);
  }

  // P0_DX12-005: Get command allocator from pool with fence tracking
  igl::d3d12::ComPtr<ID3D12CommandAllocator> cmdAlloc;
  if (iglDevice_) {
    cmdAlloc = iglDevice_->getUploadCommandAllocator();
    if (!cmdAlloc.Get()) {
      return Result(Result::Code::RuntimeError, "Failed to get command allocator from pool");
    }
  } else {
    // Fallback for textures created without Device* (shouldn't happen in normal flow)
    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAlloc.GetAddressOf()));
    if (FAILED(hr)) {
      return Result(Result::Code::RuntimeError, "Failed to create command allocator");
    }
  }

  igl::d3d12::ComPtr<ID3D12GraphicsCommandList> cmdList;
  hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), nullptr,
                                   IID_PPV_ARGS(cmdList.GetAddressOf()));
  if (FAILED(hr)) {
    if (iglDevice_) {
      // Return allocator to pool with fence value 0 (immediately available)
      iglDevice_->returnUploadCommandAllocator(cmdAlloc, 0);
    }
    return Result(Result::Code::RuntimeError, "Failed to create command list");
  }

  // Record all copy commands
  layoutIdx = 0;
  for (uint32_t mipOffset = 0; mipOffset < numMipsToUpload; ++mipOffset) {
    const uint32_t currentMip = baseMip + mipOffset;
    const uint32_t mipWidth = std::max(width >> currentMip, 1u);
    const uint32_t mipHeight = std::max(height >> currentMip, 1u);
    const uint32_t mipDepth = std::max(depth >> currentMip, 1u);

    for (uint32_t sliceOffset = 0; sliceOffset < numSlicesToUpload; ++sliceOffset) {
      const uint32_t currentSlice = baseSlice + sliceOffset;
      const uint32_t subresource = calcSubresourceIndex(currentMip, currentSlice);

      // const_cast needed because upload is const (required by ITexture interface)
      // but state tracking is non-const by design
      const_cast<Texture*>(this)->transitionTo(cmdList.Get(), D3D12_RESOURCE_STATE_COPY_DEST, currentMip, currentSlice);

      D3D12_TEXTURE_COPY_LOCATION dst = {};
      dst.pResource = resource_.Get();
      dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      dst.SubresourceIndex = subresource;

      if (type_ == TextureType::Cube) {
        IGL_D3D12_LOG_VERBOSE("CopyTextureRegion: Copying to CUBE subresource=%u (mip=%u, slice=%u)\n",
                     subresource, currentMip, currentSlice);
      }

      D3D12_TEXTURE_COPY_LOCATION src = {};
      src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

      // Use ring buffer or temporary staging buffer
      if (useRingBuffer) {
        src.pResource = ringBuffer->getUploadHeap();
        // Adjust layout offset to account for ring buffer base offset
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT adjustedLayout = layouts[layoutIdx];
        adjustedLayout.Offset += stagingBaseOffset;
        src.PlacedFootprint = adjustedLayout;
      } else {
        src.pResource = stagingBuffer.Get();
        src.PlacedFootprint = layouts[layoutIdx];
      }
      layoutIdx++;

      D3D12_BOX srcBox = {0, 0, 0, mipWidth, mipHeight, mipDepth};
      cmdList->CopyTextureRegion(&dst, range.x, range.y, range.z, &src, &srcBox);

      // const_cast needed (see above)
      const_cast<Texture*>(this)->transitionTo(cmdList.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, currentMip, currentSlice);
    }
  }

  cmdList->Close();

  // Execute once and wait once
  ID3D12CommandList* cmdLists[] = {cmdList.Get()};
  queue_->ExecuteCommandLists(1, cmdLists);

  // P0_DX12-005: Use upload fence for command allocator synchronization
  // P1_DX12-009: Use pre-allocated uploadFenceValue (already incremented for ring buffer)
  if (iglDevice_) {
    ID3D12Fence* uploadFence = iglDevice_->getUploadFence();

    hr = queue_->Signal(uploadFence, uploadFenceValue);
    if (FAILED(hr)) {
      IGL_LOG_ERROR("Texture::upload: Failed to signal upload fence: 0x%08X\n", hr);
      // Return allocator with 0 to avoid blocking the pool
      iglDevice_->returnUploadCommandAllocator(cmdAlloc, 0);
      return Result(Result::Code::RuntimeError, "Failed to signal fence");
    }

    // Return allocator to pool with fence value (will be reused after fence signaled)
    iglDevice_->returnUploadCommandAllocator(cmdAlloc, uploadFenceValue);

    // P2_DX12-FIND-07: Track staging buffer for async cleanup (no synchronous wait)
    // P1_DX12-009: Only track temporary staging buffers (ring buffer is persistent)
    // DX12-NEW-02: Pass uploadFenceValue (already signaled above) to track with correct fence
    if (!useRingBuffer && stagingBuffer.Get()) {
      iglDevice_->trackUploadBuffer(std::move(stagingBuffer), uploadFenceValue);
    }
  } else {
    // Fallback for textures without iglDevice_ (shouldn't happen in normal flow)
    // In this case, we need to wait synchronously since we can't track the buffer
    igl::d3d12::ComPtr<ID3D12Fence> fence;
    hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf()));
    if (FAILED(hr)) {
      return Result(Result::Code::RuntimeError, "Failed to create fence");
    }

    queue_->Signal(fence.Get(), 1);

    FenceWaiter waiter(fence.Get(), 1);
    Result waitResult = waiter.wait();
    if (!waitResult.isOk()) {
      return waitResult;  // Propagate detailed timeout/setup error
    }
  }

  return Result();
}

Result Texture::uploadCube(const TextureRangeDesc& range,
                          TextureCubeFace face,
                          const void* data,
                          size_t bytesPerRow) const {
  // TASK_P2_DX12-FIND-12: Implement cube texture upload
  // Cube textures are stored as texture arrays with 6 slices (one per face)
  // The upload() method already handles cube textures correctly when face/numFaces are set

  // Validate this is a cube texture
  if (type_ != TextureType::Cube) {
    return Result(Result::Code::ArgumentInvalid, "uploadCube called on non-cube texture");
  }

  // Create a modified range with the correct face index
  TextureRangeDesc cubeRange = range;
  cubeRange.face = static_cast<uint32_t>(face);  // Convert TextureCubeFace enum to face index (0-5)
  cubeRange.numFaces = 1;  // Upload single face

  // Delegate to upload() which handles cube texture subresource indexing correctly
  return upload(cubeRange, data, bytesPerRow);
}

Result Texture::uploadInternal(TextureType type,
                                const TextureRangeDesc& range,
                                const void* data,
                                size_t bytesPerRow,
                                const uint32_t* mipLevelBytes) const {
  if (!(type == TextureType::TwoD || type == TextureType::TwoDArray || type == TextureType::ThreeD || type == TextureType::Cube)) {
    return Result(Result::Code::Unimplemented, "Upload not implemented for this texture type");
  }

  // Delegate to upload() which now handles multi-mip, multi-layer, and cube textures natively
  return upload(range, data, bytesPerRow);
}

Dimensions Texture::getDimensions() const {
  return dimensions_;
}

uint32_t Texture::getNumLayers() const {
  return static_cast<uint32_t>(numLayers_);
}

TextureType Texture::getType() const {
  return type_;
}

TextureDesc::TextureUsage Texture::getUsage() const {
  return usage_;
}

uint32_t Texture::getSamples() const {
  return static_cast<uint32_t>(samples_);
}

uint32_t Texture::getNumMipLevels() const {
  return static_cast<uint32_t>(numMipLevels_);
}

uint64_t Texture::getTextureId() const {
  return reinterpret_cast<uint64_t>(resource_.Get());
}

TextureFormat Texture::getFormat() const {
  return format_;
}

bool Texture::isRequiredGenerateMipmap() const {
  return false;
}

void Texture::generateMipmap(ICommandQueue& /*cmdQueue*/, const TextureRangeDesc* /*range*/) const {
  IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap(cmdQueue) - START: numMips=%u\n", numMipLevels_);

  if (!device_ || !queue_ || !resource_.Get() || numMipLevels_ < 2) {
    IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap() - Skipping: device=%p queue=%p resource=%p numMips=%u\n",
                 device_, queue_, resource_.Get(), numMipLevels_);
    return;
  }

  D3D12_RESOURCE_DESC resourceDesc = resource_->GetDesc();

  // Only support 2D textures for mipmap generation
  if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
    IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap() - Skipping: only 2D textures supported (dimension=%d)\n",
                 (int)resourceDesc.Dimension);
    return;
  }

  // If texture wasn't created with RENDER_TARGET flag, we need to recreate it
  // D3D12 requires ALLOW_RENDER_TARGET for mipmap generation via rendering
  if (!(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
    IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap() - Recreating texture with RENDER_TARGET flag for mipmap generation\n");

    // Save current resource using ComPtr for automatic reference counting
    // Note: ComPtr copy is deleted, so we manually AddRef and Attach
    ID3D12Resource* rawOldResource = resource_.Get();
    if (rawOldResource) {
      rawOldResource->AddRef();
    }
    igl::d3d12::ComPtr<ID3D12Resource> oldResource;
    oldResource.Attach(rawOldResource);

    // Modify descriptor to add RENDER_TARGET flag
    resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    // Create new resource with RENDER_TARGET flag
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = resourceDesc.Format;

    igl::d3d12::ComPtr<ID3D12Resource> newResource;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        &clearValue,
        IID_PPV_ARGS(newResource.GetAddressOf()));

    if (FAILED(hr)) {
      IGL_LOG_ERROR("Texture::generateMipmap() - Failed to recreate texture with RENDER_TARGET flag\n");
      return;
    }

    // Copy mip 0 from old resource to new resource
    igl::d3d12::ComPtr<ID3D12CommandAllocator> copyAlloc;
    igl::d3d12::ComPtr<ID3D12GraphicsCommandList> copyList;
    if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(copyAlloc.GetAddressOf())))) {
      IGL_LOG_ERROR("Texture::generateMipmap() - Failed to create copy command allocator\n");
      return;
    }
    if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, copyAlloc.Get(), nullptr, IID_PPV_ARGS(copyList.GetAddressOf())))) {
      IGL_LOG_ERROR("Texture::generateMipmap() - Failed to create copy command list\n");
      return;
    }

    // Transition old resource to COPY_SOURCE
    D3D12_RESOURCE_BARRIER barrierOld = {};
    barrierOld.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierOld.Transition.pResource = oldResource.Get();
    barrierOld.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrierOld.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrierOld.Transition.Subresource = 0;
    copyList->ResourceBarrier(1, &barrierOld);

    // Copy mip 0
    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource = oldResource.Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource = newResource.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    copyList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    // Transition new resource to PIXEL_SHADER_RESOURCE for mipmap generation
    D3D12_RESOURCE_BARRIER barrierNew = {};
    barrierNew.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrierNew.Transition.pResource = newResource.Get();
    barrierNew.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrierNew.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrierNew.Transition.Subresource = 0;
    copyList->ResourceBarrier(1, &barrierNew);

    copyList->Close();
    ID3D12CommandList* copyLists[] = {copyList.Get()};
    queue_->ExecuteCommandLists(1, copyLists);

    // Wait for copy to complete
    igl::d3d12::ComPtr<ID3D12Fence> copyFence;
    if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(copyFence.GetAddressOf())))) {
      IGL_LOG_ERROR("Texture::generateMipmap() - Failed to create copy fence\n");
      return;
    }
    queue_->Signal(copyFence.Get(), 1);

    FenceWaiter waiter(copyFence.Get(), 1);
    Result waitResult = waiter.wait();
    if (!waitResult.isOk()) {
      IGL_LOG_ERROR("Texture::generateMipmap() - Fence wait failed: %s\n",
                    waitResult.message.c_str());
      return;
    }

    // oldResource will be automatically released by ComPtr destructor

    // Replace resource with new one (need const_cast since function is const)
    auto& mutableResource = const_cast<igl::d3d12::ComPtr<ID3D12Resource>&>(resource_);
    mutableResource.Reset();
    mutableResource = std::move(newResource);

    // Update state tracking for new resource - all mips are now in PIXEL_SHADER_RESOURCE
    // const_cast needed because generateMipmap is const (required by ITexture interface)
    // but state tracking is non-const by design
    const_cast<Texture*>(this)->initializeStateTracking(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // Update resourceDesc for the rest of the function
    resourceDesc = resource_->GetDesc();

    IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap() - Texture recreated successfully\n");
  }

  IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap() - Proceeding with mipmap generation\n");

  D3D12_DESCRIPTOR_RANGE ranges[2] = {};
  ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  ranges[0].NumDescriptors = 1;
  ranges[0].BaseShaderRegister = 0;
  ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
  ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
  ranges[1].NumDescriptors = 1;
  ranges[1].BaseShaderRegister = 0;
  ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  D3D12_ROOT_PARAMETER params[2] = {};
  params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[0].DescriptorTable.NumDescriptorRanges = 1;
  params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
  // C-006: Enable texture access in all shader stages (vertex, pixel, etc.)
  params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[1].DescriptorTable.NumDescriptorRanges = 1;
  params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
  // C-006: Enable sampler access in all shader stages (vertex, pixel, etc.)
  params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
  rsDesc.NumParameters = 2;
  rsDesc.pParameters = params;
  rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  igl::d3d12::ComPtr<ID3DBlob> sig, err;
  if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, sig.GetAddressOf(), err.GetAddressOf()))) {
    return;
  }
  igl::d3d12::ComPtr<ID3D12RootSignature> rootSig;
  if (FAILED(device_->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(rootSig.GetAddressOf())))) {
    return;
  }

  static const char* kVS = R"(
struct VSOut { float4 pos: SV_POSITION; float2 uv: TEXCOORD0; };
VSOut main(uint id: SV_VertexID) {
  float2 p = float2((id << 1) & 2, id & 2);
  VSOut o; o.pos = float4(p*float2(2,-2)+float2(-1,1), 0, 1); o.uv = p; return o;
}
)";
  static const char* kPS = R"(
Texture2D tex0 : register(t0);
SamplerState smp : register(s0);
float4 main(float4 pos:SV_POSITION, float2 uv:TEXCOORD0) : SV_TARGET { return tex0.SampleLevel(smp, uv, 0); }
)";

  // Initialize DXC compiler (static, initialized once)
  static DXCCompiler dxcCompiler;
  static bool dxcInitialized = false;
  if (!dxcInitialized) {
    Result initResult = dxcCompiler.initialize();
    if (!initResult.isOk()) {
      IGL_LOG_ERROR("Texture::generateMipMaps - DXC initialization failed: %s\n", initResult.message.c_str());
      return;
    }
    dxcInitialized = true;
  }

  // Get shader model from device context (H-009)
  // DXC requires SM 6.0 minimum (SM 5.x deprecated)
  D3D_SHADER_MODEL shaderModel = D3D_SHADER_MODEL_6_0;  // Default fallback
  if (iglDevice_) {
    shaderModel = iglDevice_->getD3D12Context().getMaxShaderModel();
    int smMajor = (shaderModel >> 4) & 0xF;
    int smMinor = shaderModel & 0xF;
    IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap - Using Shader Model %d.%d\n", smMajor, smMinor);
  } else {
    IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap - No IGL device, using SM 6.0 fallback (DXC minimum)\n");
  }

  // Build dynamic shader targets
  std::string vsTarget = getShaderTarget(shaderModel, ShaderStage::Vertex);
  std::string psTarget = getShaderTarget(shaderModel, ShaderStage::Fragment);

  // Compile shaders with dynamic shader model (H-009)
  std::vector<uint8_t> vsBytecode, psBytecode;
  std::string vsErrors, psErrors;

  IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap - Compiling VS with target: %s\n", vsTarget.c_str());
  Result vsResult = dxcCompiler.compile(kVS, strlen(kVS), "main", vsTarget.c_str(), "MipmapGenerationVS", 0, vsBytecode, vsErrors);
  if (!vsResult.isOk()) {
    IGL_LOG_ERROR("Texture::generateMipMaps - VS compilation failed: %s\n%s\n", vsResult.message.c_str(), vsErrors.c_str());
    return;
  }

  IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap - Compiling PS with target: %s\n", psTarget.c_str());
  Result psResult = dxcCompiler.compile(kPS, strlen(kPS), "main", psTarget.c_str(), "MipmapGenerationPS", 0, psBytecode, psErrors);
  if (!psResult.isOk()) {
    IGL_LOG_ERROR("Texture::generateMipMaps - PS compilation failed: %s\n%s\n", psResult.message.c_str(), psErrors.c_str());
    return;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
  pso.pRootSignature = rootSig.Get();
  pso.VS = {vsBytecode.data(), vsBytecode.size()};
  pso.PS = {psBytecode.data(), psBytecode.size()};
  pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pso.RasterizerState.DepthClipEnable = TRUE;
  pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  pso.SampleMask = UINT_MAX;
  pso.SampleDesc.Count = 1;
  pso.NumRenderTargets = 1;
  pso.RTVFormats[0] = resourceDesc.Format;
  pso.DSVFormat = DXGI_FORMAT_UNKNOWN;

  igl::d3d12::ComPtr<ID3D12PipelineState> psoObj;
  if (FAILED(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(psoObj.GetAddressOf())))) {
    return;
  }

  // Create descriptor heap large enough for all mip levels
  // We need one SRV descriptor per mip level (numMipLevels_ - 1 blits)
  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.NumDescriptors = numMipLevels_ - 1;  // One SRV per source mip level
  srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  igl::d3d12::ComPtr<ID3D12DescriptorHeap> srvHeap;
  if (FAILED(device_->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(srvHeap.GetAddressOf())))) return;

  D3D12_DESCRIPTOR_HEAP_DESC smpHeapDesc = {};
  smpHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
  smpHeapDesc.NumDescriptors = 1;
  smpHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  igl::d3d12::ComPtr<ID3D12DescriptorHeap> smpHeap;
  if (FAILED(device_->CreateDescriptorHeap(&smpHeapDesc, IID_PPV_ARGS(smpHeap.GetAddressOf())))) return;

  // Pre-creation validation (TASK_P0_DX12-004)
  IGL_DEBUG_ASSERT(device_ != nullptr, "Device is null before CreateSampler");
  IGL_DEBUG_ASSERT(smpHeap.Get() != nullptr, "Sampler heap is null");

  // Fixed sampler
  D3D12_SAMPLER_DESC samp = {};
  samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  samp.MinLOD = 0; samp.MaxLOD = D3D12_FLOAT32_MAX;

  D3D12_CPU_DESCRIPTOR_HANDLE smpHandle = smpHeap->GetCPUDescriptorHandleForHeapStart();
  IGL_DEBUG_ASSERT(smpHandle.ptr != 0, "Sampler descriptor handle is invalid");
  device_->CreateSampler(&samp, smpHandle);

  igl::d3d12::ComPtr<ID3D12CommandAllocator> alloc;
  igl::d3d12::ComPtr<ID3D12GraphicsCommandList> list;
  if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(alloc.GetAddressOf())))) return;
  if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), psoObj.Get(), IID_PPV_ARGS(list.GetAddressOf())))) return;

  ID3D12DescriptorHeap* heaps[] = {srvHeap.Get(), smpHeap.Get()};
  list->SetDescriptorHeaps(2, heaps);
  list->SetPipelineState(psoObj.Get());
  list->SetGraphicsRootSignature(rootSig.Get());
  list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Get descriptor size for incrementing through the heap
  const UINT srvDescriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart = srvHeap->GetCPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart = srvHeap->GetGPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE smpGpu = smpHeap->GetGPUDescriptorHandleForHeapStart();

  // Create single RTV descriptor heap outside the loop (reused for all mip levels)
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.NumDescriptors = 1;
  igl::d3d12::ComPtr<ID3D12DescriptorHeap> rtvHeap;
  if (FAILED(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeap.GetAddressOf())))) return;
  D3D12_CPU_DESCRIPTOR_HANDLE rtvCpu = rtvHeap->GetCPUDescriptorHandleForHeapStart();

  // Ensure mip 0 is in PIXEL_SHADER_RESOURCE state for first SRV read
  // const_cast needed because generateMipmap is const (required by ITexture interface)
  // but state tracking is non-const by design
  const_cast<Texture*>(this)->transitionTo(list.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0);

  for (UINT mip = 0; mip + 1 < numMipLevels_; ++mip) {
    // Calculate descriptor handle for this mip level
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = srvCpuStart;
    srvCpu.ptr += mip * srvDescriptorSize;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = srvGpuStart;
    srvGpu.ptr += mip * srvDescriptorSize;

    // Pre-creation validation (TASK_P0_DX12-004)
    IGL_DEBUG_ASSERT(device_ != nullptr, "Device is null before CreateShaderResourceView");
    IGL_DEBUG_ASSERT(resource_.Get() != nullptr, "Resource is null before CreateShaderResourceView");
    IGL_DEBUG_ASSERT(srvCpu.ptr != 0, "SRV descriptor handle is invalid");

    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = resourceDesc.Format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MostDetailedMip = mip;
    srv.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(resource_.Get(), &srv, srvCpu);

    // Pre-creation validation (TASK_P0_DX12-004)
    IGL_DEBUG_ASSERT(device_ != nullptr, "Device is null before CreateRenderTargetView");
    IGL_DEBUG_ASSERT(resource_.Get() != nullptr, "Resource is null before CreateRenderTargetView");
    IGL_DEBUG_ASSERT(rtvCpu.ptr != 0, "RTV descriptor handle is invalid");

    D3D12_RENDER_TARGET_VIEW_DESC rtv = {};
    rtv.Format = resourceDesc.Format;
    rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtv.Texture2D.MipSlice = mip + 1;

    // Reuse the same RTV heap by recreating the view for each mip level
    device_->CreateRenderTargetView(resource_.Get(), &rtv, rtvCpu);

    // Transition mip level to render target using state tracking
    // T10: const_cast needed (see above)
    const_cast<Texture*>(this)->transitionTo(list.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, mip + 1, 0);

    list->OMSetRenderTargets(1, &rtvCpu, FALSE, nullptr);
    const UINT w = std::max<UINT>(1u, (UINT)(resourceDesc.Width >> (mip + 1)));
    const UINT h = std::max<UINT>(1u, (UINT)(resourceDesc.Height >> (mip + 1)));
    D3D12_VIEWPORT vp{0.0f, 0.0f, (FLOAT)w, (FLOAT)h, 0.0f, 1.0f};
    D3D12_RECT sc{0, 0, (LONG)w, (LONG)h};
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &sc);

    list->SetGraphicsRootDescriptorTable(0, srvGpu);
    list->SetGraphicsRootDescriptorTable(1, smpGpu);
    list->DrawInstanced(3, 1, 0, 0);

    // Transition mip level to shader resource for next iteration
    // T10: const_cast needed (see above)
    const_cast<Texture*>(this)->transitionTo(list.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mip + 1, 0);
  }

  list->Close();
  ID3D12CommandList* lists[] = {list.Get()};
  queue_->ExecuteCommandLists(1, lists);

  igl::d3d12::ComPtr<ID3D12Fence> fence;
  if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())))) return;
  queue_->Signal(fence.Get(), 1);

  FenceWaiter waiter(fence.Get(), 1);
  Result waitResult = waiter.wait();
  if (!waitResult.isOk()) {
    IGL_LOG_ERROR("Texture::generateMipmap() - Fence wait failed: %s\n",
                  waitResult.message.c_str());
  }
}

void Texture::generateMipmap(ICommandBuffer& /*cmdBuffer*/, const TextureRangeDesc* /*range*/) const {
  IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap(cmdBuffer) - START: numMips=%u\n", numMipLevels_);

  if (!device_ || !queue_ || !resource_.Get() || numMipLevels_ < 2) {
    IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap(cmdBuffer) - Skipping: device=%p queue=%p resource=%p numMips=%u\n",
                 device_, queue_, resource_.Get(), numMipLevels_);
    return;
  }

  D3D12_RESOURCE_DESC resourceDesc = resource_->GetDesc();

  // Only support 2D textures for mipmap generation
  if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
    IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap(cmdBuffer) - Skipping: only 2D textures supported\n");
    return;
  }

  // Check if texture was created with RENDER_TARGET flag (required for mipmap generation)
  if (!(resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)) {
    IGL_D3D12_LOG_VERBOSE("Texture::generateMipmap(cmdBuffer) - Skipping: texture not created with RENDER_TARGET usage\n");
    IGL_D3D12_LOG_VERBOSE("  To enable mipmap generation, create texture with TextureDesc::TextureUsageBits::Attachment\n");
    return;
  }
  D3D12_DESCRIPTOR_RANGE ranges[2] = {};
  ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  ranges[0].NumDescriptors = 1;
  ranges[0].BaseShaderRegister = 0;
  ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
  ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
  ranges[1].NumDescriptors = 1;
  ranges[1].BaseShaderRegister = 0;
  ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
  D3D12_ROOT_PARAMETER params[2] = {};
  params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[0].DescriptorTable.NumDescriptorRanges = 1;
  params[0].DescriptorTable.pDescriptorRanges = &ranges[0];
  // C-006: Enable texture access in all shader stages (vertex, pixel, etc.)
  params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[1].DescriptorTable.NumDescriptorRanges = 1;
  params[1].DescriptorTable.pDescriptorRanges = &ranges[1];
  // C-006: Enable sampler access in all shader stages (vertex, pixel, etc.)
  params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
  rsDesc.NumParameters = 2;
  rsDesc.pParameters = params;
  rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  igl::d3d12::ComPtr<ID3DBlob> sig, err;
  if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, sig.GetAddressOf(), err.GetAddressOf()))) {
    return;
  }
  igl::d3d12::ComPtr<ID3D12RootSignature> rootSig;
  if (FAILED(device_->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(rootSig.GetAddressOf())))) {
    return;
  }
  static const char* kVS = R"(
struct VSOut { float4 pos: SV_POSITION; float2 uv: TEXCOORD0; };
VSOut main(uint id: SV_VertexID) {
  float2 p = float2((id << 1) & 2, id & 2);
  VSOut o; o.pos = float4(p*float2(2,-2)+float2(-1,1), 0, 1); o.uv = p; return o;
}
)";
  static const char* kPS = R"(
Texture2D tex0 : register(t0);
SamplerState smp : register(s0);
float4 main(float4 pos:SV_POSITION, float2 uv:TEXCOORD0) : SV_TARGET { return tex0.SampleLevel(smp, uv, 0); }
)";

  // Initialize DXC compiler (static, initialized once)
  static DXCCompiler dxcCompiler;
  static bool dxcInitialized = false;
  if (!dxcInitialized) {
    Result initResult = dxcCompiler.initialize();
    if (!initResult.isOk()) {
      IGL_LOG_ERROR("Texture::upload - DXC initialization failed: %s\n", initResult.message.c_str());
      return;
    }
    dxcInitialized = true;
  }

  // Get shader model from device context (H-009)
  // DXC requires SM 6.0 minimum (SM 5.x deprecated)
  D3D_SHADER_MODEL shaderModel = D3D_SHADER_MODEL_6_0;  // Default fallback
  if (iglDevice_) {
    shaderModel = iglDevice_->getD3D12Context().getMaxShaderModel();
    int smMajor = (shaderModel >> 4) & 0xF;
    int smMinor = shaderModel & 0xF;
    IGL_D3D12_LOG_VERBOSE("Texture::upload - Using Shader Model %d.%d\n", smMajor, smMinor);
  } else {
    IGL_D3D12_LOG_VERBOSE("Texture::upload - No IGL device, using SM 6.0 fallback (DXC minimum)\n");
  }

  // Build dynamic shader targets
  std::string vsTarget = getShaderTarget(shaderModel, ShaderStage::Vertex);
  std::string psTarget = getShaderTarget(shaderModel, ShaderStage::Fragment);

  // Compile shaders with dynamic shader model (H-009)
  std::vector<uint8_t> vsBytecode, psBytecode;
  std::string vsErrors, psErrors;

  IGL_D3D12_LOG_VERBOSE("Texture::upload - Compiling VS with target: %s\n", vsTarget.c_str());
  Result vsResult = dxcCompiler.compile(kVS, strlen(kVS), "main", vsTarget.c_str(), "TextureUploadVS", 0, vsBytecode, vsErrors);
  if (!vsResult.isOk()) {
    IGL_LOG_ERROR("Texture::upload - VS compilation failed: %s\n%s\n", vsResult.message.c_str(), vsErrors.c_str());
    return;
  }

  IGL_D3D12_LOG_VERBOSE("Texture::upload - Compiling PS with target: %s\n", psTarget.c_str());
  Result psResult = dxcCompiler.compile(kPS, strlen(kPS), "main", psTarget.c_str(), "TextureUploadPS", 0, psBytecode, psErrors);
  if (!psResult.isOk()) {
    IGL_LOG_ERROR("Texture::upload - PS compilation failed: %s\n%s\n", psResult.message.c_str(), psErrors.c_str());
    return;
  }

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
  pso.pRootSignature = rootSig.Get();
  pso.VS = {vsBytecode.data(), vsBytecode.size()};
  pso.PS = {psBytecode.data(), psBytecode.size()};
  pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pso.RasterizerState.DepthClipEnable = TRUE;
  pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  pso.SampleMask = UINT_MAX;
  pso.SampleDesc.Count = 1;
  pso.NumRenderTargets = 1;
  pso.RTVFormats[0] = resourceDesc.Format;
  pso.DSVFormat = DXGI_FORMAT_UNKNOWN;
  igl::d3d12::ComPtr<ID3D12PipelineState> psoObj;
  if (FAILED(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(psoObj.GetAddressOf())))) return;
  // Create descriptor heap large enough for all mip levels
  // We need one SRV descriptor per mip level (numMipLevels_ - 1 blits)
  D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
  srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srvHeapDesc.NumDescriptors = numMipLevels_ - 1;  // One SRV per source mip level
  srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  igl::d3d12::ComPtr<ID3D12DescriptorHeap> srvHeap;
  if (FAILED(device_->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(srvHeap.GetAddressOf())))) return;
  D3D12_DESCRIPTOR_HEAP_DESC smpHeapDesc = {};
  smpHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
  smpHeapDesc.NumDescriptors = 1;
  smpHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

  igl::d3d12::ComPtr<ID3D12DescriptorHeap> smpHeap;
  if (FAILED(device_->CreateDescriptorHeap(&smpHeapDesc, IID_PPV_ARGS(smpHeap.GetAddressOf())))) return;

  // Pre-creation validation (TASK_P0_DX12-004)
  IGL_DEBUG_ASSERT(device_ != nullptr, "Device is null before CreateSampler");
  IGL_DEBUG_ASSERT(smpHeap.Get() != nullptr, "Sampler heap is null");

  D3D12_SAMPLER_DESC samp = {};
  samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
  samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  samp.MinLOD = 0; samp.MaxLOD = D3D12_FLOAT32_MAX;

  D3D12_CPU_DESCRIPTOR_HANDLE smpHandle = smpHeap->GetCPUDescriptorHandleForHeapStart();
  IGL_DEBUG_ASSERT(smpHandle.ptr != 0, "Sampler descriptor handle is invalid");
  device_->CreateSampler(&samp, smpHandle);
  igl::d3d12::ComPtr<ID3D12CommandAllocator> alloc;
  igl::d3d12::ComPtr<ID3D12GraphicsCommandList> list;
  if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(alloc.GetAddressOf())))) return;
  if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc.Get(), psoObj.Get(), IID_PPV_ARGS(list.GetAddressOf())))) return;
  ID3D12DescriptorHeap* heaps[] = {srvHeap.Get(), smpHeap.Get()};
  list->SetDescriptorHeaps(2, heaps);
  list->SetPipelineState(psoObj.Get());
  list->SetGraphicsRootSignature(rootSig.Get());
  list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  // Get descriptor size for incrementing through the heap
  const UINT srvDescriptorSize = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  D3D12_CPU_DESCRIPTOR_HANDLE srvCpuStart = srvHeap->GetCPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE srvGpuStart = srvHeap->GetGPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE smpGpu = smpHeap->GetGPUDescriptorHandleForHeapStart();

  // Create single RTV descriptor heap outside the loop (reused for all mip levels)
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.NumDescriptors = 1;
  igl::d3d12::ComPtr<ID3D12DescriptorHeap> rtvHeap;
  if (FAILED(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeap.GetAddressOf())))) return;
  D3D12_CPU_DESCRIPTOR_HANDLE rtvCpu = rtvHeap->GetCPUDescriptorHandleForHeapStart();

  // Ensure mip 0 is in PIXEL_SHADER_RESOURCE state for first SRV read
  // const_cast needed because generateMipmap is const (required by ITexture interface)
  // but state tracking is non-const by design
  const_cast<Texture*>(this)->transitionTo(list.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0, 0);

  for (UINT mip = 0; mip + 1 < numMipLevels_; ++mip) {
    // Calculate descriptor handle for this mip level
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = srvCpuStart;
    srvCpu.ptr += mip * srvDescriptorSize;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = srvGpuStart;
    srvGpu.ptr += mip * srvDescriptorSize;

    // Pre-creation validation (TASK_P0_DX12-004)
    IGL_DEBUG_ASSERT(device_ != nullptr, "Device is null before CreateShaderResourceView");
    IGL_DEBUG_ASSERT(resource_.Get() != nullptr, "Resource is null before CreateShaderResourceView");
    IGL_DEBUG_ASSERT(srvCpu.ptr != 0, "SRV descriptor handle is invalid");

    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Format = resourceDesc.Format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MostDetailedMip = mip;
    srv.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(resource_.Get(), &srv, srvCpu);

    // Pre-creation validation (TASK_P0_DX12-004)
    IGL_DEBUG_ASSERT(device_ != nullptr, "Device is null before CreateRenderTargetView");
    IGL_DEBUG_ASSERT(resource_.Get() != nullptr, "Resource is null before CreateRenderTargetView");
    IGL_DEBUG_ASSERT(rtvCpu.ptr != 0, "RTV descriptor handle is invalid");

    D3D12_RENDER_TARGET_VIEW_DESC rtv = {};
    rtv.Format = resourceDesc.Format;
    rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtv.Texture2D.MipSlice = mip + 1;

    // Reuse the same RTV heap by recreating the view for each mip level
    device_->CreateRenderTargetView(resource_.Get(), &rtv, rtvCpu);

    // Transition mip level to render target using state tracking
    // T10: const_cast needed (see above)
    const_cast<Texture*>(this)->transitionTo(list.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, mip + 1, 0);

    list->OMSetRenderTargets(1, &rtvCpu, FALSE, nullptr);
    const UINT w = std::max<UINT>(1u, (UINT)(resourceDesc.Width >> (mip + 1)));
    const UINT h = std::max<UINT>(1u, (UINT)(resourceDesc.Height >> (mip + 1)));
    D3D12_VIEWPORT vp{0.0f, 0.0f, (FLOAT)w, (FLOAT)h, 0.0f, 1.0f};
    D3D12_RECT sc{0, 0, (LONG)w, (LONG)h};
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &sc);
    list->SetGraphicsRootDescriptorTable(0, srvGpu);
    list->SetGraphicsRootDescriptorTable(1, smpGpu);
    list->DrawInstanced(3, 1, 0, 0);

    // Transition mip level to shader resource for next iteration
    // T10: const_cast needed (see above)
    const_cast<Texture*>(this)->transitionTo(list.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, mip + 1, 0);
  }
  list->Close();
  ID3D12CommandList* lists[] = {list.Get()};
  queue_->ExecuteCommandLists(1, lists);
  igl::d3d12::ComPtr<ID3D12Fence> fence;
  if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf())))) return;
  queue_->Signal(fence.Get(), 1);

  FenceWaiter waiter(fence.Get(), 1);
  Result waitResult = waiter.wait();
  if (!waitResult.isOk()) {
    IGL_LOG_ERROR("Texture::generateMipmap(cmdBuffer) - Fence wait failed: %s\n",
                  waitResult.message.c_str());
  }
}

void Texture::initializeStateTracking(D3D12_RESOURCE_STATES initialState) {
  // T24: Simplified per-subresource state tracking - always use vector (no dual-mode)
  if (!resource_.Get()) {
    subresourceStates_.clear();
    return;
  }

  const uint32_t mipLevels = static_cast<uint32_t>(std::max<size_t>(numMipLevels_, 1));
  uint32_t arraySize;
  if (type_ == TextureType::ThreeD) {
    arraySize = 1u;
  } else if (type_ == TextureType::Cube) {
    arraySize = static_cast<uint32_t>(std::max<size_t>(numLayers_, 1)) * 6u;
  } else {
    arraySize = static_cast<uint32_t>(std::max<size_t>(numLayers_, 1));
  }
  const size_t numSubresources = static_cast<size_t>(mipLevels) * arraySize;
  subresourceStates_.assign(numSubresources, initialState);
}

uint32_t Texture::calcSubresourceIndex(uint32_t mipLevel, uint32_t layer) const {
  // For views, map view-local coordinates to resource coordinates.
  // Note: mipLevelOffset_ and arraySliceOffset_ are resource-relative (accumulated at view creation for nested views).
  const uint32_t resourceMip = isView_ ? (mipLevel + mipLevelOffset_) : mipLevel;
  const uint32_t resourceLayer = isView_ ? (layer + arraySliceOffset_) : layer;

  // Use state owner's dimensions for subresource calculation
  const Texture* owner = getStateOwner();
  IGL_DEBUG_ASSERT(owner != nullptr, "State owner must not be null");
  const uint32_t mipLevels = static_cast<uint32_t>(std::max<size_t>(owner->numMipLevels_, 1));
  uint32_t arraySize;
  if (owner->type_ == TextureType::ThreeD) {
    arraySize = 1u;
  } else if (owner->type_ == TextureType::Cube) {
    // Cube textures: 6 faces per layer
    arraySize = static_cast<uint32_t>(std::max<size_t>(owner->numLayers_, 1)) * 6u;
  } else {
    arraySize = static_cast<uint32_t>(std::max<size_t>(owner->numLayers_, 1));
  }
  const uint32_t clampedMip = std::min(resourceMip, mipLevels - 1);
  const uint32_t clampedLayer = std::min(resourceLayer, arraySize - 1);
  // D3D12CalcSubresource formula: MipSlice + (ArraySlice * MipLevels)
  const uint32_t subresource = clampedMip + (clampedLayer * mipLevels);
#ifdef IGL_DEBUG
  // Reduce log verbosity - only log in debug builds for views
  if ((type_ == TextureType::Cube || type_ == TextureType::TwoDArray) && isView_) {
    IGL_D3D12_LOG_VERBOSE("calcSubresourceIndex (view): type=%d, mip=%u, layer=%u -> resource mip=%u, layer=%u -> subresource=%u\n",
                 (int)type_, mipLevel, layer, resourceMip, resourceLayer, subresource);
  }
#endif
  return subresource;
}

void Texture::transitionTo(ID3D12GraphicsCommandList* commandList,
                           D3D12_RESOURCE_STATES newState,
                           uint32_t mipLevel,
                           uint32_t layer) {
  // T24: Simplified per-subresource state tracking
  Texture* owner = getStateOwner();
  if (!commandList || !owner || !owner->resource_.Get() || owner->subresourceStates_.empty()) {
    return;
  }

  // T10/T24: For depth-stencil textures, transition ALL subresources (both depth and stencil planes)
  const auto props = getProperties();
  const bool isDepthStencil = props.isDepthOrStencil() && props.hasStencil();

  if (isDepthStencil) {
    // T10 Fix: Verify all subresources are in same state before using ALL_SUBRESOURCES
    D3D12_RESOURCE_STATES firstState = owner->subresourceStates_[0];
    bool allSameState = true;
    for (const auto& state : owner->subresourceStates_) {
      if (state != firstState) {
        allSameState = false;
        IGL_LOG_ERROR("Depth-stencil texture has divergent subresource states - this violates invariant\n");
        break;
      }
    }

    if (firstState == newState) {
      return;  // All subresources already in target state
    }

    // Safety check: If states have diverged, return early to avoid invalid ALL_SUBRESOURCES barrier.
    if (!allSameState) {
      IGL_DEBUG_ASSERT(false, "Depth-stencil textures must have uniform state across all subresources");
      return;  // Intentionally skip transition to avoid undefined behavior
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = owner->resource_.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = firstState;
    barrier.Transition.StateAfter = newState;
    commandList->ResourceBarrier(1, &barrier);

    // Update all subresource states
    for (auto& state : owner->subresourceStates_) {
      state = newState;
    }
    return;
  }

  // Non-depth-stencil: transition single subresource
  const uint32_t subresource = calcSubresourceIndex(mipLevel, layer);
  if (subresource >= owner->subresourceStates_.size()) {
    return;
  }

  auto& currentState = owner->subresourceStates_[subresource];
  if (currentState == newState) {
    return;
  }

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = owner->resource_.Get();
  barrier.Transition.Subresource = subresource;
  barrier.Transition.StateBefore = currentState;
  barrier.Transition.StateAfter = newState;
  commandList->ResourceBarrier(1, &barrier);

  currentState = newState;
}

void Texture::transitionAll(ID3D12GraphicsCommandList* commandList,
                            D3D12_RESOURCE_STATES newState) {
  // T24: Simplified per-subresource state tracking
  Texture* owner = getStateOwner();
  if (!commandList || !owner || !owner->resource_.Get() || owner->subresourceStates_.empty()) {
    return;
  }

  // Check if all subresources are already in the target state
  bool allMatch = true;
  for (const auto& state : owner->subresourceStates_) {
    if (state != newState) {
      allMatch = false;
      break;
    }
  }
  if (allMatch) {
    return;
  }

  // Transition each subresource individually
  for (size_t i = 0; i < owner->subresourceStates_.size(); ++i) {
    auto& state = owner->subresourceStates_[i];
    if (state == newState) {
      continue;
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = owner->resource_.Get();
    barrier.Transition.Subresource = static_cast<UINT>(i);
    barrier.Transition.StateBefore = state;
    barrier.Transition.StateAfter = newState;
    commandList->ResourceBarrier(1, &barrier);

    state = newState;
  }
}

D3D12_RESOURCE_STATES Texture::getSubresourceState(uint32_t mipLevel, uint32_t layer) const {
  // T24: Simplified per-subresource state tracking
  const Texture* owner = getStateOwner();
  if (owner->subresourceStates_.empty()) {
    return D3D12_RESOURCE_STATE_COMMON;
  }

  const uint32_t index = calcSubresourceIndex(mipLevel, layer);
  if (index >= owner->subresourceStates_.size()) {
    return D3D12_RESOURCE_STATE_COMMON;
  }

  return owner->subresourceStates_[index];
}

} // namespace igl::d3d12


