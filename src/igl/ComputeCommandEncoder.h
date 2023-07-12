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
#include <igl/Uniform.h>

// TODO: Remove this once we move BindTarget from CommandBuffer.h to this header
#include <igl/CommandBuffer.h>

namespace igl {

class IBuffer;
class IComputePipelineState;
class ISamplerState;
class ITexture;

/**
 * @brief A descriptor struct for IComputeCommandEncoder
 *
 */
struct ComputeCommandEncoderDesc {
  // Placeholder
};

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

  /**
   * @brief Binds an individual uniform.
   *
   * @param uniformDesc A descriptor for the uniform.
   * @param data The uniform data to bind.
   * Exclusively used when uniform blocks are not supported.
   */
  virtual void bindUniform(const UniformDesc& uniformDesc, const void* data) = 0;
  /**
   * @brief Sets a texture for the compute function
   *
   * @param index An index in the texture argument table.
   * @param texture The texture to set in the texture argument table.
   */
  virtual void bindTexture(size_t index, ITexture* texture) = 0;
  /**
   * @brief Sets a buffer for the compute function
   *
   * @param index An index for the buffer argument table.
   * @param buffer The buffer to set in the buffer argument table.
   * @param offset Where the data begins in bytes from the start of the buffer.
   */
  virtual void bindBuffer(size_t index, const std::shared_ptr<IBuffer>& buffer, size_t offset) = 0;
  /**
   * @brief Sets a block of data for the compute function. A buffer will be created behind the
   * scenes to hold the input data and bound to the buffer argument table at the specified index.
   *
   * @param index An index for the buffer argument table.
   * @param data The memory address from which to copy the data.
   * @param length The number of bytes to copy.
   */
  virtual void bindBytes(size_t index, const void* data, size_t length) = 0;
  /**
   * @brief Sets a block of data for the compute function.
   *
   * @param offset An offset bytes into the push constants buffer.
   * @param data The memory address from which to copy the data.
   * @param length The number of bytes to copy.
   */
  virtual void bindPushConstants(size_t offset, const void* data, size_t length) = 0;
  /**
   * @brief Sets the compute pipeline state object.
   *
   * @param pipelineState A pipeline state object to set for the compute function.
   */
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
   * @param threadgroupSize The number of threads in one threadgroup, in each dimension.
   */
  virtual void dispatchThreadGroups(const Dimensions& threadgroupCount,
                                    const Dimensions& threadgroupSize) = 0;
};

} // namespace igl
