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

class Buffer final : public IBuffer {
 public:
  Buffer(const BufferDesc& desc) : desc_(desc) {}
  ~Buffer() override = default;

  Result upload(const void* data, const BufferRange& range) override;
  void* map(const BufferRange& range, Result* IGL_NULLABLE outResult) override;
  void unmap() override;

  BufferDesc::BufferAPIHint requestedApiHints() const noexcept override;
  BufferDesc::BufferAPIHint acceptedApiHints() const noexcept override;
  ResourceStorage storage() const noexcept override;

  size_t getSizeInBytes() const override;
  uint64_t gpuAddress(size_t offset = 0) const override;

  BufferDesc::BufferType getBufferType() const override;

 private:
  Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
  BufferDesc desc_;
  void* mappedPtr_ = nullptr;
};

} // namespace igl::d3d12
