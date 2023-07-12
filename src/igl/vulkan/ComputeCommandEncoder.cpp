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
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanImage.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanTexture.h>

namespace igl {
namespace vulkan {

ComputeCommandEncoder::ComputeCommandEncoder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                                             const VulkanContext& ctx) :
  ctx_(ctx),
  cmdBuffer_(commandBuffer ? commandBuffer->getVkCommandBuffer() : VK_NULL_HANDLE),
  binder_(commandBuffer, ctx_, VK_PIPELINE_BIND_POINT_COMPUTE) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(commandBuffer);

  ctx_.checkAndUpdateDescriptorSets();
  ctx_.DUBs_->update(cmdBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, nullptr);

  isEncoding_ = true;
}

void ComputeCommandEncoder::endEncoding() {
  IGL_PROFILER_FUNCTION();

  if (!isEncoding_) {
    return;
  }

  isEncoding_ = false;
}

void ComputeCommandEncoder::bindComputePipelineState(
    const std::shared_ptr<IComputePipelineState>& pipelineState) {
  IGL_PROFILER_FUNCTION();

  if (!IGL_VERIFY(pipelineState != nullptr)) {
    return;
  }

  const igl::vulkan::ComputePipelineState* cps =
      static_cast<igl::vulkan::ComputePipelineState*>(pipelineState.get());

  IGL_ASSERT(cps);

  binder_.bindPipeline(cps->getVkPipeline());
}

void ComputeCommandEncoder::dispatchThreadGroups(const Dimensions& threadgroupCount,
                                                 const Dimensions& /*threadgroupSize*/) {
  IGL_PROFILER_FUNCTION();

  binder_.updateBindings();
  // threadgroupSize is controlled inside compute shaders
  vkCmdDispatch(
      cmdBuffer_, threadgroupCount.width, threadgroupCount.height, threadgroupCount.depth);
}

void ComputeCommandEncoder::pushDebugGroupLabel(const std::string& label,
                                                const igl::Color& color) const {
  IGL_ASSERT(!label.empty());

  ivkCmdBeginDebugUtilsLabel(cmdBuffer_, label.c_str(), color.toFloatPtr());
}

void ComputeCommandEncoder::insertDebugEventLabel(const std::string& label,
                                                  const igl::Color& color) const {
  IGL_ASSERT(!label.empty());

  ivkCmdInsertDebugUtilsLabel(cmdBuffer_, label.c_str(), color.toFloatPtr());
}

void ComputeCommandEncoder::popDebugGroupLabel() const {
  ivkCmdEndDebugUtilsLabel(cmdBuffer_);
}

void ComputeCommandEncoder::bindUniform(const UniformDesc& /*uniformDesc*/, const void* /*data*/) {
  // DO NOT IMPLEMENT!
  // This is only for backends that MUST use single uniforms in some situations.
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void ComputeCommandEncoder::bindTexture(size_t index, ITexture* texture) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(texture);
  const igl::vulkan::Texture* tex = static_cast<igl::vulkan::Texture*>(texture);
  const igl::vulkan::VulkanTexture& vkTex = tex->getVulkanTexture();
  const igl::vulkan::VulkanImage& vkImage = vkTex.getVulkanImage();
  if (!vkImage.isStorageImage()) {
    IGL_ASSERT_MSG(false, "Did you forget to specify TextureUsageBits::Storage on your texture?");
    return;
  }

  // "frame graph" heuristics: if we are already in VK_IMAGE_LAYOUT_GENERAL, wait for the previous
  // compute shader, otherwise wait for previous attachment writes
  const VkPipelineStageFlags srcStage =
      (vkImage.imageLayout_ == VK_IMAGE_LAYOUT_GENERAL) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
      : vkImage.isDepthOrStencilFormat_                 ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                                        : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  vkImage.transitionLayout(
      cmdBuffer_,
      VK_IMAGE_LAYOUT_GENERAL,
      srcStage,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VkImageSubresourceRange{
          vkImage.getImageAspectFlags(), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});

  binder_.bindTexture(index, static_cast<igl::vulkan::Texture*>(texture));
}

void ComputeCommandEncoder::bindBuffer(size_t index,
                                       const std::shared_ptr<IBuffer>& buffer,
                                       size_t offset) {
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

  binder_.bindBuffer((int)index, buf, offset);
}

void ComputeCommandEncoder::bindBytes(size_t /*index*/, const void* /*data*/, size_t /*length*/) {
  IGL_ASSERT_NOT_IMPLEMENTED();
}

void ComputeCommandEncoder::bindPushConstants(size_t offset, const void* data, size_t length) {
  IGL_PROFILER_FUNCTION();

  IGL_ASSERT(length % 4 == 0); // VUID-vkCmdPushConstants-size-00369: size must be a multiple of 4

  // check push constant size is within max size
  const VkPhysicalDeviceLimits& limits = ctx_.getVkPhysicalDeviceProperties().limits;
  const size_t size = offset + length;
  if (!IGL_VERIFY(size <= limits.maxPushConstantsSize)) {
    IGL_LOG_ERROR(
        "Push constants size exceeded %u (max %u bytes)", size, limits.maxPushConstantsSize);
  }

  vkCmdPushConstants(cmdBuffer_,
                     ctx_.pipelineLayoutCompute_->getVkPipelineLayout(),
                     VK_SHADER_STAGE_COMPUTE_BIT,
                     (uint32_t)offset,
                     (uint32_t)length,
                     data);
}

} // namespace vulkan
} // namespace igl
