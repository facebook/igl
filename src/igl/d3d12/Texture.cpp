/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/Texture.h>

namespace igl::d3d12 {

std::shared_ptr<Texture> Texture::createFromResource(ID3D12Resource* resource,
                                                       TextureFormat format,
                                                       const TextureDesc& desc,
                                                       ID3D12Device* device,
                                                       ID3D12CommandQueue* queue) {
  auto texture = std::make_shared<Texture>(format);
  texture->resource_ = resource;
  texture->device_ = device;
  texture->queue_ = queue;
  texture->format_ = format;
  texture->dimensions_ = Dimensions{desc.width, desc.height, desc.depth};
  texture->type_ = desc.type;
  texture->numLayers_ = desc.numLayers;
  texture->numMipLevels_ = desc.numMipLevels;
  texture->samples_ = desc.numSamples;
  texture->usage_ = desc.usage;

  // AddRef since we're storing a raw pointer from external source
  if (resource) {
    resource->AddRef();
  }

  return texture;
}

Result Texture::upload(const TextureRangeDesc& range,
                      const void* data,
                      size_t bytesPerRow) const {
  if (!device_ || !queue_ || !resource_.Get()) {
    return Result(Result::Code::RuntimeError, "Device, queue, or resource not available for upload");
  }

  if (!data) {
    return Result(Result::Code::ArgumentInvalid, "Upload data is null");
  }

  // Calculate dimensions and data size
  const uint32_t width = range.width > 0 ? range.width : dimensions_.width;
  const uint32_t height = range.height > 0 ? range.height : dimensions_.height;

  // Calculate bytes per row if not provided
  if (bytesPerRow == 0) {
    // Estimate based on format - this is a simplification
    size_t bytesPerPixel = 4; // Assume RGBA8 for now
    bytesPerRow = width * bytesPerPixel;
  }

  // Get the resource description to calculate required size
  D3D12_RESOURCE_DESC resourceDesc = resource_->GetDesc();

  // Get the layout information for the subresource
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
  UINT numRows;
  UINT64 rowSizeInBytes;
  UINT64 totalBytes;

  device_->GetCopyableFootprints(&resourceDesc, range.mipLevel, 1, 0,
                                  &layout, &numRows, &rowSizeInBytes, &totalBytes);

  // Create staging buffer (upload heap)
  D3D12_HEAP_PROPERTIES uploadHeapProps = {};
  uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  uploadHeapProps.CreationNodeMask = 1;
  uploadHeapProps.VisibleNodeMask = 1;

  D3D12_RESOURCE_DESC stagingDesc = {};
  stagingDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  stagingDesc.Alignment = 0;
  stagingDesc.Width = totalBytes;
  stagingDesc.Height = 1;
  stagingDesc.DepthOrArraySize = 1;
  stagingDesc.MipLevels = 1;
  stagingDesc.Format = DXGI_FORMAT_UNKNOWN;
  stagingDesc.SampleDesc.Count = 1;
  stagingDesc.SampleDesc.Quality = 0;
  stagingDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  stagingDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  Microsoft::WRL::ComPtr<ID3D12Resource> stagingBuffer;
  HRESULT hr = device_->CreateCommittedResource(
      &uploadHeapProps,
      D3D12_HEAP_FLAG_NONE,
      &stagingDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(stagingBuffer.GetAddressOf()));

  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create staging buffer for texture upload");
  }

  // Map and copy data to staging buffer
  void* mappedData = nullptr;
  hr = stagingBuffer->Map(0, nullptr, &mappedData);
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to map staging buffer");
  }

  // Copy data row by row
  const uint8_t* srcData = static_cast<const uint8_t*>(data);
  uint8_t* dstData = static_cast<uint8_t*>(mappedData) + layout.Offset;

  for (UINT row = 0; row < numRows; ++row) {
    memcpy(dstData + row * layout.Footprint.RowPitch,
           srcData + row * bytesPerRow,
           std::min(static_cast<size_t>(rowSizeInBytes), bytesPerRow));
  }

  stagingBuffer->Unmap(0, nullptr);

  // Create command allocator and command list for upload
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdAlloc;
  hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        IID_PPV_ARGS(cmdAlloc.GetAddressOf()));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create command allocator for upload");
  }

  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
  hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), nullptr,
                                   IID_PPV_ARGS(cmdList.GetAddressOf()));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create command list for upload");
  }

  // Transition texture to COPY_DEST state
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = resource_.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.Subresource = range.mipLevel;

  cmdList->ResourceBarrier(1, &barrier);

  // Copy from staging buffer to texture
  D3D12_TEXTURE_COPY_LOCATION dst = {};
  dst.pResource = resource_.Get();
  dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dst.SubresourceIndex = range.mipLevel;

  D3D12_TEXTURE_COPY_LOCATION src = {};
  src.pResource = stagingBuffer.Get();
  src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  src.PlacedFootprint = layout;

  cmdList->CopyTextureRegion(&dst, range.x, range.y, range.z, &src, nullptr);

  // Transition texture back to COMMON state
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
  cmdList->ResourceBarrier(1, &barrier);

  cmdList->Close();

  // Execute command list
  ID3D12CommandList* cmdLists[] = {cmdList.Get()};
  queue_->ExecuteCommandLists(1, cmdLists);

  // Wait for upload to complete (synchronous for now)
  Microsoft::WRL::ComPtr<ID3D12Fence> fence;
  hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.GetAddressOf()));
  if (FAILED(hr)) {
    return Result(Result::Code::RuntimeError, "Failed to create fence for upload sync");
  }

  HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!fenceEvent) {
    return Result(Result::Code::RuntimeError, "Failed to create fence event");
  }

  queue_->Signal(fence.Get(), 1);
  fence->SetEventOnCompletion(1, fenceEvent);
  WaitForSingleObject(fenceEvent, INFINITE);
  CloseHandle(fenceEvent);

  return Result();
}

Result Texture::uploadCube(const TextureRangeDesc& /*range*/,
                          TextureCubeFace /*face*/,
                          const void* /*data*/,
                          size_t /*bytesPerRow*/) const {
  return Result(Result::Code::Unimplemented, "D3D12 Texture::uploadCube not yet implemented");
}

Result Texture::uploadInternal(TextureType type,
                                const TextureRangeDesc& range,
                                const void* data,
                                size_t bytesPerRow,
                                const uint32_t* /*mipLevelBytes*/) const {
  // For now, delegate to our D3D12-specific upload method
  if (type == TextureType::TwoD || type == TextureType::TwoDArray) {
    return upload(range, data, bytesPerRow);
  }

  return Result(Result::Code::Unimplemented, "Upload not implemented for this texture type");
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

void Texture::generateMipmap(ICommandQueue& /*cmdQueue*/,
                             const TextureRangeDesc* /*range*/) const {
  // TODO: Implement mipmap generation using compute shader or blit operations
}

void Texture::generateMipmap(ICommandBuffer& /*cmdBuffer*/,
                             const TextureRangeDesc* /*range*/) const {
  // TODO: Implement mipmap generation using compute shader or blit operations
}

} // namespace igl::d3d12
