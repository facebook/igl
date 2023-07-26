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

namespace igl {
namespace vulkan {

class Buffer;
class SamplerState;
class Texture;
class VulkanBuffer;
class VulkanSampler;
class VulkanTexture;

struct BufferInfo {
  igl::vulkan::Buffer* buf = nullptr;
  size_t offset = 0;
};

struct BindingsTextures {
  igl::vulkan::VulkanTexture* textures[IGL_TEXTURE_SAMPLERS_MAX] = {};
  igl::vulkan::VulkanSampler* samplers[IGL_TEXTURE_SAMPLERS_MAX] = {};
};

struct BindingsBuffers {
  BufferInfo buffers[IGL_UNIFORM_BLOCKS_BINDING_MAX] = {};
};

class ResourcesBinder final {
 public:
  ResourcesBinder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                  const VulkanContext& ctx,
                  VkPipelineBindPoint bindPoint);

  void bindBuffer(uint32_t index, igl::vulkan::Buffer* buffer, size_t bufferOffset);
  void bindSamplerState(uint32_t index, igl::vulkan::SamplerState* samplerState);
  void bindTexture(uint32_t index, igl::vulkan::Texture* tex);

  void updateBindings();
  void bindPipeline(VkPipeline pipeline);

 private:
  friend class VulkanContext;

  bool isGraphics() const {
    return bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS;
  }

 private:
  const VulkanContext& ctx_;
  VkCommandBuffer cmdBuffer_ = VK_NULL_HANDLE;
  VkPipeline lastPipelineBound_ = VK_NULL_HANDLE;
  bool isDirtyTextures_ = true;
  bool isDirtyBuffers_ = true;
  BindingsTextures bindingsTextures_;
  BindingsBuffers bindingsBuffers_;
  VkPipelineBindPoint bindPoint_ = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

} // namespace vulkan
} // namespace igl
