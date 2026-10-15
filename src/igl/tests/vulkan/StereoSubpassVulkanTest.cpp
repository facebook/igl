/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Config.h>

#if IGL_PLATFORM_LINUX

#include <gtest/gtest.h>

#include "../util/TestDevice.h"

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>
#include <igl/Buffer.h>
#include <igl/CommandBuffer.h>
#include <igl/DepthStencilState.h>
#include <igl/Device.h>
#include <igl/Framebuffer.h>
#include <igl/NameHandle.h>
#include <igl/RenderPass.h>
#include <igl/RenderPipelineState.h>
#include <igl/SamplerState.h>
#include <igl/ShaderCreator.h>
#include <igl/Texture.h>
#include <igl/VertexInputState.h>
#include <igl/vulkan/Buffer.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanFeatures.h>
#include <igl/vulkan/VulkanFunctionTable.h>

namespace igl::tests {
namespace {

constexpr uint32_t kSize = 8;

constexpr const char* kVertexShaderFullscreen = R"(#version 460
#extension GL_EXT_multiview : enable
out gl_PerVertex { vec4 gl_Position; };
void main() {
  vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
})";

// reads the positions of a quad from vertex buffer 0
constexpr const char* kVertexShaderBuffer = R"(#version 460
#extension GL_EXT_multiview : enable
layout(location = 0) in vec2 pos;
out gl_PerVertex { vec4 gl_Position; };
void main() {
  gl_Position = vec4(pos, 0.0, 1.0);
})";

// view 0: (100, 20, 0), view 1: (0, 50, 30) in attachment 1
constexpr const char* kFragmentPerView = R"(#version 460
#extension GL_EXT_multiview : enable
layout(location = 1) out vec4 outA;
void main() {
  outA = gl_ViewIndex == 0 ? vec4(100.0 / 255.0, 20.0 / 255.0, 0.0, 1.0)
                           : vec4(0.0, 50.0 / 255.0, 30.0 / 255.0, 1.0);
})";

// reads input attachment 1, swaps red and green, writes attachment 0
constexpr const char* kFragmentReadInput = R"(#version 460
layout(input_attachment_index = 1, set = 2, binding = 17) uniform subpassInput inA;
layout(location = 0) out vec4 outColor;
void main() {
  vec4 a = subpassLoad(inA);
  outColor = vec4(a.g, a.r, a.b, 1.0);
})";

// writes blue to attachments 0 and 1, and has no textures, buffers, push constants or input
// attachments
constexpr const char* kFragmentWithoutResources = R"(#version 460
layout(location = 0) out vec4 out0;
layout(location = 1) out vec4 out1;
void main() {
  out0 = vec4(0.0, 0.0, 1.0, 1.0);
  out1 = out0;
})";

// red from the texture (set 0), green from the uniform buffer (set 1), and blue from the push
// constants: tint.b for view 0 and tint.a for view 1
constexpr const char* kFragmentResources = R"(#version 460
#extension GL_EXT_multiview : enable
layout(set = 0, binding = 0) uniform sampler2D uTex;
layout(std140, set = 1, binding = 0) uniform UniformBlock {
  vec4 color;
} ub;
layout(push_constant) uniform PushConstants {
  vec4 tint;
} pc;
layout(location = 0) out vec4 out0;
layout(location = 1) out vec4 out1;
void main() {
  float blue = gl_ViewIndex == 0 ? pc.tint.b : pc.tint.a;
  vec4 c = vec4(texture(uTex, vec2(0.5)).r, ub.color.g, blue, 1.0);
  out0 = c;
  out1 = c;
})";

// writes 3 attachments from 2 vec4 of push constants
constexpr const char* kFragmentThreeOutputs = R"(#version 460
#extension GL_EXT_multiview : enable
layout(push_constant) uniform PushConstants {
  vec4 tint;
  vec4 tint2;
} pc;
layout(location = 0) out vec4 out0;
layout(location = 1) out vec4 out1;
layout(location = 2) out vec4 out2;
void main() {
  vec4 c = vec4(pc.tint.r, pc.tint2.g, gl_ViewIndex == 0 ? pc.tint.b : pc.tint2.b, 1.0);
  out0 = c;
  out1 = c;
  out2 = c;
})";

// The state that the first subpass of the replay tests binds
constexpr Viewport kViewport = {.x = 0.0f,
                                .y = 0.0f,
                                .width = static_cast<float>(kSize) / 2.0f,
                                .height = static_cast<float>(kSize),
                                .minDepth = 0.25f,
                                .maxDepth = 0.75f};
constexpr ScissorRect kScissor = {.x = 0, .y = 0, .width = kSize / 2, .height = kSize};
constexpr std::array<float, 4> kTint = {0.0f, 0.0f, 30.0f / 255.0f, 90.0f / 255.0f};
constexpr uint32_t kDynamicOffset = 256; // where the green of the uniform buffer is 60
// The vertex and index buffers start with padding that these offsets skip
constexpr uint32_t kVertexBufferOffset = 4 * sizeof(float);
constexpr uint32_t kLeftVertexBufferOffset = 8 * sizeof(float);
constexpr uint32_t kIndexBufferOffset = 4 * sizeof(uint16_t);
constexpr uint32_t kStencilReference = 7;
constexpr std::array<float, 4> kBlendColor = {0.25f, 0.5f, 0.75f, 1.0f};
constexpr uint32_t kFrontReadMask = 0x0F;
constexpr uint32_t kFrontWriteMask = 0xF0;
constexpr uint32_t kBackReadMask = 0x3C;
constexpr uint32_t kBackWriteMask = 0xC3;

// The Vulkan commands that CommandRecorder records, with the arguments that it keeps:
//  BindPipeline            ints: pipeline
//  BindDescriptorSets      ints: first set, number of sets, handle of the first set;
//                          words: dynamic offsets
//  BindVertexBuffers       ints: first binding, first buffer, first offset
//  BindIndexBuffer         ints: buffer, offset, index type
//  PushConstants           ints: offset, size; words: the pushed data
//  SetViewport             floats: x, y, width, height, min depth, max depth
//  SetScissor              ints: x, y, width, height
//  SetDepthBias            floats: constant factor, clamp, slope factor
//  SetBlendConstants       floats: red, green, blue, alpha
//  SetStencil*             ints: face mask, mask or reference
//  SetCullMode             ints: cull mode
//  SetFrontFace            ints: front face
enum class Cmd : uint8_t {
  BeginRenderPass,
  NextSubpass,
  BindPipeline,
  BindDescriptorSets,
  BindVertexBuffers,
  BindIndexBuffer,
  PushConstants,
  SetViewport,
  SetScissor,
  SetDepthBias,
  SetBlendConstants,
  SetStencilCompareMask,
  SetStencilWriteMask,
  SetStencilReference,
  SetCullMode,
  SetFrontFace,
  Draw,
};

const char* toString(Cmd cmd) {
  static constexpr std::array<const char*, 17> kNames = {"BeginRenderPass",
                                                         "NextSubpass",
                                                         "BindPipeline",
                                                         "BindDescriptorSets",
                                                         "BindVertexBuffers",
                                                         "BindIndexBuffer",
                                                         "PushConstants",
                                                         "SetViewport",
                                                         "SetScissor",
                                                         "SetDepthBias",
                                                         "SetBlendConstants",
                                                         "SetStencilCompareMask",
                                                         "SetStencilWriteMask",
                                                         "SetStencilReference",
                                                         "SetCullMode",
                                                         "SetFrontFace",
                                                         "Draw"};
  return kNames[static_cast<size_t>(cmd)];
}

struct Command {
  Cmd cmd = Cmd::Draw;
  std::array<uint64_t, 4> ints = {};
  std::array<float, 6> floats = {};
  std::vector<uint32_t> words;
};

struct RecorderState {
  VulkanFunctionTable original = {};
  std::vector<Command> commands;
};

// The command functions are plain function pointers, so they find the state of the active
// CommandRecorder here
RecorderState*& activeState() {
  static RecorderState* state = nullptr;
  return state;
}

const VulkanFunctionTable& original() {
  return activeState()->original;
}

void record(Command command) {
  activeState()->commands.push_back(std::move(command));
}

template<typename Handle>
uint64_t toInt(Handle handle) {
  return reinterpret_cast<uint64_t>(handle);
}

void recordBeginRenderPass(VkCommandBuffer cmdBuf,
                           const VkRenderPassBeginInfo* info,
                           VkSubpassContents contents) {
  record({.cmd = Cmd::BeginRenderPass});
  original().vkCmdBeginRenderPass(cmdBuf, info, contents);
}

void recordNextSubpass(VkCommandBuffer cmdBuf, VkSubpassContents contents) {
  record({.cmd = Cmd::NextSubpass});
  original().vkCmdNextSubpass(cmdBuf, contents);
}

void recordBindPipeline(VkCommandBuffer cmdBuf,
                        VkPipelineBindPoint bindPoint,
                        VkPipeline pipeline) {
  record({.cmd = Cmd::BindPipeline, .ints = {toInt(pipeline)}});
  original().vkCmdBindPipeline(cmdBuf, bindPoint, pipeline);
}

void recordBindDescriptorSets(VkCommandBuffer cmdBuf,
                              VkPipelineBindPoint bindPoint,
                              VkPipelineLayout layout,
                              uint32_t firstSet,
                              uint32_t numSets,
                              const VkDescriptorSet* sets,
                              uint32_t numDynamicOffsets,
                              const uint32_t* dynamicOffsets) {
  record({.cmd = Cmd::BindDescriptorSets,
          .ints = {firstSet, numSets, toInt(sets[0])},
          .words = std::vector<uint32_t>(dynamicOffsets, dynamicOffsets + numDynamicOffsets)});
  original().vkCmdBindDescriptorSets(
      cmdBuf, bindPoint, layout, firstSet, numSets, sets, numDynamicOffsets, dynamicOffsets);
}

void recordBindVertexBuffers(VkCommandBuffer cmdBuf,
                             uint32_t firstBinding,
                             uint32_t numBindings,
                             const VkBuffer* buffers,
                             const VkDeviceSize* offsets) {
  record({.cmd = Cmd::BindVertexBuffers, .ints = {firstBinding, toInt(buffers[0]), offsets[0]}});
  original().vkCmdBindVertexBuffers(cmdBuf, firstBinding, numBindings, buffers, offsets);
}

void recordBindIndexBuffer(VkCommandBuffer cmdBuf,
                           VkBuffer buffer,
                           VkDeviceSize offset,
                           VkIndexType type) {
  record(
      {.cmd = Cmd::BindIndexBuffer, .ints = {toInt(buffer), offset, static_cast<uint64_t>(type)}});
  original().vkCmdBindIndexBuffer(cmdBuf, buffer, offset, type);
}

void recordPushConstants(VkCommandBuffer cmdBuf,
                         VkPipelineLayout layout,
                         VkShaderStageFlags stages,
                         uint32_t offset,
                         uint32_t size,
                         const void* data) {
  Command command = {.cmd = Cmd::PushConstants,
                     .ints = {offset, size},
                     .words = std::vector<uint32_t>(size / sizeof(uint32_t))};
  std::memcpy(command.words.data(), data, command.words.size() * sizeof(uint32_t));
  record(std::move(command));
  original().vkCmdPushConstants(cmdBuf, layout, stages, offset, size, data);
}

void recordSetViewport(VkCommandBuffer cmdBuf,
                       uint32_t first,
                       uint32_t count,
                       const VkViewport* viewports) {
  const VkViewport& v = viewports[0];
  record(
      {.cmd = Cmd::SetViewport, .floats = {v.x, v.y, v.width, v.height, v.minDepth, v.maxDepth}});
  original().vkCmdSetViewport(cmdBuf, first, count, viewports);
}

void recordSetScissor(VkCommandBuffer cmdBuf,
                      uint32_t first,
                      uint32_t count,
                      const VkRect2D* scissors) {
  const VkRect2D& r = scissors[0];
  record({.cmd = Cmd::SetScissor,
          .ints = {static_cast<uint64_t>(r.offset.x),
                   static_cast<uint64_t>(r.offset.y),
                   r.extent.width,
                   r.extent.height}});
  original().vkCmdSetScissor(cmdBuf, first, count, scissors);
}

void recordSetDepthBias(VkCommandBuffer cmdBuf,
                        float constantFactor,
                        float clamp,
                        float slopeFactor) {
  record({.cmd = Cmd::SetDepthBias, .floats = {constantFactor, clamp, slopeFactor}});
  original().vkCmdSetDepthBias(cmdBuf, constantFactor, clamp, slopeFactor);
}

void recordSetBlendConstants(VkCommandBuffer cmdBuf, const float* constants) {
  record({.cmd = Cmd::SetBlendConstants,
          .floats = {constants[0], constants[1], constants[2], constants[3]}});
  original().vkCmdSetBlendConstants(cmdBuf, constants);
}

void recordSetStencilCompareMask(VkCommandBuffer cmdBuf, VkStencilFaceFlags faces, uint32_t mask) {
  record({.cmd = Cmd::SetStencilCompareMask, .ints = {faces, mask}});
  original().vkCmdSetStencilCompareMask(cmdBuf, faces, mask);
}

void recordSetStencilWriteMask(VkCommandBuffer cmdBuf, VkStencilFaceFlags faces, uint32_t mask) {
  record({.cmd = Cmd::SetStencilWriteMask, .ints = {faces, mask}});
  original().vkCmdSetStencilWriteMask(cmdBuf, faces, mask);
}

void recordSetStencilReference(VkCommandBuffer cmdBuf,
                               VkStencilFaceFlags faces,
                               uint32_t reference) {
  record({.cmd = Cmd::SetStencilReference, .ints = {faces, reference}});
  original().vkCmdSetStencilReference(cmdBuf, faces, reference);
}

void recordSetCullMode(VkCommandBuffer cmdBuf, VkCullModeFlags cullMode) {
  record({.cmd = Cmd::SetCullMode, .ints = {cullMode}});
  original().vkCmdSetCullMode(cmdBuf, cullMode);
}

void recordSetFrontFace(VkCommandBuffer cmdBuf, VkFrontFace frontFace) {
  record({.cmd = Cmd::SetFrontFace, .ints = {static_cast<uint64_t>(frontFace)}});
  original().vkCmdSetFrontFace(cmdBuf, frontFace);
}

void recordDraw(VkCommandBuffer cmdBuf,
                uint32_t vertexCount,
                uint32_t instanceCount,
                uint32_t firstVertex,
                uint32_t firstInstance) {
  record({.cmd = Cmd::Draw});
  original().vkCmdDraw(cmdBuf, vertexCount, instanceCount, firstVertex, firstInstance);
}

void recordDrawIndexed(VkCommandBuffer cmdBuf,
                       uint32_t indexCount,
                       uint32_t instanceCount,
                       uint32_t firstIndex,
                       int32_t vertexOffset,
                       uint32_t firstInstance) {
  record({.cmd = Cmd::Draw});
  original().vkCmdDrawIndexed(
      cmdBuf, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

// Replaces the command functions of a VulkanContext with ones that record the commands that the
// encoders issue, and puts the original functions back when it goes out of scope, even if an
// assertion fails. There is one recorder at a time.
class CommandRecorder {
 public:
  explicit CommandRecorder(vulkan::VulkanContext& ctx) :
    // VulkanContext exposes its function table as a const reference to a table that it owns
    table_(const_cast<VulkanFunctionTable&>(ctx.vf_)) {
    state_.original = table_;
    activeState() = &state_;
    patch(table_.vkCmdBeginRenderPass, recordBeginRenderPass);
    patch(table_.vkCmdNextSubpass, recordNextSubpass);
    patch(table_.vkCmdBindPipeline, recordBindPipeline);
    patch(table_.vkCmdBindDescriptorSets, recordBindDescriptorSets);
    patch(table_.vkCmdBindVertexBuffers, recordBindVertexBuffers);
    patch(table_.vkCmdBindIndexBuffer, recordBindIndexBuffer);
    patch(table_.vkCmdPushConstants, recordPushConstants);
    patch(table_.vkCmdSetViewport, recordSetViewport);
    patch(table_.vkCmdSetScissor, recordSetScissor);
    patch(table_.vkCmdSetDepthBias, recordSetDepthBias);
    patch(table_.vkCmdSetBlendConstants, recordSetBlendConstants);
    patch(table_.vkCmdSetStencilCompareMask, recordSetStencilCompareMask);
    patch(table_.vkCmdSetStencilWriteMask, recordSetStencilWriteMask);
    patch(table_.vkCmdSetStencilReference, recordSetStencilReference);
    patch(table_.vkCmdSetCullMode, recordSetCullMode);
    patch(table_.vkCmdSetFrontFace, recordSetFrontFace);
    patch(table_.vkCmdDraw, recordDraw);
    patch(table_.vkCmdDrawIndexed, recordDrawIndexed);
  }

  ~CommandRecorder() {
    table_ = state_.original;
    activeState() = nullptr;
  }

  CommandRecorder(const CommandRecorder&) = delete;
  CommandRecorder& operator=(const CommandRecorder&) = delete;

  [[nodiscard]] const std::vector<Command>& commands() const {
    return state_.commands;
  }

 private:
  // Only the functions that the device has: the device reports a feature as supported if its
  // functions are not null
  template<typename Function>
  static void patch(Function& function, Function wrapper) {
    if (function != nullptr) {
      function = wrapper;
    }
  }

  VulkanFunctionTable& table_;
  RecorderState state_;
};

// Makes the device report that the multiview feature is not enabled, until it goes out of scope
class ScopedMultiviewDisabled {
 public:
  explicit ScopedMultiviewDisabled(vulkan::VulkanContext& ctx) :
    // VulkanContext exposes its features as a const reference to the ones that it owns
    features_(const_cast<vulkan::VulkanFeatures&>(ctx.features())),
    wasEnabled_(features_.featuresMultiview.multiview) {
    features_.featuresMultiview.multiview = VK_FALSE;
  }

  ~ScopedMultiviewDisabled() {
    features_.featuresMultiview.multiview = wasEnabled_;
  }

  ScopedMultiviewDisabled(const ScopedMultiviewDisabled&) = delete;
  ScopedMultiviewDisabled& operator=(const ScopedMultiviewDisabled&) = delete;

 private:
  vulkan::VulkanFeatures& features_;
  const VkBool32 wasEnabled_;
};

size_t& numDebugAborts() {
  static size_t count = 0;
  return count;
}

// Counts the IGL_DEBUG_ABORT() calls and the failed IGL_DEBUG_ASSERT() and IGL_DEBUG_VERIFY() calls
// that happen while it is in scope. There is no count if the build has no debug aborts.
class ScopedDebugAbortCounter {
 public:
  ScopedDebugAbortCounter() : previousListener_(iglGetDebugAbortListener()) {
    numDebugAborts() = 0;
    iglSetDebugAbortListener([](const char* /*category*/,
                                const char* /*reason*/,
                                const char* /*file*/,
                                const char* /*func*/,
                                int /*line*/,
                                const char* /*format*/,
                                va_list /*ap*/) { ++numDebugAborts(); });
  }

  ~ScopedDebugAbortCounter() {
    iglSetDebugAbortListener(previousListener_);
  }

  ScopedDebugAbortCounter(const ScopedDebugAbortCounter&) = delete;
  ScopedDebugAbortCounter& operator=(const ScopedDebugAbortCounter&) = delete;

  [[nodiscard]] size_t count() const {
    return numDebugAborts();
  }

 private:
  IGLErrorHandlerFunc IGL_NULLABLE previousListener_;
};

// The commands before the first vkCmdNextSubpass()
std::vector<Command> commandsBeforeNextSubpass(const std::vector<Command>& commands) {
  const auto nextSubpass = std::find_if(
      commands.begin(), commands.end(), [](const Command& c) { return c.cmd == Cmd::NextSubpass; });
  return {commands.begin(), nextSubpass};
}

// The commands that follow the `nth` vkCmdNextSubpass(), up to the first draw after it
std::vector<Command> commandsAfterNextSubpass(const std::vector<Command>& commands,
                                              size_t nth = 0) {
  std::vector<Command> window;
  size_t numNextSubpass = 0;
  bool isInWindow = false;
  for (const Command& command : commands) {
    if (!isInWindow) {
      isInWindow = command.cmd == Cmd::NextSubpass && numNextSubpass++ == nth;
      continue;
    }
    if (command.cmd == Cmd::Draw) {
      break;
    }
    window.push_back(command);
  }
  return window;
}

// The commands between the first and the second draw that follow the first vkCmdNextSubpass()
std::vector<Command> commandsBetweenDrawsAfterNextSubpass(const std::vector<Command>& commands) {
  const auto isDraw = [](const Command& c) { return c.cmd == Cmd::Draw; };
  const auto nextSubpass = std::find_if(
      commands.begin(), commands.end(), [](const Command& c) { return c.cmd == Cmd::NextSubpass; });
  const auto firstDraw = std::find_if(nextSubpass, commands.end(), isDraw);
  if (firstDraw == commands.end()) {
    return {};
  }
  return {firstDraw + 1, std::find_if(firstDraw + 1, commands.end(), isDraw)};
}

size_t countCommands(const std::vector<Command>& commands, Cmd cmd) {
  return static_cast<size_t>(std::count_if(
      commands.begin(), commands.end(), [cmd](const Command& c) { return c.cmd == cmd; }));
}

// The number of commands of kind `cmd` with these integer arguments
size_t countCommands(const std::vector<Command>& commands,
                     Cmd cmd,
                     const std::array<uint64_t, 4>& ints) {
  return static_cast<size_t>(std::count_if(commands.begin(), commands.end(), [&](const Command& c) {
    return c.cmd == cmd && c.ints == ints;
  }));
}

// The first command of kind `cmd`
const Command* IGL_NULLABLE firstCommand(const std::vector<Command>& commands, Cmd cmd) {
  const auto it = std::find_if(
      commands.begin(), commands.end(), [cmd](const Command& c) { return c.cmd == cmd; });
  return it != commands.end() ? &*it : nullptr;
}

// The command of kind `cmd`, which there must be exactly one of
const Command* IGL_NULLABLE onlyCommand(const std::vector<Command>& commands, Cmd cmd) {
  EXPECT_EQ(countCommands(commands, cmd), 1u) << "number of " << toString(cmd) << " commands";
  return firstCommand(commands, cmd);
}

// The last command of kind `cmd` whose first integer is `key`: the face of a stencil command, the
// first set of BindDescriptorSets or the first binding of BindVertexBuffers
const Command* IGL_NULLABLE lastCommand(const std::vector<Command>& commands,
                                        Cmd cmd,
                                        uint64_t key) {
  const auto it = std::find_if(commands.rbegin(), commands.rend(), [cmd, key](const Command& c) {
    return c.cmd == cmd && c.ints[0] == key;
  });
  return it != commands.rend() ? &*it : nullptr;
}

std::vector<uint32_t> toWords(const float* data, size_t numFloats) {
  std::vector<uint32_t> words(numFloats);
  std::memcpy(words.data(), data, numFloats * sizeof(float));
  return words;
}

uint64_t vkBufferOf(const IBuffer& buffer) {
  return toInt(static_cast<const vulkan::Buffer&>(buffer).getVkBuffer());
}

class StereoSubpassVulkanTest : public ::testing::Test {
 protected:
  void SetUp() override {
    igl::setDebugBreakEnabled(false);
    iglDev_ = util::createTestDevice();
    ASSERT_NE(iglDev_, nullptr);
    ASSERT_EQ(iglDev_->getBackendType(), BackendType::Vulkan);
    Result ret;
    cmdQueue_ = iglDev_->createCommandQueue({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    ASSERT_NE(cmdQueue_, nullptr);
  }

  [[nodiscard]] vulkan::VulkanContext& ctx() const {
    return static_cast<vulkan::Device&>(*iglDev_).getVulkanContext();
  }

  std::shared_ptr<ITexture> createArrayTexture(TextureDesc::TextureUsage usage, uint32_t layers) {
    const TextureDesc desc =
        layers > 1
            ? TextureDesc::new2DArray(TextureFormat::RGBA_UNorm8, kSize, kSize, layers, usage)
            : TextureDesc::new2D(TextureFormat::RGBA_UNorm8, kSize, kSize, usage);
    Result ret;
    auto tex = iglDev_->createTexture(desc, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return tex;
  }

  std::shared_ptr<IFramebuffer> createFramebuffer(const std::shared_ptr<ITexture>& a0,
                                                  const std::shared_ptr<ITexture>& a1,
                                                  FramebufferMode mode) {
    FramebufferDesc desc;
    desc.colorAttachments[0].texture = a0;
    desc.colorAttachments[1].texture = a1;
    desc.mode = mode;
    Result ret;
    auto fb = iglDev_->createFramebuffer(desc, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return fb;
  }

  std::shared_ptr<IRenderPipelineState> createPipeline(
      const char* vs,
      const char* fs,
      size_t numColorAttachments,
      const char* name,
      const std::shared_ptr<IVertexInputState>& vertexInput = nullptr,
      uint32_t isDynamicBufferMask = 0) {
    Result ret;
    auto stages =
        ShaderStagesCreator::fromModuleStringInput(*iglDev_, vs, "main", "", fs, "main", "", &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    RenderPipelineDesc desc;
    desc.vertexInputState = vertexInput;
    desc.shaderStages = std::move(stages);
    desc.targetDesc.colorAttachments.resize(numColorAttachments);
    for (size_t i = 0; i < numColorAttachments; ++i) {
      desc.targetDesc.colorAttachments[i].textureFormat = TextureFormat::RGBA_UNorm8;
    }
    desc.cullMode = CullMode::Disabled;
    desc.isDynamicBufferMask = isDynamicBufferMask;
    desc.debugName = genNameHandle(name);
    auto pipeline = iglDev_->createRenderPipeline(desc, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    return pipeline;
  }

  std::array<uint32_t, kSize * kSize> readLayer(const std::shared_ptr<IFramebuffer>& fb,
                                                size_t index,
                                                uint32_t layer) {
    std::array<uint32_t, kSize * kSize> pixels{};
    TextureRangeDesc range = TextureRangeDesc::new2D(0, 0, kSize, kSize);
    range.layer = layer;
    fb->copyBytesColorAttachment(*cmdQueue_, index, pixels.data(), range);
    return pixels;
  }

  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

TEST_F(StereoSubpassVulkanTest, FeatureIsReported) {
  EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::Subpasses));
  EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::Multiview));
  EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::MultiviewSubpasses));
}

TEST_F(StereoSubpassVulkanTest, InputAttachmentReadsTheLayerOfTheView) {
  ASSERT_TRUE(iglDev_->hasFeature(DeviceFeatures::MultiviewSubpasses));

  const auto att0 = createArrayTexture(
      TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Sampled, 2);
  const auto att1 = createArrayTexture(TextureDesc::TextureUsageBits::Attachment |
                                           TextureDesc::TextureUsageBits::InputAttachment |
                                           TextureDesc::TextureUsageBits::Sampled,
                                       2);
  const auto fb = createFramebuffer(att0, att1, FramebufferMode::Stereo);
  ASSERT_NE(fb, nullptr);

  const auto p0 = createPipeline(kVertexShaderFullscreen, kFragmentPerView, 2, "stereo sp0");
  const auto p1 = createPipeline(kVertexShaderFullscreen, kFragmentReadInput, 2, "stereo sp1");
  ASSERT_NE(p0, nullptr);
  ASSERT_NE(p1, nullptr);

  const RenderPassDesc renderPass{.colorAttachments = {{.loadAction = LoadAction::Clear,
                                                        .storeAction = StoreAction::Store,
                                                        .clearColor = {1.0f, 0.0f, 1.0f, 1.0f}},
                                                       {.loadAction = LoadAction::Clear,
                                                        .storeAction = StoreAction::Store,
                                                        .clearColor = {1.0f, 0.0f, 1.0f, 1.0f}}},
                                  .subpasses = std::vector<RenderPassDesc::SubpassDesc>{
                                      {.colorAttachmentMask = 0b10, .inputAttachmentMask = 0},
                                      {.colorAttachmentMask = 0b01, .inputAttachmentMask = 0b10}}};

  Result ret;
  const auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto encoder = cmdBuf->createRenderCommandEncoder(renderPass, fb, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(encoder, nullptr);
  encoder->bindRenderPipelineState(p0);
  encoder->draw(3);
  encoder->nextSubpass();
  encoder->bindRenderPipelineState(p1);
  encoder->draw(3);
  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();

  // attachment 1 (written by subpass 0): view 0 (100, 20, 0), view 1 (0, 50, 30)
  // attachment 0 (reads attachment 1 of the same view and swaps red and green)
  const uint32_t expected1[2] = {0xFF001464, 0xFF1E3200};
  const uint32_t expected0[2] = {0xFF006414, 0xFF1E0032};
  for (uint32_t layer = 0; layer < 2; ++layer) {
    const auto pixels0 = readLayer(fb, 0, layer);
    const auto pixels1 = readLayer(fb, 1, layer);
    for (size_t i = 0; i < pixels0.size(); ++i) {
      ASSERT_EQ(pixels0[i], expected0[layer]) << "att0 layer " << layer << " pixel " << i;
      ASSERT_EQ(pixels1[i], expected1[layer]) << "att1 layer " << layer << " pixel " << i;
    }
  }
}

TEST_F(StereoSubpassVulkanTest, StereoWithDepthAttachment) {
  ASSERT_TRUE(iglDev_->hasFeature(DeviceFeatures::MultiviewSubpasses));

  const auto att0 = createArrayTexture(
      TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Sampled, 2);
  const auto att1 = createArrayTexture(TextureDesc::TextureUsageBits::Attachment |
                                           TextureDesc::TextureUsageBits::InputAttachment,
                                       2);
  TextureDesc depthDesc = TextureDesc::new2DArray(
      TextureFormat::S8_UInt_Z24_UNorm, kSize, kSize, 2, TextureDesc::TextureUsageBits::Attachment);
  depthDesc.storage = ResourceStorage::Private;
  Result ret;
  const auto depth = iglDev_->createTexture(depthDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = att0;
  fbDesc.colorAttachments[1].texture = att1;
  fbDesc.depthAttachment.texture = depth;
  fbDesc.stencilAttachment.texture = depth;
  fbDesc.mode = FramebufferMode::Stereo;
  const auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  auto makePipeline = [&](const char* fs, const char* name) {
    auto stages = ShaderStagesCreator::fromModuleStringInput(
        *iglDev_, kVertexShaderFullscreen, "main", "", fs, "main", "", &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    RenderPipelineDesc desc;
    desc.shaderStages = std::move(stages);
    desc.targetDesc.colorAttachments.resize(2);
    desc.targetDesc.colorAttachments[0].textureFormat = TextureFormat::RGBA_UNorm8;
    desc.targetDesc.colorAttachments[1].textureFormat = TextureFormat::RGBA_UNorm8;
    desc.targetDesc.depthAttachmentFormat = TextureFormat::S8_UInt_Z24_UNorm;
    desc.targetDesc.stencilAttachmentFormat = TextureFormat::S8_UInt_Z24_UNorm;
    desc.cullMode = CullMode::Disabled;
    desc.debugName = genNameHandle(name);
    return iglDev_->createRenderPipeline(desc, &ret);
  };
  const auto p0 = makePipeline(kFragmentPerView, "depth sp0");
  const auto p1 = makePipeline(kFragmentReadInput, "depth sp1");
  ASSERT_NE(p0, nullptr);
  ASSERT_NE(p1, nullptr);
  const auto dss = iglDev_->createDepthStencilState(
      DepthStencilStateDesc{.compareFunction = CompareFunction::LessEqual,
                            .isDepthWriteEnabled = true},
      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;

  RenderPassDesc renderPass{
      .colorAttachments = {{.loadAction = LoadAction::Clear,
                            .storeAction = StoreAction::Store,
                            .clearColor = {1.0f, 0.0f, 1.0f, 1.0f}},
                           {.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store}},
      .subpasses = std::vector<RenderPassDesc::SubpassDesc>{
          {.colorAttachmentMask = 0b10, .inputAttachmentMask = 0},
          {.colorAttachmentMask = 0b01, .inputAttachmentMask = 0b10}}};
  renderPass.depthAttachment.loadAction = LoadAction::Clear;
  renderPass.depthAttachment.storeAction = StoreAction::Store;
  renderPass.depthAttachment.clearDepth = 1.0f;
  renderPass.stencilAttachment.loadAction = LoadAction::Clear;
  renderPass.stencilAttachment.storeAction = StoreAction::Store;

  const auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto encoder = cmdBuf->createRenderCommandEncoder(renderPass, fb, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(encoder, nullptr);
  encoder->bindRenderPipelineState(p0);
  encoder->bindDepthStencilState(dss);
  encoder->draw(3);
  encoder->nextSubpass();
  encoder->bindRenderPipelineState(p1);
  encoder->draw(3);
  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();

  const uint32_t expected0[2] = {0xFF006414, 0xFF1E0032};
  for (uint32_t layer = 0; layer < 2; ++layer) {
    const auto pixels0 = readLayer(fb, 0, layer);
    for (size_t i = 0; i < pixels0.size(); ++i) {
      ASSERT_EQ(pixels0[i], expected0[layer]) << "att0 layer " << layer << " pixel " << i;
    }
  }
}

TEST_F(StereoSubpassVulkanTest, MemorylessInputAttachment) {
  ASSERT_TRUE(iglDev_->hasFeature(DeviceFeatures::MultiviewSubpasses));

  const auto att0 = createArrayTexture(
      TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Sampled, 3);
  TextureDesc memDesc = TextureDesc::new2DArray(TextureFormat::RGBA_UNorm8,
                                                kSize,
                                                kSize,
                                                3,
                                                TextureDesc::TextureUsageBits::Attachment |
                                                    TextureDesc::TextureUsageBits::InputAttachment);
  memDesc.storage = ResourceStorage::Memoryless;
  Result ret;
  const auto att1 = iglDev_->createTexture(memDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const auto fb = createFramebuffer(att0, att1, FramebufferMode::Stereo);
  ASSERT_NE(fb, nullptr);

  const auto p0 = createPipeline(kVertexShaderFullscreen, kFragmentPerView, 2, "stereo layer1 sp0");
  const auto p1 =
      createPipeline(kVertexShaderFullscreen, kFragmentReadInput, 2, "stereo layer1 sp1");
  ASSERT_NE(p0, nullptr);
  ASSERT_NE(p1, nullptr);

  const RenderPassDesc renderPass{.colorAttachments = {{.loadAction = LoadAction::Clear,
                                                        .storeAction = StoreAction::Store,
                                                        .layer = 1,
                                                        .clearColor = {1.0f, 0.0f, 1.0f, 1.0f}},
                                                       {.loadAction = LoadAction::Clear,
                                                        .storeAction = StoreAction::DontCare,
                                                        .layer = 1,
                                                        .clearColor = {1.0f, 0.0f, 1.0f, 1.0f}}},
                                  .subpasses = std::vector<RenderPassDesc::SubpassDesc>{
                                      {.colorAttachmentMask = 0b10, .inputAttachmentMask = 0},
                                      {.colorAttachmentMask = 0b01, .inputAttachmentMask = 0b10}}};

  const auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  auto encoder = cmdBuf->createRenderCommandEncoder(renderPass, fb, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(encoder, nullptr);
  encoder->bindRenderPipelineState(p0);
  encoder->draw(3);
  encoder->nextSubpass();
  encoder->bindRenderPipelineState(p1);
  encoder->draw(3);
  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();

  // views 0 and 1 are layers 1 and 2
  const uint32_t expected0[2] = {0xFF006414, 0xFF1E0032};
  for (uint32_t view = 0; view < 2; ++view) {
    const auto pixels = readLayer(fb, 0, 1 + view);
    for (size_t i = 0; i < pixels.size(); ++i) {
      ASSERT_EQ(pixels[i], expected0[view]) << "view " << view << " pixel " << i;
    }
  }
}

// Attachment 1 is written by subpass 0, and attachment 0 by subpass 1, which reads attachment 1
// if `secondSubpassInputMask` is 0b10
RenderPassDesc makeTwoSubpassRenderPass(uint8_t secondSubpassInputMask) {
  return RenderPassDesc{
      .colorAttachments = {{.loadAction = LoadAction::Clear,
                            .storeAction = StoreAction::Store,
                            .clearColor = {1.0f, 0.0f, 1.0f, 1.0f}},
                           {.loadAction = LoadAction::Clear,
                            .storeAction = StoreAction::Store,
                            .clearColor = {1.0f, 0.0f, 1.0f, 1.0f}}},
      .subpasses = std::vector<RenderPassDesc::SubpassDesc>{
          {.colorAttachmentMask = 0b10, .inputAttachmentMask = 0},
          {.colorAttachmentMask = 0b01, .inputAttachmentMask = secondSubpassInputMask}}};
}

// A Stereo render pass with subpasses needs the multiview feature of the device. The test device
// has it, so these tests make it report that it does not.
TEST_F(StereoSubpassVulkanTest, EncoderRejectsStereoSubpassesWithoutMultiview) {
  ASSERT_TRUE(iglDev_->hasFeature(DeviceFeatures::MultiviewSubpasses));

  const auto att0 = createArrayTexture(
      TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Sampled, 2);
  const auto att1 = createArrayTexture(TextureDesc::TextureUsageBits::Attachment |
                                           TextureDesc::TextureUsageBits::InputAttachment,
                                       2);
  const auto fb = createFramebuffer(att0, att1, FramebufferMode::Stereo);
  ASSERT_NE(fb, nullptr);
  const RenderPassDesc renderPass = makeTwoSubpassRenderPass(0b10);
  // Nothing but the missing feature makes this render pass invalid
  EXPECT_TRUE(renderPass.validate(*fb).isOk());

  {
    const ScopedMultiviewDisabled multiviewDisabled(ctx());
    EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::Subpasses));
    EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::Multiview));
    EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::MultiviewSubpasses));

    Result ret;
    const auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    const auto encoder = cmdBuf->createRenderCommandEncoder(renderPass, fb, &ret);
    EXPECT_EQ(ret.code, Result::Code::Unsupported);
    EXPECT_EQ(encoder, nullptr);
    if (encoder) {
      encoder->endEncoding();
    }
    // The command buffer is empty, but the queue expects every command buffer to be submitted
    cmdQueue_->submit(*cmdBuf);
    cmdBuf->waitUntilCompleted();
  }

  EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::MultiviewSubpasses));
}

TEST_F(StereoSubpassVulkanTest, MonoSubpassesDoNotNeedMultiview) {
  const auto att0 = createArrayTexture(
      TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Sampled, 1);
  const auto att1 = createArrayTexture(TextureDesc::TextureUsageBits::Attachment |
                                           TextureDesc::TextureUsageBits::InputAttachment,
                                       1);
  const auto fb = createFramebuffer(att0, att1, FramebufferMode::Mono);
  ASSERT_NE(fb, nullptr);
  const RenderPassDesc renderPass = makeTwoSubpassRenderPass(0b10);

  const ScopedMultiviewDisabled multiviewDisabled(ctx());
  ASSERT_TRUE(iglDev_->hasFeature(DeviceFeatures::Subpasses));
  ASSERT_FALSE(iglDev_->hasFeature(DeviceFeatures::MultiviewSubpasses));

  Result ret;
  const auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const auto encoder = cmdBuf->createRenderCommandEncoder(renderPass, fb, &ret);
  EXPECT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(encoder, nullptr);
  encoder->nextSubpass();
  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();
}

TEST_F(StereoSubpassVulkanTest, EncoderRejectsStereoDepthWithOneLayer) {
  ASSERT_TRUE(iglDev_->hasFeature(DeviceFeatures::MultiviewSubpasses));

  const auto att0 = createArrayTexture(
      TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Sampled, 2);
  const auto att1 = createArrayTexture(TextureDesc::TextureUsageBits::Attachment |
                                           TextureDesc::TextureUsageBits::InputAttachment,
                                       2);
  TextureDesc depthDesc = TextureDesc::new2D(
      TextureFormat::S8_UInt_Z24_UNorm, kSize, kSize, TextureDesc::TextureUsageBits::Attachment);
  depthDesc.storage = ResourceStorage::Private;
  Result ret;
  const auto depth = iglDev_->createTexture(depthDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = att0;
  fbDesc.colorAttachments[1].texture = att1;
  fbDesc.depthAttachment.texture = depth;
  fbDesc.mode = FramebufferMode::Stereo;
  const auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const RenderPassDesc renderPass = makeTwoSubpassRenderPass(0b10);

  // Both views need their own layer of the depth attachment
  EXPECT_EQ(renderPass.validate(*fb).code, Result::Code::ArgumentInvalid);

  const auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const auto encoder = cmdBuf->createRenderCommandEncoder(renderPass, fb, &ret);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(encoder, nullptr);
  if (encoder) {
    encoder->endEncoding();
  }
  // The command buffer is empty, but the queue expects every command buffer to be submitted
  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();
}

// The tests that record the commands that follow vkCmdNextSubpass()
class StereoSubpassReplayVulkanTest : public StereoSubpassVulkanTest {
 protected:
  enum class Scenario {
    Plain,
    // Binds a texture individually after the first draw, which replaces the texture bind group
    IndividualTextureBeforeNextSubpass,
    // Binds a texture individually right after nextSubpass()
    IndividualTextureAfterNextSubpass,
    // Destroys both bind groups after the first draw, as when the app's Holders go out of scope
    BindGroupsDestroyedBeforeNextSubpass,
    // The second subpass reads an input attachment with a pipeline that has no textures, buffers
    // or push constants
    SecondPipelineWithoutResources,
    // The second subpass draws with a pipeline that has no textures, buffers or push constants,
    // and then with the pipeline of the first subpass, after binding the bind groups again
    ResourcePipelineAfterPipelineWithoutResources,
  };

  struct Rendered {
    std::shared_ptr<IFramebuffer> framebuffer;
    std::vector<Command> commands;
  };

  void SetUp() override {
    StereoSubpassVulkanTest::SetUp();
    if (HasFatalFailure()) {
      return;
    }
    ASSERT_TRUE(iglDev_->hasFeature(DeviceFeatures::MultiviewSubpasses));
    createResources();
  }

  void createResources() {
    Result ret;

    const TextureDesc textureDesc = TextureDesc::new2D(
        TextureFormat::RGBA_UNorm8, 4, 4, TextureDesc::TextureUsageBits::Sampled);
    texture_ = iglDev_->createTexture(textureDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    const std::vector<uint32_t> pixels(16, 0xFF000032); // red is 50
    ASSERT_TRUE(texture_->upload(texture_->getFullRange(0), pixels.data()).isOk());
    otherTexture_ = iglDev_->createTexture(textureDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    const std::vector<uint32_t> otherPixels(16, 0xFF000046); // red is 70
    ASSERT_TRUE(otherTexture_->upload(otherTexture_->getFullRange(0), otherPixels.data()).isOk());

    sampler_ =
        iglDev_->createSamplerState(SamplerStateDesc{.minFilter = SamplerMinMagFilter::Nearest,
                                                     .magFilter = SamplerMinMagFilter::Nearest},
                                    &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;

    // The green is 10 at offset 0 and 60 at offset kDynamicOffset
    std::vector<float> uniformData(128, 0.0f);
    uniformData[1] = 10.0f / 255.0f;
    uniformData[kDynamicOffset / sizeof(float) + 1] = 60.0f / 255.0f;
    uniformBuffer_ = iglDev_->createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Uniform,
                                                      .data = uniformData.data(),
                                                      .length = uniformData.size() * sizeof(float),
                                                      .storage = ResourceStorage::Shared},
                                           &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;

    // A quad that covers the framebuffer, and one that covers its left half
    const std::array<float, 12> positions = {
        7.0f, 7.0f, 7.0f, 7.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
    vertexBuffer_ = iglDev_->createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Vertex,
                                                     .data = positions.data(),
                                                     .length = sizeof(positions),
                                                     .storage = ResourceStorage::Shared},
                                          &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    const std::array<float, 16> leftPositions = {7.0f,
                                                 7.0f,
                                                 7.0f,
                                                 7.0f,
                                                 7.0f,
                                                 7.0f,
                                                 7.0f,
                                                 7.0f,
                                                 -1.0f,
                                                 -1.0f,
                                                 0.0f,
                                                 -1.0f,
                                                 0.0f,
                                                 1.0f,
                                                 -1.0f,
                                                 1.0f};
    leftVertexBuffer_ = iglDev_->createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Vertex,
                                                         .data = leftPositions.data(),
                                                         .length = sizeof(leftPositions),
                                                         .storage = ResourceStorage::Shared},
                                              &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    const std::array<uint16_t, 10> indices = {9, 9, 9, 9, 0, 1, 2, 0, 2, 3};
    indexBuffer_ = iglDev_->createBuffer(BufferDesc{.type = BufferDesc::BufferTypeBits::Index,
                                                    .data = indices.data(),
                                                    .length = sizeof(indices),
                                                    .storage = ResourceStorage::Shared},
                                         &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;

    VertexInputStateDesc inputDesc;
    inputDesc.attributes[0].format = VertexAttributeFormat::Float2;
    inputDesc.attributes[0].offset = 0;
    inputDesc.attributes[0].bufferIndex = 0;
    inputDesc.attributes[0].name = "pos";
    inputDesc.attributes[0].location = 0;
    inputDesc.inputBindings[0].stride = sizeof(float) * 2;
    inputDesc.numAttributes = inputDesc.numInputBindings = 1;
    vertexInput_ = iglDev_->createVertexInputState(inputDesc, &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;

    textureGroup_ = iglDev_->createBindGroup(BindGroupTextureDesc{.textures = {texture_},
                                                                  .samplers = {sampler_},
                                                                  .debugName = "textureGroup"},
                                             nullptr,
                                             &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
    bufferGroup_ = iglDev_->createBindGroup(BindGroupBufferDesc{.buffers = {uniformBuffer_},
                                                                .offset = {0},
                                                                .size = {16},
                                                                .isDynamicBufferMask = 1u,
                                                                .debugName = "bufferGroup"},
                                            &ret);
    ASSERT_TRUE(ret.isOk()) << ret.message;
  }

  // Renders a render pass with 2 subpasses, and a draw in each. The first subpass binds all the
  // state that a replay has to bind again, and `scenario` decides what happens before the second
  // draw. Returns the framebuffer and the commands that the encoder recorded.
  [[nodiscard]] Rendered render(bool isStereo, Scenario scenario) {
    const uint32_t numLayers = isStereo ? 2 : 1;
    const bool readsInput = scenario == Scenario::SecondPipelineWithoutResources;
    const bool drawsTwice = scenario == Scenario::ResourcePipelineAfterPipelineWithoutResources;
    const bool hasPipelineWithoutResources = readsInput || drawsTwice;
    const auto att0 = createArrayTexture(TextureDesc::TextureUsageBits::Attachment |
                                             TextureDesc::TextureUsageBits::Sampled,
                                         numLayers);
    const auto att1 = createArrayTexture(
        TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Sampled |
            (readsInput ? TextureDesc::TextureUsageBits::InputAttachment : 0),
        numLayers);
    const auto fb =
        createFramebuffer(att0, att1, isStereo ? FramebufferMode::Stereo : FramebufferMode::Mono);
    EXPECT_NE(fb, nullptr);
    const auto pipeline = createPipeline(
        kVertexShaderBuffer, kFragmentResources, 2, "replay pipeline", vertexInput_, 1u);
    EXPECT_NE(pipeline, nullptr);
    const auto pipelineWithoutResources =
        hasPipelineWithoutResources
            ? createPipeline(kVertexShaderFullscreen,
                             readsInput ? kFragmentReadInput : kFragmentWithoutResources,
                             2,
                             "no resources pipeline")
            : nullptr;
    Result ret;
    const auto depthStencilState = iglDev_->createDepthStencilState(
        DepthStencilStateDesc{
            .backFaceStencil = {.readMask = kBackReadMask, .writeMask = kBackWriteMask},
            .frontFaceStencil = {.readMask = kFrontReadMask, .writeMask = kFrontWriteMask}},
        &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    if (!fb || !pipeline || (hasPipelineWithoutResources && !pipelineWithoutResources) ||
        !depthStencilState) {
      return {};
    }

    const RenderPassDesc renderPass = makeTwoSubpassRenderPass(readsInput ? 0b10 : 0);
    const auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    CommandRecorder recorder(ctx());
    const auto encoder = cmdBuf->createRenderCommandEncoder(renderPass, fb, &ret);
    EXPECT_TRUE(ret.isOk()) << ret.message;
    EXPECT_NE(encoder, nullptr);
    if (!encoder) {
      return {};
    }

    const auto bindOtherTexture = [this, &encoder]() {
      encoder->bindTexture(0, otherTexture_.get());
      encoder->bindSamplerState(0, igl::BindTarget::kFragment, sampler_.get());
    };

    encoder->bindRenderPipelineState(pipeline);
    encoder->bindDepthStencilState(depthStencilState);
    encoder->bindVertexBuffer(0, *vertexBuffer_, kVertexBufferOffset);
    encoder->bindIndexBuffer(*indexBuffer_, IndexFormat::UInt16, kIndexBufferOffset);
    encoder->bindBindGroup(textureGroup_);
    const uint32_t dynamicOffset = kDynamicOffset;
    encoder->bindBindGroup(bufferGroup_, 1, &dynamicOffset);
    encoder->bindPushConstants(kTint.data(), sizeof(kTint), 0);
    if (!readsInput) {
      // The second subpass of SecondPipelineWithoutResources reads every pixel that the first one
      // wrote, so only the other scenarios restrict it to the left half
      encoder->bindViewport(kViewport);
      encoder->bindScissorRect(kScissor);
    }
    encoder->setStencilReferenceValue(kStencilReference);
    encoder->setBlendColor(Color(kBlendColor[0], kBlendColor[1], kBlendColor[2], kBlendColor[3]));
    encoder->setDepthBias(1.0f, 2.0f, 0.0f);
    encoder->setCullMode(CullMode::Front);
    encoder->setFrontFacingWinding(WindingMode::Clockwise);
    encoder->drawIndexed(6);
    if (scenario == Scenario::IndividualTextureBeforeNextSubpass) {
      bindOtherTexture();
    }
    if (scenario == Scenario::BindGroupsDestroyedBeforeNextSubpass) {
      textureGroup_.reset();
      bufferGroup_.reset();
    }
    encoder->nextSubpass();
    if (scenario == Scenario::IndividualTextureAfterNextSubpass) {
      bindOtherTexture();
    }
    if (hasPipelineWithoutResources) {
      encoder->bindRenderPipelineState(pipelineWithoutResources);
      encoder->draw(3);
    }
    if (drawsTwice) {
      // Binding a pipeline makes the next draw call use the individually bound resources, in a
      // Mono render pass too, so the bind groups are bound again. The push constants are not.
      encoder->bindRenderPipelineState(pipeline);
      encoder->bindBindGroup(textureGroup_);
      encoder->bindBindGroup(bufferGroup_, 1, &dynamicOffset);
    }
    if (!readsInput) {
      encoder->drawIndexed(6);
    }
    encoder->endEncoding();
    cmdQueue_->submit(*cmdBuf);
    cmdBuf->waitUntilCompleted();
    return {.framebuffer = fb, .commands = recorder.commands()};
  }

  // Expects the first `drawnWidth` columns of `layer` of attachment `index` to be `drawn`, and the
  // others to have the clear color
  void expectPixels(const std::shared_ptr<IFramebuffer>& fb,
                    size_t index,
                    uint32_t layer,
                    uint32_t drawn,
                    uint32_t drawnWidth) {
    constexpr uint32_t kClearColor = 0xFFFF00FF; // (255, 0, 255, 255)
    const auto pixels = readLayer(fb, index, layer);
    for (uint32_t y = 0; y < kSize; ++y) {
      for (uint32_t x = 0; x < kSize; ++x) {
        ASSERT_EQ(pixels[y * kSize + x], x < drawnWidth ? drawn : kClearColor)
            << "attachment " << index << " layer " << layer << " pixel (" << x << ", " << y << ")";
      }
    }
  }

  std::shared_ptr<ITexture> texture_; // red is 50
  std::shared_ptr<ITexture> otherTexture_; // red is 70
  std::shared_ptr<ISamplerState> sampler_;
  std::shared_ptr<IBuffer> uniformBuffer_;
  std::shared_ptr<IBuffer> vertexBuffer_;
  std::shared_ptr<IBuffer> leftVertexBuffer_;
  std::shared_ptr<IBuffer> indexBuffer_;
  std::shared_ptr<IVertexInputState> vertexInput_;
  Holder<BindGroupTextureHandle> textureGroup_;
  Holder<BindGroupBufferHandle> bufferGroup_;
};

TEST_F(StereoSubpassReplayVulkanTest, StereoReplaysTheStateAfterNextSubpass) {
  const Rendered rendered = render(true, Scenario::Plain);
  ASSERT_NE(rendered.framebuffer, nullptr);
  const std::vector<Command>& commands = rendered.commands;

  const std::vector<Command> before = commandsBeforeNextSubpass(commands);
  const std::vector<Command> window = commandsAfterNextSubpass(commands);

  // A multiview render pass leaves the viewport and the scissor undefined when it begins, so the
  // default ones are recorded after vkCmdBeginRenderPass()
  ASSERT_FALSE(before.empty());
  EXPECT_EQ(before[0].cmd, Cmd::BeginRenderPass);
  const Command* defaultViewport = firstCommand(before, Cmd::SetViewport);
  const Command* defaultScissor = firstCommand(before, Cmd::SetScissor);
  ASSERT_NE(defaultViewport, nullptr);
  ASSERT_NE(defaultScissor, nullptr);
  // bindViewport() flips the y axis
  const float size = static_cast<float>(kSize);
  EXPECT_EQ(defaultViewport->floats, (std::array<float, 6>{0.0f, size, size, -size, 0.0f, 1.0f}));
  EXPECT_EQ(defaultScissor->ints, (std::array<uint64_t, 4>{0, 0, kSize, kSize}));

  // The commands that the replay records: one of each, and the 2 faces of the stencil masks
  const std::array<std::pair<Cmd, size_t>, 14> expectedCounts = {{{Cmd::BindPipeline, 1},
                                                                  {Cmd::BindDescriptorSets, 2},
                                                                  {Cmd::BindVertexBuffers, 1},
                                                                  {Cmd::BindIndexBuffer, 1},
                                                                  {Cmd::PushConstants, 1},
                                                                  {Cmd::SetViewport, 1},
                                                                  {Cmd::SetScissor, 1},
                                                                  {Cmd::SetDepthBias, 1},
                                                                  {Cmd::SetBlendConstants, 1},
                                                                  {Cmd::SetStencilCompareMask, 2},
                                                                  {Cmd::SetStencilWriteMask, 2},
                                                                  {Cmd::SetStencilReference, 1},
                                                                  {Cmd::SetCullMode, 1},
                                                                  {Cmd::SetFrontFace, 1}}};
  size_t numExpectedCommands = 0;
  for (const auto& [cmd, count] : expectedCounts) {
    EXPECT_EQ(countCommands(window, cmd), count) << "number of " << toString(cmd) << " commands";
    numExpectedCommands += count;
  }
  EXPECT_EQ(window.size(), numExpectedCommands) << "number of commands after nextSubpass()";

  // The dynamic state that the first subpass set, and not the default one
  const Command* viewport = onlyCommand(window, Cmd::SetViewport);
  ASSERT_NE(viewport, nullptr);
  EXPECT_EQ(viewport->floats,
            (std::array<float, 6>{kViewport.x,
                                  kViewport.height - kViewport.y,
                                  kViewport.width,
                                  -kViewport.height,
                                  kViewport.minDepth,
                                  kViewport.maxDepth}));
  const Command* scissor = onlyCommand(window, Cmd::SetScissor);
  ASSERT_NE(scissor, nullptr);
  EXPECT_EQ(scissor->ints,
            (std::array<uint64_t, 4>{kScissor.x, kScissor.y, kScissor.width, kScissor.height}));
  const Command* depthBias = onlyCommand(window, Cmd::SetDepthBias);
  ASSERT_NE(depthBias, nullptr);
  // the constant factor, the clamp and the slope factor
  EXPECT_EQ(depthBias->floats, (std::array<float, 6>{1.0f, 0.0f, 2.0f}));
  const Command* blendConstants = onlyCommand(window, Cmd::SetBlendConstants);
  ASSERT_NE(blendConstants, nullptr);
  EXPECT_EQ(blendConstants->floats,
            (std::array<float, 6>{kBlendColor[0], kBlendColor[1], kBlendColor[2], kBlendColor[3]}));
  constexpr uint64_t kFront = VK_STENCIL_FACE_FRONT_BIT;
  constexpr uint64_t kBack = VK_STENCIL_FACE_BACK_BIT;
  EXPECT_EQ(countCommands(window, Cmd::SetStencilCompareMask, {kFront, kFrontReadMask}), 1u);
  EXPECT_EQ(countCommands(window, Cmd::SetStencilWriteMask, {kFront, kFrontWriteMask}), 1u);
  EXPECT_EQ(countCommands(window, Cmd::SetStencilCompareMask, {kBack, kBackReadMask}), 1u);
  EXPECT_EQ(countCommands(window, Cmd::SetStencilWriteMask, {kBack, kBackWriteMask}), 1u);
  EXPECT_EQ(countCommands(window, Cmd::SetStencilReference, {kFront | kBack, kStencilReference}),
            1u);
  EXPECT_EQ(countCommands(window, Cmd::SetCullMode, {VK_CULL_MODE_FRONT_BIT}), 1u);
  EXPECT_EQ(countCommands(window, Cmd::SetFrontFace, {VK_FRONT_FACE_CLOCKWISE}), 1u);

  // The push constants and the vertex and index buffers
  const Command* pushConstants = onlyCommand(window, Cmd::PushConstants);
  ASSERT_NE(pushConstants, nullptr);
  EXPECT_EQ(pushConstants->ints, (std::array<uint64_t, 4>{0, sizeof(kTint)}));
  EXPECT_EQ(pushConstants->words, toWords(kTint.data(), kTint.size()));
  EXPECT_EQ(countCommands(window,
                          Cmd::BindVertexBuffers,
                          {0, vkBufferOf(*vertexBuffer_), kVertexBufferOffset}),
            1u);
  EXPECT_EQ(countCommands(window,
                          Cmd::BindIndexBuffer,
                          {vkBufferOf(*indexBuffer_), kIndexBufferOffset, VK_INDEX_TYPE_UINT16}),
            1u);

  // The descriptor sets of both bind groups, which are the ones that the first subpass bound, and
  // the dynamic offset of the buffer one
  const Command* textureSet = lastCommand(window, Cmd::BindDescriptorSets, 0);
  const Command* bufferSet = lastCommand(window, Cmd::BindDescriptorSets, 1);
  const Command* firstTextureSet = lastCommand(before, Cmd::BindDescriptorSets, 0);
  const Command* firstBufferSet = lastCommand(before, Cmd::BindDescriptorSets, 1);
  ASSERT_NE(textureSet, nullptr);
  ASSERT_NE(bufferSet, nullptr);
  ASSERT_NE(firstTextureSet, nullptr);
  ASSERT_NE(firstBufferSet, nullptr);
  EXPECT_EQ(textureSet->ints, firstTextureSet->ints);
  EXPECT_EQ(bufferSet->ints, firstBufferSet->ints);
  EXPECT_TRUE(textureSet->words.empty());
  EXPECT_EQ(bufferSet->words, std::vector<uint32_t>{kDynamicOffset});

  // red 50 from the texture, green 60 from the uniform buffer at kDynamicOffset, and blue from the
  // push constants: 30 for view 0 and 90 for view 1. Both viewport and scissor cover the left half.
  for (uint32_t layer = 0; layer < 2; ++layer) {
    const uint32_t drawn = layer == 0 ? 0xFF1E3C32 : 0xFF5A3C32;
    expectPixels(rendered.framebuffer, 0, layer, drawn, kSize / 2);
    expectPixels(rendered.framebuffer, 1, layer, drawn, kSize / 2);
  }
}

TEST_F(StereoSubpassReplayVulkanTest, MonoDoesNotReplayTheStateAfterNextSubpass) {
  const Rendered rendered = render(false, Scenario::Plain);
  ASSERT_NE(rendered.framebuffer, nullptr);

  // A Mono render pass keeps the state, so the next subpass only needs its pipeline
  const std::vector<Command> window = commandsAfterNextSubpass(rendered.commands);
  ASSERT_EQ(window.size(), 1u);
  EXPECT_EQ(window[0].cmd, Cmd::BindPipeline);

  expectPixels(rendered.framebuffer, 0, 0, 0xFF1E3C32, kSize / 2);
  expectPixels(rendered.framebuffer, 1, 0, 0xFF1E3C32, kSize / 2);
}

TEST_F(StereoSubpassReplayVulkanTest, StereoReplaysTheLatestVertexBufferAndPushConstants) {
  const auto usage =
      TextureDesc::TextureUsageBits::Attachment | TextureDesc::TextureUsageBits::Sampled;
  FramebufferDesc fbDesc;
  fbDesc.colorAttachments[0].texture = createArrayTexture(usage, 2);
  fbDesc.colorAttachments[1].texture = createArrayTexture(usage, 2);
  fbDesc.colorAttachments[2].texture = createArrayTexture(usage, 2);
  fbDesc.mode = FramebufferMode::Stereo;
  Result ret;
  const auto fb = iglDev_->createFramebuffer(fbDesc, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  const auto pipeline = createPipeline(
      kVertexShaderBuffer, kFragmentThreeOutputs, 3, "three subpasses", vertexInput_, 0);
  ASSERT_NE(pipeline, nullptr);

  const RenderPassDesc::AttachmentDesc clearAndStore = {.loadAction = LoadAction::Clear,
                                                        .storeAction = StoreAction::Store,
                                                        .clearColor = {1.0f, 0.0f, 1.0f, 1.0f}};
  const RenderPassDesc renderPass{
      .colorAttachments = {clearAndStore, clearAndStore, clearAndStore},
      .subpasses = std::vector<RenderPassDesc::SubpassDesc>{{.colorAttachmentMask = 0b010},
                                                            {.colorAttachmentMask = 0b100},
                                                            {.colorAttachmentMask = 0b001}}};

  // The first subpass pushes the last 6 floats of the push constants, and the second subpass pushes
  // 2 floats among them. Nothing pushes the first 2 floats, so what the third subpass has to push
  // starts at the third float.
  constexpr size_t kFirstPushOffset = 2 * sizeof(float);
  constexpr size_t kSecondPushOffset = 3 * sizeof(float);
  const std::array<float, 6> firstPush = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
  const std::array<float, 2> secondPush = {0.7f, 0.8f};
  const std::array<float, 6> mergedPush = {
      firstPush[0], secondPush[0], secondPush[1], firstPush[3], firstPush[4], firstPush[5]};

  const auto cmdBuf = cmdQueue_->createCommandBuffer({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  CommandRecorder recorder(ctx());
  const auto encoder = cmdBuf->createRenderCommandEncoder(renderPass, fb, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(encoder, nullptr);
  encoder->bindRenderPipelineState(pipeline);
  encoder->bindVertexBuffer(0, *vertexBuffer_, kVertexBufferOffset);
  encoder->bindIndexBuffer(*indexBuffer_, IndexFormat::UInt16, kIndexBufferOffset);
  encoder->bindPushConstants(firstPush.data(), sizeof(firstPush), kFirstPushOffset);
  encoder->drawIndexed(6); // subpass 0 writes attachment 1
  encoder->nextSubpass();
  encoder->bindVertexBuffer(0, *leftVertexBuffer_, kLeftVertexBufferOffset);
  encoder->bindPushConstants(secondPush.data(), sizeof(secondPush), kSecondPushOffset);
  encoder->drawIndexed(6); // subpass 1 writes attachment 2
  encoder->nextSubpass();
  encoder->drawIndexed(6); // subpass 2 writes attachment 0, with no new state
  encoder->endEncoding();
  cmdQueue_->submit(*cmdBuf);
  cmdBuf->waitUntilCompleted();

  // The third subpass records the latest vertex buffer, the pushes of both subpasses, and the
  // state that did not change
  const std::vector<Command> window = commandsAfterNextSubpass(recorder.commands(), 1);
  EXPECT_EQ(countCommands(window, Cmd::BindPipeline), 1u);
  EXPECT_EQ(countCommands(window, Cmd::SetViewport), 1u);
  EXPECT_EQ(countCommands(window, Cmd::SetScissor), 1u);
  EXPECT_EQ(countCommands(window,
                          Cmd::BindIndexBuffer,
                          {vkBufferOf(*indexBuffer_), kIndexBufferOffset, VK_INDEX_TYPE_UINT16}),
            1u);
  ASSERT_NE(vkBufferOf(*vertexBuffer_), vkBufferOf(*leftVertexBuffer_));
  EXPECT_EQ(countCommands(window, Cmd::BindVertexBuffers), 1u);
  EXPECT_EQ(countCommands(window,
                          Cmd::BindVertexBuffers,
                          {0, vkBufferOf(*leftVertexBuffer_), kLeftVertexBufferOffset}),
            1u);
  // The push constants that the subpasses pushed together
  const Command* pushConstants = onlyCommand(window, Cmd::PushConstants);
  ASSERT_NE(pushConstants, nullptr);
  EXPECT_EQ(pushConstants->ints, (std::array<uint64_t, 4>{kFirstPushOffset, sizeof(mergedPush)}));
  EXPECT_EQ(pushConstants->words, toWords(mergedPush.data(), mergedPush.size()));
}

TEST_F(StereoSubpassReplayVulkanTest,
       StereoIndividualTextureAfterNextSubpassOverridesTheBindGroup) {
  const Rendered rendered = render(true, Scenario::IndividualTextureAfterNextSubpass);
  ASSERT_NE(rendered.framebuffer, nullptr);

  const std::vector<Command> before = commandsBeforeNextSubpass(rendered.commands);
  const std::vector<Command> window = commandsAfterNextSubpass(rendered.commands);
  EXPECT_EQ(countCommands(window, Cmd::BindDescriptorSets), 2u);
  // The descriptor set of the textures is the binder's, and not the one of the bind group
  const Command* textureSet = lastCommand(window, Cmd::BindDescriptorSets, 0);
  const Command* groupTextureSet = lastCommand(before, Cmd::BindDescriptorSets, 0);
  ASSERT_NE(textureSet, nullptr);
  ASSERT_NE(groupTextureSet, nullptr);
  EXPECT_NE(textureSet->ints[2], groupTextureSet->ints[2]);
  // but the bind group of the buffers is still replayed, with its dynamic offset
  const Command* bufferSet = lastCommand(window, Cmd::BindDescriptorSets, 1);
  const Command* groupBufferSet = lastCommand(before, Cmd::BindDescriptorSets, 1);
  ASSERT_NE(bufferSet, nullptr);
  ASSERT_NE(groupBufferSet, nullptr);
  EXPECT_EQ(bufferSet->ints, groupBufferSet->ints);
  EXPECT_EQ(bufferSet->words, std::vector<uint32_t>{kDynamicOffset});

  // Attachment 1 is written by subpass 0 with red 50 of the bind group's texture, and attachment 0
  // by subpass 1 with red 70 of the other texture
  for (uint32_t layer = 0; layer < 2; ++layer) {
    expectPixels(rendered.framebuffer, 0, layer, layer == 0 ? 0xFF1E3C46 : 0xFF5A3C46, kSize / 2);
    expectPixels(rendered.framebuffer, 1, layer, layer == 0 ? 0xFF1E3C32 : 0xFF5A3C32, kSize / 2);
  }
}

TEST_F(StereoSubpassReplayVulkanTest,
       StereoIndividualTextureBeforeNextSubpassOverridesTheBindGroup) {
  const Rendered rendered = render(true, Scenario::IndividualTextureBeforeNextSubpass);
  ASSERT_NE(rendered.framebuffer, nullptr);

  const std::vector<Command> before = commandsBeforeNextSubpass(rendered.commands);
  const std::vector<Command> window = commandsAfterNextSubpass(rendered.commands);
  EXPECT_EQ(countCommands(window, Cmd::BindDescriptorSets), 2u);
  const Command* textureSet = lastCommand(window, Cmd::BindDescriptorSets, 0);
  const Command* groupTextureSet = lastCommand(before, Cmd::BindDescriptorSets, 0);
  ASSERT_NE(textureSet, nullptr);
  ASSERT_NE(groupTextureSet, nullptr);
  EXPECT_NE(textureSet->ints[2], groupTextureSet->ints[2]);
  const Command* bufferSet = lastCommand(window, Cmd::BindDescriptorSets, 1);
  const Command* groupBufferSet = lastCommand(before, Cmd::BindDescriptorSets, 1);
  ASSERT_NE(bufferSet, nullptr);
  ASSERT_NE(groupBufferSet, nullptr);
  EXPECT_EQ(bufferSet->ints, groupBufferSet->ints);
  EXPECT_EQ(bufferSet->words, std::vector<uint32_t>{kDynamicOffset});

  for (uint32_t layer = 0; layer < 2; ++layer) {
    expectPixels(rendered.framebuffer, 0, layer, layer == 0 ? 0xFF1E3C46 : 0xFF5A3C46, kSize / 2);
    expectPixels(rendered.framebuffer, 1, layer, layer == 0 ? 0xFF1E3C32 : 0xFF5A3C32, kSize / 2);
  }
}

// The replay binds the descriptor sets that the first subpass bound. It does not look the bind
// groups up again, because the handles are gone: the descriptor pools of the destroyed bind groups
// live until the command buffer completes.
TEST_F(StereoSubpassReplayVulkanTest, StereoReplayAfterTheBindGroupIsDestroyed) {
  const ScopedDebugAbortCounter debugAborts;
  const Rendered rendered = render(true, Scenario::BindGroupsDestroyedBeforeNextSubpass);
  ASSERT_NE(rendered.framebuffer, nullptr);
  EXPECT_EQ(debugAborts.count(), 0u) << "number of IGL_DEBUG_ABORT() calls and failed asserts";
  ASSERT_FALSE(textureGroup_.valid());
  ASSERT_FALSE(bufferGroup_.valid());

  const std::vector<Command> before = commandsBeforeNextSubpass(rendered.commands);
  const std::vector<Command> window = commandsAfterNextSubpass(rendered.commands);
  EXPECT_EQ(countCommands(window, Cmd::BindDescriptorSets), 2u);
  const Command* textureSet = lastCommand(window, Cmd::BindDescriptorSets, 0);
  const Command* bufferSet = lastCommand(window, Cmd::BindDescriptorSets, 1);
  const Command* firstTextureSet = lastCommand(before, Cmd::BindDescriptorSets, 0);
  const Command* firstBufferSet = lastCommand(before, Cmd::BindDescriptorSets, 1);
  ASSERT_NE(textureSet, nullptr);
  ASSERT_NE(bufferSet, nullptr);
  ASSERT_NE(firstTextureSet, nullptr);
  ASSERT_NE(firstBufferSet, nullptr);
  // the same descriptor sets, and the dynamic offset of the buffer one
  EXPECT_EQ(textureSet->ints, firstTextureSet->ints);
  EXPECT_EQ(bufferSet->ints, firstBufferSet->ints);
  EXPECT_NE(textureSet->ints[2], 0u);
  EXPECT_NE(bufferSet->ints[2], 0u);
  EXPECT_TRUE(textureSet->words.empty());
  EXPECT_EQ(bufferSet->words, std::vector<uint32_t>{kDynamicOffset});

  // red 50 from the texture, green 60 from the uniform buffer at kDynamicOffset, and blue from the
  // push constants: 30 for view 0 and 90 for view 1
  for (uint32_t layer = 0; layer < 2; ++layer) {
    const uint32_t drawn = layer == 0 ? 0xFF1E3C32 : 0xFF5A3C32;
    expectPixels(rendered.framebuffer, 0, layer, drawn, kSize / 2);
    expectPixels(rendered.framebuffer, 1, layer, drawn, kSize / 2);
  }
}

// The bind groups of the first subpass do not fit the pipeline of the second one, which has no
// textures and no buffers, so the replay leaves them out
TEST_F(StereoSubpassReplayVulkanTest, StereoSkipsTheBindGroupsThatTheNextPipelineCannotUse) {
  const Rendered rendered = render(true, Scenario::SecondPipelineWithoutResources);
  ASSERT_NE(rendered.framebuffer, nullptr);

  const std::vector<Command> window = commandsAfterNextSubpass(rendered.commands);
  // Only the descriptor set of the input attachment, which is set 2
  const Command* inputAttachmentSet = onlyCommand(window, Cmd::BindDescriptorSets);
  ASSERT_NE(inputAttachmentSet, nullptr);
  EXPECT_EQ(inputAttachmentSet->ints[0], 2u);
  // The pipeline has no push constants either
  EXPECT_EQ(countCommands(window, Cmd::PushConstants), 0u);

  // Attachment 1 is red 50, green 60 and blue 30 or 90, and attachment 0 swaps red and green. Both
  // subpasses cover the whole framebuffer.
  for (uint32_t layer = 0; layer < 2; ++layer) {
    expectPixels(rendered.framebuffer, 0, layer, layer == 0 ? 0xFF1E323C : 0xFF5A323C, kSize);
  }
}

// The replay at the first draw call of the second subpass pushes nothing, because that pipeline has
// no push constants. The pipeline that draws next has them, and a Mono render pass would still have
// the ones of the first subpass, so its draw call pushes them again.
TEST_F(StereoSubpassReplayVulkanTest, StereoReplaysThePushConstantsForALaterPipelineOfTheSubpass) {
  const Rendered rendered = render(true, Scenario::ResourcePipelineAfterPipelineWithoutResources);
  ASSERT_NE(rendered.framebuffer, nullptr);
  ASSERT_EQ(countCommands(rendered.commands, Cmd::Draw), 3u);

  const std::vector<Command> before = commandsBeforeNextSubpass(rendered.commands);
  const std::vector<Command> firstWindow = commandsAfterNextSubpass(rendered.commands);
  const std::vector<Command> secondWindow = commandsBetweenDrawsAfterNextSubpass(rendered.commands);

  // The pipeline without resources needs neither the push constants nor the bind groups
  EXPECT_EQ(countCommands(firstWindow, Cmd::PushConstants), 0u);
  EXPECT_EQ(countCommands(firstWindow, Cmd::BindDescriptorSets), 0u);

  // The pipeline of the first subpass gets the push constants of the first subpass, once
  EXPECT_EQ(countCommands(secondWindow, Cmd::BindPipeline), 1u);
  const Command* pushConstants = onlyCommand(secondWindow, Cmd::PushConstants);
  ASSERT_NE(pushConstants, nullptr);
  EXPECT_EQ(pushConstants->ints, (std::array<uint64_t, 4>{0, sizeof(kTint)}));
  EXPECT_EQ(pushConstants->words, toWords(kTint.data(), kTint.size()));

  // and the descriptor sets of the bind groups that were bound again, which are the ones of the
  // first subpass
  EXPECT_EQ(countCommands(secondWindow, Cmd::BindDescriptorSets), 2u);
  const Command* textureSet = lastCommand(secondWindow, Cmd::BindDescriptorSets, 0);
  const Command* bufferSet = lastCommand(secondWindow, Cmd::BindDescriptorSets, 1);
  const Command* firstTextureSet = lastCommand(before, Cmd::BindDescriptorSets, 0);
  const Command* firstBufferSet = lastCommand(before, Cmd::BindDescriptorSets, 1);
  ASSERT_NE(textureSet, nullptr);
  ASSERT_NE(bufferSet, nullptr);
  ASSERT_NE(firstTextureSet, nullptr);
  ASSERT_NE(firstBufferSet, nullptr);
  EXPECT_EQ(textureSet->ints, firstTextureSet->ints);
  EXPECT_EQ(bufferSet->ints, firstBufferSet->ints);
  EXPECT_EQ(bufferSet->words, std::vector<uint32_t>{kDynamicOffset});

  // The state that does not depend on the pipeline layout stays from the replay of the first draw
  constexpr std::array<Cmd, 9> kLayoutIndependentCommands = {Cmd::SetViewport,
                                                             Cmd::SetScissor,
                                                             Cmd::SetDepthBias,
                                                             Cmd::SetBlendConstants,
                                                             Cmd::SetStencilCompareMask,
                                                             Cmd::SetStencilWriteMask,
                                                             Cmd::SetStencilReference,
                                                             Cmd::BindVertexBuffers,
                                                             Cmd::BindIndexBuffer};
  for (const Cmd cmd : kLayoutIndependentCommands) {
    EXPECT_EQ(countCommands(secondWindow, cmd), 0u) << "number of " << toString(cmd) << " commands";
  }

  // The pipeline of the first subpass draws over the one without resources: red 50 from the
  // texture, green 60 from the uniform buffer at kDynamicOffset, and blue from the push constants:
  // 30 for view 0 and 90 for view 1
  for (uint32_t layer = 0; layer < 2; ++layer) {
    const uint32_t drawn = layer == 0 ? 0xFF1E3C32 : 0xFF5A3C32;
    expectPixels(rendered.framebuffer, 0, layer, drawn, kSize / 2);
    expectPixels(rendered.framebuffer, 1, layer, drawn, kSize / 2);
  }
}

// A Mono render pass keeps the push constants when another pipeline is bound, so the encoder does
// not push them again
TEST_F(StereoSubpassReplayVulkanTest, MonoDoesNotReplayThePushConstantsForALaterPipeline) {
  const Rendered rendered = render(false, Scenario::ResourcePipelineAfterPipelineWithoutResources);
  ASSERT_NE(rendered.framebuffer, nullptr);
  ASSERT_EQ(countCommands(rendered.commands, Cmd::Draw), 3u);

  const std::vector<Command> secondWindow = commandsBetweenDrawsAfterNextSubpass(rendered.commands);
  EXPECT_EQ(countCommands(secondWindow, Cmd::PushConstants), 0u);
  EXPECT_EQ(countCommands(secondWindow, Cmd::BindDescriptorSets), 2u);
  // Only the push of the first subpass
  EXPECT_EQ(countCommands(rendered.commands, Cmd::PushConstants), 1u);

  expectPixels(rendered.framebuffer, 0, 0, 0xFF1E3C32, kSize / 2);
  expectPixels(rendered.framebuffer, 1, 0, 0xFF1E3C32, kSize / 2);
}

} // namespace
} // namespace igl::tests

#endif // IGL_PLATFORM_LINUX
