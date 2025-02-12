/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @fb-only

#include <igl/vulkan/EnhancedShaderDebuggingStore.h>

#include <algorithm>

#include <igl/Framebuffer.h>
#include <igl/NameHandle.h>
#include <igl/RenderPipelineState.h>
#include <igl/Shader.h>
#include <igl/ShaderCreator.h>

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/CommandQueue.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/RenderCommandEncoder.h>
#include <igl/vulkan/VulkanContext.h>

namespace igl::vulkan {

EnhancedShaderDebuggingStore::EnhancedShaderDebuggingStore() {
#if !IGL_PLATFORM_ANDROID
  enabled_ = true;
#endif
}

void EnhancedShaderDebuggingStore::initialize(Device* device) {
  device_ = device;
}

std::string EnhancedShaderDebuggingStore::recordLineShaderCode(bool includeFunctionBody,
                                                               const VulkanExtensions& extensions) {
  if (!includeFunctionBody) {
    return R"(void drawLine(vec3 v0, vec3 v1, vec4 color0, vec4 color1, mat4 transform) {})";
  }

  std::string debugPrintfStatement;

  if (extensions.enabled(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME)) {
    debugPrintfStatement = R"(debugPrintfEXT("Debug draw lines buffer size exceeded.");)";
  }

  const std::string bufferIndex = std::to_string(kBufferIndex);

  return R"(
  struct Line {
    vec4 vertex_0;
    vec4 color_0;
    vec4 vertex_1;
    vec4 color_1;
    mat4 transform;
  };

  struct DrawIndirectCommand {
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
  };

  layout(std430, buffer_reference) buffer LinesWithHeader {
    uint size;
    uint pad0;
    uint pad1;
    uint pad2;
    DrawIndirectCommand command;
    Line line[];
  };

  void drawLine(vec3 v0, vec3 v1, vec4 color0, vec4 color1, mat4 transform) {
    LinesWithHeader lines = LinesWithHeader(getBuffer()" +
         bufferIndex + R"());

    const uint index = atomicAdd(lines.command.instanceCount, 1);

    if (index >= lines.size) {
      atomicMin(lines.command.instanceCount, lines.size);)" +
         debugPrintfStatement +
         R"(return;
    }

    lines.line[index].vertex_0 = vec4(v0, 1);
    lines.line[index].color_0 = color0;
    lines.line[index].vertex_1 = vec4(v1, 1);
    lines.line[index].color_1  = color1;
    lines.line[index].transform = transform;
  })";
}

std::shared_ptr<IBuffer> EnhancedShaderDebuggingStore::vertexBuffer() const {
  if (!enabled_) {
    return nullptr;
  }

  if (vertexBuffer_ == nullptr) {
    IGL_DEBUG_ASSERT(device_ != nullptr,
                     "Device is null. This object needs to be initialized to be used");

    constexpr size_t lineStructureSizeBytes = sizeof(Line);
    constexpr size_t bufferSizeBytes = lineStructureSizeBytes * kNumberOfLines;
    Header bufferHeader(kNumberOfLines,
                        VkDrawIndirectCommand{
                            2, /* vertex count */
                            0, /* instances */
                            0, /* first_vertex */
                            0, /* first_instance */
                        });

    vertexBuffer_ = device_->createBuffer(
        BufferDesc(BufferDesc::BufferTypeBits::Storage | BufferDesc::BufferTypeBits::Indirect,
                   nullptr,
                   sizeof(Header) + bufferSizeBytes,
                   ResourceStorage::Private,
                   0,
                   "Buffer: shader draw line"),
        nullptr);

    vertexBuffer_->upload(&bufferHeader, BufferRange(sizeof(Header), 0));
  }

  return vertexBuffer_;
}

RenderPassDesc EnhancedShaderDebuggingStore::renderPassDesc(
    const std::shared_ptr<IFramebuffer>& framebuffer) const {
  const auto attachmentIndices = framebuffer->getColorAttachmentIndices();
  const auto max = std::max_element(attachmentIndices.begin(), attachmentIndices.end());

  RenderPassDesc desc;
  desc.colorAttachments.resize(*max + 1);

  for (auto index = 0; index <= *max; ++index) {
    const auto colorAttachment = framebuffer->getColorAttachment(index);
    if (colorAttachment) {
      IGL_DEBUG_ASSERT(!framebuffer->getResolveColorAttachment(index),
                       "Shader lines drawing does not work with multisampled framebuffers");
      desc.colorAttachments[index].loadAction = LoadAction::Load;
      desc.colorAttachments[index].storeAction = StoreAction::Store;
    }
  }

  if (framebuffer->getDepthAttachment()) {
    desc.depthAttachment.loadAction = LoadAction::Load;
    desc.depthAttachment.storeAction = StoreAction::Store;
  }

  return desc;
}

const std::shared_ptr<IFramebuffer>& EnhancedShaderDebuggingStore::framebuffer(
    Device& device,
    const std::shared_ptr<ITexture>& resolveAttachment) const {
  auto foundFramebuffer = framebuffers_.find(resolveAttachment);
  if (foundFramebuffer != framebuffers_.end()) {
    return foundFramebuffer->second;
  }

  Result result;
  framebuffers_[resolveAttachment] = device.createFramebuffer(
      {
          .colorAttachments = {{.texture = resolveAttachment}},
          .debugName = "Framebuffer: shader debug framebuffer",
      },
      &result);

  if (!IGL_DEBUG_VERIFY(result.isOk())) {
    IGL_LOG_INFO("Error creating a framebuffer for drawing debug lines from shaders");
  }

  return framebuffers_[resolveAttachment];
}

std::shared_ptr<IDepthStencilState> EnhancedShaderDebuggingStore::depthStencilState() const {
  if (!enabled_) {
    return nullptr;
  }

  if (depthStencilState_ == nullptr) {
    IGL_DEBUG_ASSERT(device_ != nullptr,
                     "Device is null. This object needs to be initialized to be used");

    depthStencilState_ = device_->createDepthStencilState(
        {
            .compareFunction = kDepthCompareFunction,
            .isDepthWriteEnabled = kDepthWriteEnabled,
        },
        nullptr);
  }

  return depthStencilState_;
}

std::shared_ptr<IRenderPipelineState> EnhancedShaderDebuggingStore::pipeline(
    const igl::vulkan::Device& device,
    const std::shared_ptr<IFramebuffer>& framebuffer) const {
  if (!enabled_) {
    return nullptr;
  }

  const uint64_t hashedFramebufferFormats = hashFramebufferFormats(framebuffer);

  if (auto result = pipelineStates_.find(hashedFramebufferFormats);
      result != pipelineStates_.end()) {
    return result->second;
  }

  const auto attachments = framebuffer->getColorAttachmentIndices();
  IGL_DEBUG_ASSERT(!attachments.empty());

  RenderPipelineDesc desc;

  desc.topology = PrimitiveType::Line;

  const auto max = std::max_element(attachments.begin(), attachments.end());

  desc.targetDesc.colorAttachments.resize(*max + 1);

  for (size_t index = 0; index <= *max; ++index) {
    if (!framebuffer->getColorAttachment(index)) {
      continue;
    }

    // Only check for MSAA while desc.sampleCount == 1. Otherwise we already checked and updated it
    if (desc.sampleCount == 1u && framebuffer->getResolveColorAttachment(index)) {
      desc.sampleCount = framebuffer->getColorAttachment(index)->getSamples();
    }

    desc.targetDesc.colorAttachments[index].textureFormat =
        framebuffer->getColorAttachment(index)->getFormat();
  }
  if (framebuffer->getDepthAttachment()) {
    desc.targetDesc.depthAttachmentFormat = framebuffer->getDepthAttachment()->getFormat();
  }
  if (framebuffer->getStencilAttachment()) {
    desc.targetDesc.stencilAttachmentFormat = framebuffer->getStencilAttachment()->getFormat();
  }

  // Create a shader stage, along with a vertex and fragment shader modules, if they haven't been
  // created yet
  if (shaderStage_ == nullptr && device.getVulkanContext().config_.enableBufferDeviceAddress) {
    const auto vscode = renderLineVSCode();
    const auto fscode = renderLineFSCode();

    shaderStage_ = ShaderStagesCreator::fromModuleStringInput(device,
                                                              vscode.c_str(),
                                                              "main",
                                                              "Shader Module: debug lines (vert)",
                                                              fscode.c_str(),
                                                              "main",
                                                              "Shader Module: debug lines (frag)",
                                                              nullptr);
  }

  desc.shaderStages = shaderStage_;
  desc.debugName = genNameHandle("Pipeline: debug lines");

  return pipelineStates_
      .insert_or_assign(hashedFramebufferFormats, device.createRenderPipeline(desc, nullptr))
      .first->second;
}

std::string EnhancedShaderDebuggingStore::renderLineVSCode() const {
  const std::string bufferIndex = std::to_string(kBufferIndex);

  return R"(
layout(std430, buffer_reference) buffer Lines {
    Line line[];
 };

layout (location=0) out vec4 out_color;

void main() {
  const uint index = gl_InstanceIndex;

  Lines lines = Lines(getBuffer()" +
         bufferIndex +
         R"());

  if (gl_VertexIndex == 0) {
    out_color = lines.line[index].color_0;
    gl_Position = (lines.line[index].transform * lines.line[index].vertex_0).xyww;
  } else {
    out_color = lines.line[index].color_1;
    gl_Position = (lines.line[index].transform * lines.line[index].vertex_1).xyww;
  }
})";
}

std::string EnhancedShaderDebuggingStore::renderLineFSCode() const {
  return R"(
layout (location=0) in  vec4 color;
layout (location=0) out vec4 out_FragColor;

void main() {
  out_FragColor = color;
}
)";
}

void EnhancedShaderDebuggingStore::installBufferBarrier(
    const igl::ICommandBuffer& commandBuffer) const {
  if (enabled_) {
    const auto* cmdBuffer = static_cast<const vulkan::CommandBuffer*>(&commandBuffer);
    auto* buffer = static_cast<Buffer*>(vertexBuffer().get());
    const auto& ctx = device_->getVulkanContext();
    ivkBufferMemoryBarrier(&ctx.vf_,
                           cmdBuffer->getVkCommandBuffer(),
                           buffer->getVkBuffer(),
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, /* src access flag */
                           VK_ACCESS_INDIRECT_COMMAND_READ_BIT, /* dst access flag */
                           0,
                           VK_WHOLE_SIZE,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);
  }
}

uint64_t EnhancedShaderDebuggingStore::hashFramebufferFormats(
    const std::shared_ptr<IFramebuffer>& framebuffer) const {
  const std::hash<TextureFormat> hashFun;

  uint64_t hashValue = 0;

  for (size_t index = 0; index < framebuffer->getColorAttachmentIndices().size(); ++index) {
    if (framebuffer->getColorAttachment(index)) {
      hashValue ^= hashFun(framebuffer->getColorAttachment(index)->getFormat());
    }
    if (framebuffer->getResolveColorAttachment(index)) {
      hashValue ^= hashFun(framebuffer->getResolveColorAttachment(index)->getFormat());
    }
    if (framebuffer->getDepthAttachment() != nullptr) {
      hashValue ^= hashFun(framebuffer->getDepthAttachment()->getFormat());
    }
    if (framebuffer->getStencilAttachment() != nullptr) {
      hashValue ^= hashFun(framebuffer->getStencilAttachment()->getFormat());
    }
  }

  return hashValue;
}

void EnhancedShaderDebuggingStore::enhancedShaderDebuggingPass(CommandQueue& queue,
                                                               CommandBuffer* cmdBuffer) {
  IGL_PROFILER_FUNCTION();

  if (!cmdBuffer->getFramebuffer()) {
    return;
  }

  // If there are no color attachments, return, as we won't have a framebuffer to render into
  const auto indices = cmdBuffer->getFramebuffer()->getColorAttachmentIndices();
  if (indices.empty()) {
    return;
  }

  const auto min = std::min_element(indices.begin(), indices.end());

  const auto resolveAttachment = cmdBuffer->getFramebuffer()->getResolveColorAttachment(*min);
  const std::shared_ptr<IFramebuffer>& framebuffer =
      resolveAttachment ? this->framebuffer(*device_, resolveAttachment)
                        : cmdBuffer->getFramebuffer();

  Result result;
  auto lineDrawingCmdBuffer =
      queue.createCommandBuffer({"Command buffer: line drawing enhanced debugging"}, &result);

  if (!IGL_DEBUG_VERIFY(result.isOk())) {
    IGL_LOG_INFO("Error obtaining a new command buffer for drawing debug lines");
    return;
  }

  auto cmdEncoder =
      lineDrawingCmdBuffer->createRenderCommandEncoder(renderPassDesc(framebuffer), framebuffer);

  auto pipeline = this->pipeline(*device_, framebuffer);
  cmdEncoder->bindRenderPipelineState(pipeline);

  {
    // Bind the line buffer
    auto* vkEncoder = static_cast<RenderCommandEncoder*>(cmdEncoder.get());
    vkEncoder->binder().bindBuffer(EnhancedShaderDebuggingStore::kBufferIndex,
                                   static_cast<Buffer*>(vertexBuffer().get()),
                                   sizeof(EnhancedShaderDebuggingStore::Header),
                                   0);

    cmdEncoder->pushDebugGroupLabel("Render Debug Lines", kColorDebugLines);
    cmdEncoder->bindDepthStencilState(depthStencilState());

    // Disable incrementing the draw call count
    const auto resetDrawCallCountValue = vkEncoder->setDrawCallCountEnabled(false);
    IGL_SCOPE_EXIT {
      vkEncoder->setDrawCallCountEnabled(resetDrawCallCountValue);
    };
    cmdEncoder->multiDrawIndirect(
        *vertexBuffer(), sizeof(EnhancedShaderDebuggingStore::Metadata), 1, 0);
  }
  cmdEncoder->popDebugGroupLabel();
  cmdEncoder->endEncoding();

  auto* resetCmdBuffer = static_cast<CommandBuffer*>(lineDrawingCmdBuffer.get());
  auto* const vkResetCmdBuffer = resetCmdBuffer->getVkCommandBuffer();

  // End the render pass by transitioning the surface that was presented by the application
  if (cmdBuffer->getPresentedSurface()) {
    resetCmdBuffer->present(cmdBuffer->getPresentedSurface());
  }

  VulkanContext& ctx = device_->getVulkanContext();

  // Barrier to ensure we have finished rendering the lines before we clear the buffer
  auto* lineBuffer = static_cast<Buffer*>(vertexBuffer().get());
  ivkBufferMemoryBarrier(&ctx.vf_,
                         vkResetCmdBuffer,
                         lineBuffer->getVkBuffer(),
                         0, /* src access flag */
                         0, /* dst access flag */
                         0,
                         VK_WHOLE_SIZE,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT);

  // Reset instanceCount of the buffer
  ctx.vf_.vkCmdFillBuffer(vkResetCmdBuffer,
                          lineBuffer->getVkBuffer(),
                          offsetof(EnhancedShaderDebuggingStore::Header, command_) +
                              offsetof(VkDrawIndirectCommand, instanceCount),
                          sizeof(uint32_t), // reset only the instance count
                          0);

  queue.endCommandBuffer(ctx, resetCmdBuffer, true);
}

} // namespace igl::vulkan
