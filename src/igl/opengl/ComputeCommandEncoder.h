/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/ComputeCommandEncoder.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/UniformAdapter.h>

namespace igl {
class ICommandBuffer;
class IComputePipelineState;
class ISamplerState;
class ITexture;
namespace opengl {

class ArrayBuffer;
class Buffer;
class UniformBuffer;
class ComputeCommandAdapter;

class ComputeCommandEncoder final : public IComputeCommandEncoder, public WithContext {
 public:
  explicit ComputeCommandEncoder(IContext& context);
  ~ComputeCommandEncoder() override;
  void bindComputePipelineState(
      const std::shared_ptr<IComputePipelineState>& pipelineState) override;
  // threadgroupCount is how many thread groups per grid in each dimension.
  // threadgroupSize is how many threads are in each threadgroup
  // total number of threads per grid is threadgroupCount * threadgroupSize
  void dispatchThreadGroups(const Dimensions& threadgroupCount,
                            const Dimensions& threadgroupSize) override;
  void endEncoding() override;

  void pushDebugGroupLabel(const std::string& label, const igl::Color& color) const override;
  void insertDebugEventLabel(const std::string& label, const igl::Color& color) const override;
  void popDebugGroupLabel() const override;
  void bindUniform(const UniformDesc& uniformDesc, const void* data) override;
  void bindTexture(size_t index, ITexture* texture) override;
  void bindBuffer(size_t index, const std::shared_ptr<IBuffer>& buffer, size_t offset) override;
  void bindBytes(size_t index, const void* data, size_t length) override;
  void bindPushConstants(size_t offset, const void* data, size_t length) override;

 private:
  std::unique_ptr<ComputeCommandAdapter> adapter_;
};

} // namespace opengl
} // namespace igl
