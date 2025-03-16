/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/SamplerState.h>
#include <igl/vulkan/Common.h>

namespace igl::vulkan {

class Device;

/**
 * @brief Encapsulates a VulkanSampler class and its descriptor, along with a vulkan::Device for
 * creating the resource. It's a friend of the Device, which calls the private create() method when
 * realizing the resource
 */
class SamplerState final : public ISamplerState {
  friend class Device;

 public:
  /**
   * @brief Instantiates an object of the class, but doesn't create the resources on the device.
   * After instantiation, the object contains a reference to the device for creating the resource,
   * which can be done by calling the create() method with the desired configuration
   */
  explicit SamplerState(Device& device);
  ~SamplerState() override = default;

  SamplerState(const SamplerState&) = delete;
  SamplerState& operator=(const SamplerState&) = delete;

  /**
   * @brief Returns the ID of the sampler. Its ID is the index of the sampler into the vector of
   * samplers stored in VulkanContext. The ID is set by the context when the sampler is created and
   * as soon as the sampler is stored in the vector of samplers maintained there.
   * This ID is intended for bindless rendering. See the ResourcesBinder and VulkanContext classes
   * for more information
   */
  [[nodiscard]] uint32_t getSamplerId() const;

  /**
   * @brief Returns true if this sampler is a YUV sampler.
   */
  [[nodiscard]] bool isYUV() const noexcept override;

 private:
  /**
   * @brief Creates the internal object instances based on the descriptor passed in as a parameter
   */
  Result create(const SamplerStateDesc& desc);

 private:
  friend class PipelineState;
  friend class ResourcesBinder;
  friend class VulkanContext;

  /** @brief The device used to create the resource */
  Device& device_;
  /** @brief The texture sampling configuration for accessing a texture */
  SamplerStateDesc desc_;
  /** @brief The VulkanSampler instance associated with this sampler */
  Holder<SamplerHandle> sampler_;
};

} // namespace igl::vulkan
