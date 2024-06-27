/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/PipelineState.h>
#include <igl/vulkan/ResourcesBinder.h>
#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanImageView.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanTexture.h>

namespace igl::vulkan {

ResourcesBinder::ResourcesBinder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                                 const VulkanContext& ctx,
                                 VkPipelineBindPoint bindPoint) :
  ctx_(ctx), cmdBuffer_(commandBuffer->getVkCommandBuffer()), bindPoint_(bindPoint) {}

void ResourcesBinder::bindUniformBuffer(uint32_t index,
                                        igl::vulkan::Buffer* buffer,
                                        size_t bufferOffset,
                                        size_t bufferSize) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(index < IGL_UNIFORM_BLOCKS_BINDING_MAX)) {
    IGL_ASSERT_MSG(false, "Buffer index should not exceed kMaxBindingSlots");
    return;
  }

  IGL_ASSERT_MSG((buffer->getBufferType() & BufferDesc::BufferTypeBits::Uniform) != 0,
                 "The buffer must be a uniform buffer");

  VkBuffer buf = buffer ? buffer->getVkBuffer() : ctx_.dummyUniformBuffer_->getVkBuffer();
  VkDescriptorBufferInfo& slot = bindingsUniformBuffers_.buffers[index];

  if (slot.buffer != buf || slot.offset != bufferOffset) {
    slot = {buf, bufferOffset, bufferSize ? bufferSize : VK_WHOLE_SIZE};
    isDirtyFlags_ |= DirtyFlagBits_UniformBuffers;
  }
}

void ResourcesBinder::bindStorageBuffer(uint32_t index,
                                        igl::vulkan::Buffer* buffer,
                                        size_t bufferOffset,
                                        size_t bufferSize) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(index < IGL_UNIFORM_BLOCKS_BINDING_MAX)) {
    IGL_ASSERT_MSG(false, "Buffer index should not exceed kMaxBindingSlots");
    return;
  }

  IGL_ASSERT_MSG((buffer->getBufferType() & BufferDesc::BufferTypeBits::Storage) != 0,
                 "The buffer must be a storage buffer");

  VkBuffer buf = buffer ? buffer->getVkBuffer() : ctx_.dummyStorageBuffer_->getVkBuffer();
  VkDescriptorBufferInfo& slot = bindingsStorageBuffers_.buffers[index];

  if (slot.buffer != buf || slot.offset != bufferOffset) {
    slot = {buf, bufferOffset, bufferSize ? bufferSize : VK_WHOLE_SIZE};
    isDirtyFlags_ |= DirtyFlagBits_StorageBuffers;
  }
}

void ResourcesBinder::bindSamplerState(uint32_t index, igl::vulkan::SamplerState* samplerState) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(index < IGL_TEXTURE_SAMPLERS_MAX)) {
    IGL_ASSERT_MSG(false, "Invalid sampler index");
    return;
  }

  igl::vulkan::VulkanSampler* newSampler = samplerState ? samplerState->sampler_.get() : nullptr;

  if (bindingsTextures_.samplers[index] != newSampler) {
    bindingsTextures_.samplers[index] = newSampler;
    isDirtyFlags_ |= DirtyFlagBits_Textures;
  }
}

void ResourcesBinder::bindTexture(uint32_t index, igl::vulkan::Texture* tex) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(index < IGL_TEXTURE_SAMPLERS_MAX)) {
    IGL_ASSERT_MSG(false, "Invalid texture index");
    return;
  }

  if (tex) {
    const bool isSampled = tex ? (tex->getUsage() & TextureDesc::TextureUsageBits::Sampled) > 0
                               : false;
    const bool isStorage = tex ? (tex->getUsage() & TextureDesc::TextureUsageBits::Storage) > 0
                               : false;

    if (isGraphics()) {
      if (!IGL_VERIFY(isSampled || isStorage)) {
        IGL_ASSERT_MSG(false,
                       "Did you forget to specify TextureUsageBits::Sampled or "
                       "TextureUsageBits::Storage on your texture? `Sampled` is used for sampling; "
                       "`Storage` is used for load/store operations");
      }
    } else {
      if (!IGL_VERIFY(isStorage)) {
        IGL_ASSERT_MSG(false,
                       "Did you forget to specify TextureUsageBits::Storage on your texture?");
      }
    }
  }

  igl::vulkan::VulkanTexture* newTexture = tex ? &tex->getVulkanTexture() : nullptr;

#if IGL_DEBUG
  if (newTexture) {
    const auto& img = newTexture->getVulkanImage();
    IGL_ASSERT_MSG(img.samples_ == VK_SAMPLE_COUNT_1_BIT,
                   "Multisampled images cannot be sampled in shaders");
    if (bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      // If you trip this assert, then you are likely using an IGL texture
      // that was not rendered to by IGL. If that's the case, then make sure
      // the underlying image is transitioned to
      // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      // IGL_ASSERT(img.imageLayout_ == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    } else {
      IGL_ASSERT(img.imageLayout_ == VK_IMAGE_LAYOUT_GENERAL);
    }
  }
#endif // IGL_DEBUG

  if (bindingsTextures_.textures[index] != newTexture) {
    bindingsTextures_.textures[index] = newTexture;
    isDirtyFlags_ |= DirtyFlagBits_Textures;
  }
}

void ResourcesBinder::updateBindings(VkPipelineLayout layout, const vulkan::PipelineState& state) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_UPDATE);

  IGL_ASSERT(layout != VK_NULL_HANDLE);

  if (isDirtyFlags_ & DirtyFlagBits_Textures) {
    ctx_.updateBindingsTextures(cmdBuffer_,
                                layout,
                                bindPoint_,
                                bindingsTextures_,
                                *state.dslCombinedImageSamplers_,
                                state.info_);
  }
  if (isDirtyFlags_ & DirtyFlagBits_UniformBuffers) {
    ctx_.updateBindingsUniformBuffers(cmdBuffer_,
                                      layout,
                                      bindPoint_,
                                      bindingsUniformBuffers_,
                                      *state.dslUniformBuffers_,
                                      state.info_);
  }
  if (isDirtyFlags_ & DirtyFlagBits_StorageBuffers) {
    ctx_.updateBindingsStorageBuffers(cmdBuffer_,
                                      layout,
                                      bindPoint_,
                                      bindingsStorageBuffers_,
                                      *state.dslStorageBuffers_,
                                      state.info_);
  }

  isDirtyFlags_ = 0;
}

void ResourcesBinder::bindPipeline(VkPipeline pipeline, const util::SpvModuleInfo* info) {
  if (lastPipelineBound_ == pipeline) {
    return;
  }

  if (info) {
    // a new pipeline might want a new descriptors configuration
    if (!info->uniformBuffers.empty()) {
      isDirtyFlags_ |= DirtyFlagBits_UniformBuffers;
    }
    if (!info->storageBuffers.empty()) {
      isDirtyFlags_ |= DirtyFlagBits_StorageBuffers;
    }
    if (!info->textures.empty()) {
      isDirtyFlags_ |= DirtyFlagBits_Textures;
    }
  }

  lastPipelineBound_ = pipeline;

  if (pipeline != VK_NULL_HANDLE) {
#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("%p vkCmdBindPipeline(%s, %p)\n",
                 cmdBuffer_,
                 bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS ? "GRAPHICS" : "COMPUTE",
                 pipeline);
#endif // IGL_VULKAN_PRINT_COMMANDS
    ctx_.vf_.vkCmdBindPipeline(cmdBuffer_, bindPoint_, pipeline);
  }
}

} // namespace igl::vulkan
