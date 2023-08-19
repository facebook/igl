/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Device.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/VulkanContext.h>

uint32_t lvk::vulkan::VulkanPipelineBuilder::numPipelinesCreated_ = 0;

namespace {

VkStencilOp stencilOpToVkStencilOp(lvk::StencilOp op) {
  switch (op) {
  case lvk::StencilOp_Keep:
    return VK_STENCIL_OP_KEEP;
  case lvk::StencilOp_Zero:
    return VK_STENCIL_OP_ZERO;
  case lvk::StencilOp_Replace:
    return VK_STENCIL_OP_REPLACE;
  case lvk::StencilOp_IncrementClamp:
    return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
  case lvk::StencilOp_DecrementClamp:
    return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
  case lvk::StencilOp_Invert:
    return VK_STENCIL_OP_INVERT;
  case lvk::StencilOp_IncrementWrap:
    return VK_STENCIL_OP_INCREMENT_AND_WRAP;
  case lvk::StencilOp_DecrementWrap:
    return VK_STENCIL_OP_DECREMENT_AND_WRAP;
  }
  LVK_ASSERT(false);
  return VK_STENCIL_OP_KEEP;
}

VkPolygonMode polygonModeToVkPolygonMode(lvk::PolygonMode mode) {
  switch (mode) {
  case lvk::PolygonMode_Fill:
    return VK_POLYGON_MODE_FILL;
  case lvk::PolygonMode_Line:
    return VK_POLYGON_MODE_LINE;
  }
  LVK_ASSERT_MSG(false, "Implement a missing polygon fill mode");
  return VK_POLYGON_MODE_FILL;
}

VkCullModeFlags cullModeToVkCullMode(lvk::CullMode mode) {
  switch (mode) {
  case lvk::CullMode_None:
    return VK_CULL_MODE_NONE;
  case lvk::CullMode_Front:
    return VK_CULL_MODE_FRONT_BIT;
  case lvk::CullMode_Back:
    return VK_CULL_MODE_BACK_BIT;
  }
  LVK_ASSERT_MSG(false, "Implement a missing cull mode");
  return VK_CULL_MODE_NONE;
}

VkFrontFace windingModeToVkFrontFace(lvk::WindingMode mode) {
  switch (mode) {
  case lvk::WindingMode_CCW:
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  case lvk::WindingMode_CW:
    return VK_FRONT_FACE_CLOCKWISE;
  }
  LVK_ASSERT_MSG(false, "Wrong winding order (cannot be more than 2)");
  return VK_FRONT_FACE_CLOCKWISE;
}

VkFormat vertexFormatToVkFormat(lvk::VertexFormat fmt) {
  using lvk::VertexFormat;
  switch (fmt) {
  case VertexFormat::Invalid:
    LVK_ASSERT(false);
    return VK_FORMAT_UNDEFINED;
  case VertexFormat::Float1:
    return VK_FORMAT_R32_SFLOAT;
  case VertexFormat::Float2:
    return VK_FORMAT_R32G32_SFLOAT;
  case VertexFormat::Float3:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case VertexFormat::Float4:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case VertexFormat::Byte1:
    return VK_FORMAT_R8_SINT;
  case VertexFormat::Byte2:
    return VK_FORMAT_R8G8_SINT;
  case VertexFormat::Byte3:
    return VK_FORMAT_R8G8B8_SINT;
  case VertexFormat::Byte4:
    return VK_FORMAT_R8G8B8A8_SINT;
  case VertexFormat::UByte1:
    return VK_FORMAT_R8_UINT;
  case VertexFormat::UByte2:
    return VK_FORMAT_R8G8_UINT;
  case VertexFormat::UByte3:
    return VK_FORMAT_R8G8B8_UINT;
  case VertexFormat::UByte4:
    return VK_FORMAT_R8G8B8A8_UINT;
  case VertexFormat::Short1:
    return VK_FORMAT_R16_SINT;
  case VertexFormat::Short2:
    return VK_FORMAT_R16G16_SINT;
  case VertexFormat::Short3:
    return VK_FORMAT_R16G16B16_SINT;
  case VertexFormat::Short4:
    return VK_FORMAT_R16G16B16A16_SINT;
  case VertexFormat::UShort1:
    return VK_FORMAT_R16_UINT;
  case VertexFormat::UShort2:
    return VK_FORMAT_R16G16_UINT;
  case VertexFormat::UShort3:
    return VK_FORMAT_R16G16B16_UINT;
  case VertexFormat::UShort4:
    return VK_FORMAT_R16G16B16A16_UINT;
    // Normalized variants
  case VertexFormat::Byte2Norm:
    return VK_FORMAT_R8G8_SNORM;
  case VertexFormat::Byte4Norm:
    return VK_FORMAT_R8G8B8A8_SNORM;
  case VertexFormat::UByte2Norm:
    return VK_FORMAT_R8G8_UNORM;
  case VertexFormat::UByte4Norm:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case VertexFormat::Short2Norm:
    return VK_FORMAT_R16G16_SNORM;
  case VertexFormat::Short4Norm:
    return VK_FORMAT_R16G16B16A16_SNORM;
  case VertexFormat::UShort2Norm:
    return VK_FORMAT_R16G16_UNORM;
  case VertexFormat::UShort4Norm:
    return VK_FORMAT_R16G16B16A16_UNORM;
  case VertexFormat::Int1:
    return VK_FORMAT_R32_SINT;
  case VertexFormat::Int2:
    return VK_FORMAT_R32G32_SINT;
  case VertexFormat::Int3:
    return VK_FORMAT_R32G32B32_SINT;
  case VertexFormat::Int4:
    return VK_FORMAT_R32G32B32A32_SINT;
  case VertexFormat::UInt1:
    return VK_FORMAT_R32_UINT;
  case VertexFormat::UInt2:
    return VK_FORMAT_R32G32_UINT;
  case VertexFormat::UInt3:
    return VK_FORMAT_R32G32B32_UINT;
  case VertexFormat::UInt4:
    return VK_FORMAT_R32G32B32A32_UINT;
  case VertexFormat::HalfFloat1:
    return VK_FORMAT_R16_SFLOAT;
  case VertexFormat::HalfFloat2:
    return VK_FORMAT_R16G16_SFLOAT;
  case VertexFormat::HalfFloat3:
    return VK_FORMAT_R16G16B16_SFLOAT;
  case VertexFormat::HalfFloat4:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case VertexFormat::Int_2_10_10_10_REV:
    return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
  }
  LVK_ASSERT(false);
  return VK_FORMAT_UNDEFINED;
}

VkBlendOp blendOpToVkBlendOp(lvk::BlendOp value) {
  switch (value) {
  case lvk::BlendOp_Add:
    return VK_BLEND_OP_ADD;
  case lvk::BlendOp_Subtract:
    return VK_BLEND_OP_SUBTRACT;
  case lvk::BlendOp_ReverseSubtract:
    return VK_BLEND_OP_REVERSE_SUBTRACT;
  case lvk::BlendOp_Min:
    return VK_BLEND_OP_MIN;
  case lvk::BlendOp_Max:
    return VK_BLEND_OP_MAX;
  }

  LVK_ASSERT(false);
  return VK_BLEND_OP_ADD;
}

VkBlendFactor blendFactorToVkBlendFactor(lvk::BlendFactor value) {
  switch (value) {
  case lvk::BlendFactor_Zero:
    return VK_BLEND_FACTOR_ZERO;
  case lvk::BlendFactor_One:
    return VK_BLEND_FACTOR_ONE;
  case lvk::BlendFactor_SrcColor:
    return VK_BLEND_FACTOR_SRC_COLOR;
  case lvk::BlendFactor_OneMinusSrcColor:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
  case lvk::BlendFactor_DstColor:
    return VK_BLEND_FACTOR_DST_COLOR;
  case lvk::BlendFactor_OneMinusDstColor:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  case lvk::BlendFactor_SrcAlpha:
    return VK_BLEND_FACTOR_SRC_ALPHA;
  case lvk::BlendFactor_OneMinusSrcAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  case lvk::BlendFactor_DstAlpha:
    return VK_BLEND_FACTOR_DST_ALPHA;
  case lvk::BlendFactor_OneMinusDstAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  case lvk::BlendFactor_BlendColor:
    return VK_BLEND_FACTOR_CONSTANT_COLOR;
  case lvk::BlendFactor_OneMinusBlendColor:
    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
  case lvk::BlendFactor_BlendAlpha:
    return VK_BLEND_FACTOR_CONSTANT_ALPHA;
  case lvk::BlendFactor_OneMinusBlendAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
  case lvk::BlendFactor_SrcAlphaSaturated:
    return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
  case lvk::BlendFactor_Src1Color:
    return VK_BLEND_FACTOR_SRC1_COLOR;
  case lvk::BlendFactor_OneMinusSrc1Color:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
  case lvk::BlendFactor_Src1Alpha:
    return VK_BLEND_FACTOR_SRC1_ALPHA;
  case lvk::BlendFactor_OneMinusSrc1Alpha:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
  default:
    LVK_ASSERT(false);
    return VK_BLEND_FACTOR_ONE; // default for unsupported values
  }
}

} // namespace

namespace lvk::vulkan {

RenderPipelineState::RenderPipelineState(lvk::vulkan::Device* device, const RenderPipelineDesc& desc) : device_(device), desc_(desc) {
  // Iterate and cache vertex input bindings and attributes
  const lvk::VertexInput& vstate = desc_.vertexInput;

  vertexInputStateCreateInfo_ = VkPipelineVertexInputStateCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = nullptr,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = nullptr,
  };

  bool bufferAlreadyBound[VertexInput::LVK_VERTEX_BUFFER_MAX] = {};

  const uint32_t numAttributes = vstate.getNumAttributes();

  for (uint32_t i = 0; i != numAttributes; i++) {
    const auto& attr = vstate.attributes[i];

    vkAttributes_.push_back({.location = attr.location,
                             .binding = attr.binding,
                             .format = vertexFormatToVkFormat(attr.format),
                             .offset = (uint32_t)attr.offset});

    if (!bufferAlreadyBound[attr.binding]) {
      bufferAlreadyBound[attr.binding] = true;
      vkBindings_.push_back(
          {.binding = attr.binding, .stride = vstate.inputBindings[attr.binding].stride, .inputRate = VK_VERTEX_INPUT_RATE_VERTEX});
    }

    vertexInputStateCreateInfo_.vertexBindingDescriptionCount = vstate.getNumInputBindings();
    vertexInputStateCreateInfo_.pVertexBindingDescriptions = vkBindings_.data();
    vertexInputStateCreateInfo_.vertexAttributeDescriptionCount = vstate.getNumAttributes();
    vertexInputStateCreateInfo_.pVertexAttributeDescriptions = vkAttributes_.data();
  }
}

RenderPipelineState::~RenderPipelineState() {
  if (!device_) {
    return;
  }

  if (!desc_.smVert.empty()) {
    device_->destroy(desc_.smVert);
  }
  if (!desc_.smGeom.empty()) {
    device_->destroy(desc_.smGeom);
  }
  if (!desc_.smFrag.empty()) {
    device_->destroy(desc_.smFrag);
  }

  for (auto p : pipelines_) {
    if (p.second != VK_NULL_HANDLE) {
      device_->getVulkanContext().deferredTask(std::packaged_task<void()>(
          [device = device_->getVulkanContext().getVkDevice(), pipeline = p.second]() { vkDestroyPipeline(device, pipeline, nullptr); }));
    }
  }
}

RenderPipelineState::RenderPipelineState(RenderPipelineState&& other) :
  device_(other.device_), vertexInputStateCreateInfo_(other.vertexInputStateCreateInfo_) {
  std::swap(desc_, other.desc_);
  std::swap(vkBindings_, other.vkBindings_);
  std::swap(vkAttributes_, other.vkAttributes_);
  std::swap(pipelines_, other.pipelines_);
  other.device_ = nullptr;
}

RenderPipelineState& RenderPipelineState::operator=(RenderPipelineState&& other) {
  std::swap(device_, other.device_);
  std::swap(desc_, other.desc_);
  std::swap(vertexInputStateCreateInfo_, other.vertexInputStateCreateInfo_);
  std::swap(vkBindings_, other.vkBindings_);
  std::swap(vkAttributes_, other.vkAttributes_);
  std::swap(pipelines_, other.pipelines_);
  return *this;
}

VkPipeline RenderPipelineState::getVkPipeline(const RenderPipelineDynamicState& dynamicState) const {
  const auto it = pipelines_.find(dynamicState);

  if (it != pipelines_.end()) {
    return it->second;
  }

  // build a new Vulkan pipeline

  const VulkanContext& ctx = device_->getVulkanContext();

  VkPipeline pipeline = VK_NULL_HANDLE;

  const uint32_t numColorAttachments = desc_.getNumColorAttachments();

  // Not all attachments are valid. We need to create color blend attachments only for active
  // attachments
  std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
  colorBlendAttachmentStates.resize(numColorAttachments);

  std::vector<VkFormat> colorAttachmentFormats;
  colorAttachmentFormats.resize(numColorAttachments);

  for (uint32_t i = 0; i != numColorAttachments; i++) {
    const auto& attachment = desc_.color[i];
    LVK_ASSERT(attachment.format != Format_Invalid);
    colorAttachmentFormats[i] = formatToVkFormat(attachment.format);
    if (!attachment.blendEnabled) {
      colorBlendAttachmentStates[i] = VkPipelineColorBlendAttachmentState{
          .blendEnable = VK_FALSE,
          .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
          .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
          .colorBlendOp = VK_BLEND_OP_ADD,
          .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
          .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
          .alphaBlendOp = VK_BLEND_OP_ADD,
          .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      };
    } else {
      colorBlendAttachmentStates[i] = VkPipelineColorBlendAttachmentState{
          .blendEnable = VK_TRUE,
          .srcColorBlendFactor = blendFactorToVkBlendFactor(attachment.srcRGBBlendFactor),
          .dstColorBlendFactor = blendFactorToVkBlendFactor(attachment.dstRGBBlendFactor),
          .colorBlendOp = blendOpToVkBlendOp(attachment.rgbBlendOp),
          .srcAlphaBlendFactor = blendFactorToVkBlendFactor(attachment.srcAlphaBlendFactor),
          .dstAlphaBlendFactor = blendFactorToVkBlendFactor(attachment.dstAlphaBlendFactor),
          .alphaBlendOp = blendOpToVkBlendOp(attachment.alphaBlendOp),
          .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      };
    }
  }

  const VkShaderModule* vertModule = ctx.shaderModulesPool_.get(desc_.smVert);
  const VkShaderModule* geomModule = ctx.shaderModulesPool_.get(desc_.smGeom);
  const VkShaderModule* fragModule = ctx.shaderModulesPool_.get(desc_.smFrag);

  LVK_ASSERT(vertModule);
  LVK_ASSERT(fragModule);

  std::vector<VkPipelineShaderStageCreateInfo> stages = {
      lvk::getPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, *vertModule, desc_.entryPointVert),
      lvk::getPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, *fragModule, desc_.entryPointFrag),
  };

  if (geomModule) {
    stages.push_back(lvk::getPipelineShaderStageCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT, *geomModule, desc_.entryPointGeom));
  }

  lvk::vulkan::VulkanPipelineBuilder()
      .dynamicStates({
          // from Vulkan 1.0
          VK_DYNAMIC_STATE_VIEWPORT,
          VK_DYNAMIC_STATE_SCISSOR,
          VK_DYNAMIC_STATE_DEPTH_BIAS,
          VK_DYNAMIC_STATE_BLEND_CONSTANTS,
      })
      .primitiveTopology(dynamicState.getTopology())
      .depthBiasEnable(dynamicState.depthBiasEnable_)
      .depthCompareOp(dynamicState.getDepthCompareOp())
      .depthWriteEnable(dynamicState.depthWriteEnable_)
      .rasterizationSamples(getVulkanSampleCountFlags(desc_.samplesCount))
      .polygonMode(polygonModeToVkPolygonMode(desc_.polygonMode))
      .stencilStateOps(VK_STENCIL_FACE_FRONT_BIT,
                       stencilOpToVkStencilOp(desc_.frontFaceStencil.stencilFailureOp),
                       stencilOpToVkStencilOp(desc_.frontFaceStencil.depthStencilPassOp),
                       stencilOpToVkStencilOp(desc_.frontFaceStencil.depthFailureOp),
                       compareOpToVkCompareOp(desc_.frontFaceStencil.stencilCompareOp))
      .stencilStateOps(VK_STENCIL_FACE_BACK_BIT,
                       stencilOpToVkStencilOp(desc_.backFaceStencil.stencilFailureOp),
                       stencilOpToVkStencilOp(desc_.backFaceStencil.depthStencilPassOp),
                       stencilOpToVkStencilOp(desc_.backFaceStencil.depthFailureOp),
                       compareOpToVkCompareOp(desc_.backFaceStencil.stencilCompareOp))
      .stencilMasks(VK_STENCIL_FACE_FRONT_BIT, 0xFF, desc_.frontFaceStencil.writeMask, desc_.frontFaceStencil.readMask)
      .stencilMasks(VK_STENCIL_FACE_BACK_BIT, 0xFF, desc_.backFaceStencil.writeMask, desc_.backFaceStencil.readMask)
      .shaderStages(stages)
      .cullMode(cullModeToVkCullMode(desc_.cullMode))
      .frontFace(windingModeToVkFrontFace(desc_.frontFaceWinding))
      .vertexInputState(vertexInputStateCreateInfo_)
      .colorBlendAttachmentStates(colorBlendAttachmentStates)
      .colorAttachmentFormats(colorAttachmentFormats)
      .depthAttachmentFormat(formatToVkFormat(desc_.depthFormat))
      .stencilAttachmentFormat(formatToVkFormat(desc_.stencilFormat))
      .build(ctx.getVkDevice(), ctx.pipelineCache_, ctx.vkPipelineLayout_, &pipeline, desc_.debugName);

  pipelines_[dynamicState] = pipeline;

  return pipeline;
}

VulkanPipelineBuilder::VulkanPipelineBuilder() :
  vertexInputState_(VkPipelineVertexInputStateCreateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = nullptr,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = nullptr,
  }),
  inputAssembly_({
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .flags = 0,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  }),
  rasterizationState_({
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .flags = 0,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f,
      .depthBiasClamp = 0.0f,
      .depthBiasSlopeFactor = 0.0f,
      .lineWidth = 1.0f,
  }),
  multisampleState_({
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0f,
      .pSampleMask = nullptr,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  }),
  depthStencilState_({
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .depthTestEnable = VK_FALSE,
      .depthWriteEnable = VK_FALSE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .front =
          {
              .failOp = VK_STENCIL_OP_KEEP,
              .passOp = VK_STENCIL_OP_KEEP,
              .depthFailOp = VK_STENCIL_OP_KEEP,
              .compareOp = VK_COMPARE_OP_NEVER,
              .compareMask = 0,
              .writeMask = 0,
              .reference = 0,
          },
      .back =
          {
              .failOp = VK_STENCIL_OP_KEEP,
              .passOp = VK_STENCIL_OP_KEEP,
              .depthFailOp = VK_STENCIL_OP_KEEP,
              .compareOp = VK_COMPARE_OP_NEVER,
              .compareMask = 0,
              .writeMask = 0,
              .reference = 0,
          },
      .minDepthBounds = 0.0f,
      .maxDepthBounds = 1.0f,
  }) {}

VulkanPipelineBuilder& VulkanPipelineBuilder::depthBiasEnable(bool enable) {
  rasterizationState_.depthBiasEnable = enable ? VK_TRUE : VK_FALSE;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::depthWriteEnable(bool enable) {
  depthStencilState_.depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::depthCompareOp(VkCompareOp compareOp) {
  depthStencilState_.depthTestEnable = compareOp != VK_COMPARE_OP_ALWAYS;
  depthStencilState_.depthCompareOp = compareOp;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::dynamicState(VkDynamicState state) {
  dynamicStates_.push_back(state);
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::dynamicStates(const std::vector<VkDynamicState>& states) {
  dynamicStates_.reserve(dynamicStates_.size() + states.size());
  dynamicStates_.insert(std::end(dynamicStates_), std::begin(states), std::end(states));
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::primitiveTopology(VkPrimitiveTopology topology) {
  inputAssembly_.topology = topology;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::rasterizationSamples(VkSampleCountFlagBits samples) {
  multisampleState_.rasterizationSamples = samples;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::cullMode(VkCullModeFlags mode) {
  rasterizationState_.cullMode = mode;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::frontFace(VkFrontFace mode) {
  rasterizationState_.frontFace = mode;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::polygonMode(VkPolygonMode mode) {
  rasterizationState_.polygonMode = mode;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::vertexInputState(const VkPipelineVertexInputStateCreateInfo& state) {
  vertexInputState_ = state;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::colorBlendAttachmentStates(std::vector<VkPipelineColorBlendAttachmentState>& states) {
  colorBlendAttachmentStates_ = std::move(states);
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::colorAttachmentFormats(std::vector<VkFormat>& formats) {
  colorAttachmentFormats_ = std::move(formats);
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::depthAttachmentFormat(VkFormat format) {
  depthAttachmentFormat_ = format;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::stencilAttachmentFormat(VkFormat format) {
  stencilAttachmentFormat_ = format;
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::shaderStage(VkPipelineShaderStageCreateInfo stage) {
  shaderStages_.push_back(stage);
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::shaderStages(const std::vector<VkPipelineShaderStageCreateInfo>& stages) {
  shaderStages_.insert(std::end(shaderStages_), std::begin(stages), std::end(stages));
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::stencilStateOps(VkStencilFaceFlags faceMask,
                                                              VkStencilOp failOp,
                                                              VkStencilOp passOp,
                                                              VkStencilOp depthFailOp,
                                                              VkCompareOp compareOp) {
  if (faceMask & VK_STENCIL_FACE_FRONT_BIT) {
    VkStencilOpState& s = depthStencilState_.front;
    s.failOp = failOp;
    s.passOp = passOp;
    s.depthFailOp = depthFailOp;
    s.compareOp = compareOp;
  }

  if (faceMask & VK_STENCIL_FACE_BACK_BIT) {
    VkStencilOpState& s = depthStencilState_.back;
    s.failOp = failOp;
    s.passOp = passOp;
    s.depthFailOp = depthFailOp;
    s.compareOp = compareOp;
  }
  return *this;
}

VulkanPipelineBuilder& VulkanPipelineBuilder::stencilMasks(VkStencilFaceFlags faceMask,
                                                           uint32_t compareMask,
                                                           uint32_t writeMask,
                                                           uint32_t reference) {
  if (faceMask & VK_STENCIL_FACE_FRONT_BIT) {
    VkStencilOpState& s = depthStencilState_.front;
    s.compareMask = compareMask;
    s.writeMask = writeMask;
    s.reference = reference;
  }

  if (faceMask & VK_STENCIL_FACE_BACK_BIT) {
    VkStencilOpState& s = depthStencilState_.back;
    s.compareMask = compareMask;
    s.writeMask = writeMask;
    s.reference = reference;
  }
  return *this;
}

VkResult VulkanPipelineBuilder::build(VkDevice device,
                                      VkPipelineCache pipelineCache,
                                      VkPipelineLayout pipelineLayout,
                                      VkPipeline* outPipeline,
                                      const char* debugName) noexcept {
  const VkPipelineDynamicStateCreateInfo dynamicState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = (uint32_t)dynamicStates_.size(),
      .pDynamicStates = dynamicStates_.data(),
  };
  // viewport and scissor can be NULL if the viewport state is dynamic
  // https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkPipelineViewportStateCreateInfo.html
  const VkPipelineViewportStateCreateInfo viewportState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = nullptr,
      .scissorCount = 1,
      .pScissors = nullptr,
  };
  const VkPipelineColorBlendStateCreateInfo colorBlendState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = uint32_t(colorBlendAttachmentStates_.size()),
      .pAttachments = colorBlendAttachmentStates_.data(),
  };
  LVK_ASSERT(colorAttachmentFormats_.size() == colorBlendAttachmentStates_.size());

  const VkPipelineRenderingCreateInfo renderingInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
      .pNext = nullptr,
      .colorAttachmentCount = (uint32_t)colorAttachmentFormats_.size(),
      .pColorAttachmentFormats = colorAttachmentFormats_.data(),
      .depthAttachmentFormat = depthAttachmentFormat_,
      .stencilAttachmentFormat = stencilAttachmentFormat_,
  };

  const VkGraphicsPipelineCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &renderingInfo,
      .flags = 0,
      .stageCount = (uint32_t)shaderStages_.size(),
      .pStages = shaderStages_.data(),
      .pVertexInputState = &vertexInputState_,
      .pInputAssemblyState = &inputAssembly_,
      .pTessellationState = nullptr,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizationState_,
      .pMultisampleState = &multisampleState_,
      .pDepthStencilState = &depthStencilState_,
      .pColorBlendState = &colorBlendState,
      .pDynamicState = &dynamicState,
      .layout = pipelineLayout,
      .renderPass = VK_NULL_HANDLE,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };

  const auto result = vkCreateGraphicsPipelines(device, pipelineCache, 1, &ci, nullptr, outPipeline);

  if (!LVK_VERIFY(result == VK_SUCCESS)) {
    return result;
  }

  numPipelinesCreated_++;

  // set debug name
  return lvk::setDebugObjectName(device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)*outPipeline, debugName);
}

} // namespace lvk::vulkan
