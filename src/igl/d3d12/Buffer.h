/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/d3d12/Common.h>

namespace igl::d3d12 {

class Device;

class Buffer final : public IBuffer, public std::enable_shared_from_this<Buffer> {
 public:
  Buffer(Device& device,
         Microsoft::WRL::ComPtr<ID3D12Resource> resource,
         const BufferDesc& desc,
         D3D12_RESOURCE_STATES initialState);
  ~Buffer() override;

  Result upload(const void* data, const BufferRange& range) override;
  void* map(const BufferRange& range, Result* IGL_NULLABLE outResult) override;
  void unmap() override;

  BufferDesc::BufferAPIHint requestedApiHints() const noexcept override;
  BufferDesc::BufferAPIHint acceptedApiHints() const noexcept override;
  ResourceStorage storage() const noexcept override;

  size_t getSizeInBytes() const override;
  uint64_t gpuAddress(size_t offset = 0) const override;

  BufferDesc::BufferType getBufferType() const override;

  // D3D12-specific accessors
  ID3D12Resource* getResource() const { return resource_.Get(); }

 private:
  [[nodiscard]] D3D12_RESOURCE_STATES computeDefaultState(const BufferDesc& desc) const;

  Device* device_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
  BufferDesc desc_;
  void* mappedPtr_ = nullptr;
  ResourceStorage storage_ = ResourceStorage::Private;
  D3D12_RESOURCE_STATES defaultState_ = D3D12_RESOURCE_STATE_GENERIC_READ;
  D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_COMMON;

  // Staging buffer for mapping DEFAULT heap storage buffers requested as Shared
  Microsoft::WRL::ComPtr<ID3D12Resource> readbackStagingBuffer_;
};

} // namespace igl::d3d12
