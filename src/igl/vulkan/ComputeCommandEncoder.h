/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/ComputeCommandEncoder.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/ResourcesBinder.h>

namespace igl {

class ICommandBuffer;
class IComputePipelineState;
class ISamplerState;
class ITexture;

namespace vulkan {

class ComputeCommandEncoder : public IComputeCommandEncoder {
 public:
  ComputeCommandEncoder(const std::shared_ptr<CommandBuffer>& commandBuffer,
                        const VulkanContext& ctx);
  ~ComputeCommandEncoder() override {
    IGL_ASSERT(!isEncoding_); // did you forget to call endEncoding()?
    endEncoding();
  }

  void bindComputePipelineState(
      const std::shared_ptr<IComputePipelineState>& pipelineState) override;
  void dispatchThreadGroups(const Dimensions& threadgroupCount,
                            const Dimensions& threadgroupSize) override;
  void endEncoding() override;

  void pushDebugGroupLabel(const std::string& label, const igl::Color& color) const override;
  void insertDebugEventLabel(const std::string& label, const igl::Color& color) const override;
  void popDebugGroupLabel() const override;
  void bindUniform(const UniformDesc& uniformDesc, const void* data) override;
  void bindTexture(size_t index, ITexture* texture) override;
  void bindBuffer(size_t index, const std::shared_ptr<IBuffer>& buffer, size_t offset) override;
  void bindBytes(size_t index, const void* data, size_t length) override;
  void bindPushConstants(size_t offset, const void* data, size_t length) override;

  VkCommandBuffer getVkCommandBuffer() const {
    return cmdBuffer_;
  }

 private:
  const VulkanContext& ctx_;
  VkCommandBuffer cmdBuffer_ = VK_NULL_HANDLE;
  bool isEncoding_ = false;

  igl::vulkan::ResourcesBinder binder_;
};

} // namespace vulkan
} // namespace igl
