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
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanPipelineLayout.h>

namespace igl {

namespace vulkan {

ResourcesBinder::ResourcesBinder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                                 const VulkanContext& ctx,
                                 VkPipelineBindPoint bindPoint) :
  ctx_(ctx), cmdBuffer_(commandBuffer->getVkCommandBuffer()), bindPoint_(bindPoint) {}

void ResourcesBinder::bindBuffer(uint32_t index, igl::vulkan::Buffer* buffer, size_t bufferOffset) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(index < IGL_UNIFORM_BLOCKS_BINDING_MAX)) {
    IGL_ASSERT_MSG(false, "Buffer index should not exceed kMaxBindingSlots");
    return;
  }

  if (bindingsBuffers_.buffers[index].buf != buffer ||
      bindingsBuffers_.buffers[index].offset != bufferOffset) {
    bindingsBuffers_.buffers[index] = {buffer, bufferOffset};
    isDirtyBuffers_ = true;
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

  if (bindingsTextures_.textures[index] != newTexture) {
    bindingsTextures_.textures[index] = newTexture;
    isDirtyTextures_ = true;
  }
}

void ResourcesBinder::updateBindings() {
  if (isDirtyTextures_) {
    ctx_.updateBindingsTextures(cmdBuffer_, bindPoint_, bindingsTextures_);
    isDirtyTextures_ = false;
  }
  if (isDirtyBuffers_) {
    ctx_.updateBindingsBuffers(cmdBuffer_, bindPoint_, bindingsBuffers_);
    isDirtyBuffers_ = false;
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
    vkCmdBindPipeline(cmdBuffer_, bindPoint_, pipeline);
  }
}

} // namespace vulkan
} // namespace igl
