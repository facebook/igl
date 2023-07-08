/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/CommandEncoder.h>
#include <igl/Common.h>
#include <igl/Framebuffer.h>

namespace igl {

class IBuffer;
class IComputePipelineState;
class ISamplerState;
class ITexture;

/**
 * @brief Object for encoding commands in a compute pass
 *
 * Create IComputeCommandEncoder object by calling ICommandBuffer::createComputeCommandEncoder.
 * You can encode multiple commands in a single compute pass.
 * To encode a compute command:
 * - Call bindComputePipelineState method to set compute pipeline state.
 * - Call one or more other bind functions to specify parameters for the compute function.
 * - Call dispatchThreadgroups method to encode a compute command.
 * You can encode another compute command after calling dispatchThreadgroups or call endEncoding to
 * finish the compute pass. You must always call endEncoding before the encoder is released or
 * before creating another encoder.
 */
class IComputeCommandEncoder : public ICommandEncoder {
 public:
  /**
   * @brief Construct a new IComputeCommandEncoder object
   *
   */
  IComputeCommandEncoder() : ICommandEncoder::ICommandEncoder(nullptr) {}

  /**
   * @brief Destroy the IComputeCommandEncoder object
   *
   */
  ~IComputeCommandEncoder() override = default;

  virtual void useTexture(const std::shared_ptr<ITexture>& texture) = 0;
  virtual void bindPushConstants(size_t offset, const void* data, size_t length) = 0;
  virtual void bindComputePipelineState(
      const std::shared_ptr<IComputePipelineState>& pipelineState) = 0;
  /**
   * @brief Encodes a compute command using a grid aligned to threadgroup boundaries. If the size of
   * your data doesn't match the size of the grid, you may have to perform boundary checks in your
   * compute function. When the compute command is encoded, any necessary references to parameters
   * or resources previously set on the encoder are recorded as part of the command. After encoding
   * a command, you can safely change the encoding state to set up parameters needed to encode other
   * commands. The total number of threads per grid is threadgroupCount * threadgroupSize.
   *
   * @param threadgroupCount The number of thread groups in the grid, in each dimension.
   */
  virtual void dispatchThreadGroups(const Dimensions& threadgroupCount) = 0;
};

} // namespace igl
