/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/ResourcesBinder.h>
#include <igl/vulkan/SamplerState.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanTexture.h>

namespace igl {

namespace vulkan {

ResourcesBinder::ResourcesBinder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                                 const VulkanContext& ctx,
                                 VkPipelineBindPoint bindPoint) :
  ctx_(ctx), cmdBuffer_(commandBuffer->getVkCommandBuffer()), bindPoint_(bindPoint) {}

void ResourcesBinder::bindUniformBuffer(uint32_t index,
                                        igl::vulkan::Buffer* buffer,
                                        size_t bufferOffset) {
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
    slot = {buf, bufferOffset, VK_WHOLE_SIZE};
    isDirtyUniformBuffers_ = true;
  }
}

void ResourcesBinder::bindStorageBuffer(uint32_t index,
                                        igl::vulkan::Buffer* buffer,
                                        size_t bufferOffset) {
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
    slot = {buf, bufferOffset, VK_WHOLE_SIZE};
    isDirtyStorageBuffers_ = true;
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
    isDirtyTextures_ = true;
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
    igl::vulkan::VulkanImage& img = newTexture->getVulkanImage();
    IGL_ASSERT_MSG(img.samples_ == VK_SAMPLE_COUNT_1_BIT,
                   "Multisampled images cannot be sampled in shaders");
    if (bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      IGL_ASSERT(img.imageLayout_ == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    } else {
      IGL_ASSERT(img.imageLayout_ == VK_IMAGE_LAYOUT_GENERAL);
    }
  }
#endif // IGL_DEBUG

  if (bindingsTextures_.textures[index] != newTexture) {
    bindingsTextures_.textures[index] = newTexture;
    isDirtyTextures_ = true;
  }
}

void ResourcesBinder::updateBindings() {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_UPDATE);

  if (isDirtyTextures_) {
    ctx_.updateBindingsTextures(cmdBuffer_, bindPoint_, bindingsTextures_);
    isDirtyTextures_ = false;
  }
  if (isDirtyUniformBuffers_) {
    ctx_.updateBindingsUniformBuffers(cmdBuffer_, bindPoint_, bindingsUniformBuffers_);
    isDirtyUniformBuffers_ = false;
  }
  if (isDirtyStorageBuffers_) {
    ctx_.updateBindingsStorageBuffers(cmdBuffer_, bindPoint_, bindingsStorageBuffers_);
    isDirtyStorageBuffers_ = false;
  }
}

void ResourcesBinder::bindPipeline(VkPipeline pipeline) {
  if (lastPipelineBound_ == pipeline) {
    return;
  }

  lastPipelineBound_ = pipeline;

  if (pipeline != VK_NULL_HANDLE) {
#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("%p vkCmdBindPipeline(%u, %p)\n", cmdBuffer_, bindPoint_, pipeline);
#endif // IGL_VULKAN_PRINT_COMMANDS
    ctx_.vf_.vkCmdBindPipeline(cmdBuffer_, bindPoint_, pipeline);
  }
}

} // namespace vulkan
} // namespace igl
