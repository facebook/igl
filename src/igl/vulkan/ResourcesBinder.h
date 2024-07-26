/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Buffer.h>
#include <igl/Common.h>
#include <igl/Texture.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/VulkanImmediateCommands.h>

namespace igl::vulkan {

namespace util {
struct SpvModuleInfo;
} // namespace util

class Buffer;
class PipelineState;
class SamplerState;
class Texture;
class VulkanBuffer;
class VulkanSampler;
class VulkanTexture;

struct BindingsBuffers {
  VkDescriptorBufferInfo buffers[IGL_UNIFORM_BLOCKS_BINDING_MAX] = {};
};

struct BindingsTextures {
  igl::vulkan::VulkanTexture* textures[IGL_TEXTURE_SAMPLERS_MAX] = {};
  igl::vulkan::VulkanSampler* samplers[IGL_TEXTURE_SAMPLERS_MAX] = {};
};

/** @brief Stores uniform and storage buffer bindings, as well as bindings for textures and sampler
 * states for Vulkan. This class maintains vectors for each type of shader resource available in IGL
 * and records the association between binding locations (indices) and the Vulkan objects, while
 * performing specific checks for each type of resource when they are bound. The associations
 * between indices and resources is kept locally and does not affect the GPU until
 * `updateBindings()` is called. This class also records which resource types need to be updated
 * when `updateBindings()` is called and provides a convenience function to update the descriptor
 * sets on the context for all resource types. It only performs the update for a resource type that
 * has been modified after the last call to update the bindings. An instance of this class is bound
 * to one bind point only (VkPipelineBindPoint), which is VK_PIPELINE_BIND_POINT_GRAPHICS by
 * default.
 */
class ResourcesBinder final {
 public:
  ResourcesBinder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                  const VulkanContext& ctx,
                  VkPipelineBindPoint bindPoint);

  /// @brief Binds a uniform buffer with an offset to index equal to `index`
  void bindBuffer(uint32_t index,
                  igl::vulkan::Buffer* buffer,
                  size_t bufferOffset,
                  size_t bufferSize);

  /// @brief Binds a sampler state to index equal to `index`
  void bindSamplerState(uint32_t index, igl::vulkan::SamplerState* samplerState);

  /// @brief Binds a texture to index equal to `index`
  void bindTexture(uint32_t index, igl::vulkan::Texture* tex);

  /// @brief Convenience function that updates all bindings in the context for all resource types
  /// that have been modified since the last time this function was called
  void updateBindings(VkPipelineLayout layout, const vulkan::PipelineState& state);

  /// @brief If the pipeline passed in as a parameter is different than the last pipeline bound
  /// through this class, binds it and cache it as the last pipeline bound. Does nothing otherwise
  void bindPipeline(VkPipeline pipeline, const util::SpvModuleInfo* info);

 private:
  friend class VulkanContext;
  friend class RenderCommandEncoder;

  [[nodiscard]] bool isGraphics() const {
    return bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS;
  }

  /*
   * @brief Bitwise flags for dirty descriptor sets (per each supported resource type)
   */
  enum DirtyFlagBits : uint8_t {
    DirtyFlagBits_Textures = 1 << 0,
    DirtyFlagBits_Buffers = 1 << 1,
  };

 private:
  const VulkanContext& ctx_;
  VkCommandBuffer cmdBuffer_ = VK_NULL_HANDLE;
  VkPipeline lastPipelineBound_ = VK_NULL_HANDLE;
  uint32_t isDirtyFlags_ = DirtyFlagBits_Textures | DirtyFlagBits_Buffers;
  BindingsTextures bindingsTextures_;
  BindingsBuffers bindingsBuffers_;
  VkPipelineBindPoint bindPoint_ = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

} // namespace igl::vulkan
