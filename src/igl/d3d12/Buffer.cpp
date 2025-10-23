/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/Buffer.h>
#include <cstring>

namespace igl::d3d12 {

Buffer::Buffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource, const BufferDesc& desc)
    : resource_(std::move(resource)), desc_(desc) {
  // Determine storage type based on heap properties
  if (resource_.Get()) {
    D3D12_HEAP_PROPERTIES heapProps;
    D3D12_HEAP_FLAGS heapFlags;
    resource_->GetHeapProperties(&heapProps, &heapFlags);

    if (heapProps.Type == D3D12_HEAP_TYPE_UPLOAD) {
      storage_ = ResourceStorage::Shared;
    } else if (heapProps.Type == D3D12_HEAP_TYPE_READBACK) {
      storage_ = ResourceStorage::Shared;
    } else {
      storage_ = ResourceStorage::Private;
    }
  }
}

Buffer::~Buffer() {
  if (mappedPtr_) {
    unmap();
  }
}

Result Buffer::upload(const void* data, const BufferRange& range) {
  if (resource_.Get() == nullptr) {
    return Result(Result::Code::ArgumentInvalid, "Buffer resource is null");
  }

  if (!data) {
    return Result(Result::Code::ArgumentInvalid, "Upload data is null");
  }

  // For UPLOAD heap, map, copy, unmap
  if (storage_ == ResourceStorage::Shared) {
    void* mappedData = nullptr;
    D3D12_RANGE readRange = {0, 0}; // Not reading from GPU

    HRESULT hr = resource_->Map(0, &readRange, &mappedData);
    if (FAILED(hr)) {
      return Result(Result::Code::RuntimeError, "Failed to map buffer");
    }

    uint8_t* dest = static_cast<uint8_t*>(mappedData) + range.offset;
    std::memcpy(dest, data, range.size);

    D3D12_RANGE writtenRange = {range.offset, range.offset + range.size};
    resource_->Unmap(0, &writtenRange);

    return Result();
  }

  // For DEFAULT heap, need upload via intermediate buffer
  // TODO: Implement staging buffer upload for GPU-only buffers
  return Result(Result::Code::Unimplemented, "Upload to DEFAULT heap not yet implemented");
}

void* Buffer::map(const BufferRange& range, Result* IGL_NULLABLE outResult) {
  if (resource_.Get() == nullptr) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Buffer resource is null");
    return nullptr;
  }

  // Validate range
  if (range.offset > desc_.length || range.size > desc_.length ||
      (range.offset + range.size) > desc_.length) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Map range is out of bounds");
    return nullptr;
  }

  if (storage_ != ResourceStorage::Shared) {
    Result::setResult(outResult, Result::Code::Unsupported,
                      "Cannot map GPU-only buffer (use ResourceStorage::Shared)");
    return nullptr;
  }

  if (mappedPtr_) {
    // Already mapped, return offset pointer
    Result::setOk(outResult);
    return static_cast<uint8_t*>(mappedPtr_) + range.offset;
  }

  D3D12_RANGE readRange = {0, 0}; // Not reading from GPU
  HRESULT hr = resource_->Map(0, &readRange, &mappedPtr_);

  if (FAILED(hr)) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Failed to map buffer");
    return nullptr;
  }

  Result::setOk(outResult);
  return static_cast<uint8_t*>(mappedPtr_) + range.offset;
}

void Buffer::unmap() {
  if (!mappedPtr_) {
    return;
  }

  if (resource_.Get()) {
    resource_->Unmap(0, nullptr);
  }

  mappedPtr_ = nullptr;
}

BufferDesc::BufferAPIHint Buffer::requestedApiHints() const noexcept {
  return desc_.hint;
}

BufferDesc::BufferAPIHint Buffer::acceptedApiHints() const noexcept {
  return desc_.hint;
}

ResourceStorage Buffer::storage() const noexcept {
  return storage_;
}

size_t Buffer::getSizeInBytes() const {
  return desc_.length;
}

uint64_t Buffer::gpuAddress(size_t offset) const {
  if (resource_.Get() == nullptr) {
    return 0;
  }

  return resource_->GetGPUVirtualAddress() + offset;
}

BufferDesc::BufferType Buffer::getBufferType() const {
  return desc_.type;
}

} // namespace igl::d3d12
