/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Device.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanPipelineBuilder.h>
#include <igl/vulkan/VulkanPipelineLayout.h>
#include <igl/vulkan/VulkanShaderModule.h>

namespace {

VkPolygonMode polygonModeToVkPolygonMode(igl::PolygonMode mode) {
  switch (mode) {
  case igl::PolygonMode_Fill:
    return VK_POLYGON_MODE_FILL;
  case igl::PolygonMode_Line:
    return VK_POLYGON_MODE_LINE;
  }
  IGL_ASSERT_MSG(false, "Implement a missing polygon fill mode");
  return VK_POLYGON_MODE_FILL;
}

VkCullModeFlags cullModeToVkCullMode(igl::CullMode mode) {
  switch (mode) {
  case igl::CullMode_None:
    return VK_CULL_MODE_NONE;
  case igl::CullMode_Front:
    return VK_CULL_MODE_FRONT_BIT;
  case igl::CullMode_Back:
    return VK_CULL_MODE_BACK_BIT;
  }
  IGL_ASSERT_MSG(false, "Implement a missing cull mode");
  return VK_CULL_MODE_NONE;
}

VkFrontFace windingModeToVkFrontFace(igl::WindingMode mode) {
  switch (mode) {
  case igl::WindingMode_CCW:
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  case igl::WindingMode_CW:
    return VK_FRONT_FACE_CLOCKWISE;
  }
  IGL_ASSERT_MSG(false, "Wrong winding order (cannot be more than 2)");
  return VK_FRONT_FACE_CLOCKWISE;
}

VkFormat vertexAttributeFormatToVkFormat(igl::VertexAttributeFormat fmt) {
  using igl::VertexAttributeFormat;
  switch (fmt) {
  case VertexAttributeFormat::Float1:
    return VK_FORMAT_R32_SFLOAT;
  case VertexAttributeFormat::Float2:
    return VK_FORMAT_R32G32_SFLOAT;
  case VertexAttributeFormat::Float3:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case VertexAttributeFormat::Float4:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case VertexAttributeFormat::Byte1:
    return VK_FORMAT_R8_SINT;
  case VertexAttributeFormat::Byte2:
    return VK_FORMAT_R8G8_SINT;
  case VertexAttributeFormat::Byte3:
    return VK_FORMAT_R8G8B8_SINT;
  case VertexAttributeFormat::Byte4:
    return VK_FORMAT_R8G8B8A8_SINT;
  case VertexAttributeFormat::UByte1:
    return VK_FORMAT_R8_UINT;
  case VertexAttributeFormat::UByte2:
    return VK_FORMAT_R8G8_UINT;
  case VertexAttributeFormat::UByte3:
    return VK_FORMAT_R8G8B8_UINT;
  case VertexAttributeFormat::UByte4:
    return VK_FORMAT_R8G8B8A8_UINT;
  case VertexAttributeFormat::Short1:
    return VK_FORMAT_R16_SINT;
  case VertexAttributeFormat::Short2:
    return VK_FORMAT_R16G16_SINT;
  case VertexAttributeFormat::Short3:
    return VK_FORMAT_R16G16B16_SINT;
  case VertexAttributeFormat::Short4:
    return VK_FORMAT_R16G16B16A16_SINT;
  case VertexAttributeFormat::UShort1:
    return VK_FORMAT_R16_UINT;
  case VertexAttributeFormat::UShort2:
    return VK_FORMAT_R16G16_UINT;
  case VertexAttributeFormat::UShort3:
    return VK_FORMAT_R16G16B16_UINT;
  case VertexAttributeFormat::UShort4:
    return VK_FORMAT_R16G16B16A16_UINT;
    // Normalized variants
  case VertexAttributeFormat::Byte2Norm:
    return VK_FORMAT_R8G8_SNORM;
  case VertexAttributeFormat::Byte4Norm:
    return VK_FORMAT_R8G8B8A8_SNORM;
  case VertexAttributeFormat::UByte2Norm:
    return VK_FORMAT_R8G8_UNORM;
  case VertexAttributeFormat::UByte4Norm:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case VertexAttributeFormat::Short2Norm:
    return VK_FORMAT_R16G16_SNORM;
  case VertexAttributeFormat::Short4Norm:
    return VK_FORMAT_R16G16B16A16_SNORM;
  case VertexAttributeFormat::UShort2Norm:
    return VK_FORMAT_R16G16_UNORM;
  case VertexAttributeFormat::UShort4Norm:
    return VK_FORMAT_R16G16B16A16_UNORM;
  // Integer formats
  case VertexAttributeFormat::Int1:
    return VK_FORMAT_R32_SINT;
  case VertexAttributeFormat::Int2:
    return VK_FORMAT_R32G32_SINT;
  case VertexAttributeFormat::Int3:
    return VK_FORMAT_R32G32B32_SINT;
  case VertexAttributeFormat::Int4:
    return VK_FORMAT_R32G32B32A32_SINT;
  case VertexAttributeFormat::UInt1:
    return VK_FORMAT_R32_UINT;
  case VertexAttributeFormat::UInt2:
    return VK_FORMAT_R32G32_UINT;
  case VertexAttributeFormat::UInt3:
    return VK_FORMAT_R32G32B32_UINT;
  case VertexAttributeFormat::UInt4:
    return VK_FORMAT_R32G32B32A32_UINT;
  // Half-float
  case VertexAttributeFormat::HalfFloat1:
    return VK_FORMAT_R16_SFLOAT;
  case VertexAttributeFormat::HalfFloat2:
    return VK_FORMAT_R16G16_SFLOAT;
  case VertexAttributeFormat::HalfFloat3:
    return VK_FORMAT_R16G16B16_SFLOAT;
  case VertexAttributeFormat::HalfFloat4:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case VertexAttributeFormat::Int_2_10_10_10_REV:
    return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
  }
  IGL_ASSERT(false);
  return VK_FORMAT_UNDEFINED;
}

VkBlendOp blendOpToVkBlendOp(igl::BlendOp value) {
  switch (value) {
  case igl::BlendOp_Add:
    return VK_BLEND_OP_ADD;
  case igl::BlendOp_Subtract:
    return VK_BLEND_OP_SUBTRACT;
  case igl::BlendOp_ReverseSubtract:
    return VK_BLEND_OP_REVERSE_SUBTRACT;
  case igl::BlendOp_Min:
    return VK_BLEND_OP_MIN;
  case igl::BlendOp_Max:
    return VK_BLEND_OP_MAX;
  }

  IGL_ASSERT(false);
  return VK_BLEND_OP_ADD;
}

VkBlendFactor blendFactorToVkBlendFactor(igl::BlendFactor value) {
  switch (value) {
  case igl::BlendFactor_Zero:
    return VK_BLEND_FACTOR_ZERO;
  case igl::BlendFactor_One:
    return VK_BLEND_FACTOR_ONE;
  case igl::BlendFactor_SrcColor:
    return VK_BLEND_FACTOR_SRC_COLOR;
  case igl::BlendFactor_OneMinusSrcColor:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
  case igl::BlendFactor_DstColor:
    return VK_BLEND_FACTOR_DST_COLOR;
  case igl::BlendFactor_OneMinusDstColor:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  case igl::BlendFactor_SrcAlpha:
    return VK_BLEND_FACTOR_SRC_ALPHA;
  case igl::BlendFactor_OneMinusSrcAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  case igl::BlendFactor_DstAlpha:
    return VK_BLEND_FACTOR_DST_ALPHA;
  case igl::BlendFactor_OneMinusDstAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  case igl::BlendFactor_BlendColor:
    return VK_BLEND_FACTOR_CONSTANT_COLOR;
  case igl::BlendFactor_OneMinusBlendColor:
    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
  case igl::BlendFactor_BlendAlpha:
    return VK_BLEND_FACTOR_CONSTANT_ALPHA;
  case igl::BlendFactor_OneMinusBlendAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
  case igl::BlendFactor_SrcAlphaSaturated:
    return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
  case igl::BlendFactor_Src1Color:
    return VK_BLEND_FACTOR_SRC1_COLOR;
  case igl::BlendFactor_OneMinusSrc1Color:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
  case igl::BlendFactor_Src1Alpha:
    return VK_BLEND_FACTOR_SRC1_ALPHA;
  case igl::BlendFactor_OneMinusSrc1Alpha:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
  default:
    IGL_ASSERT(false);
    return VK_BLEND_FACTOR_ONE; // default for unsupported values
  }
}

} // namespace

namespace igl {

namespace vulkan {

RenderPipelineState::RenderPipelineState(const igl::vulkan::Device& device,
                                         RenderPipelineDesc desc) :
  device_(device), desc_(std::move(desc)) {
  // Iterate and cache vertex input bindings and attributes
  const igl::VertexInputState& vstate = desc_.vertexInputState;

  vertexInputStateCreateInfo_ = ivkGetPipelineVertexInputStateCreateInfo_Empty();

  bool bufferAlreadyBound[VertexInputState::IGL_VERTEX_BUFFER_MAX] = {};

  for (uint32_t i = 0; i != vstate.numAttributes; i++) {
    const VertexAttribute& attr = vstate.attributes[i];
    const VkFormat format = vertexAttributeFormatToVkFormat(attr.format);
    const uint32_t bufferIndex = attr.bufferIndex;

    vkAttributes_.push_back(ivkGetVertexInputAttributeDescription(
        attr.location, bufferIndex, format, (uint32_t)attr.offset));

    if (!bufferAlreadyBound[bufferIndex]) {
      bufferAlreadyBound[bufferIndex] = true;

      const VertexInputBinding& binding = vstate.inputBindings[bufferIndex];
      const VkVertexInputRate rate = (binding.sampleFunction == VertexSampleFunction_PerVertex)
                                         ? VK_VERTEX_INPUT_RATE_VERTEX
                                         : VK_VERTEX_INPUT_RATE_INSTANCE;
      vkBindings_.push_back(ivkGetVertexInputBindingDescription(bufferIndex, binding.stride, rate));
    }

    vertexInputStateCreateInfo_.vertexBindingDescriptionCount =
        static_cast<uint32_t>(vstate.numInputBindings);
    vertexInputStateCreateInfo_.pVertexBindingDescriptions = vkBindings_.data();
    vertexInputStateCreateInfo_.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(vstate.numAttributes);
    vertexInputStateCreateInfo_.pVertexAttributeDescriptions = vkAttributes_.data();
  }
}

RenderPipelineState::~RenderPipelineState() {
  VkDevice device = device_.getVulkanContext().device_->getVkDevice();

  for (auto p : pipelines_) {
    if (p.second != VK_NULL_HANDLE) {
      device_.getVulkanContext().deferredTask(std::packaged_task<void()>(
          [device, pipeline = p.second]() { vkDestroyPipeline(device, pipeline, nullptr); }));
    }
  }
}

VkPipeline RenderPipelineState::getVkPipeline(
    const RenderPipelineDynamicState& dynamicState) const {
  const auto it = pipelines_.find(dynamicState);

  if (it != pipelines_.end()) {
    return it->second;
  }

  // build a new Vulkan pipeline

  const VulkanContext& ctx = device_.getVulkanContext();

  VkPipeline pipeline = VK_NULL_HANDLE;

  // Not all attachments are valid. We need to create color blend attachments only for active
  // attachments
  std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
  colorBlendAttachmentStates.resize(desc_.numColorAttachments);

  std::vector<VkFormat> colorAttachmentFormats;
  colorAttachmentFormats.resize(desc_.numColorAttachments);

  for (uint32_t i = 0; i != desc_.numColorAttachments; i++) {
    const auto& attachment = desc_.colorAttachments[i];
    IGL_ASSERT(attachment.textureFormat != TextureFormat::Invalid);
    colorAttachmentFormats[i] = textureFormatToVkFormat(attachment.textureFormat);
    if (!attachment.blendEnabled) {
      colorBlendAttachmentStates[i] = ivkGetPipelineColorBlendAttachmentState_NoBlending();
    } else {
      colorBlendAttachmentStates[i] = ivkGetPipelineColorBlendAttachmentState(
          attachment.blendEnabled,
          blendFactorToVkBlendFactor(attachment.srcRGBBlendFactor),
          blendFactorToVkBlendFactor(attachment.dstRGBBlendFactor),
          blendOpToVkBlendOp(attachment.rgbBlendOp),
          blendFactorToVkBlendFactor(attachment.srcAlphaBlendFactor),
          blendFactorToVkBlendFactor(attachment.dstAlphaBlendFactor),
          blendOpToVkBlendOp(attachment.alphaBlendOp),
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
              VK_COLOR_COMPONENT_A_BIT);
    }
  }

  VulkanShaderModule* vertexModule = device_.getShaderModule(desc_.shaderStages.getModule(Stage_Vertex));
  VulkanShaderModule* fragmentModule = device_.getShaderModule(desc_.shaderStages.getModule(Stage_Fragment));

  IGL_ASSERT(vertexModule);
  IGL_ASSERT(fragmentModule);

  igl::vulkan::VulkanPipelineBuilder()
      .dynamicStates({
          // from Vulkan 1.0
          VK_DYNAMIC_STATE_VIEWPORT,
          VK_DYNAMIC_STATE_SCISSOR,
          VK_DYNAMIC_STATE_DEPTH_BIAS,
          VK_DYNAMIC_STATE_BLEND_CONSTANTS,
          VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
          VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
          VK_DYNAMIC_STATE_STENCIL_REFERENCE,
      })
      .primitiveTopology(dynamicState.getTopology())
      .depthBiasEnable(dynamicState.depthBiasEnable_)
      .depthCompareOp(dynamicState.getDepthCompareOp())
      .depthWriteEnable(dynamicState.depthWriteEnable_)
      .rasterizationSamples(getVulkanSampleCountFlags(desc_.sampleCount))
      .polygonMode(polygonModeToVkPolygonMode(desc_.polygonMode))
      .stencilStateOps(VK_STENCIL_FACE_FRONT_BIT,
                       dynamicState.getStencilStateFailOp(true),
                       dynamicState.getStencilStatePassOp(true),
                       dynamicState.getStencilStateDepthFailOp(true),
                       dynamicState.getStencilStateComapreOp(true))
      .stencilStateOps(VK_STENCIL_FACE_BACK_BIT,
                       dynamicState.getStencilStateFailOp(false),
                       dynamicState.getStencilStatePassOp(false),
                       dynamicState.getStencilStateDepthFailOp(false),
                       dynamicState.getStencilStateComapreOp(false))
      .shaderStages({
          ivkGetPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                              vertexModule->getVkShaderModule(),
                                              vertexModule->getEntryPoint()),
          ivkGetPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                              fragmentModule->getVkShaderModule(),
                                              fragmentModule->getEntryPoint()),
      })
      .cullMode(cullModeToVkCullMode(desc_.cullMode))
      .frontFace(windingModeToVkFrontFace(desc_.frontFaceWinding))
      .vertexInputState(vertexInputStateCreateInfo_)
      .colorBlendAttachmentStates(colorBlendAttachmentStates)
      .colorAttachmentFormats(colorAttachmentFormats)
      .depthAttachmentFormat(textureFormatToVkFormat(desc_.depthAttachmentFormat))
      .stencilAttachmentFormat(textureFormatToVkFormat(desc_.stencilAttachmentFormat))
      .build(
          ctx.device_->getVkDevice(),
          // TODO: use ctx.pipelineCache_
          VK_NULL_HANDLE,
          ctx.pipelineLayout_->getVkPipelineLayout(),
          &pipeline,
          desc_.debugName);

  pipelines_[dynamicState] = pipeline;

  return pipeline;
}

} // namespace vulkan
} // namespace igl
