/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>

namespace iglu::sentinel {

/**
 * Sentinel Buffer intended for safe use where access to a real buffer is not available.
 * Use cases include returning a reference to a buffer from a raw pointer when a valid buffer is not
 * available.
 * All methods return nullptr, the default value or an error.
 */
class Buffer : public igl::IBuffer {
 public:
  explicit Buffer(bool shouldAssert = true, size_t size = 0);

  [[nodiscard]] igl::Result upload(const void* IGL_NULLABLE data,
                                   const igl::BufferRange& range) final;
  void* IGL_NULLABLE map(const igl::BufferRange& range, igl::Result* IGL_NULLABLE outResult) final;
  void unmap() final;
  [[nodiscard]] igl::BufferDesc::BufferAPIHint requestedApiHints() const noexcept final;
  [[nodiscard]] igl::BufferDesc::BufferAPIHint acceptedApiHints() const noexcept final;
  [[nodiscard]] igl::ResourceStorage storage() const noexcept final;
  [[nodiscard]] size_t getSizeInBytes() const final;
  [[nodiscard]] uint64_t gpuAddress(size_t offset = 0) const final;

 private:
  size_t size_;
  [[maybe_unused]] bool shouldAssert_;
};

} // namespace iglu::sentinel
