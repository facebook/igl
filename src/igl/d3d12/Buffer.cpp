/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/Buffer.h>

namespace igl::d3d12 {

Result Buffer::upload(const void* /*data*/, const BufferRange& /*range*/) {
  return Result(Result::Code::Unimplemented, "D3D12 Buffer::upload not yet implemented");
}

void* Buffer::map(const BufferRange& /*range*/, Result* IGL_NULLABLE outResult) {
  Result::setResult(outResult, Result::Code::Unimplemented, "D3D12 Buffer::map not yet implemented");
  return nullptr;
}

void Buffer::unmap() {}

BufferDesc::BufferAPIHint Buffer::requestedApiHints() const noexcept {
  return desc_.hint;
}

BufferDesc::BufferAPIHint Buffer::acceptedApiHints() const noexcept {
  return desc_.hint;
}

ResourceStorage Buffer::storage() const noexcept {
  return desc_.storage;
}

size_t Buffer::getSizeInBytes() const {
  return desc_.length;
}

uint64_t Buffer::gpuAddress(size_t /*offset*/) const {
  return 0;
}

BufferDesc::BufferType Buffer::getBufferType() const {
  return desc_.type;
}

} // namespace igl::d3d12
