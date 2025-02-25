/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Metal/Metal.h>
#include <igl/ComputeCommandEncoder.h>

namespace igl::metal {

class ComputeCommandEncoder final : public IComputeCommandEncoder {
 public:
  explicit ComputeCommandEncoder(id<MTLCommandBuffer> buffer);
  ~ComputeCommandEncoder() override = default;

  void endEncoding() override;

  void bindComputePipelineState(
      const std::shared_ptr<IComputePipelineState>& pipelineState) override;
  // threadgroupCount is how many thread groups per grid in each dimension.
  // threadgroupSize is how many threads are in each threadgroup
  // total number of threads per grid is threadgroupCount * threadgroupSize
  void dispatchThreadGroups(const Dimensions& threadgroupCount,
                            const Dimensions& threadgroupSize,
                            const Dependencies& dependencies) override;
  void pushDebugGroupLabel(const char* label, const igl::Color& color) const override;
  void insertDebugEventLabel(const char* label, const igl::Color& color) const override;
  void popDebugGroupLabel() const override;
  void bindUniform(const UniformDesc& uniformDesc, const void* data) override;
  void bindTexture(uint32_t index, ITexture* texture) override;
  void bindImageTexture(uint32_t index, ITexture* texture, TextureFormat format) override;
  void bindBuffer(uint32_t index, IBuffer* buffer, size_t offset, size_t bufferSize) override;
  void bindBytes(size_t index, const void* data, size_t length) override;
  void bindPushConstants(const void* data, size_t length, size_t offset) override;

 private:
  id<MTLComputeCommandEncoder> encoder_ = nil;
  // 4 KB - page aligned memory for metal managed resource
  static constexpr uint32_t MAX_RECOMMENDED_BYTES = 4 * 1024;
};

} // namespace igl::metal
