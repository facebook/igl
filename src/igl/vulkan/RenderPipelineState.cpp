/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Device.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanPipelineBuilder.h>
#include <igl/vulkan/VulkanShaderModule.h>

namespace {

VkPolygonMode polygonModeToVkPolygonMode(lvk::PolygonMode mode) {
  switch (mode) {
  case lvk::PolygonMode_Fill:
    return VK_POLYGON_MODE_FILL;
  case lvk::PolygonMode_Line:
    return VK_POLYGON_MODE_LINE;
  }
  IGL_ASSERT_MSG(false, "Implement a missing polygon fill mode");
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
  IGL_ASSERT_MSG(false, "Implement a missing cull mode");
  return VK_CULL_MODE_NONE;
}

VkFrontFace windingModeToVkFrontFace(lvk::WindingMode mode) {
  switch (mode) {
  case lvk::WindingMode_CCW:
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  case lvk::WindingMode_CW:
    return VK_FRONT_FACE_CLOCKWISE;
  }
  IGL_ASSERT_MSG(false, "Wrong winding order (cannot be more than 2)");
  return VK_FRONT_FACE_CLOCKWISE;
}

VkFormat vertexFormatToVkFormat(lvk::VertexFormat fmt) {
  using lvk::VertexFormat;
  switch (fmt) {
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
  IGL_ASSERT(false);
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

  IGL_ASSERT(false);
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
    IGL_ASSERT(false);
    return VK_BLEND_FACTOR_ONE; // default for unsupported values
  }
}

} // namespace

namespace lvk::vulkan {

RenderPipelineState::RenderPipelineState(lvk::vulkan::Device* device,
                                         const RenderPipelineDesc& desc) :
  device_(device), desc_(desc) {
  // Iterate and cache vertex input bindings and attributes
  const lvk::VertexInput& vstate = desc_.vertexInput;

  vertexInputStateCreateInfo_ = ivkGetPipelineVertexInputStateCreateInfo_Empty();

  bool bufferAlreadyBound[VertexInput::IGL_VERTEX_BUFFER_MAX] = {};

  const uint32_t numAttributes = vstate.getNumAttributes();

  for (uint32_t i = 0; i != numAttributes; i++) {
    const auto& attr = vstate.attributes[i];

    vkAttributes_.push_back({.location = attr.location,
                             .binding = attr.binding,
                             .format = vertexFormatToVkFormat(attr.format),
                             .offset = (uint32_t)attr.offset});

    if (!bufferAlreadyBound[attr.binding]) {
      bufferAlreadyBound[attr.binding] = true;
      vkBindings_.push_back({.binding = attr.binding,
                             .stride = vstate.inputBindings[attr.binding].stride,
                             .inputRate = VK_VERTEX_INPUT_RATE_VERTEX});
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

  for (lvk::ShaderModuleHandle m : desc_.shaderStages.modules_) {
    device_->destroy(m);
  }

  for (auto p : pipelines_) {
    if (p.second != VK_NULL_HANDLE) {
      device_->getVulkanContext().deferredTask(std::packaged_task<void()>(
          [device = device_->getVulkanContext().getVkDevice(), pipeline = p.second]() {
            vkDestroyPipeline(device, pipeline, nullptr);
          }));
    }
  }
}

RenderPipelineState::RenderPipelineState(RenderPipelineState&& other) :
  device_(other.device_), vertexInputStateCreateInfo_(other.vertexInputStateCreateInfo_) {
  std::swap(shaderStages_, other.shaderStages_);
  std::swap(desc_, other.desc_);
  std::swap(vkBindings_, other.vkBindings_);
  std::swap(vkAttributes_, other.vkAttributes_);
  std::swap(pipelines_, other.pipelines_);
  other.device_ = nullptr;
}

RenderPipelineState& RenderPipelineState::operator=(RenderPipelineState&& other) {
  std::swap(device_, other.device_);
  std::swap(shaderStages_, other.shaderStages_);
  std::swap(desc_, other.desc_);
  std::swap(vertexInputStateCreateInfo_, other.vertexInputStateCreateInfo_);
  std::swap(vkBindings_, other.vkBindings_);
  std::swap(vkAttributes_, other.vkAttributes_);
  std::swap(pipelines_, other.pipelines_);
  return *this;
}

VkPipeline RenderPipelineState::getVkPipeline(
    const RenderPipelineDynamicState& dynamicState) const {
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

  const VulkanShaderModule* vertexModule =
      ctx.shaderModulesPool_.get(desc_.shaderStages.getModule(Stage_Vertex));
  const VulkanShaderModule* geometryModule =
      ctx.shaderModulesPool_.get(desc_.shaderStages.getModule(Stage_Geometry));
  const VulkanShaderModule* fragmentModule =
      ctx.shaderModulesPool_.get(desc_.shaderStages.getModule(Stage_Fragment));

  IGL_ASSERT(vertexModule);
  IGL_ASSERT(fragmentModule);

  std::vector<VkPipelineShaderStageCreateInfo> stages = {
      ivkGetPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                          vertexModule->getVkShaderModule(),
                                          vertexModule->getEntryPoint()),
      ivkGetPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                          fragmentModule->getVkShaderModule(),
                                          fragmentModule->getEntryPoint()),
  };

  if (geometryModule) {
    stages.push_back(ivkGetPipelineShaderStageCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT,
                                                         geometryModule->getVkShaderModule(),
                                                         geometryModule->getEntryPoint()));
  }

  lvk::vulkan::VulkanPipelineBuilder()
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
      .rasterizationSamples(getVulkanSampleCountFlags(desc_.samplesCount))
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
      .shaderStages(stages)
      .cullMode(cullModeToVkCullMode(desc_.cullMode))
      .frontFace(windingModeToVkFrontFace(desc_.frontFaceWinding))
      .vertexInputState(vertexInputStateCreateInfo_)
      .colorBlendAttachmentStates(colorBlendAttachmentStates)
      .colorAttachmentFormats(colorAttachmentFormats)
      .depthAttachmentFormat(textureFormatToVkFormat(desc_.depthAttachmentFormat))
      .stencilAttachmentFormat(textureFormatToVkFormat(desc_.stencilAttachmentFormat))
      .build(ctx.getVkDevice(),
             // TODO: use ctx.pipelineCache_
             VK_NULL_HANDLE,
             ctx.vkPipelineLayout_,
             &pipeline,
             desc_.debugName);

  pipelines_[dynamicState] = pipeline;

  return pipeline;
}

} // namespace lvk::vulkan
