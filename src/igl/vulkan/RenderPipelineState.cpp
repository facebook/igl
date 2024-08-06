/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/Device.h>
#include <igl/vulkan/RenderPipelineState.h>
#include <igl/vulkan/ShaderModule.h>
#include <igl/vulkan/VertexInputState.h>
#include <igl/vulkan/VulkanContext.h>
#include <igl/vulkan/VulkanDescriptorSetLayout.h>
#include <igl/vulkan/VulkanDevice.h>
#include <igl/vulkan/VulkanPipelineBuilder.h>
#include <igl/vulkan/VulkanPipelineLayout.h>

namespace {

VkPrimitiveTopology primitiveTypeToVkPrimitiveTopology(igl::PrimitiveType t) {
  switch (t) {
  case igl::PrimitiveType::Point:
    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  case igl::PrimitiveType::Line:
    return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  case igl::PrimitiveType::LineStrip:
    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  case igl::PrimitiveType::Triangle:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  case igl::PrimitiveType::TriangleStrip:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  }
  IGL_ASSERT_MSG(false, "Implement PrimitiveType = %u", (uint32_t)t);
  return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

VkPolygonMode polygonFillModeToVkPolygonMode(igl::PolygonFillMode mode) {
  switch (mode) {
  case igl::PolygonFillMode::Fill:
    return VK_POLYGON_MODE_FILL;
  case igl::PolygonFillMode::Line:
    return VK_POLYGON_MODE_LINE;
  }
  IGL_ASSERT_MSG(false, "Implement a missing polygon fill mode");
  return VK_POLYGON_MODE_FILL;
}

VkCullModeFlags cullModeToVkCullMode(igl::CullMode mode) {
  switch (mode) {
  case igl::CullMode::Disabled:
    return VK_CULL_MODE_NONE;
  case igl::CullMode::Front:
    return VK_CULL_MODE_FRONT_BIT;
  case igl::CullMode::Back:
    return VK_CULL_MODE_BACK_BIT;
  }
  IGL_ASSERT_MSG(false, "Implement a missing cull mode");
  return VK_CULL_MODE_NONE;
}

VkFrontFace windingModeToVkFrontFace(igl::WindingMode mode) {
  switch (mode) {
  case igl::WindingMode::Clockwise:
    return VK_FRONT_FACE_CLOCKWISE;
  case igl::WindingMode::CounterClockwise:
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
  case VertexAttributeFormat::Byte1Norm:
    return VK_FORMAT_R8_SNORM;
  case VertexAttributeFormat::Byte2Norm:
    return VK_FORMAT_R8G8_SNORM;
  case VertexAttributeFormat::Byte3Norm:
    return VK_FORMAT_R8G8B8_SNORM;
  case VertexAttributeFormat::Byte4Norm:
    return VK_FORMAT_R8G8B8A8_SNORM;
  case VertexAttributeFormat::UByte1Norm:
    return VK_FORMAT_R8_UNORM;
  case VertexAttributeFormat::UByte2Norm:
    return VK_FORMAT_R8G8_UNORM;
  case VertexAttributeFormat::UByte3Norm:
    return VK_FORMAT_R8G8B8_UNORM;
  case VertexAttributeFormat::UByte4Norm:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case VertexAttributeFormat::Short1Norm:
    return VK_FORMAT_R16_SNORM;
  case VertexAttributeFormat::Short2Norm:
    return VK_FORMAT_R16G16_SNORM;
  case VertexAttributeFormat::Short3Norm:
    return VK_FORMAT_R16G16B16_SNORM;
  case VertexAttributeFormat::Short4Norm:
    return VK_FORMAT_R16G16B16A16_SNORM;
  case VertexAttributeFormat::UShort1Norm:
    return VK_FORMAT_R16_UNORM;
  case VertexAttributeFormat::UShort2Norm:
    return VK_FORMAT_R16G16_UNORM;
  case VertexAttributeFormat::UShort3Norm:
    return VK_FORMAT_R16G16B16_UNORM;
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
  using igl::BlendOp;
  switch (value) {
  case BlendOp::Add:
    return VK_BLEND_OP_ADD;
  case BlendOp::Subtract:
    return VK_BLEND_OP_SUBTRACT;
  case BlendOp::ReverseSubtract:
    return VK_BLEND_OP_REVERSE_SUBTRACT;
  case BlendOp::Min:
    return VK_BLEND_OP_MIN;
  case BlendOp::Max:
    return VK_BLEND_OP_MAX;
  }

  IGL_ASSERT(false);
  return VK_BLEND_OP_ADD;
}

VkBool32 checkDualSrcBlendFactor(igl::BlendFactor value, VkBool32 dualSrcBlendSupported) {
  if (!dualSrcBlendSupported) {
    switch (value) {
    case igl::BlendFactor::Src1Color:
    case igl::BlendFactor::OneMinusSrc1Color:
    case igl::BlendFactor::Src1Alpha:
    case igl::BlendFactor::OneMinusSrc1Alpha:
      IGL_ASSERT(false);
      return VK_FALSE;
    default:
      return VK_TRUE;
    }
  }
  return VK_TRUE;
}

VkBlendFactor blendFactorToVkBlendFactor(igl::BlendFactor value) {
  using igl::BlendFactor;
  switch (value) {
  case BlendFactor::Zero:
    return VK_BLEND_FACTOR_ZERO;
  case BlendFactor::One:
    return VK_BLEND_FACTOR_ONE;
  case BlendFactor::SrcColor:
    return VK_BLEND_FACTOR_SRC_COLOR;
  case BlendFactor::OneMinusSrcColor:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
  case BlendFactor::DstColor:
    return VK_BLEND_FACTOR_DST_COLOR;
  case BlendFactor::OneMinusDstColor:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  case BlendFactor::SrcAlpha:
    return VK_BLEND_FACTOR_SRC_ALPHA;
  case BlendFactor::OneMinusSrcAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  case BlendFactor::DstAlpha:
    return VK_BLEND_FACTOR_DST_ALPHA;
  case BlendFactor::OneMinusDstAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  case BlendFactor::BlendColor:
    return VK_BLEND_FACTOR_CONSTANT_COLOR;
  case BlendFactor::OneMinusBlendColor:
    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
  case BlendFactor::BlendAlpha:
    return VK_BLEND_FACTOR_CONSTANT_ALPHA;
  case BlendFactor::OneMinusBlendAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
  case BlendFactor::SrcAlphaSaturated:
    return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
  case BlendFactor::Src1Color:
    return VK_BLEND_FACTOR_SRC1_COLOR;
  case BlendFactor::OneMinusSrc1Color:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
  case BlendFactor::Src1Alpha:
    return VK_BLEND_FACTOR_SRC1_ALPHA;
  case BlendFactor::OneMinusSrc1Alpha:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
  default:
    IGL_ASSERT(false);
    return VK_BLEND_FACTOR_ONE; // default for unsupported values
  }
}

VkColorComponentFlags colorWriteMaskToVkColorComponentFlags(igl::ColorWriteMask value) {
  VkColorComponentFlags result = 0;
  if (value & igl::ColorWriteBitsRed) {
    result |= VK_COLOR_COMPONENT_R_BIT;
  }
  if (value & igl::ColorWriteBitsGreen) {
    result |= VK_COLOR_COMPONENT_G_BIT;
  }
  if (value & igl::ColorWriteBitsBlue) {
    result |= VK_COLOR_COMPONENT_B_BIT;
  }
  if (value & igl::ColorWriteBitsAlpha) {
    result |= VK_COLOR_COMPONENT_A_BIT;
  }
  return result;
}

} // namespace

namespace igl::vulkan {

RenderPipelineState::RenderPipelineState(const igl::vulkan::Device& device,
                                         RenderPipelineDesc desc) :
  IRenderPipelineState(desc),
  PipelineState(device.getVulkanContext(),
                desc.shaderStages.get(),
                desc.immutableSamplers,
                desc.isDynamicBufferMask,
                desc.debugName.c_str()),
  device_(device),
  reflection_(std::make_shared<RenderPipelineReflection>()) {
  // Iterate and cache vertex input bindings and attributes
  const igl::vulkan::VertexInputState* vstate =
      static_cast<igl::vulkan::VertexInputState*>(desc_.vertexInputState.get());

  vertexInputStateCreateInfo_ = ivkGetPipelineVertexInputStateCreateInfo_Empty();

  if (vstate) {
    std::array<bool, IGL_VERTEX_BUFFER_MAX> bufferAlreadyBound{};
    vkBindings_.reserve(vstate->desc_.numInputBindings);

    for (size_t i = 0; i != vstate->desc_.numAttributes; i++) {
      const VertexAttribute& attr = vstate->desc_.attributes[i];
      const VkFormat format = vertexAttributeFormatToVkFormat(attr.format);
      const size_t bufferIndex = attr.bufferIndex;

      vkAttributes_[i] = ivkGetVertexInputAttributeDescription(
          (uint32_t)attr.location, (uint32_t)bufferIndex, format, (uint32_t)attr.offset);

      if (!bufferAlreadyBound[bufferIndex]) {
        bufferAlreadyBound[bufferIndex] = true;

        const VertexInputBinding& binding = vstate->desc_.inputBindings[bufferIndex];
        const VkVertexInputRate rate = (binding.sampleFunction == VertexSampleFunction::PerVertex)
                                           ? VK_VERTEX_INPUT_RATE_VERTEX
                                           : VK_VERTEX_INPUT_RATE_INSTANCE;
        vkBindings_.emplace_back(ivkGetVertexInputBindingDescription(
            (uint32_t)bufferIndex, (uint32_t)binding.stride, rate));
      }
    }

    vertexInputStateCreateInfo_.vertexBindingDescriptionCount =
        static_cast<uint32_t>(vstate->desc_.numInputBindings);
    vertexInputStateCreateInfo_.pVertexBindingDescriptions = vkBindings_.data();
    vertexInputStateCreateInfo_.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(vstate->desc_.numAttributes);
    vertexInputStateCreateInfo_.pVertexAttributeDescriptions = vkAttributes_.data();
  }
}

RenderPipelineState::~RenderPipelineState() {
  const VulkanContext& ctx = device_.getVulkanContext();
  VkDevice device = ctx.device_->getVkDevice();

  for (const auto& p : pipelines_) {
    if (p.second != VK_NULL_HANDLE) {
      device_.getVulkanContext().deferredTask(
          std::packaged_task<void()>([vf = &ctx.vf_, device, pipeline = p.second]() {
            vf->vkDestroyPipeline(device, pipeline, nullptr);
          }));
    }
  }
}

VkPipeline RenderPipelineState::getVkPipeline(
    const RenderPipelineDynamicState& dynamicState) const {
  const VulkanContext& ctx = device_.getVulkanContext();

  if (ctx.config_.enableDescriptorIndexing) {
    // the bindless descriptor set layout can be changed in VulkanContext when the number of
    // existing textures increases
    if (lastBindlessVkDescriptorSetLayout_ != ctx.getBindlessVkDescriptorSetLayout()) {
      // there's a new descriptor set layout - drop the previous Vulkan pipeline
      VkDevice device = ctx.device_->getVkDevice();
      for (const auto& p : pipelines_) {
        if (p.second != VK_NULL_HANDLE) {
          ctx.deferredTask(
              std::packaged_task<void()>([vf = &ctx.vf_, device, pipeline = p.second]() {
                vf->vkDestroyPipeline(device, pipeline, nullptr);
              }));
        }
      }
      pipelines_.clear();
      lastBindlessVkDescriptorSetLayout_ = ctx.getBindlessVkDescriptorSetLayout();
    }
  }

  const auto it = pipelines_.find(dynamicState);

  if (it != pipelines_.end()) {
    return it->second;
  }

  // @fb-only
  const VkDescriptorSetLayout DSLs[] = {
      dslCombinedImageSamplers_->getVkDescriptorSetLayout(),
      dslBuffers_->getVkDescriptorSetLayout(),
      ctx.getBindlessVkDescriptorSetLayout(),
  };

  pipelineLayout_ = std::make_unique<VulkanPipelineLayout>(
      ctx,
      ctx.getVkDevice(),
      DSLs,
      static_cast<uint32_t>(ctx.config_.enableDescriptorIndexing
                                ? IGL_ARRAY_NUM_ELEMENTS(DSLs)
                                : IGL_ARRAY_NUM_ELEMENTS(DSLs) - 1u),
      info_.hasPushConstants ? &pushConstantRange_ : nullptr,
      IGL_FORMAT("Pipeline Layout: {}", desc_.debugName.c_str()).c_str());

  const VkPhysicalDeviceFeatures2& deviceFeatures = ctx.getVkPhysicalDeviceFeatures2();
  VkBool32 dualSrcBlendSupported = deviceFeatures.features.dualSrcBlend;

  // build a new Vulkan pipeline
  VkRenderPass renderPass = ctx.getRenderPass(dynamicState.renderPassIndex_).pass;

  VkPipeline pipeline = VK_NULL_HANDLE;

  // Not all attachments are valid. We need to create color blend attachments only for active
  // attachments
  std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
  colorBlendAttachmentStates.reserve(desc_.targetDesc.colorAttachments.size());
  std::for_each(
      desc_.targetDesc.colorAttachments.begin(),
      desc_.targetDesc.colorAttachments.end(),
      [&colorBlendAttachmentStates, dualSrcBlendSupported](auto attachment) mutable {
        if (attachment.textureFormat != TextureFormat::Invalid) {
          // In Vulkan color write bits are part of blending.
          if (!attachment.blendEnabled && attachment.colorWriteMask == igl::ColorWriteBitsAll) {
            colorBlendAttachmentStates.push_back(
                ivkGetPipelineColorBlendAttachmentState_NoBlending());
          } else {
            checkDualSrcBlendFactor(attachment.srcRGBBlendFactor, dualSrcBlendSupported);
            checkDualSrcBlendFactor(attachment.dstRGBBlendFactor, dualSrcBlendSupported);
            checkDualSrcBlendFactor(attachment.srcAlphaBlendFactor, dualSrcBlendSupported);
            checkDualSrcBlendFactor(attachment.dstAlphaBlendFactor, dualSrcBlendSupported);

            colorBlendAttachmentStates.push_back(ivkGetPipelineColorBlendAttachmentState(
                true,
                blendFactorToVkBlendFactor(attachment.srcRGBBlendFactor),
                blendFactorToVkBlendFactor(attachment.dstRGBBlendFactor),
                blendOpToVkBlendOp(attachment.rgbBlendOp),
                blendFactorToVkBlendFactor(attachment.srcAlphaBlendFactor),
                blendFactorToVkBlendFactor(attachment.dstAlphaBlendFactor),
                blendOpToVkBlendOp(attachment.alphaBlendOp),
                colorWriteMaskToVkColorComponentFlags(attachment.colorWriteMask)));
          }
        }
      });

  const auto& vertexModule = desc_.shaderStages->getVertexModule();
  const auto& fragmentModule = desc_.shaderStages->getFragmentModule();
  VK_ASSERT_RETURN_NULL_HANDLE(
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
          .primitiveTopology(primitiveTypeToVkPrimitiveTopology(desc_.topology))
          .depthBiasEnable(dynamicState.depthBiasEnable_)
          .depthCompareOp(dynamicState.getDepthCompareOp())
          .depthWriteEnable(dynamicState.depthWriteEnable_)
          .rasterizationSamples(getVulkanSampleCountFlags(desc_.sampleCount))
          .polygonMode(polygonFillModeToVkPolygonMode(desc_.polygonFillMode))
          .stencilStateOps(VK_STENCIL_FACE_FRONT_BIT,
                           dynamicState.getStencilStateFailOp(true),
                           dynamicState.getStencilStatePassOp(true),
                           dynamicState.getStencilStateDepthFailOp(true),
                           dynamicState.getStencilStateCompareOp(true))
          .stencilStateOps(VK_STENCIL_FACE_BACK_BIT,
                           dynamicState.getStencilStateFailOp(false),
                           dynamicState.getStencilStatePassOp(false),
                           dynamicState.getStencilStateDepthFailOp(false),
                           dynamicState.getStencilStateCompareOp(false))
          .shaderStages({
              ivkGetPipelineShaderStageCreateInfo(
                  VK_SHADER_STAGE_VERTEX_BIT,
                  igl::vulkan::ShaderModule::getVkShaderModule(vertexModule),
                  vertexModule->info().entryPoint.c_str()),
              ivkGetPipelineShaderStageCreateInfo(
                  VK_SHADER_STAGE_FRAGMENT_BIT,
                  igl::vulkan::ShaderModule::getVkShaderModule(fragmentModule),
                  fragmentModule->info().entryPoint.c_str()),
          })
          .cullMode(cullModeToVkCullMode(desc_.cullMode))
          .frontFace(windingModeToVkFrontFace(desc_.frontFaceWinding))
          .vertexInputState(vertexInputStateCreateInfo_)
          .colorBlendAttachmentStates(colorBlendAttachmentStates)
          .build(ctx.vf_,
                 ctx.device_->getVkDevice(),
                 ctx.pipelineCache_,
                 pipelineLayout_->getVkPipelineLayout(),
                 renderPass,
                 &pipeline,
                 desc_.debugName.c_str()));

  IGL_ASSERT(pipeline != VK_NULL_HANDLE);

  pipelines_[dynamicState] = pipeline;

  // @fb-only
  // @lint-ignore CLANGTIDY
  return pipeline;
}

int RenderPipelineState::getIndexByName(const igl::NameHandle& name, ShaderStage stage) const {
  IGL_ASSERT_NOT_IMPLEMENTED();
  (void)name;
  (void)stage;
  return 0;
}

int RenderPipelineState::getIndexByName(const std::string& name, ShaderStage stage) const {
  IGL_ASSERT_NOT_IMPLEMENTED();
  (void)name;
  (void)stage;
  return 0;
}

std::shared_ptr<igl::IRenderPipelineReflection> RenderPipelineState::renderPipelineReflection() {
  return reflection_;
}

void RenderPipelineState::setRenderPipelineReflection(
    const IRenderPipelineReflection& renderPipelineReflection) {
  const auto& vulkanReflection =
      static_cast<const RenderPipelineReflection&>(renderPipelineReflection);
  auto copy = RenderPipelineReflection(vulkanReflection);

  reflection_ = std::make_shared<RenderPipelineReflection>(std::move(copy));
}

} // namespace igl::vulkan
