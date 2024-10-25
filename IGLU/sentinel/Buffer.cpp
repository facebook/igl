/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include <IGLU/sentinel/Buffer.h>

#include <IGLU/sentinel/Assert.h>
#include <igl/IGL.h>

namespace iglu::sentinel {

Buffer::Buffer(bool shouldAssert, size_t size) : size_(size), shouldAssert_(shouldAssert) {}

igl::Result Buffer::upload(const void* IGL_NULLABLE /*data*/, const igl::BufferRange& /*range*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return igl::Result(igl::Result::Code::Unimplemented, "Not Implemented");
}

void* IGL_NULLABLE Buffer::map(const igl::BufferRange& /*range*/,
                               igl::Result* IGL_NULLABLE /*outResult*/) {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return nullptr;
}

void Buffer::unmap() {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
}

igl::BufferDesc::BufferAPIHint Buffer::requestedApiHints() const noexcept {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return 0;
}

igl::BufferDesc::BufferAPIHint Buffer::acceptedApiHints() const noexcept {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return 0;
}

igl::ResourceStorage Buffer::storage() const noexcept {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return igl::ResourceStorage::Invalid;
}

size_t Buffer::getSizeInBytes() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return size_;
}

uint64_t Buffer::gpuAddress(size_t /*offset*/) const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return 0;
}

igl::BufferDesc::BufferType Buffer::getBufferType() const {
  IGLU_SENTINEL_ASSERT_IF_NOT(shouldAssert_);
  return 0;
}

} // namespace iglu::sentinel
