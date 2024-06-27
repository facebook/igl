/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/EnhancedShaderDebuggingStore.h>

#include <algorithm>

#include <igl/Framebuffer.h>
#include <igl/NameHandle.h>
#include <igl/RenderPipelineState.h>
#include <igl/Shader.h>
#include <igl/ShaderCreator.h>

#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/CommandBuffer.h>
#include <igl/vulkan/Common.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/Framebuffer.h>
#include <igl/vulkan/VulkanContext.h>

namespace igl::vulkan {

EnhancedShaderDebuggingStore::EnhancedShaderDebuggingStore() {
#if !IGL_PLATFORM_ANDROID
  enabled_ = true;
#endif
}

void EnhancedShaderDebuggingStore::initialize(const igl::vulkan::Device* device) {
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

void EnhancedShaderDebuggingStore::initializeBuffer() const {
  IGL_ASSERT_MSG(device_ != nullptr,
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

  vertexBuffer_->upload(&bufferHeader, igl::BufferRange(sizeof(Header), 0));
}

std::shared_ptr<igl::IBuffer> EnhancedShaderDebuggingStore::vertexBuffer() const {
  if (!enabled_) {
    return nullptr;
  }

  if (vertexBuffer_ == nullptr) {
    initializeBuffer();
  }

  return vertexBuffer_;
}

igl::RenderPassDesc EnhancedShaderDebuggingStore::renderPassDesc(
    const std::shared_ptr<IFramebuffer>& framebuffer) const {
  const auto attachmentIndices = framebuffer->getColorAttachmentIndices();
  const auto max = std::max_element(attachmentIndices.begin(), attachmentIndices.end());

  igl::RenderPassDesc desc;
  desc.colorAttachments.resize(*max + 1);

  for (auto index = 0; index <= *max; ++index) {
    const auto colorAttachment = framebuffer->getColorAttachment(index);
    if (colorAttachment) {
      IGL_ASSERT_MSG(!framebuffer->getResolveColorAttachment(index),
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

std::shared_ptr<igl::IFramebuffer> EnhancedShaderDebuggingStore::framebuffer(
    igl::vulkan::Device& device,
    const std::shared_ptr<igl::ITexture>& resolveAttachment) const {
  auto foundFramebuffer = framebuffers_.find(resolveAttachment);
  if (foundFramebuffer != framebuffers_.end()) {
    return foundFramebuffer->second;
  }

  igl::Result result;
  FramebufferDesc framebufferDesc;
  framebufferDesc.debugName = "Framebuffer: shader debug framebuffer";
  framebufferDesc.colorAttachments[0].texture = resolveAttachment;
  framebuffers_[resolveAttachment] = device.createFramebuffer(framebufferDesc, &result);

  if (!IGL_VERIFY(result.isOk())) {
    IGL_LOG_INFO("Error creating a framebuffer for drawing debug lines from shaders");
  }

  return framebuffers_[resolveAttachment];
}

std::shared_ptr<igl::IDepthStencilState> EnhancedShaderDebuggingStore::depthStencilState() const {
  if (!enabled_) {
    return nullptr;
  }

  if (depthStencilState_ == nullptr) {
    initializeDepthState();
  }

  return depthStencilState_;
}

std::shared_ptr<igl::IRenderPipelineState> EnhancedShaderDebuggingStore::pipeline(
    const igl::vulkan::Device& device,
    const std::shared_ptr<igl::IFramebuffer>& framebuffer) const {
  if (!enabled_) {
    return nullptr;
  }

  const auto hashedFramebufferFormats = hashFramebufferFormats(framebuffer);

  auto result = pipelineStates_.find(hashedFramebufferFormats);
  if (result != pipelineStates_.end()) {
    return result->second;
  }

  const auto attachments = framebuffer->getColorAttachmentIndices();
  IGL_ASSERT(!attachments.empty());

  RenderPipelineDesc desc;

  desc.topology = PrimitiveType::Line;

  const auto max = std::max_element(attachments.begin(), attachments.end());

  desc.targetDesc.colorAttachments.resize(*max + 1);

  for (size_t index = 0; index <= *max; ++index) {
    if (!framebuffer->getColorAttachment(index)) {
      continue;
    }

    // Only check for MSAA while desc.sampleCount == 1. Otherwise we already checked and updated it
    if (desc.sampleCount == 1 && framebuffer->getResolveColorAttachment(index)) {
      desc.sampleCount = (int)framebuffer->getColorAttachment(index)->getSamples();
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
  if (shaderStage_ == nullptr) {
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

  pipelineStates_.insert({hashedFramebufferFormats, device.createRenderPipeline(desc, nullptr)});

  return pipelineStates_[hashedFramebufferFormats];
}

void EnhancedShaderDebuggingStore::initializeDepthState() const {
  IGL_ASSERT_MSG(device_ != nullptr,
                 "Device is null. This object needs to be initialized to be used");

  DepthStencilStateDesc desc;
  desc.isDepthWriteEnabled = kDepthWriteEnabled;
  desc.compareFunction = kDepthCompareFunction;
  depthStencilState_ = device_->createDepthStencilState(desc, nullptr);
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
    auto* buffer = static_cast<vulkan::Buffer*>(vertexBuffer().get());
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
    const std::shared_ptr<igl::IFramebuffer>& framebuffer) const {
  const std::hash<igl::TextureFormat> hashFun;

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

} // namespace igl::vulkan
