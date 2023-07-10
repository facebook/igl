/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/Uniform.h>

#include <array>
#include <unordered_map>
#include <vector>

namespace igl {
namespace opengl {
class IContext;

class UniformAdapter {
 public:
  // Feel like this can be placed somewhere better
  enum PipelineType {
    Render = 1,
    Compute = 2,
  };

  UniformAdapter(const IContext& context, PipelineType type);
  void shrinkUniformUsage();
  void clearUniformBuffers();
  void setUniform(const UniformDesc& uniformDesc, const void* data, Result* outResult);
  void setUniformBuffer(const std::shared_ptr<IBuffer>& buffer,
                        size_t offset,
                        int index,
                        Result* outResult);

  uint32_t getMaxUniforms() const {
    return maxUniforms_;
  }

  void bindToPipeline(IContext& context);

 private:
  struct UniformState {
    UniformState() = default;
    UniformState(UniformDesc d, std::ptrdiff_t o) : desc(std::move(d)), dataOffset(o) {}

    UniformDesc desc;
    std::ptrdiff_t dataOffset = 0;
  };

  std::vector<UniformState> uniforms_;
  std::vector<uint8_t> uniformData_;
  uint32_t maxUniforms_ = 1024;

  // map for uniform binding indices to the buffers
  std::unordered_map<int, std::pair<std::shared_ptr<IBuffer>, size_t>> uniformBufferBindingMap_;
  uint32_t uniformBuffersDirtyMask_ = 0;
  static_assert(sizeof(uniformBuffersDirtyMask_) * 8 >= IGL_UNIFORM_BLOCKS_BINDING_MAX,
                "uniformBuffersDirtyMask size is not enough to fit the flags");

  // Store a copy of uniform data when setUniform is used to avoid the client from managing the
  // memory
  std::ptrdiff_t usedUniformDataBytes_ = 0;
  uint16_t shrinkUniformDataCounter_ = 0;
  PipelineType pipelineType_;

#if IGL_DEBUG
  std::vector<bool> uniformsDirty_;
#endif // IGL_DEBUG
};

} // namespace opengl
} // namespace igl
