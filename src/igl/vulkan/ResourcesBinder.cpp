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
#include <igl/vulkan/VulkanTexture.h>

namespace igl::vulkan {

ResourcesBinder::ResourcesBinder(const CommandBuffer* commandBuffer,
                                 VulkanContext& ctx,
                                 VkPipelineBindPoint bindPoint) :
  ctx_(ctx),
  cmdBuffer_(commandBuffer ? commandBuffer->getVkCommandBuffer() : VK_NULL_HANDLE),
  bindPoint_(bindPoint),
  nextSubmitHandle_(commandBuffer ? commandBuffer->getNextSubmitHandle()
                                  : VulkanImmediateCommands::SubmitHandle{}) {}

void ResourcesBinder::bindBuffer(uint32_t index,
                                 igl::vulkan::Buffer* buffer,
                                 size_t bufferOffset,
                                 size_t bufferSize) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_DEBUG_VERIFY(index < IGL_UNIFORM_BLOCKS_BINDING_MAX)) {
    IGL_DEBUG_ABORT("Buffer index should not exceed kMaxBindingSlots");
    return;
  }

  const bool isUniformBuffer =
      ((buffer->getBufferType() & BufferDesc::BufferTypeBits::Uniform) != 0);

  IGL_DEBUG_ASSERT(isUniformBuffer ||
                       ((buffer->getBufferType() & BufferDesc::BufferTypeBits::Storage) != 0),
                   "The buffer must be a uniform or storage buffer");
  if (bufferOffset) {
    const auto& limits = ctx_.getVkPhysicalDeviceProperties().limits;
    const uint32_t alignment =
        static_cast<uint32_t>(isUniformBuffer ? limits.minUniformBufferOffsetAlignment
                                              : limits.minStorageBufferOffsetAlignment);
    if (!IGL_DEBUG_VERIFY((alignment == 0) || (bufferOffset % alignment == 0))) {
      IGL_LOG_ERROR("`bufferOffset = %u` must be a multiple of `VkPhysicalDeviceLimits::%s = %u`",
                    static_cast<uint32_t>(bufferOffset),
                    isUniformBuffer ? "minUniformBufferOffsetAlignment"
                                    : "minStorageBufferOffsetAlignment",
                    alignment);
      return;
    }
  }

  VkBuffer buf = buffer ? buffer->getVkBuffer() : ctx_.dummyUniformBuffer_->getVkBuffer();
  VkDescriptorBufferInfo& slot = bindingsBuffers_.buffers[index];

  if (slot.buffer != buf || slot.offset != bufferOffset) {
    slot = {buf, bufferOffset, bufferSize ? bufferSize : VK_WHOLE_SIZE};
    isDirtyFlags_ |= DirtyFlagBits_Buffers;
  }
}

void ResourcesBinder::bindSamplerState(uint32_t index, igl::vulkan::SamplerState* samplerState) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_DEBUG_VERIFY(index < IGL_TEXTURE_SAMPLERS_MAX)) {
    IGL_DEBUG_ABORT("Invalid sampler index");
    return;
  }

  igl::vulkan::VulkanSampler* newSampler = samplerState ? ctx_.samplers_.get(samplerState->sampler_)
                                                        : nullptr;

  if (bindingsTextures_.samplers[index] != newSampler->vkSampler) {
    bindingsTextures_.samplers[index] = newSampler->vkSampler;
    isDirtyFlags_ |= DirtyFlagBits_Textures;
  }
}

void ResourcesBinder::bindTexture(uint32_t index, igl::vulkan::Texture* tex) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_DEBUG_VERIFY(index < IGL_TEXTURE_SAMPLERS_MAX)) {
    IGL_DEBUG_ABORT("Invalid texture index");
    return;
  }

  if (tex) {
    const bool isSampled = tex ? (tex->getUsage() & TextureDesc::TextureUsageBits::Sampled) > 0
                               : false;
    const bool isStorage = tex ? (tex->getUsage() & TextureDesc::TextureUsageBits::Storage) > 0
                               : false;

    if (isGraphics()) {
      if (!IGL_DEBUG_VERIFY(isSampled || isStorage)) {
        IGL_DEBUG_ABORT(
            "Did you forget to specify TextureUsageBits::Sampled or "
            "TextureUsageBits::Storage on your texture? `Sampled` is used for sampling; "
            "`Storage` is used for load/store operations");
      }
    } else {
      if (!IGL_DEBUG_VERIFY(isStorage)) {
        IGL_DEBUG_ABORT("Did you forget to specify TextureUsageBits::Storage on your texture?");
      }
    }
  }

  igl::vulkan::VulkanTexture* newTexture = tex ? &tex->getVulkanTexture() : nullptr;

#if IGL_DEBUG
  if (newTexture) {
    const igl::vulkan::VulkanImage& img = newTexture->image_;
    IGL_DEBUG_ASSERT(img.samples_ == VK_SAMPLE_COUNT_1_BIT,
                     "Multisampled images cannot be sampled in shaders");
    if (bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS) {
      // If you trip this assert, then you are likely using an IGL texture
      // that was not rendered to by IGL. If that's the case, then make sure
      // the underlying image is transitioned to
      // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      // IGL_DEBUG_ASSERT(img.imageLayout_ == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    } else {
      IGL_DEBUG_ASSERT(img.imageLayout_ == VK_IMAGE_LAYOUT_GENERAL);
    }
  }
#endif // IGL_DEBUG

  // multisampled images cannot be directly accessed from shaders
  const bool isTextureAvailable =
      (newTexture != nullptr) &&
      ((newTexture->image_.samples_ & VK_SAMPLE_COUNT_1_BIT) == VK_SAMPLE_COUNT_1_BIT);
  const bool isSampledImage = isTextureAvailable && newTexture->image_.isSampledImage();

  VkImageView imageView = isSampledImage ? newTexture->imageView_.vkImageView_ : VK_NULL_HANDLE;

  if (bindingsTextures_.textures[index] != imageView) {
    bindingsTextures_.textures[index] = imageView;
    isDirtyFlags_ |= DirtyFlagBits_Textures;
  }
}

void ResourcesBinder::updateBindings(VkPipelineLayout layout, const vulkan::PipelineState& state) {
  IGL_PROFILER_FUNCTION_COLOR(IGL_PROFILER_COLOR_UPDATE);

  IGL_DEBUG_ASSERT(layout != VK_NULL_HANDLE);

  if (isDirtyFlags_ & DirtyFlagBits_Textures) {
    ctx_.updateBindingsTextures(cmdBuffer_,
                                layout,
                                bindPoint_,
                                nextSubmitHandle_,
                                bindingsTextures_,
                                *state.dslCombinedImageSamplers_,
                                state.info_);
  }
  if (isDirtyFlags_ & DirtyFlagBits_Buffers) {
    ctx_.updateBindingsBuffers(cmdBuffer_,
                               layout,
                               bindPoint_,
                               nextSubmitHandle_,
                               bindingsBuffers_,
                               *state.dslBuffers_,
                               state.info_);
  }

  isDirtyFlags_ = 0;
}

void ResourcesBinder::bindPipeline(VkPipeline pipeline, const util::SpvModuleInfo* info) {
  IGL_PROFILER_FUNCTION();

  if (lastPipelineBound_ == pipeline) {
    return;
  }

  if (info) {
    // a new pipeline might want a new descriptors configuration
    if (!info->buffers.empty()) {
      isDirtyFlags_ |= DirtyFlagBits_Buffers;
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
