/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/ComputeCommandEncoder.h>

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/ComputePipelineState.h>
#include <igl/vulkan/Texture.h>
#include <igl/vulkan/VulkanBuffer.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanImageView.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanTexture.h>

namespace igl::vulkan {

ComputeCommandEncoder::ComputeCommandEncoder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                                             VulkanContext& ctx) :
  ctx_(ctx),
  cmdBuffer_(commandBuffer ? commandBuffer->getVkCommandBuffer() : VK_NULL_HANDLE),
  binder_(commandBuffer, ctx_, VK_PIPELINE_BIND_POINT_COMPUTE) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(commandBuffer);

  ctx_.checkAndUpdateDescriptorSets();

  isEncoding_ = true;
}

void ComputeCommandEncoder::endEncoding() {
  IGL_PROFILER_FUNCTION();

  if (!isEncoding_) {
    return;
  }

  isEncoding_ = false;

  for (const auto* img : restoreLayout_) {
    if (img->isSampledImage()) {
      // only sampled images can be transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      img->transitionLayout(cmdBuffer_,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VkImageSubresourceRange{img->getImageAspectFlags(),
                                                    0,
                                                    VK_REMAINING_MIP_LEVELS,
                                                    0,
                                                    VK_REMAINING_ARRAY_LAYERS});
    }
  }
  restoreLayout_.clear();
}

void ComputeCommandEncoder::bindComputePipelineState(
    const std::shared_ptr<IComputePipelineState>& pipelineState) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(pipelineState)) {
    return;
  }

  cps_ = static_cast<igl::vulkan::ComputePipelineState*>(pipelineState.get());

  binder_.bindPipeline(cps_->getVkPipeline(), &cps_->getSpvModuleInfo());

  if (ctx_.config_.enableDescriptorIndexing) {
    VkDescriptorSet dset = ctx_.getBindlessVkDescriptorSet();

#if IGL_VULKAN_PRINT_COMMANDS
    IGL_LOG_INFO("%p vkCmdBindDescriptorSets(COMPUTE) - bindless\n", cmdBuffer_);
#endif // IGL_VULKAN_PRINT_COMMANDS
    ctx_.vf_.vkCmdBindDescriptorSets(cmdBuffer_,
                                     VK_PIPELINE_BIND_POINT_COMPUTE,
                                     cps_->getVkPipelineLayout(),
                                     kBindPoint_Bindless,
                                     1,
                                     &dset,
                                     0,
                                     nullptr);
  }
}

void ComputeCommandEncoder::processDependencies(const Dependencies& dependencies) {
  // 1. Process all textures
  {
    const Dependencies* deps = &dependencies;

    while (deps) {
      for (ITexture* tex : deps->textures) {
        if (!tex) {
          break;
        }
        igl::vulkan::transitionToGeneral(cmdBuffer_, tex);
      }
      deps = deps->next;
    }
  }

  // 2. Process all buffers
  {
    const Dependencies* deps = &dependencies;

    while (deps) {
      for (IBuffer* buf : deps->buffers) {
        if (!buf) {
          break;
        }
        const auto* vkBuf = static_cast<igl::vulkan::Buffer*>(buf);
        ivkBufferBarrier(&ctx_.vf_,
                         cmdBuffer_,
                         vkBuf->getVkBuffer(),
                         vkBuf->getBufferUsageFlags(),
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      }
      deps = deps->next;
    }
  }
}

void ComputeCommandEncoder::dispatchThreadGroups(const Dimensions& threadgroupCount,
                                                 const Dimensions& /*threadgroupSize*/,
                                                 const Dependencies& dependencies) {
  IGL_PROFILER_FUNCTION();

  if (!cps_) {
    IGL_ASSERT_MSG(false, "Did you forget to call bindComputePipelineState()?");
    return;
  }

  processDependencies(dependencies);

  binder_.updateBindings(cps_->getVkPipelineLayout(), *cps_);
  // threadgroupSize is controlled inside compute shaders
  ctx_.vf_.vkCmdDispatch(
      cmdBuffer_, threadgroupCount.width, threadgroupCount.height, threadgroupCount.depth);
}

void ComputeCommandEncoder::pushDebugGroupLabel(const char* label, const igl::Color& color) const {
  IGL_ASSERT(label != nullptr && *label);
  ivkCmdBeginDebugUtilsLabel(&ctx_.vf_, cmdBuffer_, label, color.toFloatPtr());
}

void ComputeCommandEncoder::insertDebugEventLabel(const char* label,
                                                  const igl::Color& color) const {
  IGL_ASSERT(label != nullptr && *label);
  ivkCmdInsertDebugUtilsLabel(&ctx_.vf_, cmdBuffer_, label, color.toFloatPtr());
}

void ComputeCommandEncoder::popDebugGroupLabel() const {
  ivkCmdEndDebugUtilsLabel(&ctx_.vf_, cmdBuffer_);
}

void ComputeCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {
  // DO NOT IMPLEMENT!
  // This is only for backends that MUST use single uniforms in some situations.
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void ComputeCommandEncoder::bindTexture(uint32_t index, ITexture* texture) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(texture);

  const igl::vulkan::Texture* tex = static_cast<igl::vulkan::Texture*>(texture);
  const igl::vulkan::VulkanTexture& vkTex = tex->getVulkanTexture();
  const igl::vulkan::VulkanImage* vkImage = &vkTex.getVulkanImage();

  igl::vulkan::transitionToGeneral(cmdBuffer_, texture);

  restoreLayout_.push_back(vkImage);

  binder_.bindTexture(index, static_cast<igl::vulkan::Texture*>(texture));
}

void ComputeCommandEncoder::bindBuffer(size_t index,
                                       const std::shared_ptr<IBuffer>& buffer,
                                       size_t offset,
                                       size_t bufferSize) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(buffer != nullptr)) {
    return;
  }

  auto* buf = static_cast<igl::vulkan::Buffer*>(buffer.get());

  const bool isStorageBuffer = (buf->getBufferType() & BufferDesc::BufferTypeBits::Storage) != 0;

  if (!isStorageBuffer) {
    IGL_ASSERT_MSG(
        false,
        "Did you forget to specify igl::BufferDesc::BufferTypeBits::Storage on your buffer?");
    return;
  }

  binder_.bindStorageBuffer((int)index, buf, offset, bufferSize);
}

void ComputeCommandEncoder::bindBytes(size_t /*index*/, const void* /*data*/, size_t /*length*/) {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void ComputeCommandEncoder::bindPushConstants(const void* data, size_t length, size_t offset) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(length % 4 == 0); // VUID-vkCmdPushConstants-size-00369: size must be a multiple of 4

  IGL_ASSERT_MSG(cps_, "Did you forget to call bindComputePipelineState()?");
  IGL_ASSERT_MSG(cps_->pushConstantRange_.size,
                 "Currently bound compute pipeline state has no push constants");
  IGL_ASSERT_MSG(offset + length <= cps_->pushConstantRange_.offset + cps_->pushConstantRange_.size,
                 "Push constants size exceeded");

#if IGL_VULKAN_PRINT_COMMANDS
  IGL_LOG_INFO("%p vkCmdPushConstants(%u) - COMPUTE\n", cmdBuffer_, length);
#endif // IGL_VULKAN_PRINT_COMMANDS
  ctx_.vf_.vkCmdPushConstants(cmdBuffer_,
                              cps_->getVkPipelineLayout(),
                              VK_SHADER_STAGE_COMPUTE_BIT,
                              (uint32_t)offset,
                              (uint32_t)length,
                              data);
}

} // namespace igl::vulkan
