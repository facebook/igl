/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#pragma once

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float4.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <igl/Buffer.h>
#include <igl/RenderPass.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/Common.h>

namespace igl {

class IRenderPipelineState;

namespace vulkan {

class CommandQueue;
class Device;
class VulkanContext;
class VulkanFeatures;

/**
 * @brief Encapsulates and stores the resources needed to enable enhanced shader debugging
 */
class EnhancedShaderDebuggingStore {
 public:
  struct Line {
    glm::vec4 v0;
    glm::vec4 color0;
    glm::vec4 v1;
    glm::vec4 color1;
    glm::mat4 transform;
  };

  struct Metadata {
    explicit Metadata(uint32_t maxNumberOfLines) : maxNumLines{maxNumberOfLines} {}
    uint32_t maxNumLines = 0u;
    uint32_t padding1 = 0u;
    uint32_t padding2 = 0u;
    uint32_t padding3 = 0u;
  };

  struct Header {
    Header(uint32_t maxNumberOfLines, VkDrawIndirectCommand command) :
      metadata{maxNumberOfLines}, command{command} {}
    Metadata metadata;
    VkDrawIndirectCommand command = {
        0u,
        0u,
        0u,
        0u,
    };
  };

  /* Parameters */
  static constexpr uint32_t kBufferIndex = IGL_UNIFORM_BLOCKS_BINDING_MAX - 1;
  static constexpr uint32_t kNumberOfLines = 16'384;
  static constexpr bool kDepthWriteEnabled = false;
  static constexpr igl::CompareFunction kDepthCompareFunction = igl::CompareFunction::AlwaysPass;

  /** @brief Constructs the object */
  explicit EnhancedShaderDebuggingStore();

  /** @brief Initialize the object and stores the `Device` needed to create resources */
  void initialize(Device* device);

  /** @brief Returns the shader code that stores the line vertices in the buffer. This code can be
   * injected into all shaders compiled by the device.
   * @param[in] includeFunctionBody a flag that determines if the returned code should include the
   * function's code in the body of the function. If false the function returns a function with an
   * empty body
   * @param[in] extensions the extensions available for the device.
   */
  static std::string recordLineShaderCode(bool includeFunctionBody,
                                          const VulkanFeatures& extensions);

  /** @brief Returns the vertex buffer used to store the lines' vertices */
  std::shared_ptr<IBuffer> vertexBuffer() const;

  /** @brief Returns the depth stencil state needed to render the lines */
  std::shared_ptr<IDepthStencilState> depthStencilState() const;

  /** @brief Returns the `RenderPassDesc` needed to render the lines
   * @param[in] framebuffer the framebuffer used as a reference to populate the render pass
   * description structure
   */
  RenderPassDesc renderPassDesc(const std::shared_ptr<IFramebuffer>& framebuffer) const;

  /** @brief If a framebuffer has been been created with the resolveAttachment as a color attachment
   * the cached framebuffer is returned. Otherwise a new one will be created and returned.
   */
  const std::shared_ptr<IFramebuffer>& framebuffer(
      Device& device,
      const std::shared_ptr<ITexture>& resolveAttachment) const;

  /** @brief Returns a pipeline compatible with the framebuffer passed in as a parameter. If a
   * pipeline compatible with the framebuffer passed as a parameter isn't found, one is created and
   * cached. If one already exists, that one is returned instead. Pipelines are created based on the
   * framebuffer attachments' formats */
  std::shared_ptr<IRenderPipelineState> pipeline(
      const igl::vulkan::Device& device,
      const std::shared_ptr<IFramebuffer>& framebuffer) const;

  /** @brief Installs a barrier for the lines' vertices buffer. This barrier guarantees that the
   * previous render pass is done writing to the buffer. It's placed between the application's
   * render pass and the line drawing pass */
  void installBufferBarrier(const igl::ICommandBuffer& commandBuffer) const;

  /// @brief Executes the shader debugging render pass. Also presents the image if the command
  /// buffer being submitted was from a swapchain.
  void enhancedShaderDebuggingPass(CommandQueue& queue, CommandBuffer* cmdBuffer);

 private:
  /** @brief Vertex shader code to render the lines */
  std::string renderLineVSCode() const;

  /** @brief Fragment shader code to render the lines */
  std::string renderLineFSCode() const;

  /** @brief Returns a hash value based on the format of color attachments of the
   * framebuffer and the format of the depth buffer */
  uint64_t hashFramebufferFormats(const std::shared_ptr<IFramebuffer>& framebuffer) const;

 private:
  bool enabled_ = false;
  Device* device_ = nullptr;
  mutable std::shared_ptr<IBuffer> vertexBuffer_;
  mutable std::shared_ptr<IDepthStencilState> depthStencilState_;

  mutable std::unordered_map<std::shared_ptr<ITexture>, std::shared_ptr<IFramebuffer>>
      framebuffers_;
  mutable std::unordered_map<uint64_t, std::shared_ptr<IRenderPipelineState>> pipelineStates_;

  mutable std::shared_ptr<IShaderStages> shaderStage_;
  std::shared_ptr<IShaderModule> vertexShaderModule_;
  std::shared_ptr<IShaderModule> fragmentShaderModule_;
};

} // namespace vulkan
} // namespace igl
