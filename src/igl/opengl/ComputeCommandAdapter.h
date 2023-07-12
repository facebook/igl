/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <array>
#include <bitset>
#include <functional>
#include <igl/Buffer.h>
#include <igl/Common.h>
#include <igl/opengl/UnbindPolicy.h>
#include <igl/opengl/UniformAdapter.h>
#include <igl/opengl/WithContext.h>
#include <unordered_map>

namespace igl {

class ITexture;
class IComputePipelineState;
class ISamplerState;

namespace opengl {
class Buffer;

class ComputeCommandAdapter final : public WithContext {
 private:
  using StateBits = uint8_t;
  enum class StateMask : StateBits { NONE = 0, PIPELINE = 1 << 1 };

  struct BufferState {
    std::shared_ptr<Buffer> resource;
    size_t offset;
  };

  using TextureState = ITexture*;
  using TextureStates = std::array<TextureState, IGL_TEXTURE_SAMPLERS_MAX>;

 public:
  ComputeCommandAdapter(IContext& context);

  void clearTextures();
  void setTexture(ITexture* texture, size_t index);

  void clearBuffers();
  void setBuffer(std::shared_ptr<Buffer> buffer, size_t offset, int index);

  void clearUniformBuffers();
  void setBlockUniform(const std::shared_ptr<Buffer>& buffer,
                       size_t offset,
                       int index,
                       Result* outResult = nullptr);
  void setUniform(const UniformDesc& uniformDesc, const void* data, Result* outResult = nullptr);

  void setPipelineState(const std::shared_ptr<IComputePipelineState>& newValue);
  void dispatchThreadGroups(const Dimensions& threadgroupCount,
                            const Dimensions& /*threadgroupSize*/);

  void endEncoding();

 private:
  void clearDependentResources(const std::shared_ptr<IComputePipelineState>& newValue);
  void willDispatch();
  void didDispatch();

  bool isDirty(StateMask mask) const {
    return (dirtyStateBits_ & EnumToValue(mask)) != 0;
  }
  void setDirty(StateMask mask) {
    dirtyStateBits_ |= EnumToValue(mask);
  }
  void clearDirty(StateMask mask) {
    dirtyStateBits_ &= ~EnumToValue(mask);
  }

 private:
  std::array<BufferState, IGL_VERTEX_BUFFER_MAX> buffers_;
  std::bitset<IGL_VERTEX_BUFFER_MAX> buffersDirty_;
  std::bitset<IGL_TEXTURE_SAMPLERS_MAX> textureStatesDirty_;
  TextureStates textureStates_;
  UniformAdapter uniformAdapter_;
  StateBits dirtyStateBits_ = EnumToValue(StateMask::NONE);
  std::shared_ptr<IComputePipelineState> pipelineState_;
};
} // namespace opengl
} // namespace igl
