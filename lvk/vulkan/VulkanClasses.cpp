/*
 * LightweightVK
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstring>
#include <deque>
#include <set>
#include <vector>

#define VMA_IMPLEMENTATION
#define VOLK_IMPLEMENTATION

#include "VulkanClasses.h"
#include "VulkanUtils.h"

#include <glslang/Include/glslang_c_interface.h>
#include <ldrutils/lutils/ScopeExit.h>

#ifndef VK_USE_PLATFORM_WIN32_KHR
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <MoltenVK/mvk_config.h>
#include <dlfcn.h>
#endif

uint32_t lvk::VulkanPipelineBuilder::numPipelinesCreated_ = 0;

static_assert(lvk::HWDeviceDesc::LVK_MAX_PHYSICAL_DEVICE_NAME_SIZE == VK_MAX_PHYSICAL_DEVICE_NAME_SIZE);

namespace {

const char* kDefaultValidationLayers[] = {"VK_LAYER_KHRONOS_validation"};

// These bindings should match GLSL declarations injected into shaders in VulkanContext::createShaderModule().
enum Bindings {
  kBinding_Textures = 0,
  kBinding_Samplers = 1,
  kBinding_StorageImages = 2,
  kBinding_NumBindings = 3,
};

VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
                                                   [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT msgType,
                                                   const VkDebugUtilsMessengerCallbackDataEXT* cbData,
                                                   void* userData) {
  if (msgSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    return VK_FALSE;
  }

  const bool isError = (msgSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;

  char errorName[128] = {};
  int object = 0;
  void* handle = nullptr;
  char typeName[128] = {};
  void* messageID = nullptr;

  if (sscanf(cbData->pMessage,
             "Validation Error : [ %127s ] Object %i: handle = %p, type = %127s | MessageID = %p",
             errorName,
             &object,
             &handle,
             typeName,
             &messageID) >= 2) {
    const char* message = strrchr(cbData->pMessage, '|') + 1;
    LLOGL(
        "%sValidation layer:\n Validation Error: %s \n Object %i: handle = %p, type = %s\n "
        "MessageID = %p \n%s \n",
        isError ? "\nERROR:\n" : "",
        errorName,
        object,
        handle,
        typeName,
        messageID,
        message);
  } else {
    LLOGL("%sValidation layer:\n%s\n", isError ? "\nERROR:\n" : "", cbData->pMessage);
  }

  if (isError) {
    lvk::VulkanContext* ctx = static_cast<lvk::VulkanContext*>(userData);
    if (ctx->config_.terminateOnValidationError) {
      LVK_ASSERT(false);
      std::terminate();
    }
  }

  return VK_FALSE;
}

VkIndexType indexFormatToVkIndexType(lvk::IndexFormat fmt) {
  switch (fmt) {
  case lvk::IndexFormat_UI16:
    return VK_INDEX_TYPE_UINT16;
  case lvk::IndexFormat_UI32:
    return VK_INDEX_TYPE_UINT32;
  };
  LVK_ASSERT(false);
  return VK_INDEX_TYPE_NONE_KHR;
}

VkPrimitiveTopology primitiveTypeToVkPrimitiveTopology(lvk::PrimitiveType t) {
  switch (t) {
  case lvk::Primitive_Point:
    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  case lvk::Primitive_Line:
    return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  case lvk::Primitive_LineStrip:
    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  case lvk::Primitive_Triangle:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  case lvk::Primitive_TriangleStrip:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  }
  LVK_ASSERT_MSG(false, "Implement PrimitiveType = %u", (uint32_t)t);
  return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

VkAttachmentLoadOp loadOpToVkAttachmentLoadOp(lvk::LoadOp a) {
  switch (a) {
  case lvk::LoadOp_Invalid:
    LVK_ASSERT(false);
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  case lvk::LoadOp_DontCare:
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  case lvk::LoadOp_Load:
    return VK_ATTACHMENT_LOAD_OP_LOAD;
  case lvk::LoadOp_Clear:
    return VK_ATTACHMENT_LOAD_OP_CLEAR;
  case lvk::LoadOp_None:
    return VK_ATTACHMENT_LOAD_OP_NONE_EXT;
  }
  LVK_ASSERT(false);
  return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp storeOpToVkAttachmentStoreOp(lvk::StoreOp a) {
  switch (a) {
  case lvk::StoreOp_DontCare:
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  case lvk::StoreOp_Store:
    return VK_ATTACHMENT_STORE_OP_STORE;
  case lvk::StoreOp_MsaaResolve:
    // for MSAA resolve, we have to store data into a special "resolve" attachment
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  case lvk::StoreOp_None:
    return VK_ATTACHMENT_STORE_OP_NONE;
  }
  LVK_ASSERT(false);
  return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkShaderStageFlagBits shaderStageToVkShaderStage(lvk::ShaderStage stage) {
  switch (stage) {
  case lvk::Stage_Vert:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case lvk::Stage_Geom:
    return VK_SHADER_STAGE_GEOMETRY_BIT;
  case lvk::Stage_Frag:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case lvk::Stage_Comp:
    return VK_SHADER_STAGE_COMPUTE_BIT;
  };
  return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}

VkMemoryPropertyFlags storageTypeToVkMemoryPropertyFlags(lvk::StorageType storage) {
  VkMemoryPropertyFlags memFlags{0};

  switch (storage) {
  case lvk::StorageType_Device:
    memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    break;
  case lvk::StorageType_HostVisible:
    memFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    break;
  case lvk::StorageType_Memoryless:
    memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    break;
  }
  return memFlags;
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

bool supportsFormat(VkPhysicalDevice physicalDevice, VkFormat format) {
  VkFormatProperties properties;
  vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
  return properties.bufferFeatures != 0 || properties.linearTilingFeatures != 0 || properties.optimalTilingFeatures != 0;
}

std::vector<VkFormat> getCompatibleDepthStencilFormats(lvk::Format format) {
  switch (format) {
  case lvk::Format_Z_UN16:
    return {VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT};
  case lvk::Format_Z_UN24:
    return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM_S8_UINT};
  case lvk::Format_Z_F32:
    return {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
  case lvk::Format_Z_UN24_S_UI8:
    return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT};
  case lvk::Format_Z_F32_S_UI8:
    return {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT};
  default:
    return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT};
  }
  return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT};
}

bool validateImageLimits(VkImageType imageType,
                         VkSampleCountFlagBits samples,
                         const VkExtent3D& extent,
                         const VkPhysicalDeviceLimits& limits,
                         lvk::Result* outResult) {
  using lvk::Result;

  if (samples != VK_SAMPLE_COUNT_1_BIT && !LVK_VERIFY(imageType == VK_IMAGE_TYPE_2D)) {
    Result::setResult(outResult, Result(Result::Code::ArgumentOutOfRange, "Multisampling is supported only for 2D images"));
    return false;
  }

  if (imageType == VK_IMAGE_TYPE_2D &&
      !LVK_VERIFY(extent.width <= limits.maxImageDimension2D && extent.height <= limits.maxImageDimension2D)) {
    Result::setResult(outResult, Result(Result::Code::ArgumentOutOfRange, "2D texture size exceeded"));
    return false;
  }
  if (imageType == VK_IMAGE_TYPE_3D &&
      !LVK_VERIFY(extent.width <= limits.maxImageDimension3D && extent.height <= limits.maxImageDimension3D &&
                  extent.depth <= limits.maxImageDimension3D)) {
    Result::setResult(outResult, Result(Result::Code::ArgumentOutOfRange, "3D texture size exceeded"));
    return false;
  }

  return true;
}

lvk::Result validateRange(const VkExtent3D& ext, uint32_t numLevels, const lvk::TextureRangeDesc& range) {
  if (!LVK_VERIFY(range.dimensions.width > 0 && range.dimensions.height > 0 || range.dimensions.depth > 0 || range.numLayers > 0 ||
                  range.numMipLevels > 0)) {
    return lvk::Result{lvk::Result::Code::ArgumentOutOfRange, "width, height, depth numLayers, and numMipLevels must be > 0"};
  }
  if (range.mipLevel > numLevels) {
    return lvk::Result{lvk::Result::Code::ArgumentOutOfRange, "range.mipLevel exceeds texture mip-levels"};
  }

  const auto texWidth = std::max(ext.width >> range.mipLevel, 1u);
  const auto texHeight = std::max(ext.height >> range.mipLevel, 1u);
  const auto texDepth = std::max(ext.depth >> range.mipLevel, 1u);

  if (range.dimensions.width > texWidth || range.dimensions.height > texHeight || range.dimensions.depth > texDepth) {
    return lvk::Result{lvk::Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }
  if (range.x > texWidth - range.dimensions.width || range.y > texHeight - range.dimensions.height ||
      range.z > texDepth - range.dimensions.depth) {
    return lvk::Result{lvk::Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }

  return lvk::Result{};
}

bool isHostVisibleSingleHeapMemory(VkPhysicalDevice physDev) {
  VkPhysicalDeviceMemoryProperties memProperties;

  vkGetPhysicalDeviceMemoryProperties(physDev, &memProperties);

  if (memProperties.memoryHeapCount != 1) {
    return false;
  }

  const uint32_t flag = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((memProperties.memoryTypes[i].propertyFlags & flag) == flag) {
      return true;
    }
  }

  return false;
}

void getInstanceExtensionProps(std::vector<VkExtensionProperties>& props, const char* validationLayer = nullptr) {
  uint32_t numExtensions = 0;
  vkEnumerateInstanceExtensionProperties(validationLayer, &numExtensions, nullptr);
  std::vector<VkExtensionProperties> p(numExtensions);
  vkEnumerateInstanceExtensionProperties(validationLayer, &numExtensions, p.data());
  props.insert(props.end(), p.begin(), p.end());
}

void getDeviceExtensionProps(VkPhysicalDevice dev, std::vector<VkExtensionProperties>& props, const char* validationLayer = nullptr) {
  uint32_t numExtensions = 0;
  vkEnumerateDeviceExtensionProperties(dev, validationLayer, &numExtensions, nullptr);
  std::vector<VkExtensionProperties> p(numExtensions);
  vkEnumerateDeviceExtensionProperties(dev, validationLayer, &numExtensions, p.data());
  props.insert(props.end(), p.begin(), p.end());
}

bool hasExtension(const char* ext, const std::vector<VkExtensionProperties>& props) {
  for (const VkExtensionProperties& p : props) {
    if (strcmp(ext, p.extensionName) == 0)
      return true;
  }
  return false;
}

void transitionColorAttachment(VkCommandBuffer buffer, lvk::VulkanTexture* colorTex) {
  if (!LVK_VERIFY(colorTex)) {
    return;
  }

  const lvk::VulkanImage& colorImg = *colorTex->image_.get();
  if (!LVK_VERIFY(!colorImg.isDepthFormat_ && !colorImg.isStencilFormat_)) {
    LVK_ASSERT_MSG(false, "Color attachments cannot have depth/stencil formats");
    return;
  }
  LVK_ASSERT_MSG(colorImg.vkImageFormat_ != VK_FORMAT_UNDEFINED, "Invalid color attachment format");
  colorImg.transitionLayout(buffer,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for all subsequent
                                                                                                          // fragment/compute shaders
                            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
}

bool isDepthOrStencilVkFormat(VkFormat format) {
  switch (format) {
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_X8_D24_UNORM_PACK32:
  case VK_FORMAT_D32_SFLOAT:
  case VK_FORMAT_S8_UINT:
  case VK_FORMAT_D16_UNORM_S8_UINT:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return true;
  default:
    return false;
  }
  return false;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats, lvk::ColorSpace colorSpace) {
  LVK_ASSERT(!formats.empty());

  auto isNativeSwapChainBGR = [](const std::vector<VkSurfaceFormatKHR>& formats) -> bool {
    for (auto& fmt : formats) {
      // The preferred format should be the one which is closer to the beginning of the formats
      // container. If BGR is encountered earlier, it should be picked as the format of choice. If RGB
      // happens to be earlier, take it.
      if (fmt.format == VK_FORMAT_R8G8B8A8_UNORM || fmt.format == VK_FORMAT_R8G8B8A8_SRGB ||
          fmt.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32) {
        return false;
      }
      if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM || fmt.format == VK_FORMAT_B8G8R8A8_SRGB ||
          fmt.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32) {
        return true;
      }
    }
    return false;
  };

  auto colorSpaceToVkSurfaceFormat = [](lvk::ColorSpace colorSpace, bool isBGR) -> VkSurfaceFormatKHR {
    switch (colorSpace) {
    case lvk::ColorSpace_SRGB_LINEAR:
      // the closest thing to sRGB linear
      return VkSurfaceFormatKHR{isBGR ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_BT709_LINEAR_EXT};
    case lvk::ColorSpace_SRGB_NONLINEAR:
      [[fallthrough]];
    default:
      // default to normal sRGB non linear.
      return VkSurfaceFormatKHR{isBGR ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }
  };

  const VkSurfaceFormatKHR preferred = colorSpaceToVkSurfaceFormat(colorSpace, isNativeSwapChainBGR(formats));

  for (const VkSurfaceFormatKHR& fmt : formats) {
    if (fmt.format == preferred.format && fmt.colorSpace == preferred.colorSpace) {
      return fmt;
    }
  }

  // if we can't find a matching format and color space, fallback on matching only format
  for (const VkSurfaceFormatKHR& fmt : formats) {
    if (fmt.format == preferred.format) {
      return fmt;
    }
  }

  LLOGL("Could not find a native swap chain format that matched our designed swapchain format. Defaulting to first supported format.");

  return formats[0];
}

} // namespace

namespace lvk {

struct DeferredTask {
  DeferredTask(std::packaged_task<void()>&& task, SubmitHandle handle) : task_(std::move(task)), handle_(handle) {}
  std::packaged_task<void()> task_;
  SubmitHandle handle_;
};

struct VulkanContextImpl final {
  // Vulkan Memory Allocator
  VmaAllocator vma_ = VK_NULL_HANDLE;

  lvk::CommandBuffer currentCommandBuffer_;

  mutable std::deque<DeferredTask> deferredTasks_;
};

} // namespace lvk

lvk::VulkanBuffer::VulkanBuffer(lvk::VulkanContext* ctx,
                                VkDevice device,
                                VkDeviceSize bufferSize,
                                VkBufferUsageFlags usageFlags,
                                VkMemoryPropertyFlags memFlags,
                                const char* debugName) :
  ctx_(ctx), device_(device), bufferSize_(bufferSize), vkUsageFlags_(usageFlags), vkMemFlags_(memFlags) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  LVK_ASSERT(ctx);
  LVK_ASSERT(bufferSize > 0);

  const VkBufferCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = bufferSize,
      .usage = usageFlags,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
  };

  if (LVK_VULKAN_USE_VMA) {
    // Initialize VmaAllocation Info
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      vmaAllocInfo_.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
      vmaAllocInfo_.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
      vmaAllocInfo_.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    }
    if (memFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
      vmaAllocInfo_.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    vmaAllocInfo_.usage = VMA_MEMORY_USAGE_AUTO;

    vmaCreateBuffer((VmaAllocator)ctx_->getVmaAllocator(), &ci, &vmaAllocInfo_, &vkBuffer_, &vmaAllocation_, nullptr);

    // handle memory-mapped buffers
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      vmaMapMemory((VmaAllocator)ctx_->getVmaAllocator(), vmaAllocation_, &mappedPtr_);
    }
  } else {
    // create buffer
    VK_ASSERT(vkCreateBuffer(device_, &ci, nullptr, &vkBuffer_));

    // back the buffer with some memory
    {
      VkMemoryRequirements requirements = {};
      vkGetBufferMemoryRequirements(device_, vkBuffer_, &requirements);

      VK_ASSERT(lvk::allocateMemory(ctx_->getVkPhysicalDevice(), device_, &requirements, memFlags, &vkMemory_));
      VK_ASSERT(vkBindBufferMemory(device_, vkBuffer_, vkMemory_, 0));
    }

    // handle memory-mapped buffers
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      VK_ASSERT(vkMapMemory(device_, vkMemory_, 0, bufferSize_, 0, &mappedPtr_));
    }
  }

  LVK_ASSERT(vkBuffer_ != VK_NULL_HANDLE);

  // set debug name
  VK_ASSERT(lvk::setDebugObjectName(device_, VK_OBJECT_TYPE_BUFFER, (uint64_t)vkBuffer_, debugName));

  // handle shader access
  if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    const VkBufferDeviceAddressInfo ai = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = vkBuffer_,
    };
    vkDeviceAddress_ = vkGetBufferDeviceAddress(device_, &ai);
    LVK_ASSERT(vkDeviceAddress_);
  }
}

lvk::VulkanBuffer::~VulkanBuffer() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  if (!ctx_) {
    return;
  }

  if (LVK_VULKAN_USE_VMA) {
    if (mappedPtr_) {
      vmaUnmapMemory((VmaAllocator)ctx_->getVmaAllocator(), vmaAllocation_);
    }
    ctx_->deferredTask(std::packaged_task<void()>([vma = ctx_->getVmaAllocator(), buffer = vkBuffer_, allocation = vmaAllocation_]() {
      vmaDestroyBuffer((VmaAllocator)vma, buffer, allocation);
    }));
  } else {
    if (mappedPtr_) {
      vkUnmapMemory(device_, vkMemory_);
    }
    ctx_->deferredTask(std::packaged_task<void()>([device = device_, buffer = vkBuffer_, memory = vkMemory_]() {
      vkDestroyBuffer(device, buffer, nullptr);
      vkFreeMemory(device, memory, nullptr);
    }));
  }
}

lvk::VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) :
  ctx_(other.ctx_),
  device_(other.device_),
  vkBuffer_(other.vkBuffer_),
  vkMemory_(other.vkMemory_),
  vmaAllocInfo_(other.vmaAllocInfo_),
  vmaAllocation_(other.vmaAllocation_),
  vkDeviceAddress_(other.vkDeviceAddress_),
  bufferSize_(other.bufferSize_),
  vkUsageFlags_(other.vkUsageFlags_),
  vkMemFlags_(other.vkMemFlags_),
  mappedPtr_(other.mappedPtr_) {
  other.ctx_ = nullptr;
}

lvk::VulkanBuffer& lvk::VulkanBuffer::operator=(VulkanBuffer&& other) {
  std::swap(ctx_, other.ctx_);
  std::swap(device_, other.device_);
  std::swap(vkBuffer_, other.vkBuffer_);
  std::swap(vkMemory_, other.vkMemory_);
  std::swap(vmaAllocInfo_, other.vmaAllocInfo_);
  std::swap(vmaAllocation_, other.vmaAllocation_);
  std::swap(vkDeviceAddress_, other.vkDeviceAddress_);
  std::swap(bufferSize_, other.bufferSize_);
  std::swap(vkUsageFlags_, other.vkUsageFlags_);
  std::swap(vkMemFlags_, other.vkMemFlags_);
  std::swap(mappedPtr_, other.mappedPtr_);
  return *this;
}

void lvk::VulkanBuffer::flushMappedMemory(VkDeviceSize offset, VkDeviceSize size) const {
  if (!LVK_VERIFY(isMapped())) {
    return;
  }

  if (LVK_VULKAN_USE_VMA) {
    vmaFlushAllocation((VmaAllocator)ctx_->getVmaAllocator(), vmaAllocation_, offset, size);
  } else {
    const VkMappedMemoryRange memoryRange = {
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = vkMemory_,
        .offset = offset,
        .size = size,
    };
    vkFlushMappedMemoryRanges(device_, 1, &memoryRange);
  }
}

void lvk::VulkanBuffer::getBufferSubData(size_t offset, size_t size, void* data) {
  // only host-visible buffers can be downloaded this way
  LVK_ASSERT(mappedPtr_);

  if (!mappedPtr_) {
    return;
  }

  LVK_ASSERT(offset + size <= bufferSize_);

  const uint8_t* src = static_cast<uint8_t*>(mappedPtr_) + offset;
  memcpy(data, src, size);
}

void lvk::VulkanBuffer::bufferSubData(size_t offset, size_t size, const void* data) {
  // only host-visible buffers can be uploaded this way
  LVK_ASSERT(mappedPtr_);

  if (!mappedPtr_) {
    return;
  }

  LVK_ASSERT(offset + size <= bufferSize_);

  if (data) {
    memcpy((uint8_t*)mappedPtr_ + offset, data, size);
  } else {
    memset((uint8_t*)mappedPtr_ + offset, 0, size);
  }
}

lvk::VulkanImage::VulkanImage(lvk::VulkanContext& ctx,
                              VkDevice device,
                              VkImage image,
                              VkImageUsageFlags usageFlags,
                              VkFormat imageFormat,
                              VkExtent3D extent,
                              const char* debugName) :
  ctx_(ctx),
  vkDevice_(device),
  vkImage_(image),
  vkUsageFlags_(usageFlags),
  isSwapchainImage_(true),
  vkExtent_(extent),
  vkType_(VK_IMAGE_TYPE_2D),
  vkImageFormat_(imageFormat),
  isDepthFormat_(isDepthFormat(imageFormat)),
  isStencilFormat_(isStencilFormat(imageFormat)) {
  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));
}

lvk::VulkanImage::VulkanImage(lvk::VulkanContext& ctx,
                              VkDevice device,
                              VkExtent3D extent,
                              VkImageType type,
                              VkFormat format,
                              uint32_t numLevels,
                              uint32_t numLayers,
                              VkImageTiling tiling,
                              VkImageUsageFlags usageFlags,
                              VkMemoryPropertyFlags memFlags,
                              VkImageCreateFlags createFlags,
                              VkSampleCountFlagBits samples,
                              const char* debugName) :
  ctx_(ctx),
  vkDevice_(device),
  vkUsageFlags_(usageFlags),
  vkExtent_(extent),
  vkType_(type),
  vkImageFormat_(format),
  numLevels_(numLevels),
  numLayers_(numLayers),
  vkSamples_(samples),
  isDepthFormat_(isDepthFormat(format)),
  isStencilFormat_(isStencilFormat(format)) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  LVK_ASSERT_MSG(numLevels_ > 0, "The image must contain at least one mip-level");
  LVK_ASSERT_MSG(numLayers_ > 0, "The image must contain at least one layer");
  LVK_ASSERT_MSG(vkImageFormat_ != VK_FORMAT_UNDEFINED, "Invalid VkFormat value");
  LVK_ASSERT_MSG(vkSamples_ > 0, "The image must contain at least one sample");
  LVK_ASSERT(extent.width > 0);
  LVK_ASSERT(extent.height > 0);
  LVK_ASSERT(extent.depth > 0);

  const VkImageCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = createFlags,
      .imageType = type,
      .format = vkImageFormat_,
      .extent = vkExtent_,
      .mipLevels = numLevels_,
      .arrayLayers = numLayers_,
      .samples = samples,
      .tiling = tiling,
      .usage = usageFlags,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  if (LVK_VULKAN_USE_VMA) {
    vmaAllocInfo_.usage = memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_AUTO;
    VkResult result = vmaCreateImage((VmaAllocator)ctx_.getVmaAllocator(), &ci, &vmaAllocInfo_, &vkImage_, &vmaAllocation_, nullptr);

    if (!LVK_VERIFY(result == VK_SUCCESS)) {
      LLOGW("failed: error result: %d, memflags: %d,  imageformat: %d\n", result, memFlags, vkImageFormat_);
    }

    // handle memory-mapped buffers
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      vmaMapMemory((VmaAllocator)ctx_.getVmaAllocator(), vmaAllocation_, &mappedPtr_);
    }
  } else {
    // create image
    VK_ASSERT(vkCreateImage(vkDevice_, &ci, nullptr, &vkImage_));

    // back the image with some memory
    {
      VkMemoryRequirements memRequirements;
      vkGetImageMemoryRequirements(device, vkImage_, &memRequirements);

      VK_ASSERT(lvk::allocateMemory(ctx.getVkPhysicalDevice(), vkDevice_, &memRequirements, memFlags, &vkMemory_));
      VK_ASSERT(vkBindImageMemory(vkDevice_, vkImage_, vkMemory_, 0));
    }

    // handle memory-mapped images
    if (memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      VK_ASSERT(vkMapMemory(vkDevice_, vkMemory_, 0, VK_WHOLE_SIZE, 0, &mappedPtr_));
    }
  }

  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_IMAGE, (uint64_t)vkImage_, debugName));

  // Get physical device's properties for the image's format
  vkGetPhysicalDeviceFormatProperties(ctx.getVkPhysicalDevice(), vkImageFormat_, &vkFormatProperties_);
}

lvk::VulkanImage::~VulkanImage() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  if (!isSwapchainImage_) {
    if (LVK_VULKAN_USE_VMA) {
      if (mappedPtr_) {
        vmaUnmapMemory((VmaAllocator)ctx_.getVmaAllocator(), vmaAllocation_);
      }
      ctx_.deferredTask(std::packaged_task<void()>([vma = ctx_.getVmaAllocator(), image = vkImage_, allocation = vmaAllocation_]() {
        vmaDestroyImage((VmaAllocator)vma, image, allocation);
      }));
    } else {
      if (mappedPtr_) {
        vkUnmapMemory(vkDevice_, vkMemory_);
      }
      ctx_.deferredTask(std::packaged_task<void()>([device = vkDevice_, image = vkImage_, memory = vkMemory_]() {
        vkDestroyImage(device, image, nullptr);
        if (memory != VK_NULL_HANDLE) {
          vkFreeMemory(device, memory, nullptr);
        }
      }));
    }
  }
}

VkImageView lvk::VulkanImage::createImageView(VkImageViewType type,
                                              VkFormat format,
                                              VkImageAspectFlags aspectMask,
                                              uint32_t baseLevel,
                                              uint32_t numLevels,
                                              uint32_t baseLayer,
                                              uint32_t numLayers,
                                              const char* debugName) const {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  const VkImageViewCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = vkImage_,
      .viewType = type,
      .format = format,
      .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                     .a = VK_COMPONENT_SWIZZLE_IDENTITY},
      .subresourceRange = {aspectMask, baseLevel, numLevels ? numLevels : numLevels_, baseLayer, numLayers},
  };
  VkImageView vkView = VK_NULL_HANDLE;
  VK_ASSERT(vkCreateImageView(vkDevice_, &ci, nullptr, &vkView));
  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vkView, debugName));

  return vkView;
}

void lvk::VulkanImage::transitionLayout(VkCommandBuffer commandBuffer,
                                        VkImageLayout newImageLayout,
                                        VkPipelineStageFlags srcStageMask,
                                        VkPipelineStageFlags dstStageMask,
                                        const VkImageSubresourceRange& subresourceRange) const {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_TRANSITION);

  VkAccessFlags srcAccessMask = 0;
  VkAccessFlags dstAccessMask = 0;

  if (vkImageLayout_ == VK_IMAGE_LAYOUT_UNDEFINED) {
    // we do not need to wait for any previous operations in this case
    srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  }

  switch (srcStageMask) {
  case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:
  case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:
  case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
  case VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT:
  case VK_PIPELINE_STAGE_ALL_COMMANDS_BIT:
  case VK_PIPELINE_STAGE_TRANSFER_BIT:
  case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
    break;
  default:
    LVK_ASSERT_MSG(false, "Automatic access mask deduction is not implemented (yet) for this srcStageMask");
    break;
  }

  // once you want to add a new pipeline stage to this block of if's, don't forget to add it to the
  // switch() statement above
  if (srcStageMask & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) {
    srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) {
    srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_TRANSFER_BIT) {
    srcAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
  }
  if (srcStageMask & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
    srcAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
  }

  switch (dstStageMask) {
  case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
  case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:
  case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
  case VK_PIPELINE_STAGE_TRANSFER_BIT:
  case VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
  case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT:
    break;
  default:
    LVK_ASSERT_MSG(false, "Automatic access mask deduction is not implemented (yet) for this dstStageMask");
    break;
  }

  // once you want to add a new pipeline stage to this block of if's, don't forget to add it to the
  // switch() statement above
  if (dstStageMask & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
    dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    dstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT) {
    dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) {
    dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
    dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
  }
  if (dstStageMask & VK_PIPELINE_STAGE_TRANSFER_BIT) {
    dstAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
  }

  lvk::imageMemoryBarrier(
      commandBuffer, vkImage_, srcAccessMask, dstAccessMask, vkImageLayout_, newImageLayout, srcStageMask, dstStageMask, subresourceRange);

  vkImageLayout_ = newImageLayout;
}

VkImageAspectFlags lvk::VulkanImage::getImageAspectFlags() const {
  VkImageAspectFlags flags = 0;

  flags |= isDepthFormat_ ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
  flags |= isStencilFormat_ ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
  flags |= !(isDepthFormat_ || isStencilFormat_) ? VK_IMAGE_ASPECT_COLOR_BIT : 0;

  return flags;
}

void lvk::VulkanImage::generateMipmap(VkCommandBuffer commandBuffer) const {
  LVK_PROFILER_FUNCTION();

  // Check if device supports downscaling for color or depth/stencil buffer based on image format
  {
    const uint32_t formatFeatureMask = (VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT);

    const bool hardwareDownscalingSupported = (vkFormatProperties_.optimalTilingFeatures & formatFeatureMask) == formatFeatureMask;

    if (!LVK_VERIFY(hardwareDownscalingSupported)) {
      LVK_ASSERT_MSG(false, "Doesn't support hardware downscaling of this image format: {}");
      return;
    }
  }

  // Choose linear filter for color formats if supported by the device, else use nearest filter
  // Choose nearest filter by default for depth/stencil formats
  const VkFilter blitFilter = [](bool isDepthOrStencilFormat, bool imageFilterLinear) {
    if (isDepthOrStencilFormat) {
      return VK_FILTER_NEAREST;
    }
    if (imageFilterLinear) {
      return VK_FILTER_LINEAR;
    }
    return VK_FILTER_NEAREST;
  }(isDepthFormat_ || isStencilFormat_, vkFormatProperties_.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

  const VkImageAspectFlags imageAspectFlags = getImageAspectFlags();

  const VkDebugUtilsLabelEXT utilsLabel = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pLabelName = "Generate mipmaps",
      .color = {1.0f, 0.75f, 1.0f, 1.0f},
  };
  vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &utilsLabel);

  const VkImageLayout originalImageLayout = vkImageLayout_;

  LVK_ASSERT(originalImageLayout != VK_IMAGE_LAYOUT_UNDEFINED);

  // 0: Transition the first level and all layers into VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  transitionLayout(commandBuffer,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                   VkImageSubresourceRange{imageAspectFlags, 0, 1, 0, numLayers_});

  for (uint32_t layer = 0; layer < numLayers_; ++layer) {
    int32_t mipWidth = (int32_t)vkExtent_.width;
    int32_t mipHeight = (int32_t)vkExtent_.height;

    for (uint32_t i = 1; i < numLevels_; ++i) {
      // 1: Transition the i-th level to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; it will be copied into from the (i-1)-th layer
      lvk::imageMemoryBarrier(commandBuffer,
                              vkImage_,
                              0, /* srcAccessMask */
                              VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask
                              VK_IMAGE_LAYOUT_UNDEFINED, // oldImageLayout
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // newImageLayout
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // srcStageMask
                              VK_PIPELINE_STAGE_TRANSFER_BIT, // dstStageMask
                              VkImageSubresourceRange{imageAspectFlags, i, 1, layer, 1});

      const int32_t nextLevelWidth = mipWidth > 1 ? mipWidth / 2 : 1;
      const int32_t nextLevelHeight = mipHeight > 1 ? mipHeight / 2 : 1;

      const VkOffset3D srcOffsets[2] = {
          VkOffset3D{0, 0, 0},
          VkOffset3D{mipWidth, mipHeight, 1},
      };
      const VkOffset3D dstOffsets[2] = {
          VkOffset3D{0, 0, 0},
          VkOffset3D{nextLevelWidth, nextLevelHeight, 1},
      };

      // 2: Blit the image from the prev mip-level (i-1) (VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) to the current mip-level (i)
      // (VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
#if LVK_VULKAN_PRINT_COMMANDS
      LLOGL("%p vkCmdBlitImage()\n", commandBuffer);
#endif // LVK_VULKAN_PRINT_COMMANDS
      const VkImageBlit blit = {
          .srcSubresource = VkImageSubresourceLayers{imageAspectFlags, i - 1, layer, 1},
          .srcOffsets = {srcOffsets[0], srcOffsets[1]},
          .dstSubresource = VkImageSubresourceLayers{imageAspectFlags, i, layer, 1},
          .dstOffsets = {dstOffsets[0], dstOffsets[1]},
      };
      vkCmdBlitImage(commandBuffer,
                     vkImage_,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     vkImage_,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     1,
                     &blit,
                     blitFilter);
      // 3: Transition i-th level to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL as it will be read from in
      // the next iteration
      lvk::imageMemoryBarrier(commandBuffer,
                              vkImage_,
                              VK_ACCESS_TRANSFER_WRITE_BIT, /* srcAccessMask */
                              VK_ACCESS_TRANSFER_READ_BIT, /* dstAccessMask */
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, /* oldImageLayout */
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, /* newImageLayout */
                              VK_PIPELINE_STAGE_TRANSFER_BIT, /* srcStageMask */
                              VK_PIPELINE_STAGE_TRANSFER_BIT /* dstStageMask */,
                              VkImageSubresourceRange{imageAspectFlags, i, 1, layer, 1});

      // Compute the size of the next mip level
      mipWidth = nextLevelWidth;
      mipHeight = nextLevelHeight;
    }
  }

  // 4: Transition all levels and layers (faces) to their final layout
  lvk::imageMemoryBarrier(commandBuffer,
                          vkImage_,
                          VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask
                          0, // dstAccessMask
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // oldImageLayout
                          originalImageLayout, // newImageLayout
                          VK_PIPELINE_STAGE_TRANSFER_BIT, // srcStageMask
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // dstStageMask
                          VkImageSubresourceRange{imageAspectFlags, 0, numLevels_, 0, numLayers_});
  vkCmdEndDebugUtilsLabelEXT(commandBuffer);

  vkImageLayout_ = originalImageLayout;
}

bool lvk::VulkanImage::isDepthFormat(VkFormat format) {
  return (format == VK_FORMAT_D16_UNORM) || (format == VK_FORMAT_X8_D24_UNORM_PACK32) || (format == VK_FORMAT_D32_SFLOAT) ||
         (format == VK_FORMAT_D16_UNORM_S8_UINT) || (format == VK_FORMAT_D24_UNORM_S8_UINT) || (format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

bool lvk::VulkanImage::isStencilFormat(VkFormat format) {
  return (format == VK_FORMAT_S8_UINT) || (format == VK_FORMAT_D16_UNORM_S8_UINT) || (format == VK_FORMAT_D24_UNORM_S8_UINT) ||
         (format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

lvk::VulkanTexture::VulkanTexture(std::shared_ptr<VulkanImage> image, VkImageView imageView) :
  image_(std::move(image)), imageView_(imageView) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  LVK_ASSERT(image_.get());
  LVK_ASSERT(imageView_ != VK_NULL_HANDLE);
}

lvk::VulkanTexture::~VulkanTexture() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  if (image_) {
    image_->ctx_.deferredTask(std::packaged_task<void()>(
        [device = image_->ctx_.getVkDevice(), imageView = imageView_]() { vkDestroyImageView(device, imageView, nullptr); }));
    for (VkImageView v : imageViewForFramebuffer_) {
      if (v != VK_NULL_HANDLE) {
        image_->ctx_.deferredTask(std::packaged_task<void()>(
            [device = image_->ctx_.getVkDevice(), imageView = v]() { vkDestroyImageView(device, imageView, nullptr); }));
      }
    }
  }
}

lvk::VulkanTexture::VulkanTexture(VulkanTexture&& other) {
  std::swap(image_, other.image_);
  std::swap(imageView_, other.imageView_);
  for (size_t i = 0; i != LVK_MAX_MIP_LEVELS; i++) {
    std::swap(imageViewForFramebuffer_[i], other.imageViewForFramebuffer_[i]);
  }
}

lvk::VulkanTexture& lvk::VulkanTexture::operator=(VulkanTexture&& other) {
  std::swap(image_, other.image_);
  std::swap(imageView_, other.imageView_);
  for (size_t i = 0; i != LVK_MAX_MIP_LEVELS; i++) {
    std::swap(imageViewForFramebuffer_[i], other.imageViewForFramebuffer_[i]);
  }
  return *this;
}

VkImageView lvk::VulkanTexture::getOrCreateVkImageViewForFramebuffer(uint8_t level) {
  LVK_ASSERT(image_ != nullptr);
  LVK_ASSERT(level < LVK_MAX_MIP_LEVELS);

  if (!image_ || level >= LVK_MAX_MIP_LEVELS) {
    return VK_NULL_HANDLE;
  }

  if (imageViewForFramebuffer_[level] != VK_NULL_HANDLE) {
    return imageViewForFramebuffer_[level];
  }

  imageViewForFramebuffer_[level] =
      image_->createImageView(VK_IMAGE_VIEW_TYPE_2D, image_->vkImageFormat_, image_->getImageAspectFlags(), level, 1u, 0u, 1u);

  return imageViewForFramebuffer_[level];
}

lvk::VulkanSwapchain::VulkanSwapchain(VulkanContext& ctx, uint32_t width, uint32_t height) :
  ctx_(ctx), device_(ctx.vkDevice_), graphicsQueue_(ctx.deviceQueues_.graphicsQueue), width_(width), height_(height) {
  surfaceFormat_ = chooseSwapSurfaceFormat(ctx.deviceSurfaceFormats_, ctx.config_.swapChainColorSpace);

  acquireSemaphore_ = lvk::createSemaphore(device_, "Semaphore: swapchain-acquire");

  LVK_ASSERT_MSG(ctx.vkSurface_ != VK_NULL_HANDLE,
                 "You are trying to create a swapchain but your OS surface is empty. Did you want to "
                 "create an offscreen rendering context? If so, set 'width' and 'height' to 0 when you "
                 "create your lvk::IContext");

  VkBool32 queueFamilySupportsPresentation = VK_FALSE;
  VK_ASSERT(vkGetPhysicalDeviceSurfaceSupportKHR(
      ctx.getVkPhysicalDevice(), ctx.deviceQueues_.graphicsQueueFamilyIndex, ctx.vkSurface_, &queueFamilySupportsPresentation));
  LVK_ASSERT_MSG(queueFamilySupportsPresentation == VK_TRUE, "The queue family used with the swapchain does not support presentation");

  auto chooseSwapImageCount = [](const VkSurfaceCapabilitiesKHR& caps) -> uint32_t {
    const uint32_t desired = caps.minImageCount + 1;
    const bool exceeded = caps.maxImageCount > 0 && desired > caps.maxImageCount;
    return exceeded ? caps.maxImageCount : desired;
  };

  auto chooseSwapPresentMode = [](const std::vector<VkPresentModeKHR>& modes) -> VkPresentModeKHR {
#if defined(__linux__)
    if (std::find(modes.cbegin(), modes.cend(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.cend()) {
      return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
#endif // __linux__
    if (std::find(modes.cbegin(), modes.cend(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.cend()) {
      return VK_PRESENT_MODE_MAILBOX_KHR;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
  };

  auto chooseUsageFlags = [](VkPhysicalDevice pd, VkSurfaceKHR surface, VkFormat format) -> VkImageUsageFlags {
    VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkSurfaceCapabilitiesKHR caps = {};
    VK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps));

    VkFormatProperties props = {};
    vkGetPhysicalDeviceFormatProperties(pd, format, &props);

    const bool isStorageSupported = (caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) > 0;
    const bool isTilingOptimalSupported = (props.optimalTilingFeatures & VK_IMAGE_USAGE_STORAGE_BIT) > 0;

    if (isStorageSupported && isTilingOptimalSupported) {
      usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    return usageFlags;
  };

  const VkImageUsageFlags usageFlags = chooseUsageFlags(ctx.getVkPhysicalDevice(), ctx.vkSurface_, surfaceFormat_.format);
  const bool isCompositeAlphaOpaqueSupported = (ctx.deviceSurfaceCaps_.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0;
  const VkSwapchainCreateInfoKHR ci = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = ctx.vkSurface_,
      .minImageCount = chooseSwapImageCount(ctx.deviceSurfaceCaps_),
      .imageFormat = surfaceFormat_.format,
      .imageColorSpace = surfaceFormat_.colorSpace,
      .imageExtent = {.width = width, .height = height},
      .imageArrayLayers = 1,
      .imageUsage = usageFlags,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &ctx.deviceQueues_.graphicsQueueFamilyIndex,
      .preTransform = ctx.deviceSurfaceCaps_.currentTransform,
      .compositeAlpha = isCompositeAlphaOpaqueSupported ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
      .presentMode = chooseSwapPresentMode(ctx.devicePresentModes_),
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };
  VK_ASSERT(vkCreateSwapchainKHR(device_, &ci, nullptr, &swapchain_));

  VkImage swapchainImages[LVK_MAX_SWAPCHAIN_IMAGES];
  VK_ASSERT(vkGetSwapchainImagesKHR(device_, swapchain_, &numSwapchainImages_, nullptr));
  if (numSwapchainImages_ > LVK_MAX_SWAPCHAIN_IMAGES) {
    LVK_ASSERT(numSwapchainImages_ <= LVK_MAX_SWAPCHAIN_IMAGES);
    numSwapchainImages_ = LVK_MAX_SWAPCHAIN_IMAGES;
  }
  VK_ASSERT(vkGetSwapchainImagesKHR(device_, swapchain_, &numSwapchainImages_, swapchainImages));

  LVK_ASSERT(numSwapchainImages_ > 0);

  char debugNameImage[256] = {0};
  char debugNameImageView[256] = {0};

  // create images, image views and framebuffers
  for (uint32_t i = 0; i < numSwapchainImages_; i++) {
    snprintf(debugNameImage, sizeof(debugNameImage) - 1, "Image: swapchain %u", i);
    snprintf(debugNameImageView, sizeof(debugNameImageView) - 1, "Image View: swapchain %u", i);
    std::shared_ptr<VulkanImage> image = std::make_shared<VulkanImage>(ctx_,
                                                                       device_,
                                                                       swapchainImages[i],
                                                                       usageFlags,
                                                                       surfaceFormat_.format,
                                                                       VkExtent3D{.width = width_, .height = height_, .depth = 1},
                                                                       debugNameImage);
    VkImageView imageView = image->createImageView(
        VK_IMAGE_VIEW_TYPE_2D, surfaceFormat_.format, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1, debugNameImageView);
    swapchainTextures_[i] = ctx_.texturesPool_.create(VulkanTexture(std::move(image), std::move(imageView)));
  }
}

lvk::VulkanSwapchain::~VulkanSwapchain() {
  for (TextureHandle handle : swapchainTextures_) {
    ctx_.texturesPool_.destroy(handle);
  }
  vkDestroySwapchainKHR(device_, swapchain_, nullptr);
  vkDestroySemaphore(device_, acquireSemaphore_, nullptr);
}

VkImage lvk::VulkanSwapchain::getCurrentVkImage() const {
  if (LVK_VERIFY(currentImageIndex_ < numSwapchainImages_)) {
    lvk::VulkanTexture* tex = ctx_.texturesPool_.get(swapchainTextures_[currentImageIndex_]);
    return tex->image_->vkImage_;
  }
  return VK_NULL_HANDLE;
}

VkImageView lvk::VulkanSwapchain::getCurrentVkImageView() const {
  if (LVK_VERIFY(currentImageIndex_ < numSwapchainImages_)) {
    lvk::VulkanTexture* tex = ctx_.texturesPool_.get(swapchainTextures_[currentImageIndex_]);
    return tex->imageView_;
  }
  return VK_NULL_HANDLE;
}

lvk::TextureHandle lvk::VulkanSwapchain::getCurrentTexture() {
  LVK_PROFILER_FUNCTION();

  if (getNextImage_) {
    // when timeout is set to UINT64_MAX, we wait until the next image has been acquired
    VkResult r = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, acquireSemaphore_, VK_NULL_HANDLE, &currentImageIndex_);
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
      VK_ASSERT(r);
    }
    getNextImage_ = false;
  }

  if (LVK_VERIFY(currentImageIndex_ < numSwapchainImages_)) {
    return swapchainTextures_[currentImageIndex_];
  }

  return {};
}

const VkSurfaceFormatKHR& lvk::VulkanSwapchain::getSurfaceFormat() const {
  return surfaceFormat_;
}

uint32_t lvk::VulkanSwapchain::getNumSwapchainImages() const {
  return numSwapchainImages_;
}

lvk::Result lvk::VulkanSwapchain::present(VkSemaphore waitSemaphore) {
  LVK_PROFILER_FUNCTION();

  LVK_PROFILER_ZONE("vkQueuePresent()", LVK_PROFILER_COLOR_PRESENT);
  const VkPresentInfoKHR pi = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &waitSemaphore,
      .swapchainCount = 1u,
      .pSwapchains = &swapchain_,
      .pImageIndices = &currentImageIndex_,
  };
  VkResult r = vkQueuePresentKHR(graphicsQueue_, &pi);
  if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
    VK_ASSERT(r);
  }
  LVK_PROFILER_ZONE_END();

  // Ready to call acquireNextImage() on the next getCurrentVulkanTexture();
  getNextImage_ = true;

  LVK_PROFILER_FRAME(nullptr);

  return Result();
}

lvk::VulkanImmediateCommands::VulkanImmediateCommands(VkDevice device, uint32_t queueFamilyIndex, const char* debugName) :
  device_(device), queueFamilyIndex_(queueFamilyIndex), debugName_(debugName) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue_);

  const VkCommandPoolCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = queueFamilyIndex,
  };
  VK_ASSERT(vkCreateCommandPool(device, &ci, nullptr, &commandPool_));
  lvk::setDebugObjectName(device, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)commandPool_, debugName);

  const VkCommandBufferAllocateInfo ai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  for (uint32_t i = 0; i != kMaxCommandBuffers; i++) {
    auto& buf = buffers_[i];
    char fenceName[256] = {0};
    char semaphoreName[256] = {0};
    if (debugName) {
      snprintf(fenceName, sizeof(fenceName) - 1, "Fence: %s (cmdbuf %u)", debugName, i);
      snprintf(semaphoreName, sizeof(semaphoreName) - 1, "Semaphore: %s (cmdbuf %u)", debugName, i);
    }
    buf.semaphore_ = lvk::createSemaphore(device, semaphoreName);
    buf.fence_ = lvk::createFence(device, fenceName);
    VK_ASSERT(vkAllocateCommandBuffers(device, &ai, &buf.cmdBufAllocated_));
    buffers_[i].handle_.bufferIndex_ = i;
  }
}

lvk::VulkanImmediateCommands::~VulkanImmediateCommands() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  waitAll();

  for (auto& buf : buffers_) {
    // lifetimes of all VkFence objects are managed explicitly we do not use deferredTask() for them
    vkDestroyFence(device_, buf.fence_, nullptr);
    vkDestroySemaphore(device_, buf.semaphore_, nullptr);
  }

  vkDestroyCommandPool(device_, commandPool_, nullptr);
}

void lvk::VulkanImmediateCommands::purge() {
  LVK_PROFILER_FUNCTION();

  for (CommandBufferWrapper& buf : buffers_) {
    if (buf.cmdBuf_ == VK_NULL_HANDLE || buf.isEncoding_) {
      continue;
    }

    const VkResult result = vkWaitForFences(device_, 1, &buf.fence_, VK_TRUE, 0);

    if (result == VK_SUCCESS) {
      VK_ASSERT(vkResetCommandBuffer(buf.cmdBuf_, VkCommandBufferResetFlags{0}));
      VK_ASSERT(vkResetFences(device_, 1, &buf.fence_));
      buf.cmdBuf_ = VK_NULL_HANDLE;
      numAvailableCommandBuffers_++;
    } else {
      if (result != VK_TIMEOUT) {
        VK_ASSERT(result);
      }
    }
  }
}

const lvk::VulkanImmediateCommands::CommandBufferWrapper& lvk::VulkanImmediateCommands::acquire() {
  LVK_PROFILER_FUNCTION();

  if (!numAvailableCommandBuffers_) {
    purge();
  }

  while (!numAvailableCommandBuffers_) {
    LLOGL("Waiting for command buffers...\n");
    LVK_PROFILER_ZONE("Waiting for command buffers...", LVK_PROFILER_COLOR_WAIT);
    purge();
    LVK_PROFILER_ZONE_END();
  }

  VulkanImmediateCommands::CommandBufferWrapper* current = nullptr;

  // we are ok with any available buffer
  for (auto& buf : buffers_) {
    if (buf.cmdBuf_ == VK_NULL_HANDLE) {
      current = &buf;
      break;
    }
  }

  // make clang happy
  assert(current);

  LVK_ASSERT_MSG(numAvailableCommandBuffers_, "No available command buffers");
  LVK_ASSERT_MSG(current, "No available command buffers");
  LVK_ASSERT(current->cmdBufAllocated_ != VK_NULL_HANDLE);

  current->handle_.submitId_ = submitCounter_;
  numAvailableCommandBuffers_--;

  current->cmdBuf_ = current->cmdBufAllocated_;
  current->isEncoding_ = true;
  const VkCommandBufferBeginInfo bi = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  VK_ASSERT(vkBeginCommandBuffer(current->cmdBuf_, &bi));

  return *current;
}

void lvk::VulkanImmediateCommands::wait(const SubmitHandle handle) {
  if (isReady(handle)) {
    return;
  }

  if (!LVK_VERIFY(!buffers_[handle.bufferIndex_].isEncoding_)) {
    // we are waiting for a buffer which has not been submitted - this is probably a logic error somewhere in the calling code
    return;
  }

  VK_ASSERT(vkWaitForFences(device_, 1, &buffers_[handle.bufferIndex_].fence_, VK_TRUE, UINT64_MAX));

  purge();
}

void lvk::VulkanImmediateCommands::waitAll() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_WAIT);

  VkFence fences[kMaxCommandBuffers];

  uint32_t numFences = 0;

  for (const auto& buf : buffers_) {
    if (buf.cmdBuf_ != VK_NULL_HANDLE && !buf.isEncoding_) {
      fences[numFences++] = buf.fence_;
    }
  }

  if (numFences) {
    VK_ASSERT(vkWaitForFences(device_, numFences, fences, VK_TRUE, UINT64_MAX));
  }

  purge();
}

bool lvk::VulkanImmediateCommands::isReady(const SubmitHandle handle, bool fastCheckNoVulkan) const {
  LVK_ASSERT(handle.bufferIndex_ < kMaxCommandBuffers);

  if (handle.empty()) {
    // a null handle
    return true;
  }

  const CommandBufferWrapper& buf = buffers_[handle.bufferIndex_];

  if (buf.cmdBuf_ == VK_NULL_HANDLE) {
    // already recycled and not yet reused
    return true;
  }

  if (buf.handle_.submitId_ != handle.submitId_) {
    // already recycled and reused by another command buffer
    return true;
  }

  if (fastCheckNoVulkan) {
    // do not ask the Vulkan API about it, just let it retire naturally (when submitId for this bufferIndex gets incremented)
    return false;
  }

  return vkWaitForFences(device_, 1, &buf.fence_, VK_TRUE, 0) == VK_SUCCESS;
}

lvk::SubmitHandle lvk::VulkanImmediateCommands::submit(const CommandBufferWrapper& wrapper) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_SUBMIT);
  LVK_ASSERT(wrapper.isEncoding_);
  VK_ASSERT(vkEndCommandBuffer(wrapper.cmdBuf_));

  const VkPipelineStageFlags waitStageMasks[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
  VkSemaphore waitSemaphores[] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
  uint32_t numWaitSemaphores = 0;
  if (waitSemaphore_) {
    waitSemaphores[numWaitSemaphores++] = waitSemaphore_;
  }
  if (lastSubmitSemaphore_) {
    waitSemaphores[numWaitSemaphores++] = lastSubmitSemaphore_;
  }

  LVK_PROFILER_ZONE("vkQueueSubmit()", LVK_PROFILER_COLOR_SUBMIT);
#if LVK_VULKAN_PRINT_COMMANDS
  LLOGL("%p vkQueueSubmit()\n\n", wrapper.cmdBuf_);
#endif // LVK_VULKAN_PRINT_COMMANDS
  const VkSubmitInfo si = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = numWaitSemaphores,
      .pWaitSemaphores = waitSemaphores,
      .pWaitDstStageMask = waitStageMasks,
      .commandBufferCount = 1u,
      .pCommandBuffers = &wrapper.cmdBuf_,
      .signalSemaphoreCount = 1u,
      .pSignalSemaphores = &wrapper.semaphore_,
  };
  VK_ASSERT(vkQueueSubmit(queue_, 1u, &si, wrapper.fence_));
  LVK_PROFILER_ZONE_END();

  lastSubmitSemaphore_ = wrapper.semaphore_;
  lastSubmitHandle_ = wrapper.handle_;
  waitSemaphore_ = VK_NULL_HANDLE;

  // reset
  const_cast<CommandBufferWrapper&>(wrapper).isEncoding_ = false;
  submitCounter_++;

  if (!submitCounter_) {
    // skip the 0 value - when uint32_t wraps around (null SubmitHandle)
    submitCounter_++;
  }

  return lastSubmitHandle_;
}

void lvk::VulkanImmediateCommands::waitSemaphore(VkSemaphore semaphore) {
  LVK_ASSERT(waitSemaphore_ == VK_NULL_HANDLE);

  waitSemaphore_ = semaphore;
}

VkSemaphore lvk::VulkanImmediateCommands::acquireLastSubmitSemaphore() {
  return std::exchange(lastSubmitSemaphore_, VK_NULL_HANDLE);
}

lvk::SubmitHandle lvk::VulkanImmediateCommands::getLastSubmitHandle() const {
  return lastSubmitHandle_;
}

void lvk::RenderPipelineState::destroyPipelines(lvk::VulkanContext* ctx) {
#ifndef __APPLE__
  for (uint32_t topology = 0; topology != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST + 1; topology++) {
    for (uint32_t depthBiasEnabled = 0; depthBiasEnabled != VK_TRUE + 1; depthBiasEnabled++) {
      VkPipeline& vkPipeline = pipelines_[topology][depthBiasEnabled];
      if (vkPipeline != VK_NULL_HANDLE) {
        ctx->deferredTask(std::packaged_task<void()>(
            [device = ctx->getVkDevice(), pipeline = vkPipeline]() { vkDestroyPipeline(device, pipeline, nullptr); }));
        vkPipeline = VK_NULL_HANDLE;
      }
    }
  }
#else
  for (uint32_t topology = 0; topology != VK_PRIMITIVE_TOPOLOGY_PATCH_LIST + 1; topology++) {
    for (uint32_t depthCompareOp = 0; depthCompareOp != VK_COMPARE_OP_ALWAYS + 1; depthCompareOp++) {
      for (uint32_t depthWriteEnabled = 0; depthWriteEnabled != VK_TRUE + 1; depthWriteEnabled++) {
        for (uint32_t depthBiasEnabled = 0; depthBiasEnabled != VK_TRUE + 1; depthBiasEnabled++) {
          VkPipeline& vkPipeline = pipelines_[topology][depthCompareOp][depthWriteEnabled][depthBiasEnabled];
          if (vkPipeline != VK_NULL_HANDLE) {
            ctx->deferredTask(std::packaged_task<void()>(
                [device = ctx->getVkDevice(), pipeline = vkPipeline]() { vkDestroyPipeline(device, pipeline, nullptr); }));
            vkPipeline = VK_NULL_HANDLE;
          }
        }
      }
    }
  }
#endif
}

lvk::VulkanPipelineBuilder::VulkanPipelineBuilder() :
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

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::depthBiasEnable(bool enable) {
  rasterizationState_.depthBiasEnable = enable ? VK_TRUE : VK_FALSE;
  return *this;
}

#if defined(__APPLE__)
lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::depthWriteEnable(bool enable) {
  depthStencilState_.depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
  return *this;
}

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::depthCompareOp(VkCompareOp compareOp) {
  depthStencilState_.depthTestEnable = compareOp != VK_COMPARE_OP_ALWAYS;
  depthStencilState_.depthCompareOp = compareOp;
  return *this;
}
#endif

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::dynamicState(VkDynamicState state) {
  LVK_ASSERT(numDynamicStates_ < LVK_MAX_DYNAMIC_STATES);
  dynamicStates_[numDynamicStates_++] = state;
  return *this;
}

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::primitiveTopology(VkPrimitiveTopology topology) {
  inputAssembly_.topology = topology;
  return *this;
}

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::rasterizationSamples(VkSampleCountFlagBits samples) {
  multisampleState_.rasterizationSamples = samples;
  return *this;
}

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::cullMode(VkCullModeFlags mode) {
  rasterizationState_.cullMode = mode;
  return *this;
}

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::frontFace(VkFrontFace mode) {
  rasterizationState_.frontFace = mode;
  return *this;
}

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::polygonMode(VkPolygonMode mode) {
  rasterizationState_.polygonMode = mode;
  return *this;
}

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::vertexInputState(const VkPipelineVertexInputStateCreateInfo& state) {
  vertexInputState_ = state;
  return *this;
}

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::colorAttachments(const VkPipelineColorBlendAttachmentState* states,
                                                                         const VkFormat* formats,
                                                                         uint32_t numColorAttachments) {
  LVK_ASSERT(states);
  LVK_ASSERT(formats);
  LVK_ASSERT(numColorAttachments <= LVK_ARRAY_NUM_ELEMENTS(colorBlendAttachmentStates_));
  LVK_ASSERT(numColorAttachments <= LVK_ARRAY_NUM_ELEMENTS(colorAttachmentFormats_));
  for (uint32_t i = 0; i != numColorAttachments; i++) {
    colorBlendAttachmentStates_[i] = states[i];
    colorAttachmentFormats_[i] = formats[i];
  }
  numColorAttachments_ = numColorAttachments;
  return *this;
}

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::depthAttachmentFormat(VkFormat format) {
  depthAttachmentFormat_ = format;
  return *this;
}

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::stencilAttachmentFormat(VkFormat format) {
  stencilAttachmentFormat_ = format;
  return *this;
}

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::shaderStage(VkPipelineShaderStageCreateInfo stage) {
  if (stage.module != VK_NULL_HANDLE) {
    LVK_ASSERT(numShaderStages_ < LVK_ARRAY_NUM_ELEMENTS(shaderStages_));
    shaderStages_[numShaderStages_++] = stage;
  }
  return *this;
}

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::stencilStateOps(VkStencilFaceFlags faceMask,
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

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::stencilMasks(VkStencilFaceFlags faceMask,
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

VkResult lvk::VulkanPipelineBuilder::build(VkDevice device,
                                           VkPipelineCache pipelineCache,
                                           VkPipelineLayout pipelineLayout,
                                           VkPipeline* outPipeline,
                                           const char* debugName) noexcept {
  const VkPipelineDynamicStateCreateInfo dynamicState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = numDynamicStates_,
      .pDynamicStates = dynamicStates_,
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
      .attachmentCount = numColorAttachments_,
      .pAttachments = colorBlendAttachmentStates_,
  };
  const VkPipelineRenderingCreateInfo renderingInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
      .pNext = nullptr,
      .colorAttachmentCount = numColorAttachments_,
      .pColorAttachmentFormats = colorAttachmentFormats_,
      .depthAttachmentFormat = depthAttachmentFormat_,
      .stencilAttachmentFormat = stencilAttachmentFormat_,
  };

  const VkGraphicsPipelineCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &renderingInfo,
      .flags = 0,
      .stageCount = numShaderStages_,
      .pStages = shaderStages_,
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

lvk::CommandBuffer::CommandBuffer(VulkanContext* ctx) : ctx_(ctx), wrapper_(&ctx_->immediate_->acquire()) {}

lvk::CommandBuffer::~CommandBuffer() {
  // did you forget to call cmdEndRendering()?
  LVK_ASSERT(!isRendering_);
}

void lvk::CommandBuffer::transitionToShaderReadOnly(TextureHandle handle) const {
  LVK_PROFILER_FUNCTION();

  const lvk::VulkanTexture& tex = *ctx_->texturesPool_.get(handle);
  const lvk::VulkanImage* img = tex.image_.get();

  LVK_ASSERT(!img->isSwapchainImage_);

  // transition only non-multisampled images - MSAA images cannot be accessed from shaders
  if (img->vkSamples_ == VK_SAMPLE_COUNT_1_BIT) {
    const VkImageAspectFlags flags = tex.image_->getImageAspectFlags();
    const VkPipelineStageFlags srcStage = isDepthOrStencilVkFormat(tex.image_->vkImageFormat_)
                                              ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                                              : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // set the result of the previous render pass
    img->transitionLayout(wrapper_->cmdBuf_,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          srcStage,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // wait for subsequent
                                                                                                        // fragment/compute shaders
                          VkImageSubresourceRange{flags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }
}

void lvk::CommandBuffer::cmdBindComputePipeline(lvk::ComputePipelineHandle handle) {
  LVK_PROFILER_FUNCTION();

  if (!LVK_VERIFY(!handle.empty())) {
    return;
  }

  VkPipeline pipeline = ctx_->getVkPipeline(handle);

  LVK_ASSERT(pipeline != VK_NULL_HANDLE);

  if (lastPipelineBound_ != pipeline) {
    lastPipelineBound_ = pipeline;
    if (pipeline != VK_NULL_HANDLE) {
      vkCmdBindPipeline(wrapper_->cmdBuf_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    }
  }
}

void lvk::CommandBuffer::cmdDispatchThreadGroups(const Dimensions& threadgroupCount, const Dependencies& deps) {
  LVK_ASSERT(!isRendering_);

  for (uint32_t i = 0; i != Dependencies::LVK_MAX_SUBMIT_DEPENDENCIES && deps.textures[i]; i++) {
    useComputeTexture(deps.textures[i]);
  }

  ctx_->checkAndUpdateDescriptorSets();
  ctx_->bindDefaultDescriptorSets(wrapper_->cmdBuf_, VK_PIPELINE_BIND_POINT_COMPUTE);

  vkCmdDispatch(wrapper_->cmdBuf_, threadgroupCount.width, threadgroupCount.height, threadgroupCount.depth);
}

void lvk::CommandBuffer::cmdPushDebugGroupLabel(const char* label, uint32_t colorRGBA) const {
  LVK_ASSERT(label);

  if (!label) {
    return;
  }
  const VkDebugUtilsLabelEXT utilsLabel = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = nullptr,
      .pLabelName = label,
      .color = {float((colorRGBA >> 0) & 0xff) / 255.0f,
                float((colorRGBA >> 8) & 0xff) / 255.0f,
                float((colorRGBA >> 16) & 0xff) / 255.0f,
                float((colorRGBA >> 24) & 0xff) / 255.0f},
  };
  vkCmdBeginDebugUtilsLabelEXT(wrapper_->cmdBuf_, &utilsLabel);
}

void lvk::CommandBuffer::cmdInsertDebugEventLabel(const char* label, uint32_t colorRGBA) const {
  LVK_ASSERT(label);

  if (!label) {
    return;
  }
  const VkDebugUtilsLabelEXT utilsLabel = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = nullptr,
      .pLabelName = label,
      .color = {float((colorRGBA >> 0) & 0xff) / 255.0f,
                float((colorRGBA >> 8) & 0xff) / 255.0f,
                float((colorRGBA >> 16) & 0xff) / 255.0f,
                float((colorRGBA >> 24) & 0xff) / 255.0f},
  };
  vkCmdInsertDebugUtilsLabelEXT(wrapper_->cmdBuf_, &utilsLabel);
}

void lvk::CommandBuffer::cmdPopDebugGroupLabel() const {
  vkCmdEndDebugUtilsLabelEXT(wrapper_->cmdBuf_);
}

void lvk::CommandBuffer::useComputeTexture(TextureHandle handle) {
  LVK_PROFILER_FUNCTION();

  LVK_ASSERT(!handle.empty());
  lvk::VulkanTexture* tex = ctx_->texturesPool_.get(handle);
  const lvk::VulkanImage& vkImage = *tex->image_.get();
  if (!vkImage.isStorageImage()) {
    LVK_ASSERT_MSG(false, "Did you forget to specify TextureUsageBits::Storage on your texture?");
    return;
  }

  // "frame graph" heuristics: if we are already in VK_IMAGE_LAYOUT_GENERAL, wait for the previous
  // compute shader
  const VkPipelineStageFlags srcStage = (vkImage.vkImageLayout_ == VK_IMAGE_LAYOUT_GENERAL) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                                                                            : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  vkImage.transitionLayout(
      wrapper_->cmdBuf_,
      VK_IMAGE_LAYOUT_GENERAL,
      srcStage,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VkImageSubresourceRange{vkImage.getImageAspectFlags(), 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
}

void lvk::CommandBuffer::cmdBeginRendering(const lvk::RenderPass& renderPass, const lvk::Framebuffer& fb) {
  LVK_PROFILER_FUNCTION();

  LVK_ASSERT(!isRendering_);

  isRendering_ = true;

  const uint32_t numFbColorAttachments = fb.getNumColorAttachments();
  const uint32_t numPassColorAttachments = renderPass.getNumColorAttachments();

  LVK_ASSERT(numPassColorAttachments == numFbColorAttachments);

  framebuffer_ = fb;

  // transition all the color attachments
  for (uint32_t i = 0; i != numFbColorAttachments; i++) {
    if (const auto handle = fb.color[i].texture) {
      lvk::VulkanTexture* colorTex = ctx_->texturesPool_.get(handle);
      transitionColorAttachment(wrapper_->cmdBuf_, colorTex);
    }
    // handle MSAA
    if (TextureHandle handle = fb.color[i].resolveTexture) {
      lvk::VulkanTexture* colorResolveTex = ctx_->texturesPool_.get(handle);
      transitionColorAttachment(wrapper_->cmdBuf_, colorResolveTex);
    }
  }
  // transition depth-stencil attachment
  TextureHandle depthTex = fb.depthStencil.texture;
  if (depthTex) {
    lvk::VulkanTexture& vkDepthTex = *ctx_->texturesPool_.get(depthTex);
    const lvk::VulkanImage* depthImg = vkDepthTex.image_.get();
    LVK_ASSERT_MSG(depthImg->vkImageFormat_ != VK_FORMAT_UNDEFINED, "Invalid depth attachment format");
    const VkImageAspectFlags flags = vkDepthTex.image_->getImageAspectFlags();
    depthImg->transitionLayout(wrapper_->cmdBuf_,
                               VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                               VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // wait for all subsequent
                                                                  // operations
                               VkImageSubresourceRange{flags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }

  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
  uint32_t mipLevel = 0;
  uint32_t fbWidth = 0;
  uint32_t fbHeight = 0;

  // Process depth attachment
  dynamicState_.depthBiasEnable_ = false;

  VkRenderingAttachmentInfo colorAttachments[LVK_MAX_COLOR_ATTACHMENTS];

  for (uint32_t i = 0; i != numFbColorAttachments; i++) {
    const lvk::Framebuffer::AttachmentDesc& attachment = fb.color[i];
    LVK_ASSERT(!attachment.texture.empty());

    lvk::VulkanTexture& colorTexture = *ctx_->texturesPool_.get(attachment.texture);
    const auto& descColor = renderPass.color[i];
    if (mipLevel && descColor.level) {
      LVK_ASSERT_MSG(descColor.level == mipLevel, "All color attachments should have the same mip-level");
    }
    const VkExtent3D dim = colorTexture.getExtent();
    if (fbWidth) {
      LVK_ASSERT_MSG(dim.width == fbWidth, "All attachments should have the save width");
    }
    if (fbHeight) {
      LVK_ASSERT_MSG(dim.height == fbHeight, "All attachments should have the save width");
    }
    mipLevel = descColor.level;
    fbWidth = dim.width;
    fbHeight = dim.height;
    samples = colorTexture.image_->vkSamples_;
    colorAttachments[i] = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = colorTexture.getOrCreateVkImageViewForFramebuffer(descColor.level),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = (samples > 1) ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = loadOpToVkAttachmentLoadOp(descColor.loadOp),
        .storeOp = storeOpToVkAttachmentStoreOp(descColor.storeOp),
        .clearValue =
            {.color = {.float32 = {descColor.clearColor[0], descColor.clearColor[1], descColor.clearColor[2], descColor.clearColor[3]}}},
    };
    // handle MSAA
    if (descColor.storeOp == StoreOp_MsaaResolve) {
      LVK_ASSERT(samples > 1);
      LVK_ASSERT_MSG(!attachment.resolveTexture.empty(), "Framebuffer attachment should contain a resolve texture");
      lvk::VulkanTexture& colorResolveTexture = *ctx_->texturesPool_.get(attachment.resolveTexture);
      colorAttachments[i].resolveImageView = colorResolveTexture.getOrCreateVkImageViewForFramebuffer(descColor.level);
      colorAttachments[i].resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
  }

  VkRenderingAttachmentInfo depthAttachment = {};

  if (fb.depthStencil.texture) {
    auto& depthTexture = *ctx_->texturesPool_.get(fb.depthStencil.texture);
    const auto& descDepth = renderPass.depth;
    LVK_ASSERT_MSG(descDepth.level == mipLevel, "Depth attachment should have the same mip-level as color attachments");
    depthAttachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = depthTexture.getOrCreateVkImageViewForFramebuffer(descDepth.level),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = loadOpToVkAttachmentLoadOp(descDepth.loadOp),
        .storeOp = storeOpToVkAttachmentStoreOp(descDepth.storeOp),
        .clearValue = {.depthStencil = {.depth = descDepth.clearDepth, .stencil = descDepth.clearStencil}},
    };
    const VkExtent3D dim = depthTexture.getExtent();
    if (fbWidth) {
      LVK_ASSERT_MSG(dim.width == fbWidth, "All attachments should have the save width");
    }
    if (fbHeight) {
      LVK_ASSERT_MSG(dim.height == fbHeight, "All attachments should have the save width");
    }
    mipLevel = descDepth.level;
    fbWidth = dim.width;
    fbHeight = dim.height;
  }

  const uint32_t width = std::max(fbWidth >> mipLevel, 1u);
  const uint32_t height = std::max(fbHeight >> mipLevel, 1u);
  const lvk::Viewport viewport = {0.0f, 0.0f, (float)width, (float)height, 0.0f, +1.0f};
  const lvk::ScissorRect scissor = {0, 0, width, height};

  VkRenderingAttachmentInfo stencilAttachment = depthAttachment;

  const bool isStencilFormat = renderPass.stencil.loadOp != lvk::LoadOp_Invalid;

  const VkRenderingInfo renderingInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .pNext = nullptr,
      .flags = 0,
      .renderArea = {VkOffset2D{(int32_t)scissor.x, (int32_t)scissor.y}, VkExtent2D{scissor.width, scissor.height}},
      .layerCount = 1,
      .viewMask = 0,
      .colorAttachmentCount = numFbColorAttachments,
      .pColorAttachments = colorAttachments,
      .pDepthAttachment = depthTex ? &depthAttachment : nullptr,
      .pStencilAttachment = isStencilFormat ? &stencilAttachment : nullptr,
  };

  cmdBindViewport(viewport);
  cmdBindScissorRect(scissor);

  ctx_->checkAndUpdateDescriptorSets();
  ctx_->bindDefaultDescriptorSets(wrapper_->cmdBuf_, VK_PIPELINE_BIND_POINT_GRAPHICS);

#ifndef __APPLE__
  vkCmdSetDepthCompareOp(wrapper_->cmdBuf_, VK_COMPARE_OP_ALWAYS);
#endif

  vkCmdBeginRendering(wrapper_->cmdBuf_, &renderingInfo);
}

void lvk::CommandBuffer::cmdEndRendering() {
  LVK_ASSERT(isRendering_);

  isRendering_ = false;

  vkCmdEndRendering(wrapper_->cmdBuf_);

  const uint32_t numFbColorAttachments = framebuffer_.getNumColorAttachments();

  // set image layouts after the render pass
  for (uint32_t i = 0; i != numFbColorAttachments; i++) {
    const auto& attachment = framebuffer_.color[i];
    const VulkanTexture& tex = *ctx_->texturesPool_.get(attachment.texture);
    // this must match the final layout of the render pass
    tex.image_->vkImageLayout_ = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  if (framebuffer_.depthStencil.texture) {
    const VulkanTexture& tex = *ctx_->texturesPool_.get(framebuffer_.depthStencil.texture);
    // this must match the final layout of the render pass
    tex.image_->vkImageLayout_ = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }

  framebuffer_ = {};
}

void lvk::CommandBuffer::cmdBindViewport(const Viewport& viewport) {
  // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
  const VkViewport vp = {
      .x = viewport.x, // float x;
      .y = viewport.height - viewport.y, // float y;
      .width = viewport.width, // float width;
      .height = -viewport.height, // float height;
      .minDepth = viewport.minDepth, // float minDepth;
      .maxDepth = viewport.maxDepth, // float maxDepth;
  };
  vkCmdSetViewport(wrapper_->cmdBuf_, 0, 1, &vp);
}

void lvk::CommandBuffer::cmdBindScissorRect(const ScissorRect& rect) {
  const VkRect2D scissor = {
      VkOffset2D{(int32_t)rect.x, (int32_t)rect.y},
      VkExtent2D{rect.width, rect.height},
  };
  vkCmdSetScissor(wrapper_->cmdBuf_, 0, 1, &scissor);
}

void lvk::CommandBuffer::cmdBindRenderPipeline(lvk::RenderPipelineHandle handle) {
  if (!LVK_VERIFY(!handle.empty())) {
    return;
  }

  currentPipeline_ = handle;

  const lvk::RenderPipelineState* rps = ctx_->renderPipelinesPool_.get(handle);

  LVK_ASSERT(rps);

  const bool hasDepthAttachmentPipeline = rps->desc_.depthFormat != Format_Invalid;
  const bool hasDepthAttachmentPass = !framebuffer_.depthStencil.texture.empty();

  if (hasDepthAttachmentPipeline != hasDepthAttachmentPass) {
    LVK_ASSERT(false);
    LLOGW("Make sure your render pass and render pipeline both have matching depth attachments");
  }

  lastPipelineBound_ = VK_NULL_HANDLE;
}

void lvk::CommandBuffer::cmdBindDepthState(const DepthState& desc) {
  LVK_PROFILER_FUNCTION();

  const VkCompareOp op = compareOpToVkCompareOp(desc.compareOp);
#ifndef __APPLE__
  vkCmdSetDepthWriteEnable(wrapper_->cmdBuf_, desc.isDepthWriteEnabled ? VK_TRUE : VK_FALSE);
  vkCmdSetDepthTestEnable(wrapper_->cmdBuf_, op != VK_COMPARE_OP_ALWAYS);
  vkCmdSetDepthCompareOp(wrapper_->cmdBuf_, op);
#else
  dynamicState_.depthWriteEnable_ = desc.isDepthWriteEnabled;
  dynamicState_.depthCompareOp_ = op;
#endif
}

void lvk::CommandBuffer::cmdBindVertexBuffer(uint32_t index, BufferHandle buffer, uint64_t bufferOffset) {
  LVK_PROFILER_FUNCTION();

  if (!LVK_VERIFY(!buffer.empty())) {
    return;
  }

  lvk::VulkanBuffer* buf = ctx_->buffersPool_.get(buffer);

  LVK_ASSERT(buf->vkUsageFlags_ & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

  vkCmdBindVertexBuffers(wrapper_->cmdBuf_, index, 1, &buf->vkBuffer_, &bufferOffset);
}

void lvk::CommandBuffer::cmdBindIndexBuffer(BufferHandle indexBuffer, IndexFormat indexFormat, uint64_t indexBufferOffset) {
  lvk::VulkanBuffer* buf = ctx_->buffersPool_.get(indexBuffer);

  LVK_ASSERT(buf->vkUsageFlags_ & VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

  const VkIndexType type = indexFormatToVkIndexType(indexFormat);
  vkCmdBindIndexBuffer(wrapper_->cmdBuf_, buf->vkBuffer_, indexBufferOffset, type);
}

void lvk::CommandBuffer::cmdPushConstants(const void* data, size_t size, size_t offset) {
  LVK_PROFILER_FUNCTION();

  LVK_ASSERT(size % 4 == 0); // VUID-vkCmdPushConstants-size-00369: size must be a multiple of 4

  // check push constant size is within max size
  const VkPhysicalDeviceLimits& limits = ctx_->getVkPhysicalDeviceProperties().limits;
  if (!LVK_VERIFY(size + offset <= limits.maxPushConstantsSize)) {
    LLOGW("Push constants size exceeded %u (max %u bytes)", size + offset, limits.maxPushConstantsSize);
  }

  vkCmdPushConstants(wrapper_->cmdBuf_,
                     ctx_->vkPipelineLayout_,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
                     (uint32_t)offset,
                     (uint32_t)size,
                     data);
}

void lvk::CommandBuffer::bindGraphicsPipeline() {
  VkPipeline pipeline = ctx_->getVkPipeline(currentPipeline_, dynamicState_);

  if (lastPipelineBound_ != pipeline) {
    lastPipelineBound_ = pipeline;
    if (pipeline != VK_NULL_HANDLE) {
      vkCmdBindPipeline(wrapper_->cmdBuf_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }
  }
}

void lvk::CommandBuffer::cmdDraw(PrimitiveType primitiveType, size_t vertexStart, size_t vertexCount) {
  LVK_PROFILER_FUNCTION();

  if (vertexCount == 0) {
    return;
  }

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindGraphicsPipeline();

  vkCmdDraw(wrapper_->cmdBuf_, (uint32_t)vertexCount, 1, (uint32_t)vertexStart, 0);
}

void lvk::CommandBuffer::cmdDrawIndexed(PrimitiveType primitiveType,
                                        uint32_t indexCount,
                                        uint32_t instanceCount,
                                        uint32_t firstIndex,
                                        int32_t vertexOffset,
                                        uint32_t baseInstance) {
  LVK_PROFILER_FUNCTION();

  if (indexCount == 0) {
    return;
  }

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindGraphicsPipeline();

  vkCmdDrawIndexed(wrapper_->cmdBuf_, indexCount, instanceCount, firstIndex, vertexOffset, baseInstance);
}

void lvk::CommandBuffer::cmdDrawIndirect(PrimitiveType primitiveType,
                                         BufferHandle indirectBuffer,
                                         size_t indirectBufferOffset,
                                         uint32_t drawCount,
                                         uint32_t stride) {
  LVK_PROFILER_FUNCTION();

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindGraphicsPipeline();

  lvk::VulkanBuffer* bufIndirect = ctx_->buffersPool_.get(indirectBuffer);

  vkCmdDrawIndirect(
      wrapper_->cmdBuf_, bufIndirect->vkBuffer_, indirectBufferOffset, drawCount, stride ? stride : sizeof(VkDrawIndirectCommand));
}

void lvk::CommandBuffer::cmdDrawIndexedIndirect(PrimitiveType primitiveType,
                                                BufferHandle indirectBuffer,
                                                size_t indirectBufferOffset,
                                                uint32_t drawCount,
                                                uint32_t stride) {
  LVK_PROFILER_FUNCTION();

  dynamicState_.setTopology(primitiveTypeToVkPrimitiveTopology(primitiveType));
  bindGraphicsPipeline();

  lvk::VulkanBuffer* bufIndirect = ctx_->buffersPool_.get(indirectBuffer);

  vkCmdDrawIndexedIndirect(
      wrapper_->cmdBuf_, bufIndirect->vkBuffer_, indirectBufferOffset, drawCount, stride ? stride : sizeof(VkDrawIndexedIndirectCommand));
}

void lvk::CommandBuffer::cmdSetBlendColor(const float color[4]) {
  vkCmdSetBlendConstants(wrapper_->cmdBuf_, color);
}

void lvk::CommandBuffer::cmdSetDepthBias(float depthBias, float slopeScale, float clamp) {
  dynamicState_.depthBiasEnable_ = true;
  vkCmdSetDepthBias(wrapper_->cmdBuf_, depthBias, clamp, slopeScale);
}

lvk::VulkanStagingDevice::VulkanStagingDevice(VulkanContext& ctx) : ctx_(ctx) {
  LVK_PROFILER_FUNCTION();

  const auto& limits = ctx_.getVkPhysicalDeviceProperties().limits;

  // Use default value of 256 MB, and clamp it to the max limits
  stagingBufferSize_ = std::min(limits.maxStorageBufferRange, 256u * 1024u * 1024u);

  bufferCapacity_ = stagingBufferSize_;

  stagingBuffer_ = ctx_.createBuffer(stagingBufferSize_,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                     nullptr,
                                     "Buffer: staging buffer");
  LVK_ASSERT(!stagingBuffer_.empty());

  immediate_ = std::make_unique<lvk::VulkanImmediateCommands>(
      ctx_.getVkDevice(), ctx_.deviceQueues_.graphicsQueueFamilyIndex, "VulkanStagingDevice::immediate_");
}

lvk::VulkanStagingDevice::~VulkanStagingDevice() {
  ctx_.buffersPool_.destroy(stagingBuffer_);
}

void lvk::VulkanStagingDevice::bufferSubData(VulkanBuffer& buffer, size_t dstOffset, size_t size, const void* data) {
  LVK_PROFILER_FUNCTION();
  if (buffer.isMapped()) {
    buffer.bufferSubData(dstOffset, size, data);
    return;
  }

  lvk::VulkanBuffer* stagingBuffer = ctx_.buffersPool_.get(stagingBuffer_);

  while (size) {
    // get next staging buffer free offset
    MemoryRegionDesc desc = getNextFreeOffset((uint32_t)size);
    const uint32_t chunkSize = std::min((uint32_t)size, desc.alignedSize_);

    // copy data into staging buffer
    stagingBuffer->bufferSubData(desc.srcOffset_, chunkSize, data);

    // do the transfer
    const VkBufferCopy copy = {desc.srcOffset_, dstOffset, chunkSize};

    auto& wrapper = immediate_->acquire();
    vkCmdCopyBuffer(wrapper.cmdBuf_, stagingBuffer->vkBuffer_, buffer.vkBuffer_, 1, &copy);
    desc.handle_ = immediate_->submit(wrapper);
    regions_.push_back(desc);

    size -= chunkSize;
    data = (uint8_t*)data + chunkSize;
    dstOffset += chunkSize;
  }
}

void lvk::VulkanStagingDevice::imageData2D(VulkanImage& image,
                                           const VkRect2D& imageRegion,
                                           uint32_t baseMipLevel,
                                           uint32_t numMipLevels,
                                           uint32_t layer,
                                           uint32_t numLayers,
                                           VkFormat format,
                                           const void* data[]) {
  LVK_PROFILER_FUNCTION();

  // cache the dimensions of each mip-level for later
  uint32_t mipSizes[LVK_MAX_MIP_LEVELS];

  LVK_ASSERT(numMipLevels <= LVK_MAX_MIP_LEVELS);

  // divide the width and height by 2 until we get to the size of level 'baseMipLevel'
  uint32_t width = image.vkExtent_.width >> baseMipLevel;
  uint32_t height = image.vkExtent_.height >> baseMipLevel;

  const Format texFormat(vkFormatToFormat(format));

  LVK_ASSERT_MSG(!imageRegion.offset.x && !imageRegion.offset.y && imageRegion.extent.width == width && imageRegion.extent.height == height,
                 "Uploading mip-levels with an image region that is smaller than the base mip level is not supported");

  // find the storage size for all mip-levels being uploaded
  uint32_t layerStorageSize = 0;
  for (uint32_t i = 0; i < numMipLevels; ++i) {
    const uint32_t mipSize = lvk::getTextureBytesPerLayer(image.vkExtent_.width, image.vkExtent_.height, texFormat, i);
    layerStorageSize += mipSize;
    mipSizes[i] = mipSize;
    width = width <= 1 ? 1 : width >> 1;
    height = height <= 1 ? 1 : height >> 1;
  }
  const uint32_t storageSize = layerStorageSize * numLayers;

  LVK_ASSERT(storageSize <= stagingBufferSize_);

  MemoryRegionDesc desc = getNextFreeOffset(layerStorageSize);
  // No support for copying image in multiple smaller chunk sizes. If we get smaller buffer size than storageSize, we will wait for GPU idle and get bigger chunk.
  if (desc.alignedSize_ < storageSize) {
    waitAndReset();
    desc = getNextFreeOffset(storageSize);
  }
  LVK_ASSERT(desc.alignedSize_ >= storageSize);

  auto& wrapper = immediate_->acquire();

  lvk::VulkanBuffer* stagingBuffer = ctx_.buffersPool_.get(stagingBuffer_);

  for (uint32_t layer = 0; layer != numLayers; layer++) {
    stagingBuffer->bufferSubData(desc.srcOffset_ + layer * layerStorageSize, layerStorageSize, data[layer]);

    uint32_t mipLevelOffset = 0;

    for (uint32_t mipLevel = 0; mipLevel < numMipLevels; ++mipLevel) {
      const auto currentMipLevel = baseMipLevel + mipLevel;

      LVK_ASSERT(currentMipLevel < image.numLevels_);
      LVK_ASSERT(mipLevel < image.numLevels_);

      // 1. Transition initial image layout into TRANSFER_DST_OPTIMAL
      lvk::imageMemoryBarrier(wrapper.cmdBuf_,
                              image.vkImage_,
                              0,
                              VK_ACCESS_TRANSFER_WRITE_BIT,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, currentMipLevel, 1, layer, 1});

#if LVK_VULKAN_PRINT_COMMANDS
      LLOGL("%p vkCmdCopyBufferToImage()\n", wrapper.cmdBuf_);
#endif // LVK_VULKAN_PRINT_COMMANDS
      // 2. Copy the pixel data from the staging buffer into the image
      const VkRect2D region = {
          .offset = {.x = imageRegion.offset.x >> mipLevel, .y = imageRegion.offset.y >> mipLevel},
          .extent = {.width = std::max(1u, imageRegion.extent.width >> mipLevel),
                     .height = std::max(1u, imageRegion.extent.height >> mipLevel)},
      };
      const VkBufferImageCopy copy = {
          // the offset for this level is at the start of all mip-levels plus the size of all previous mip-levels being uploaded
          .bufferOffset = desc.srcOffset_ + layer * layerStorageSize + mipLevelOffset,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, currentMipLevel, layer, 1},
          .imageOffset = {.x = region.offset.x, .y = region.offset.y, .z = 0},
          .imageExtent = {.width = region.extent.width, .height = region.extent.height, .depth = 1u},
      };
      vkCmdCopyBufferToImage(wrapper.cmdBuf_, stagingBuffer->vkBuffer_, image.vkImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

      // 3. Transition TRANSFER_DST_OPTIMAL into SHADER_READ_ONLY_OPTIMAL
      lvk::imageMemoryBarrier(wrapper.cmdBuf_,
                              image.vkImage_,
                              VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                              VK_ACCESS_SHADER_READ_BIT,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                              VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, currentMipLevel, 1, layer, 1});

      // Compute the offset for the next level
      mipLevelOffset += mipSizes[mipLevel];
    }
  }

  image.vkImageLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  desc.handle_ = immediate_->submit(wrapper);
  regions_.push_back(desc);
}

void lvk::VulkanStagingDevice::imageData3D(VulkanImage& image,
                                           const VkOffset3D& offset,
                                           const VkExtent3D& extent,
                                           VkFormat format,
                                           const void* data) {
  LVK_PROFILER_FUNCTION();
  LVK_ASSERT_MSG(image.numLevels_ == 1, "Can handle only 3D images with exactly 1 mip-level");
  LVK_ASSERT_MSG((offset.x == 0) && (offset.y == 0) && (offset.z == 0), "Can upload only full-size 3D images");
  const uint32_t storageSize = extent.width * extent.height * extent.depth * getBytesPerPixel(format);

  LVK_ASSERT(storageSize <= stagingBufferSize_);

  // get next staging buffer free offset
  MemoryRegionDesc desc = getNextFreeOffset(storageSize);

  // No support for copying image in multiple smaller chunk sizes.
  // If we get smaller buffer size than storageSize, we will wait for GPU idle and get a bigger chunk.
  if (desc.alignedSize_ < storageSize) {
    waitAndReset();
    desc = getNextFreeOffset(storageSize);
  }

  LVK_ASSERT(desc.alignedSize_ >= storageSize);

  lvk::VulkanBuffer* stagingBuffer = ctx_.buffersPool_.get(stagingBuffer_);

  // 1. Copy the pixel data into the host visible staging buffer
  stagingBuffer->bufferSubData(desc.srcOffset_, storageSize, data);

  auto& wrapper = immediate_->acquire();

  // 1. Transition initial image layout into TRANSFER_DST_OPTIMAL
  lvk::imageMemoryBarrier(wrapper.cmdBuf_,
                          image.vkImage_,
                          0,
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  // 2. Copy the pixel data from the staging buffer into the image
  const VkBufferImageCopy copy = {
      .bufferOffset = desc.srcOffset_,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
      .imageOffset = offset,
      .imageExtent = extent,
  };
  vkCmdCopyBufferToImage(wrapper.cmdBuf_, stagingBuffer->vkBuffer_, image.vkImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  // 3. Transition TRANSFER_DST_OPTIMAL into SHADER_READ_ONLY_OPTIMAL
  lvk::imageMemoryBarrier(wrapper.cmdBuf_,
                          image.vkImage_,
                          VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

  image.vkImageLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  desc.handle_ = immediate_->submit(wrapper);
  regions_.push_back(desc);
}

lvk::VulkanStagingDevice::MemoryRegionDesc lvk::VulkanStagingDevice::getNextFreeOffset(uint32_t size) {
  LVK_PROFILER_FUNCTION();

  constexpr uint32_t kStagingBufferAlignment_ = 16;

  uint32_t alignedSize = (size + kStagingBufferAlignment_ - 1) & ~(kStagingBufferAlignment_ - 1);

  // track maximum previously used region
  auto bestIt = regions_.begin();

  // check if we can reuse any of previously used memory region
  for (auto it = regions_.begin(); it != regions_.end(); it++) {
    if (immediate_->isReady(SubmitHandle(it->handle_))) {
      if (it->alignedSize_ >= alignedSize) {
        SCOPE_EXIT {
          regions_.erase(it);
        };
        return *it;
      }
      if (bestIt->alignedSize_ < it->alignedSize_) {
        bestIt = it;
      }
    }
  }

  if (bestIt != regions_.end() && bufferCapacity_ < bestIt->alignedSize_) {
    regions_.erase(bestIt);
    return *bestIt;
  }

  if (bufferCapacity_ == 0) {
    // no more space available in the staging buffer
    waitAndReset();
  }

  // allocate from free staging memory
  alignedSize = (alignedSize <= bufferCapacity_) ? alignedSize : bufferCapacity_;
  const uint32_t srcOffset = stagingBufferFrontOffset_;
  stagingBufferFrontOffset_ = (stagingBufferFrontOffset_ + alignedSize) % stagingBufferSize_;
  bufferCapacity_ -= alignedSize;

  return {srcOffset, alignedSize};
}

void lvk::VulkanStagingDevice::waitAndReset() {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_WAIT);

  for (const auto& r : regions_) {
    immediate_->wait(r.handle_);
  };

  regions_.clear();
  stagingBufferFrontOffset_ = 0;
  bufferCapacity_ = stagingBufferSize_;
}

lvk::VulkanContext::VulkanContext(const lvk::ContextConfig& config, void* window, void* display) : config_(config) {
  LVK_PROFILER_THREAD("MainThread");

  pimpl_ = std::make_unique<VulkanContextImpl>();

  if (volkInitialize() != VK_SUCCESS) {
    LLOGW("volkInitialize() failed\n");
    exit(255);
  };

  glslang_initialize_process();

  createInstance();

  if (window) {
    createSurface(window, display);
  }
}

lvk::VulkanContext::~VulkanContext() {
  LVK_PROFILER_FUNCTION();

  VK_ASSERT(vkDeviceWaitIdle(vkDevice_));

  stagingDevice_.reset(nullptr);
  swapchain_.reset(nullptr); // swapchain has to be destroyed prior to Surface

  if (shaderModulesPool_.numObjects()) {
    LLOGW("Leaked %u shader modules\n", shaderModulesPool_.numObjects());
  }
  if (renderPipelinesPool_.numObjects()) {
    LLOGW("Leaked %u render pipelines\n", renderPipelinesPool_.numObjects());
  }
  if (computePipelinesPool_.numObjects()) {
    LLOGW("Leaked %u compute pipelines\n", computePipelinesPool_.numObjects());
  }
  if (samplersPool_.numObjects() > 1) {
    // the dummy value is owned by the context
    LLOGW("Leaked %u samplers\n", samplersPool_.numObjects() - 1);
  }
  if (texturesPool_.numObjects() > 1) {
    // the dummy value is owned by the context
    LLOGW("Leaked %u textures\n", texturesPool_.numObjects() - 1);
  }
  if (buffersPool_.numObjects()) {
    LLOGW("Leaked %u buffers\n", buffersPool_.numObjects());
  }

  // manually destroy the dummy sampler
  vkDestroySampler(vkDevice_, samplersPool_.objects_.front().obj_, nullptr);
  samplersPool_.clear();
  computePipelinesPool_.clear();
  renderPipelinesPool_.clear();
  shaderModulesPool_.clear();
  texturesPool_.clear();

  waitDeferredTasks();

  immediate_.reset(nullptr);

  vkDestroyDescriptorSetLayout(vkDevice_, vkDSLBindless_, nullptr);
  vkDestroyPipelineLayout(vkDevice_, vkPipelineLayout_, nullptr);
  vkDestroyDescriptorPool(vkDevice_, vkDPBindless_, nullptr);
  vkDestroySurfaceKHR(vkInstance_, vkSurface_, nullptr);
  vkDestroyPipelineCache(vkDevice_, pipelineCache_, nullptr);

  // Clean up VMA
  if (LVK_VULKAN_USE_VMA) {
    vmaDestroyAllocator(pimpl_->vma_);
  }

  // Device has to be destroyed prior to Instance
  vkDestroyDevice(vkDevice_, nullptr);

  vkDestroyDebugUtilsMessengerEXT(vkInstance_, vkDebugUtilsMessenger_, nullptr);
  vkDestroyInstance(vkInstance_, nullptr);

  glslang_finalize_process();

  LLOGL("Vulkan graphics pipelines created: %u\n", VulkanPipelineBuilder::getNumPipelinesCreated());
}

lvk::ICommandBuffer& lvk::VulkanContext::acquireCommandBuffer() {
  LVK_PROFILER_FUNCTION();

  LVK_ASSERT_MSG(!pimpl_->currentCommandBuffer_.ctx_, "Cannot acquire more than 1 command buffer simultaneously");

  pimpl_->currentCommandBuffer_ = CommandBuffer(this);

  return pimpl_->currentCommandBuffer_;
}

lvk::SubmitHandle lvk::VulkanContext::submit(lvk::ICommandBuffer& commandBuffer, TextureHandle present) {
  LVK_PROFILER_FUNCTION();

  CommandBuffer* vkCmdBuffer = static_cast<CommandBuffer*>(&commandBuffer);

  LVK_ASSERT(vkCmdBuffer);
  LVK_ASSERT(vkCmdBuffer->ctx_);
  LVK_ASSERT(vkCmdBuffer->wrapper_);

  if (present) {
    const lvk::VulkanTexture& tex = *texturesPool_.get(present);

    LVK_ASSERT(tex.image_->isSwapchainImage_);

    // prepare image for presentation the image might be coming from a compute shader
    const VkPipelineStageFlagBits srcStage = (tex.image_->vkImageLayout_ == VK_IMAGE_LAYOUT_GENERAL)
                                                 ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                                 : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    tex.image_->transitionLayout(
        vkCmdBuffer->wrapper_->cmdBuf_,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        srcStage,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // wait for all subsequent operations
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
  }

  const bool shouldPresent = hasSwapchain() && present;

  if (shouldPresent) {
    immediate_->waitSemaphore(swapchain_->acquireSemaphore_);
  }

  vkCmdBuffer->lastSubmitHandle_ = immediate_->submit(*vkCmdBuffer->wrapper_);

  if (shouldPresent) {
    swapchain_->present(immediate_->acquireLastSubmitSemaphore());
  }

  processDeferredTasks();

  // reset
  pimpl_->currentCommandBuffer_ = {};

  return vkCmdBuffer->lastSubmitHandle_;
}

void lvk::VulkanContext::wait(SubmitHandle handle) {
  immediate_->wait(handle);
}

lvk::Holder<lvk::BufferHandle> lvk::VulkanContext::createBuffer(const BufferDesc& requestedDesc, Result* outResult) {
  BufferDesc desc = requestedDesc;

  if (!useStaging_ && (desc.storage == StorageType_Device)) {
    desc.storage = StorageType_HostVisible;
  }

  // Use staging device to transfer data into the buffer when the storage is private to the device
  VkBufferUsageFlags usageFlags = (desc.storage == StorageType_Device) ? VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                                                                       : 0;

  if (desc.usage == 0) {
    Result::setResult(outResult, Result(Result::Code::ArgumentOutOfRange, "Invalid buffer usage"));
    return {};
  }

  if (desc.usage & BufferUsageBits_Index) {
    usageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  if (desc.usage & BufferUsageBits_Vertex) {
    usageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  if (desc.usage & BufferUsageBits_Uniform) {
    usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
  }

  if (desc.usage & BufferUsageBits_Storage) {
    usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
  }

  if (desc.usage & BufferUsageBits_Indirect) {
    usageFlags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
  }

  const VkMemoryPropertyFlags memFlags = storageTypeToVkMemoryPropertyFlags(desc.storage);

  Result result;
  BufferHandle handle = createBuffer(desc.size, usageFlags, memFlags, &result, desc.debugName);

  if (!LVK_VERIFY(result.isOk())) {
    Result::setResult(outResult, result);
    return {};
  }

  if (desc.data) {
    upload(handle, desc.data, desc.size, 0);
  }

  Result::setResult(outResult, Result());

  return {this, handle};
}

lvk::Holder<lvk::SamplerHandle> lvk::VulkanContext::createSampler(const SamplerStateDesc& desc, Result* outResult) {
  LVK_PROFILER_FUNCTION();

  Result result;

  SamplerHandle handle =
      createSampler(samplerStateDescToVkSamplerCreateInfo(desc, getVkPhysicalDeviceProperties().limits), &result, desc.debugName);

  if (!LVK_VERIFY(result.isOk())) {
    Result::setResult(outResult, Result(Result::Code::RuntimeError, "Cannot create Sampler"));
    return {};
  }

  Result::setResult(outResult, result);

  return {this, handle};
}

lvk::Holder<lvk::TextureHandle> lvk::VulkanContext::createTexture(const TextureDesc& requestedDesc,
                                                                  const char* debugName,
                                                                  Result* outResult) {
  TextureDesc desc(requestedDesc);

  if (debugName && *debugName) {
    desc.debugName = debugName;
  }

  const VkFormat vkFormat = lvk::isDepthOrStencilFormat(desc.format) ? getClosestDepthStencilFormat(desc.format)
                                                                     : formatToVkFormat(desc.format);

  const lvk::TextureType type = desc.type;
  if (!LVK_VERIFY(type == TextureType_2D || type == TextureType_Cube || type == TextureType_3D)) {
    LVK_ASSERT_MSG(false, "Only 2D, 3D and Cube textures are supported");
    Result::setResult(outResult, Result::Code::RuntimeError);
    return {};
  }

  if (desc.numMipLevels == 0) {
    LVK_ASSERT_MSG(false, "The number of mip levels specified must be greater than 0");
    desc.numMipLevels = 1;
  }

  if (desc.numSamples > 1 && desc.numMipLevels != 1) {
    LVK_ASSERT_MSG(false, "The number of mip levels for multisampled images should be 1");
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "The number of mip-levels for multisampled images should be 1");
    return {};
  }

  if (desc.numSamples > 1 && type == TextureType_3D) {
    LVK_ASSERT_MSG(false, "Multisampled 3D images are not supported");
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Multisampled 3D images are not supported");
    return {};
  }

  if (!LVK_VERIFY(desc.numMipLevels <= lvk::calcNumMipLevels(desc.dimensions.width, desc.dimensions.height))) {
    Result::setResult(outResult,
                      Result::Code::ArgumentOutOfRange,
                      "The number of specified mip-levels is greater than the maximum possible "
                      "number of mip-levels.");
    return {};
  }

  if (desc.usage == 0) {
    LVK_ASSERT_MSG(false, "Texture usage flags are not set");
    desc.usage = lvk::TextureUsageBits_Sampled;
  }

  /* Use staging device to transfer data into the image when the storage is private to the device */
  VkImageUsageFlags usageFlags = (desc.storage == StorageType_Device) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;

  if (desc.usage & lvk::TextureUsageBits_Sampled) {
    usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (desc.usage & lvk::TextureUsageBits_Storage) {
    LVK_ASSERT_MSG(desc.numSamples <= 1, "Storage images cannot be multisampled");
    usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (desc.usage & lvk::TextureUsageBits_Attachment) {
    usageFlags |= lvk::isDepthOrStencilFormat(desc.format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                                           : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  // For now, always set this flag so we can read it back
  usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  LVK_ASSERT_MSG(usageFlags != 0, "Invalid usage flags");

  const VkMemoryPropertyFlags memFlags = storageTypeToVkMemoryPropertyFlags(desc.storage);

  const bool hasDebugName = desc.debugName && *desc.debugName;

  char debugNameImage[256] = {0};
  char debugNameImageView[256] = {0};

  if (hasDebugName) {
    snprintf(debugNameImage, sizeof(debugNameImage) - 1, "Image: %s", desc.debugName);
    snprintf(debugNameImageView, sizeof(debugNameImageView) - 1, "Image View: %s", desc.debugName);
  }

  VkImageCreateFlags createFlags = 0;
  uint32_t arrayLayerCount = static_cast<uint32_t>(desc.numLayers);
  VkImageViewType imageViewType;
  VkImageType imageType;
  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
  switch (desc.type) {
  case TextureType_2D:
    imageViewType = VK_IMAGE_VIEW_TYPE_2D;
    imageType = VK_IMAGE_TYPE_2D;
    samples = lvk::getVulkanSampleCountFlags(desc.numSamples);
    break;
  case TextureType_3D:
    imageViewType = VK_IMAGE_VIEW_TYPE_3D;
    imageType = VK_IMAGE_TYPE_3D;
    break;
  case TextureType_Cube:
    imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
    arrayLayerCount *= 6;
    imageType = VK_IMAGE_TYPE_2D;
    createFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    break;
  default:
    LVK_ASSERT_MSG(false, "Code should NOT be reached");
    Result::setResult(outResult, Result::Code::RuntimeError, "Unsupported texture type");
    return {};
  }

  Result result;
  auto image = createImage(imageType,
                           VkExtent3D{desc.dimensions.width, desc.dimensions.height, desc.dimensions.depth},
                           vkFormat,
                           desc.numMipLevels,
                           arrayLayerCount,
                           VK_IMAGE_TILING_OPTIMAL,
                           usageFlags,
                           memFlags,
                           createFlags,
                           samples,
                           &result,
                           debugNameImage);
  if (!LVK_VERIFY(result.isOk())) {
    Result::setResult(outResult, result);
    return {};
  }
  if (!LVK_VERIFY(image.get())) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Cannot create VulkanImage");
    return {};
  }

  VkImageAspectFlags aspect = 0;
  if (image->isDepthFormat_ || image->isStencilFormat_) {
    if (image->isDepthFormat_) {
      aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if (image->isStencilFormat_) {
      aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  VkImageView view =
      image->createImageView(imageViewType, vkFormat, aspect, 0, VK_REMAINING_MIP_LEVELS, 0, arrayLayerCount, debugNameImageView);

  if (!LVK_VERIFY(view != VK_NULL_HANDLE)) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Cannot create VkImageView");
    return {};
  }

  TextureHandle handle = texturesPool_.create(lvk::VulkanTexture(std::move(image), view));

  awaitingCreation_ = true;

  if (desc.data) {
    LVK_ASSERT(desc.type == TextureType_2D);
    const void* mipMaps[] = {desc.data};
    Result res = upload(handle, {.dimensions = desc.dimensions, .numMipLevels = 1}, mipMaps);
    if (!res.isOk()) {
      Result::setResult(outResult, res);
      return {};
    }
  }

  Result::setResult(outResult, Result());

  return {this, handle};
}

VkPipeline lvk::VulkanContext::getVkPipeline(RenderPipelineHandle handle, const RenderPipelineDynamicState& dynamicState) {
  lvk::RenderPipelineState* rps = renderPipelinesPool_.get(handle);

  if (!rps) {
    return VK_NULL_HANDLE;
  }

  if (rps->pipelineLayout_ != vkPipelineLayout_) {
    rps->destroyPipelines(this);
    rps->pipelineLayout_ = vkPipelineLayout_;
  }

#ifndef __APPLE__
  if (rps->pipelines_[dynamicState.topology_][dynamicState.depthBiasEnable_] != VK_NULL_HANDLE) {
    return rps->pipelines_[dynamicState.topology_][dynamicState.depthBiasEnable_];
  }
#else
  if (rps->pipelines_[dynamicState.topology_][dynamicState.depthCompareOp_][dynamicState.depthWriteEnable_]
                     [dynamicState.depthBiasEnable_] != VK_NULL_HANDLE) {
    return rps
        ->pipelines_[dynamicState.topology_][dynamicState.depthCompareOp_][dynamicState.depthWriteEnable_][dynamicState.depthBiasEnable_];
  }
#endif

  // build a new Vulkan pipeline

  VkPipeline pipeline = VK_NULL_HANDLE;

  const RenderPipelineDesc& desc = rps->desc_;

  const uint32_t numColorAttachments = rps->desc_.getNumColorAttachments();

  // Not all attachments are valid. We need to create color blend attachments only for active attachments
  VkPipelineColorBlendAttachmentState colorBlendAttachmentStates[LVK_MAX_COLOR_ATTACHMENTS] = {};
  VkFormat colorAttachmentFormats[LVK_MAX_COLOR_ATTACHMENTS] = {};

  for (uint32_t i = 0; i != numColorAttachments; i++) {
    const auto& attachment = desc.color[i];
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

  const VkShaderModule* vertModule = shaderModulesPool_.get(desc.smVert);
  const VkShaderModule* geomModule = shaderModulesPool_.get(desc.smGeom);
  const VkShaderModule* fragModule = shaderModulesPool_.get(desc.smFrag);

  LVK_ASSERT(vertModule);
  LVK_ASSERT(fragModule);

  const VkPipelineVertexInputStateCreateInfo ciVertexInputState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = rps->numBindings_,
      .pVertexBindingDescriptions = rps->numBindings_ ? rps->vkBindings_ : nullptr,
      .vertexAttributeDescriptionCount = rps->numAttributes_,
      .pVertexAttributeDescriptions = rps->numAttributes_ ? rps->vkAttributes_ : nullptr,
  };

  VkSpecializationMapEntry entries[SpecializationConstantDesc::LVK_SPECIALIZATION_CONSTANTS_MAX] = {};

  const VkSpecializationInfo si = lvk::getPipelineShaderStageSpecializationInfo(desc.specInfo, entries);

  lvk::VulkanPipelineBuilder()
      // from Vulkan 1.0
      .dynamicState(VK_DYNAMIC_STATE_VIEWPORT)
      .dynamicState(VK_DYNAMIC_STATE_SCISSOR)
      .dynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS)
      .dynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS)
  // from Vulkan 1.3
#ifndef __APPLE__
      .dynamicState(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE)
      .dynamicState(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE)
      .dynamicState(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP)
#else
      .depthCompareOp(dynamicState.depthCompareOp_)
      .depthWriteEnable(dynamicState.depthWriteEnable_)
#endif
      .primitiveTopology(dynamicState.topology_)
      .depthBiasEnable(dynamicState.depthBiasEnable_)
      .rasterizationSamples(getVulkanSampleCountFlags(desc.samplesCount))
      .polygonMode(polygonModeToVkPolygonMode(desc.polygonMode))
      .stencilStateOps(VK_STENCIL_FACE_FRONT_BIT,
                       stencilOpToVkStencilOp(desc.frontFaceStencil.stencilFailureOp),
                       stencilOpToVkStencilOp(desc.frontFaceStencil.depthStencilPassOp),
                       stencilOpToVkStencilOp(desc.frontFaceStencil.depthFailureOp),
                       compareOpToVkCompareOp(desc.frontFaceStencil.stencilCompareOp))
      .stencilStateOps(VK_STENCIL_FACE_BACK_BIT,
                       stencilOpToVkStencilOp(desc.backFaceStencil.stencilFailureOp),
                       stencilOpToVkStencilOp(desc.backFaceStencil.depthStencilPassOp),
                       stencilOpToVkStencilOp(desc.backFaceStencil.depthFailureOp),
                       compareOpToVkCompareOp(desc.backFaceStencil.stencilCompareOp))
      .stencilMasks(VK_STENCIL_FACE_FRONT_BIT, 0xFF, desc.frontFaceStencil.writeMask, desc.frontFaceStencil.readMask)
      .stencilMasks(VK_STENCIL_FACE_BACK_BIT, 0xFF, desc.backFaceStencil.writeMask, desc.backFaceStencil.readMask)
      .shaderStage(lvk::getPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, *vertModule, desc.entryPointVert, &si))
      .shaderStage(lvk::getPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, *fragModule, desc.entryPointFrag, &si))
      .shaderStage(geomModule ? lvk::getPipelineShaderStageCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT, *geomModule, desc.entryPointGeom, &si)
                              : VkPipelineShaderStageCreateInfo{.module = VK_NULL_HANDLE})
      .cullMode(cullModeToVkCullMode(desc.cullMode))
      .frontFace(windingModeToVkFrontFace(desc.frontFaceWinding))
      .vertexInputState(ciVertexInputState)
      .colorAttachments(colorBlendAttachmentStates, colorAttachmentFormats, numColorAttachments)
      .depthAttachmentFormat(formatToVkFormat(desc.depthFormat))
      .stencilAttachmentFormat(formatToVkFormat(desc.stencilFormat))
      .build(vkDevice_, pipelineCache_, vkPipelineLayout_, &pipeline, desc.debugName);

#ifndef __APPLE__
  rps->pipelines_[dynamicState.topology_][dynamicState.depthBiasEnable_] = pipeline;
#else
  rps->pipelines_[dynamicState.topology_][dynamicState.depthCompareOp_][dynamicState.depthWriteEnable_][dynamicState.depthBiasEnable_] =
      pipeline;
#endif
  return pipeline;
}

VkPipeline lvk::VulkanContext::getVkPipeline(ComputePipelineHandle handle) {
  lvk::ComputePipelineState* cps = computePipelinesPool_.get(handle);

  if (!cps) {
    return VK_NULL_HANDLE;
  }

  if (cps->pipelineLayout_ != vkPipelineLayout_) {
    deferredTask(
        std::packaged_task<void()>([device = vkDevice_, pipeline = cps->pipeline_]() { vkDestroyPipeline(device, pipeline, nullptr); }));
    cps->pipeline_ = VK_NULL_HANDLE;
    cps->pipelineLayout_ = vkPipelineLayout_;
  }

  if (cps->pipeline_ == VK_NULL_HANDLE) {
    const VkShaderModule* sm = shaderModulesPool_.get(cps->desc_.shaderModule);

    LVK_ASSERT(sm);

    VkSpecializationMapEntry entries[SpecializationConstantDesc::LVK_SPECIALIZATION_CONSTANTS_MAX] = {};

    const VkSpecializationInfo siComp = lvk::getPipelineShaderStageSpecializationInfo(cps->desc_.specInfo, entries);

    const VkComputePipelineCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .flags = 0,
        .stage = lvk::getPipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT, *sm, cps->desc_.entryPoint, &siComp),
        .layout = vkPipelineLayout_,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_ASSERT(vkCreateComputePipelines(vkDevice_, pipelineCache_, 1, &ci, nullptr, &cps->pipeline_));
    VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_PIPELINE, (uint64_t)cps->pipeline_, cps->desc_.debugName));
  }

  return cps->pipeline_;
}

lvk::Holder<lvk::ComputePipelineHandle> lvk::VulkanContext::createComputePipeline(const ComputePipelineDesc& desc, Result* outResult) {
  if (!LVK_VERIFY(desc.shaderModule.valid())) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Missing compute shader");
    return {};
  }

  return {this, computePipelinesPool_.create(lvk::ComputePipelineState{desc})};
}

lvk::Holder<lvk::RenderPipelineHandle> lvk::VulkanContext::createRenderPipeline(const RenderPipelineDesc& desc, Result* outResult) {
  const bool hasColorAttachments = desc.getNumColorAttachments() > 0;
  const bool hasDepthAttachment = desc.depthFormat != Format_Invalid;
  const bool hasAnyAttachments = hasColorAttachments || hasDepthAttachment;
  if (!LVK_VERIFY(hasAnyAttachments)) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Need at least one attachment");
    return {};
  }

  if (!LVK_VERIFY(desc.smVert.valid())) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Missing vertex shader");
    return {};
  }

  if (!LVK_VERIFY(desc.smFrag.valid())) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Missing fragment shader");
    return {};
  }

  RenderPipelineState rps = {.desc_ = desc};

  // Iterate and cache vertex input bindings and attributes
  const lvk::VertexInput& vstate = rps.desc_.vertexInput;

  bool bufferAlreadyBound[VertexInput::LVK_VERTEX_BUFFER_MAX] = {};

  rps.numAttributes_ = vstate.getNumAttributes();

  for (uint32_t i = 0; i != rps.numAttributes_; i++) {
    const auto& attr = vstate.attributes[i];

    rps.vkAttributes_[i] = {
        .location = attr.location, .binding = attr.binding, .format = vertexFormatToVkFormat(attr.format), .offset = (uint32_t)attr.offset};

    if (!bufferAlreadyBound[attr.binding]) {
      bufferAlreadyBound[attr.binding] = true;
      rps.vkBindings_[rps.numBindings_++] = {
          .binding = attr.binding, .stride = vstate.inputBindings[attr.binding].stride, .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    }
  }

  return {this, renderPipelinesPool_.create(std::move(rps))};
}

void lvk::VulkanContext::destroy(lvk::ComputePipelineHandle handle) {
  lvk::ComputePipelineState* cps = computePipelinesPool_.get(handle);

  if (!cps) {
    return;
  }

  deferredTask(
      std::packaged_task<void()>([device = getVkDevice(), pipeline = cps->pipeline_]() { vkDestroyPipeline(device, pipeline, nullptr); }));

  computePipelinesPool_.destroy(handle);
}

void lvk::VulkanContext::destroy(lvk::RenderPipelineHandle handle) {
  lvk::RenderPipelineState* rps = renderPipelinesPool_.get(handle);

  if (!rps) {
    return;
  }

  rps->destroyPipelines(this);

  renderPipelinesPool_.destroy(handle);
}

void lvk::VulkanContext::destroy(lvk::ShaderModuleHandle handle) {
  const VkShaderModule* sm = shaderModulesPool_.get(handle);

  if (!sm) {
    return;
  }

  if (*sm != VK_NULL_HANDLE) {
    // a shader module can be destroyed while pipelines created using its shaders are still in use
    // https://registry.khronos.org/vulkan/specs/1.3/html/chap9.html#vkDestroyShaderModule
    vkDestroyShaderModule(getVkDevice(), *sm, nullptr);
  }

  shaderModulesPool_.destroy(handle);
}

void lvk::VulkanContext::destroy(SamplerHandle handle) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_DESTROY);

  VkSampler sampler = *samplersPool_.get(handle);

  samplersPool_.destroy(handle);

  deferredTask(std::packaged_task<void()>([device = vkDevice_, sampler = sampler]() { vkDestroySampler(device, sampler, nullptr); }));
}

void lvk::VulkanContext::destroy(BufferHandle handle) {
  // deferred deletion handled in VulkanBuffer
  buffersPool_.destroy(handle);
}

void lvk::VulkanContext::destroy(lvk::TextureHandle handle) {
  // deferred deletion handled in VulkanTexture
  texturesPool_.destroy(handle);
}

void lvk::VulkanContext::destroy(Framebuffer& fb) {
  auto destroyFbTexture = [this](TextureHandle& handle) {
    {
      if (handle.empty())
        return;
      lvk::VulkanTexture* tex = texturesPool_.get(handle);
      if (!tex || tex->image_->isSwapchainImage_)
        return;
      destroy(handle);
      handle = {};
    }
  };

  for (Framebuffer::AttachmentDesc& a : fb.color) {
    destroyFbTexture(a.texture);
    destroyFbTexture(a.resolveTexture);
  }
  destroyFbTexture(fb.depthStencil.texture);
  destroyFbTexture(fb.depthStencil.resolveTexture);
}

lvk::Result lvk::VulkanContext::upload(lvk::BufferHandle handle, const void* data, size_t size, size_t offset) {
  LVK_PROFILER_FUNCTION();

  if (!LVK_VERIFY(data)) {
    return lvk::Result();
  }

  lvk::VulkanBuffer* buf = buffersPool_.get(handle);

  if (!LVK_VERIFY(buf)) {
    return lvk::Result();
  }

  if (!LVK_VERIFY(offset + size <= buf->bufferSize_)) {
    return lvk::Result(Result::Code::ArgumentOutOfRange, "Out of range");
  }

  stagingDevice_->bufferSubData(*buf, offset, size, data);

  return lvk::Result();
}

uint8_t* lvk::VulkanContext::getMappedPtr(BufferHandle handle) const {
  const lvk::VulkanBuffer* buf = buffersPool_.get(handle);

  LVK_ASSERT(buf);

  return buf->isMapped() ? buf->getMappedPtr() : nullptr;
}

uint64_t lvk::VulkanContext::gpuAddress(BufferHandle handle, size_t offset) const {
  LVK_ASSERT_MSG((offset & 7) == 0, "Buffer offset must be 8 bytes aligned as per GLSL_EXT_buffer_reference spec.");

  const lvk::VulkanBuffer* buf = buffersPool_.get(handle);

  LVK_ASSERT(buf);

  return buf ? (uint64_t)buf->vkDeviceAddress_ + offset : 0u;
}

void lvk::VulkanContext::flushMappedMemory(BufferHandle handle, size_t offset, size_t size) const {
  const lvk::VulkanBuffer* buf = buffersPool_.get(handle);

  LVK_ASSERT(buf);

  buf->flushMappedMemory(offset, size);
}

lvk::Result lvk::VulkanContext::upload(lvk::TextureHandle handle, const TextureRangeDesc& range, const void* data[]) {
  if (!data) {
    return Result();
  }

  lvk::VulkanTexture* texture = texturesPool_.get(handle);

  const Result result = validateRange(texture->getExtent(), texture->image_->numLevels_, range);

  if (!LVK_VERIFY(result.isOk())) {
    return result;
  }

  const uint32_t numLayers = std::max(range.numLayers, 1u);

  const VkImageType type = texture->image_->vkType_;
  VkFormat vkFormat = texture->image_->vkImageFormat_;

  if (type == VK_IMAGE_TYPE_3D) {
    const void* uploadData = data[0];
    stagingDevice_->imageData3D(*texture->image_.get(),
                                VkOffset3D{(int32_t)range.x, (int32_t)range.y, (int32_t)range.z},
                                VkExtent3D{range.dimensions.width, range.dimensions.height, range.dimensions.depth},
                                vkFormat,
                                uploadData);
  } else {
    const VkRect2D imageRegion = {
        .offset = {.x = (int)range.x, .y = (int)range.y},
        .extent = {.width = range.dimensions.width, .height = range.dimensions.height},
    };
    stagingDevice_->imageData2D(
        *texture->image_.get(), imageRegion, range.mipLevel, range.numMipLevels, range.layer, range.numLayers, vkFormat, data);
  }

  return Result();
}

lvk::Dimensions lvk::VulkanContext::getDimensions(TextureHandle handle) const {
  if (!handle) {
    return {};
  }

  const lvk::VulkanTexture* tex = texturesPool_.get(handle);

  LVK_ASSERT(tex);

  if (!tex || !tex->image_) {
    return {};
  }

  return {
      .width = tex->image_->vkExtent_.width,
      .height = tex->image_->vkExtent_.height,
      .depth = tex->image_->vkExtent_.depth,
  };
}

void lvk::VulkanContext::generateMipmap(TextureHandle handle) const {
  if (handle.empty()) {
    return;
  }

  const lvk::VulkanTexture* tex = texturesPool_.get(handle);

  if (tex->image_->numLevels_ > 1) {
    LVK_ASSERT(tex->image_->vkImageLayout_ != VK_IMAGE_LAYOUT_UNDEFINED);
    const auto& wrapper = immediate_->acquire();
    tex->image_->generateMipmap(wrapper.cmdBuf_);
    immediate_->submit(wrapper);
  }
}

lvk::Format lvk::VulkanContext::getFormat(TextureHandle handle) const {
  if (handle.empty()) {
    return Format_Invalid;
  }

  return vkFormatToFormat(texturesPool_.get(handle)->image_->vkImageFormat_);
}

lvk::Holder<lvk::ShaderModuleHandle> lvk::VulkanContext::createShaderModule(const ShaderModuleDesc& desc, Result* outResult) {
  Result result;
  VkShaderModule sm = desc.dataSize ?
                                    // binary
                          createShaderModule(desc.data, desc.dataSize, desc.debugName, &result)
                                    :
                                    // text
                          createShaderModule(desc.stage, desc.data, desc.debugName, &result);

  if (!result.isOk()) {
    Result::setResult(outResult, result);
    return {};
  }
  Result::setResult(outResult, result);

  return {this, shaderModulesPool_.create(std::move(sm))};
}

VkShaderModule lvk::VulkanContext::createShaderModule(const void* data, size_t length, const char* debugName, Result* outResult) const {
  VkShaderModule vkShaderModule = VK_NULL_HANDLE;

  const VkShaderModuleCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = length,
      .pCode = (const uint32_t*)data,
  };
  const VkResult result = vkCreateShaderModule(vkDevice_, &ci, nullptr, &vkShaderModule);

  lvk::setResultFrom(outResult, result);

  if (result != VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }

  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)vkShaderModule, debugName));

  LVK_ASSERT(vkShaderModule != VK_NULL_HANDLE);

  return vkShaderModule;
}

VkShaderModule lvk::VulkanContext::createShaderModule(ShaderStage stage,
                                                      const char* source,
                                                      const char* debugName,
                                                      Result* outResult) const {
  const VkShaderStageFlagBits vkStage = shaderStageToVkShaderStage(stage);
  LVK_ASSERT(vkStage != VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
  LVK_ASSERT(source);

  std::string sourcePatched;

  if (!source || !*source) {
    Result::setResult(outResult, Result::Code::ArgumentOutOfRange, "Shader source is empty");
    return VK_NULL_HANDLE;
  }

  if (strstr(source, "#version ") == nullptr) {
    if (vkStage == VK_SHADER_STAGE_VERTEX_BIT || vkStage == VK_SHADER_STAGE_COMPUTE_BIT) {
      sourcePatched += R"(
      #version 460
      #extension GL_EXT_buffer_reference : require
      #extension GL_EXT_buffer_reference_uvec2 : require
      #extension GL_EXT_debug_printf : enable
      #extension GL_EXT_nonuniform_qualifier : require
      #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
      )";
    }
    if (vkStage == VK_SHADER_STAGE_FRAGMENT_BIT) {
      sourcePatched += R"(
      #version 460
      #extension GL_EXT_buffer_reference_uvec2 : require
      #extension GL_EXT_debug_printf : enable
      #extension GL_EXT_nonuniform_qualifier : require
      #extension GL_EXT_samplerless_texture_functions : require
      #extension GL_EXT_shader_explicit_arithmetic_types_float16 : require
      )";

#ifndef __APPLE__
      sourcePatched += R"(
      layout (set = 0, binding = 0) uniform texture2D kTextures2D[];
      layout (set = 0, binding = 0) uniform texture3D kTextures3D[];
      layout (set = 0, binding = 0) uniform textureCube kTexturesCube[];
      layout (set = 0, binding = 1) uniform sampler kSamplers[];
      layout (set = 0, binding = 1) uniform samplerShadow kSamplersShadow[];
      )";
#else
      sourcePatched += R"(
      layout (set = 0, binding = 0) uniform texture2D kTextures2D[];
      layout (set = 1, binding = 0) uniform texture3D kTextures3D[];
      layout (set = 2, binding = 0) uniform textureCube kTexturesCube[];
      layout (set = 0, binding = 1) uniform sampler kSamplers[];
      layout (set = 1, binding = 1) uniform samplerShadow kSamplersShadow[];
      )";
#endif

      sourcePatched += R"(
      vec4 textureBindless2D(uint textureid, uint samplerid, vec2 uv) {
        return texture(sampler2D(kTextures2D[textureid], kSamplers[samplerid]), uv);
      }
      vec4 textureBindless2DLod(uint textureid, uint samplerid, vec2 uv, float lod) {
        return textureLod(sampler2D(kTextures2D[textureid], kSamplers[samplerid]), uv, lod);
      }
      float textureBindless2DShadow(uint textureid, uint samplerid, vec3 uvw) {
        return texture(sampler2DShadow(kTextures2D[textureid], kSamplersShadow[samplerid]), uvw);
      }
      ivec2 textureBindlessSize2D(uint textureid) {
        return textureSize(kTextures2D[textureid], 0);
      }
      vec4 textureBindlessCube(uint textureid, uint samplerid, vec3 uvw) {
        return texture(samplerCube(kTexturesCube[textureid], kSamplers[samplerid]), uvw);
      }
      vec4 textureBindlessCubeLod(uint textureid, uint samplerid, vec3 uvw, float lod) {
        return textureLod(samplerCube(kTexturesCube[textureid], kSamplers[samplerid]), uvw, lod);
      }
      int textureBindlessQueryLevels2D(uint textureid) {
        return textureQueryLevels(kTextures2D[textureid]);
      }
      )";
    }
    sourcePatched += source;
    source = sourcePatched.c_str();
  }

  const glslang_resource_t glslangResource = lvk::getGlslangResource(getVkPhysicalDeviceProperties().limits);

  VkShaderModule vkShaderModule = VK_NULL_HANDLE;
  const Result result = lvk::compileShader(vkDevice_, vkStage, source, &vkShaderModule, &glslangResource);

  Result::setResult(outResult, result);

  if (!result.isOk()) {
    return VK_NULL_HANDLE;
  }

  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)vkShaderModule, debugName));

  LVK_ASSERT(vkShaderModule != VK_NULL_HANDLE);

  return vkShaderModule;
}

lvk::Format lvk::VulkanContext::getSwapchainFormat() const {
  if (!hasSwapchain()) {
    return Format_Invalid;
  }

  return vkFormatToFormat(swapchain_->getSurfaceFormat().format);
}

uint32_t lvk::VulkanContext::getNumSwapchainImages() const {
  return hasSwapchain() ? swapchain_->getNumSwapchainImages() : 0;
}

lvk::TextureHandle lvk::VulkanContext::getCurrentSwapchainTexture() {
  LVK_PROFILER_FUNCTION();

  if (!hasSwapchain()) {
    return {};
  }

  TextureHandle tex = swapchain_->getCurrentTexture();

  if (!LVK_VERIFY(tex.valid())) {
    LVK_ASSERT_MSG(false, "Swapchain has no valid texture");
    return {};
  }

  LVK_ASSERT_MSG(texturesPool_.get(tex)->image_->vkImageFormat_ != VK_FORMAT_UNDEFINED, "Invalid image format");

  return tex;
}

void lvk::VulkanContext::recreateSwapchain(int newWidth, int newHeight) {
  initSwapchain(newWidth, newHeight);
}

void lvk::VulkanContext::createInstance() {
  vkInstance_ = VK_NULL_HANDLE;

  const char* instanceExtensionNames[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#if defined(_WIN32)
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__linux__)
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(__APPLE__)
    VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
#endif
    VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME // enabled only for validation
  };

  const uint32_t numInstanceExtensions = config_.enableValidation ? (uint32_t)LVK_ARRAY_NUM_ELEMENTS(instanceExtensionNames)
                                                                  : (uint32_t)LVK_ARRAY_NUM_ELEMENTS(instanceExtensionNames) - 1;

  const VkValidationFeatureEnableEXT validationFeaturesEnabled[] = {
      VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
  };

  const VkValidationFeaturesEXT features = {
      .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
      .pNext = nullptr,
      .enabledValidationFeatureCount = config_.enableValidation ? (uint32_t)LVK_ARRAY_NUM_ELEMENTS(validationFeaturesEnabled) : 0u,
      .pEnabledValidationFeatures = config_.enableValidation ? validationFeaturesEnabled : nullptr,
  };

  const VkApplicationInfo appInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .pApplicationName = "IGL/Vulkan",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "IGL/Vulkan",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_3,
  };

  VkInstanceCreateFlags flags = 0;
#if defined(__APPLE__)
  flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
  const VkInstanceCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = config_.enableValidation ? &features : nullptr,
      .flags = flags,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = config_.enableValidation ? (uint32_t)LVK_ARRAY_NUM_ELEMENTS(kDefaultValidationLayers) : 0,
      .ppEnabledLayerNames = config_.enableValidation ? kDefaultValidationLayers : nullptr,
      .enabledExtensionCount = numInstanceExtensions,
      .ppEnabledExtensionNames = instanceExtensionNames,
  };
  VK_ASSERT(vkCreateInstance(&ci, nullptr, &vkInstance_));

  volkLoadInstance(vkInstance_);

  // Update MoltenVK configuration.
#if defined(__APPLE__)
  void* moltenVkModule = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
  PFN_vkGetMoltenVKConfigurationMVK vkGetMoltenVKConfigurationMVK =
      (PFN_vkGetMoltenVKConfigurationMVK)dlsym(moltenVkModule, "vkGetMoltenVKConfigurationMVK");
  PFN_vkSetMoltenVKConfigurationMVK vkSetMoltenVKConfigurationMVK =
      (PFN_vkSetMoltenVKConfigurationMVK)dlsym(moltenVkModule, "vkSetMoltenVKConfigurationMVK");

  MVKConfiguration configuration = {};
  size_t configurationSize = sizeof(MVKConfiguration);
  VK_ASSERT(vkGetMoltenVKConfigurationMVK(vkInstance_, &configuration, &configurationSize));

  configuration.useMetalArgumentBuffers = MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS_ALWAYS;
  VK_ASSERT(vkSetMoltenVKConfigurationMVK(vkInstance_, &configuration, &configurationSize));
#endif

  // debug messenger
  {
    const VkDebugUtilsMessengerCreateInfoEXT ci = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &vulkanDebugCallback,
        .pUserData = this,
    };
    VK_ASSERT(vkCreateDebugUtilsMessengerEXT(vkInstance_, &ci, nullptr, &vkDebugUtilsMessenger_));
  }

  uint32_t count = 0;
  VK_ASSERT(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));

  std::vector<VkExtensionProperties> allInstanceExtensions(count);

  VK_ASSERT(vkEnumerateInstanceExtensionProperties(nullptr, &count, allInstanceExtensions.data()));

  // log available instance extensions
  LLOGL("\nVulkan instance extensions:\n");

  for (const auto& extension : allInstanceExtensions) {
    LLOGL("  %s\n", extension.extensionName);
  }
}

void lvk::VulkanContext::createSurface(void* window, void* display) {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
  const VkWin32SurfaceCreateInfoKHR ci = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = GetModuleHandle(nullptr),
      .hwnd = (HWND)window,
  };
  VK_ASSERT(vkCreateWin32SurfaceKHR(vkInstance_, &ci, nullptr, &vkSurface_));
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
  const VkXlibSurfaceCreateInfoKHR ci = {
      .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
      .flags = 0,
      .dpy = (Display*)display,
      .window = (Window)window,
  };
  VK_ASSERT(vkCreateXlibSurfaceKHR(vkInstance_, &ci, nullptr, &vkSurface_));
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
  const VkMacOSSurfaceCreateInfoMVK ci = {
      .sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK,
      .flags = 0,
      .pView = window,
  };
  VK_ASSERT(vkCreateMacOSSurfaceMVK(vkInstance_, &ci, nullptr, &vkSurface_));
#else
#error Implement for other platforms
#endif
}

uint32_t lvk::VulkanContext::queryDevices(HWDeviceType deviceType, HWDeviceDesc* outDevices, uint32_t maxOutDevices) {
  // Physical devices
  uint32_t deviceCount = 0;
  VK_ASSERT(vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, nullptr));
  std::vector<VkPhysicalDevice> vkDevices(deviceCount);
  VK_ASSERT(vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, vkDevices.data()));

  auto convertVulkanDeviceTypeToIGL = [](VkPhysicalDeviceType vkDeviceType) -> HWDeviceType {
    switch (vkDeviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      return HWDeviceType_Integrated;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      return HWDeviceType_Discrete;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      return HWDeviceType_External;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      return HWDeviceType_Software;
    default:
      return HWDeviceType_Software;
    }
  };

  const HWDeviceType desiredDeviceType = deviceType;

  uint32_t numCompatibleDevices = 0;

  for (uint32_t i = 0; i < deviceCount; ++i) {
    VkPhysicalDevice physicalDevice = vkDevices[i];
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    const HWDeviceType deviceType = convertVulkanDeviceTypeToIGL(deviceProperties.deviceType);

    // filter non-suitable hardware devices
    if (desiredDeviceType != HWDeviceType_Software && deviceType != desiredDeviceType) {
      continue;
    }

    if (outDevices && numCompatibleDevices < maxOutDevices) {
      outDevices[numCompatibleDevices] = {.guid = (uintptr_t)vkDevices[i], .type = deviceType};
      strncpy(outDevices[numCompatibleDevices].name, deviceProperties.deviceName, strlen(deviceProperties.deviceName));
      numCompatibleDevices++;
    }
  }

  return numCompatibleDevices;
}

lvk::Result lvk::VulkanContext::initContext(const HWDeviceDesc& desc) {
  if (desc.guid == 0UL) {
    LLOGW("Invalid hardwareGuid(%lu)", desc.guid);
    return Result(Result::Code::RuntimeError, "Vulkan is not supported");
  }

  vkPhysicalDevice_ = (VkPhysicalDevice)desc.guid;

  useStaging_ = !isHostVisibleSingleHeapMemory(vkPhysicalDevice_);

  vkGetPhysicalDeviceFeatures2(vkPhysicalDevice_, &vkFeatures10_);
  vkGetPhysicalDeviceProperties2(vkPhysicalDevice_, &vkPhysicalDeviceProperties2_);

  const uint32_t apiVersion = vkPhysicalDeviceProperties2_.properties.apiVersion;

  LLOGL("Vulkan physical device: %s\n", vkPhysicalDeviceProperties2_.properties.deviceName);
  LLOGL("           API version: %i.%i.%i.%i\n",
        VK_API_VERSION_MAJOR(apiVersion),
        VK_API_VERSION_MINOR(apiVersion),
        VK_API_VERSION_PATCH(apiVersion),
        VK_API_VERSION_VARIANT(apiVersion));
  LLOGL("           Driver info: %s %s\n", vkPhysicalDeviceDriverProperties_.driverName, vkPhysicalDeviceDriverProperties_.driverInfo);

  uint32_t count = 0;
  VK_ASSERT(vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_, nullptr, &count, nullptr));

  std::vector<VkExtensionProperties> allPhysicalDeviceExtensions(count);

  VK_ASSERT(vkEnumerateDeviceExtensionProperties(vkPhysicalDevice_, nullptr, &count, allPhysicalDeviceExtensions.data()));

  LLOGL("Vulkan physical device extensions:\n");

  // log available physical device extensions
  for (const auto& ext : allPhysicalDeviceExtensions) {
    LLOGL("  %s\n", ext.extensionName);
  }

  deviceQueues_.graphicsQueueFamilyIndex = lvk::findQueueFamilyIndex(vkPhysicalDevice_, VK_QUEUE_GRAPHICS_BIT);
  deviceQueues_.computeQueueFamilyIndex = lvk::findQueueFamilyIndex(vkPhysicalDevice_, VK_QUEUE_COMPUTE_BIT);

  if (deviceQueues_.graphicsQueueFamilyIndex == DeviceQueues::INVALID) {
    LLOGW("VK_QUEUE_GRAPHICS_BIT is not supported");
    return Result(Result::Code::RuntimeError, "VK_QUEUE_GRAPHICS_BIT is not supported");
  }

  if (deviceQueues_.computeQueueFamilyIndex == DeviceQueues::INVALID) {
    LLOGW("VK_QUEUE_COMPUTE_BIT is not supported");
    return Result(Result::Code::RuntimeError, "VK_QUEUE_COMPUTE_BIT is not supported");
  }

  const float queuePriority = 1.0f;

  const VkDeviceQueueCreateInfo ciQueue[2] = {
      {
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = deviceQueues_.graphicsQueueFamilyIndex,
          .queueCount = 1,
          .pQueuePriorities = &queuePriority,
      },
      {
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = deviceQueues_.computeQueueFamilyIndex,
          .queueCount = 1,
          .pQueuePriorities = &queuePriority,
      },
  };
  const uint32_t numQueues = ciQueue[0].queueFamilyIndex == ciQueue[1].queueFamilyIndex ? 1 : 2;

  const char* deviceExtensionNames[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#if defined(LVK_WITH_TRACY)
    VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
#endif
#if defined(__APPLE__)
    // All supported Vulkan 1.3 extensions
    // https://github.com/KhronosGroup/MoltenVK/issues/1930
    VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
    VK_EXT_4444_FORMATS_EXTENSION_NAME,
    VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME,
    VK_EXT_INLINE_UNIFORM_BLOCK_EXTENSION_NAME,
    VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME,
    VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME,
    VK_EXT_PRIVATE_DATA_EXTENSION_NAME,
    VK_EXT_SHADER_DEMOTE_TO_HELPER_INVOCATION_EXTENSION_NAME,
    VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME,
    VK_EXT_TEXEL_BUFFER_ALIGNMENT_EXTENSION_NAME,
    VK_EXT_TEXTURE_COMPRESSION_ASTC_HDR_EXTENSION_NAME,
    "VK_KHR_portability_subset",
#endif
  };

  VkPhysicalDeviceFeatures deviceFeatures10 = {
#ifndef __APPLE__
      .geometryShader = VK_TRUE,
#endif
      .multiDrawIndirect = VK_TRUE,
      .drawIndirectFirstInstance = VK_TRUE,
      .depthBiasClamp = VK_TRUE,
      .fillModeNonSolid = VK_TRUE,
      .samplerAnisotropy = VK_TRUE,
      .textureCompressionBC = VK_TRUE,
  };
  VkPhysicalDeviceVulkan11Features deviceFeatures11 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .storageBuffer16BitAccess = VK_TRUE,
      .shaderDrawParameters = VK_TRUE,
  };
  VkPhysicalDeviceVulkan12Features deviceFeatures12 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &deviceFeatures11,
      .descriptorIndexing = VK_TRUE,
      .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
      .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
      .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
      .descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
      .descriptorBindingPartiallyBound = VK_TRUE,
      .descriptorBindingVariableDescriptorCount = VK_TRUE,
      .runtimeDescriptorArray = VK_TRUE,
      .uniformBufferStandardLayout = VK_TRUE,
      .timelineSemaphore = VK_TRUE,
      .bufferDeviceAddress = VK_TRUE,
  };
  VkPhysicalDeviceVulkan13Features deviceFeatures13 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = &deviceFeatures12,
      .subgroupSizeControl = VK_TRUE,
      .synchronization2 = VK_TRUE,
      .dynamicRendering = VK_TRUE,
      .maintenance4 = VK_TRUE,
  };
  const VkDeviceCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &deviceFeatures13,
      .queueCreateInfoCount = numQueues,
      .pQueueCreateInfos = ciQueue,
      .enabledLayerCount = (uint32_t)LVK_ARRAY_NUM_ELEMENTS(kDefaultValidationLayers),
      .ppEnabledLayerNames = kDefaultValidationLayers,
      .enabledExtensionCount = (uint32_t)LVK_ARRAY_NUM_ELEMENTS(deviceExtensionNames),
      .ppEnabledExtensionNames = deviceExtensionNames,
      .pEnabledFeatures = &deviceFeatures10,
  };

  // check extensions
  {
    std::vector<VkExtensionProperties> props;
    getDeviceExtensionProps(vkPhysicalDevice_, props);
    for (const char* layer : kDefaultValidationLayers) {
      getDeviceExtensionProps(vkPhysicalDevice_, props, layer);
    }
    std::string missingExtensions;
    for (const char* ext : deviceExtensionNames) {
      if (!hasExtension(ext, props))
        missingExtensions += "\n   " + std::string(ext);
    }
    if (!missingExtensions.empty()) {
      MINILOG_LOG_PROC(minilog::FatalError, "Missing Vulkan device extensions: %s\n", missingExtensions.c_str());
      assert(false);
      return Result(Result::Code::RuntimeError);
    }
  }

  // check features
  {
    std::string missingFeatures;
#define CHECK_VULKAN_FEATURE(reqFeatures, availFeatures, feature, version)     \
  if ((reqFeatures.feature == VK_TRUE) && (availFeatures.feature == VK_FALSE)) \
    missingFeatures.append("\n   " version " ." #feature);
#define CHECK_FEATURE_1_0(feature) CHECK_VULKAN_FEATURE(deviceFeatures10, vkFeatures10_.features, feature, "1.0 ");
    CHECK_FEATURE_1_0(robustBufferAccess);
    CHECK_FEATURE_1_0(fullDrawIndexUint32);
    CHECK_FEATURE_1_0(imageCubeArray);
    CHECK_FEATURE_1_0(independentBlend);
    CHECK_FEATURE_1_0(geometryShader);
    CHECK_FEATURE_1_0(tessellationShader);
    CHECK_FEATURE_1_0(sampleRateShading);
    CHECK_FEATURE_1_0(dualSrcBlend);
    CHECK_FEATURE_1_0(logicOp);
    CHECK_FEATURE_1_0(multiDrawIndirect);
    CHECK_FEATURE_1_0(drawIndirectFirstInstance);
    CHECK_FEATURE_1_0(depthClamp);
    CHECK_FEATURE_1_0(depthBiasClamp);
    CHECK_FEATURE_1_0(fillModeNonSolid);
    CHECK_FEATURE_1_0(depthBounds);
    CHECK_FEATURE_1_0(wideLines);
    CHECK_FEATURE_1_0(largePoints);
    CHECK_FEATURE_1_0(alphaToOne);
    CHECK_FEATURE_1_0(multiViewport);
    CHECK_FEATURE_1_0(samplerAnisotropy);
    CHECK_FEATURE_1_0(textureCompressionETC2);
    CHECK_FEATURE_1_0(textureCompressionASTC_LDR);
    CHECK_FEATURE_1_0(textureCompressionBC);
    CHECK_FEATURE_1_0(occlusionQueryPrecise);
    CHECK_FEATURE_1_0(pipelineStatisticsQuery);
    CHECK_FEATURE_1_0(vertexPipelineStoresAndAtomics);
    CHECK_FEATURE_1_0(fragmentStoresAndAtomics);
    CHECK_FEATURE_1_0(shaderTessellationAndGeometryPointSize);
    CHECK_FEATURE_1_0(shaderImageGatherExtended);
    CHECK_FEATURE_1_0(shaderStorageImageExtendedFormats);
    CHECK_FEATURE_1_0(shaderStorageImageMultisample);
    CHECK_FEATURE_1_0(shaderStorageImageReadWithoutFormat);
    CHECK_FEATURE_1_0(shaderStorageImageWriteWithoutFormat);
    CHECK_FEATURE_1_0(shaderUniformBufferArrayDynamicIndexing);
    CHECK_FEATURE_1_0(shaderSampledImageArrayDynamicIndexing);
    CHECK_FEATURE_1_0(shaderStorageBufferArrayDynamicIndexing);
    CHECK_FEATURE_1_0(shaderStorageImageArrayDynamicIndexing);
    CHECK_FEATURE_1_0(shaderClipDistance);
    CHECK_FEATURE_1_0(shaderCullDistance);
    CHECK_FEATURE_1_0(shaderFloat64);
    CHECK_FEATURE_1_0(shaderInt64);
    CHECK_FEATURE_1_0(shaderInt16);
    CHECK_FEATURE_1_0(shaderResourceResidency);
    CHECK_FEATURE_1_0(shaderResourceMinLod);
    CHECK_FEATURE_1_0(sparseBinding);
    CHECK_FEATURE_1_0(sparseResidencyBuffer);
    CHECK_FEATURE_1_0(sparseResidencyImage2D);
    CHECK_FEATURE_1_0(sparseResidencyImage3D);
    CHECK_FEATURE_1_0(sparseResidency2Samples);
    CHECK_FEATURE_1_0(sparseResidency4Samples);
    CHECK_FEATURE_1_0(sparseResidency8Samples);
    CHECK_FEATURE_1_0(sparseResidency16Samples);
    CHECK_FEATURE_1_0(sparseResidencyAliased);
    CHECK_FEATURE_1_0(variableMultisampleRate);
    CHECK_FEATURE_1_0(inheritedQueries);
#undef CHECK_FEATURE_1_0
#define CHECK_FEATURE_1_1(feature) CHECK_VULKAN_FEATURE(deviceFeatures11, vkFeatures11_, feature, "1.1 ");
    CHECK_FEATURE_1_1(storageBuffer16BitAccess);
    CHECK_FEATURE_1_1(uniformAndStorageBuffer16BitAccess);
    CHECK_FEATURE_1_1(storagePushConstant16);
    CHECK_FEATURE_1_1(storageInputOutput16);
    CHECK_FEATURE_1_1(multiview);
    CHECK_FEATURE_1_1(multiviewGeometryShader);
    CHECK_FEATURE_1_1(multiviewTessellationShader);
    CHECK_FEATURE_1_1(variablePointersStorageBuffer);
    CHECK_FEATURE_1_1(variablePointers);
    CHECK_FEATURE_1_1(protectedMemory);
    CHECK_FEATURE_1_1(samplerYcbcrConversion);
    CHECK_FEATURE_1_1(shaderDrawParameters);
#undef CHECK_FEATURE_1_1
#define CHECK_FEATURE_1_2(feature) CHECK_VULKAN_FEATURE(deviceFeatures12, vkFeatures12_, feature, "1.2 ");
    CHECK_FEATURE_1_2(samplerMirrorClampToEdge);
    CHECK_FEATURE_1_2(drawIndirectCount);
    CHECK_FEATURE_1_2(storageBuffer8BitAccess);
    CHECK_FEATURE_1_2(uniformAndStorageBuffer8BitAccess);
    CHECK_FEATURE_1_2(storagePushConstant8);
    CHECK_FEATURE_1_2(shaderBufferInt64Atomics);
    CHECK_FEATURE_1_2(shaderSharedInt64Atomics);
    CHECK_FEATURE_1_2(shaderFloat16);
    CHECK_FEATURE_1_2(shaderInt8);
    CHECK_FEATURE_1_2(descriptorIndexing);
    CHECK_FEATURE_1_2(shaderInputAttachmentArrayDynamicIndexing);
    CHECK_FEATURE_1_2(shaderUniformTexelBufferArrayDynamicIndexing);
    CHECK_FEATURE_1_2(shaderStorageTexelBufferArrayDynamicIndexing);
    CHECK_FEATURE_1_2(shaderUniformBufferArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderSampledImageArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderStorageBufferArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderStorageImageArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderInputAttachmentArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderUniformTexelBufferArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderStorageTexelBufferArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(descriptorBindingUniformBufferUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingSampledImageUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingStorageImageUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingStorageBufferUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingUniformTexelBufferUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingStorageTexelBufferUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingUpdateUnusedWhilePending);
    CHECK_FEATURE_1_2(descriptorBindingPartiallyBound);
    CHECK_FEATURE_1_2(descriptorBindingVariableDescriptorCount);
    CHECK_FEATURE_1_2(runtimeDescriptorArray);
    CHECK_FEATURE_1_2(samplerFilterMinmax);
    CHECK_FEATURE_1_2(scalarBlockLayout);
    CHECK_FEATURE_1_2(imagelessFramebuffer);
    CHECK_FEATURE_1_2(uniformBufferStandardLayout);
    CHECK_FEATURE_1_2(shaderSubgroupExtendedTypes);
    CHECK_FEATURE_1_2(separateDepthStencilLayouts);
    CHECK_FEATURE_1_2(hostQueryReset);
    CHECK_FEATURE_1_2(timelineSemaphore);
    CHECK_FEATURE_1_2(bufferDeviceAddress);
    CHECK_FEATURE_1_2(bufferDeviceAddressCaptureReplay);
    CHECK_FEATURE_1_2(bufferDeviceAddressMultiDevice);
    CHECK_FEATURE_1_2(vulkanMemoryModel);
    CHECK_FEATURE_1_2(vulkanMemoryModelDeviceScope);
    CHECK_FEATURE_1_2(vulkanMemoryModelAvailabilityVisibilityChains);
    CHECK_FEATURE_1_2(shaderOutputViewportIndex);
    CHECK_FEATURE_1_2(shaderOutputLayer);
    CHECK_FEATURE_1_2(subgroupBroadcastDynamicId);
#undef CHECK_FEATURE_1_2
#define CHECK_FEATURE_1_3(feature) CHECK_VULKAN_FEATURE(deviceFeatures13, vkFeatures13_, feature, "1.3 ");
    CHECK_FEATURE_1_3(robustImageAccess);
    CHECK_FEATURE_1_3(inlineUniformBlock);
    CHECK_FEATURE_1_3(descriptorBindingInlineUniformBlockUpdateAfterBind);
    CHECK_FEATURE_1_3(pipelineCreationCacheControl);
    CHECK_FEATURE_1_3(privateData);
    CHECK_FEATURE_1_3(shaderDemoteToHelperInvocation);
    CHECK_FEATURE_1_3(shaderTerminateInvocation);
    CHECK_FEATURE_1_3(subgroupSizeControl);
    CHECK_FEATURE_1_3(computeFullSubgroups);
    CHECK_FEATURE_1_3(synchronization2);
    CHECK_FEATURE_1_3(textureCompressionASTC_HDR);
    CHECK_FEATURE_1_3(shaderZeroInitializeWorkgroupMemory);
    CHECK_FEATURE_1_3(dynamicRendering);
    CHECK_FEATURE_1_3(shaderIntegerDotProduct);
    CHECK_FEATURE_1_3(maintenance4);
#undef CHECK_FEATURE_1_3
    if (!missingFeatures.empty()) {
      MINILOG_LOG_PROC(minilog::FatalError, "Missing Vulkan features: %s\n", missingFeatures.c_str());
      // Do not exit here in case of MoltenVK, some 1.3 features are available via extensions.
#ifndef __APPLE__
      assert(false);
      return Result(Result::Code::RuntimeError);
#endif
    }
  }

  VK_ASSERT_RETURN(vkCreateDevice(vkPhysicalDevice_, &ci, nullptr, &vkDevice_));

  volkLoadDevice(vkDevice_);

#if defined(__APPLE__)
  vkCmdBeginRendering = vkCmdBeginRenderingKHR;
  vkCmdEndRendering = vkCmdEndRenderingKHR;
#endif

  vkGetDeviceQueue(vkDevice_, deviceQueues_.graphicsQueueFamilyIndex, 0, &deviceQueues_.graphicsQueue);
  vkGetDeviceQueue(vkDevice_, deviceQueues_.computeQueueFamilyIndex, 0, &deviceQueues_.computeQueue);

  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_DEVICE, (uint64_t)vkDevice_, "Device: VulkanContext::vkDevice_"));

  immediate_ =
      std::make_unique<lvk::VulkanImmediateCommands>(vkDevice_, deviceQueues_.graphicsQueueFamilyIndex, "VulkanContext::immediate_");

  // create Vulkan pipeline cache
  {
    const VkPipelineCacheCreateInfo ci = {
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        nullptr,
        VkPipelineCacheCreateFlags(0),
        config_.pipelineCacheDataSize,
        config_.pipelineCacheData,
    };
    vkCreatePipelineCache(vkDevice_, &ci, nullptr, &pipelineCache_);
  }

  if (LVK_VULKAN_USE_VMA) {
    pimpl_->vma_ = lvk::createVmaAllocator(vkPhysicalDevice_, vkDevice_, vkInstance_, apiVersion);
    LVK_ASSERT(pimpl_->vma_ != VK_NULL_HANDLE);
  }

  stagingDevice_ = std::make_unique<lvk::VulkanStagingDevice>(*this);

  // default texture
  {
    const VkFormat dummyTextureFormat = VK_FORMAT_R8G8B8A8_UNORM;
    const VkMemoryPropertyFlags memFlags = useStaging_ ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    Result result;
    auto image = createImage(VK_IMAGE_TYPE_2D,
                             VkExtent3D{1, 1, 1},
                             dummyTextureFormat,
                             1,
                             1,
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                             memFlags,
                             0,
                             VK_SAMPLE_COUNT_1_BIT,
                             &result,
                             "Image: dummy 1x1");
    if (!LVK_VERIFY(result.isOk())) {
      return result;
    }
    if (!LVK_VERIFY(image.get())) {
      return Result(Result::Code::RuntimeError, "Cannot create VulkanImage");
    }
    VkImageView imageView = image->createImageView(
        VK_IMAGE_VIEW_TYPE_2D, dummyTextureFormat, VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 1, "Image View: dummy 1x1");
    if (!LVK_VERIFY(imageView != VK_NULL_HANDLE)) {
      return Result(Result::Code::RuntimeError, "Cannot create VkImageView");
    }
    VulkanTexture dummyTexture(std::move(image), imageView);
    const uint32_t pixel = 0xFF000000;
    const void* data[] = {&pixel};
    const VkRect2D imageRegion = {.offset = {.x = 0, .y = 0}, .extent = {.width = 1, .height = 1}};
    stagingDevice_->imageData2D(*dummyTexture.image_.get(), imageRegion, 0, 1, 0, 1, dummyTextureFormat, data);
    texturesPool_.create(std::move(dummyTexture));
    LVK_ASSERT(texturesPool_.numObjects() == 1);
  }

  // default sampler
  LVK_ASSERT(samplersPool_.numObjects() == 0);
  createSampler(
      {
          .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .magFilter = VK_FILTER_LINEAR,
          .minFilter = VK_FILTER_LINEAR,
          .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
          .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
          .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
          .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
          .mipLodBias = 0.0f,
          .anisotropyEnable = VK_FALSE,
          .maxAnisotropy = 0.0f,
          .compareEnable = VK_FALSE,
          .compareOp = VK_COMPARE_OP_ALWAYS,
          .minLod = 0.0f,
          .maxLod = 0.0f,
          .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
          .unnormalizedCoordinates = VK_FALSE,
      },
      nullptr,
      "Sampler: default");

  growDescriptorPool(currentMaxTextures_, currentMaxSamplers_);

  querySurfaceCapabilities();

  return Result();
}

lvk::Result lvk::VulkanContext::initSwapchain(uint32_t width, uint32_t height) {
  if (!vkDevice_ || !immediate_) {
    LLOGW("Call initContext() first");
    return Result(Result::Code::RuntimeError, "Call initContext() first");
  }

  if (swapchain_) {
    // destroy the old swapchain first
    VK_ASSERT(vkDeviceWaitIdle(vkDevice_));
    swapchain_ = nullptr;
  }

  if (!width || !height) {
    return Result();
  }

  swapchain_ = std::make_unique<lvk::VulkanSwapchain>(*this, width, height);

  return swapchain_ ? Result() : Result(Result::Code::RuntimeError, "Failed to create swapchain");
}

lvk::Result lvk::VulkanContext::growDescriptorPool(uint32_t maxTextures, uint32_t maxSamplers) {
  currentMaxTextures_ = maxTextures;
  currentMaxSamplers_ = maxSamplers;

#if LVK_VULKAN_PRINT_COMMANDS
  LLOGL("growDescriptorPool(%u, %u)\n", maxTextures, maxSamplers);
#endif // LVK_VULKAN_PRINT_COMMANDS

  if (!LVK_VERIFY(maxTextures <= vkPhysicalDeviceVulkan12Properties_.maxDescriptorSetUpdateAfterBindSampledImages)) {
    LLOGW("Max Textures exceeded: %u (max %u)",
          maxTextures,
          vkPhysicalDeviceVulkan12Properties_.maxDescriptorSetUpdateAfterBindSampledImages);
  }

  if (!LVK_VERIFY(maxSamplers <= vkPhysicalDeviceVulkan12Properties_.maxDescriptorSetUpdateAfterBindSamplers)) {
    LLOGW("Max Samplers exceeded %u (max %u)", maxSamplers, vkPhysicalDeviceVulkan12Properties_.maxDescriptorSetUpdateAfterBindSamplers);
  }

  if (vkDSLBindless_ != VK_NULL_HANDLE) {
    deferredTask(
        std::packaged_task<void()>([device = vkDevice_, dsl = vkDSLBindless_]() { vkDestroyDescriptorSetLayout(device, dsl, nullptr); }));
  }
  if (vkDPBindless_ != VK_NULL_HANDLE) {
    deferredTask(std::packaged_task<void()>([device = vkDevice_, dp = vkDPBindless_]() { vkDestroyDescriptorPool(device, dp, nullptr); }));
  }
  if (vkPipelineLayout_ != VK_NULL_HANDLE) {
    deferredTask(std::packaged_task<void()>(
        [device = vkDevice_, layout = vkPipelineLayout_]() { vkDestroyPipelineLayout(device, layout, nullptr); }));
  }

  // create default descriptor set layout which is going to be shared by graphics pipelines
  const VkDescriptorSetLayoutBinding bindings[kBinding_NumBindings] = {
      lvk::getDSLBinding(kBinding_Textures, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, maxTextures),
      lvk::getDSLBinding(kBinding_Samplers, VK_DESCRIPTOR_TYPE_SAMPLER, maxSamplers),
      lvk::getDSLBinding(kBinding_StorageImages, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, maxTextures),
  };
  const uint32_t flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
  VkDescriptorBindingFlags bindingFlags[kBinding_NumBindings];
  for (int i = 0; i < kBinding_NumBindings; ++i) {
    bindingFlags[i] = flags;
  }
  const VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlagsCI = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
      .bindingCount = kBinding_NumBindings,
      .pBindingFlags = bindingFlags,
  };
  const VkDescriptorSetLayoutCreateInfo dslci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = &setLayoutBindingFlagsCI,
      .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
      .bindingCount = kBinding_NumBindings,
      .pBindings = bindings,
  };
  VK_ASSERT(vkCreateDescriptorSetLayout(vkDevice_, &dslci, nullptr, &vkDSLBindless_));
  VK_ASSERT(lvk::setDebugObjectName(
      vkDevice_, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)vkDSLBindless_, "Descriptor Set Layout: VulkanContext::vkDSLBindless_"));

  // create default descriptor pool and allocate 1 descriptor set
  const VkDescriptorPoolSize poolSizes[kBinding_NumBindings]{
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, (uint32_t)LVK_ARRAY_NUM_ELEMENTS(BindlessDescriptorSet::ds) * maxTextures},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, (uint32_t)LVK_ARRAY_NUM_ELEMENTS(BindlessDescriptorSet::ds) * maxSamplers},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, (uint32_t)LVK_ARRAY_NUM_ELEMENTS(BindlessDescriptorSet::ds) * maxTextures},
  };
  const VkDescriptorPoolCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
      .maxSets = (uint32_t)LVK_ARRAY_NUM_ELEMENTS(BindlessDescriptorSet::ds),
      .poolSizeCount = kBinding_NumBindings,
      .pPoolSizes = poolSizes,
  };
  VK_ASSERT_RETURN(vkCreateDescriptorPool(vkDevice_, &ci, nullptr, &vkDPBindless_));

  VkDescriptorSetLayout dsLayouts[LVK_ARRAY_NUM_ELEMENTS(BindlessDescriptorSet::ds)];
  for (uint32_t i = 0; i != LVK_ARRAY_NUM_ELEMENTS(BindlessDescriptorSet::ds); i++) {
    dsLayouts[i] = vkDSLBindless_;
  }

  {
    const VkDescriptorSetAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vkDPBindless_,
        .descriptorSetCount = (uint32_t)LVK_ARRAY_NUM_ELEMENTS(BindlessDescriptorSet::ds),
        .pSetLayouts = &dsLayouts[0],
    };
    VK_ASSERT_RETURN(vkAllocateDescriptorSets(vkDevice_, &ai, &bindlessDSets_.ds[0]));
  }

  // create pipeline layout
  {
    // maxPushConstantsSize is guaranteed to be at least 128 bytes
    // https://www.khronos.org/registry/vulkan/specs/1.3/html/vkspec.html#features-limits
    // Table 32. Required Limits
    const uint32_t kPushConstantsSize = 128;
    const VkPhysicalDeviceLimits& limits = getVkPhysicalDeviceProperties().limits;
    if (!LVK_VERIFY(kPushConstantsSize <= limits.maxPushConstantsSize)) {
      LLOGW("Push constants size exceeded %u (max %u bytes)", kPushConstantsSize, limits.maxPushConstantsSize);
    }

    const VkPushConstantRange range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = kPushConstantsSize,
    };
    const VkPipelineLayoutCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = (uint32_t)LVK_ARRAY_NUM_ELEMENTS(BindlessDescriptorSet::ds),
        .pSetLayouts = &dsLayouts[0],
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &range,
    };
    VK_ASSERT(vkCreatePipelineLayout(vkDevice_, &ci, nullptr, &vkPipelineLayout_));
    VK_ASSERT(lvk::setDebugObjectName(
        vkDevice_, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)vkPipelineLayout_, "Pipeline Layout: VulkanContext::pipelineLayout_"));
  }

  return Result();
}

lvk::BufferHandle lvk::VulkanContext::createBuffer(VkDeviceSize bufferSize,
                                                   VkBufferUsageFlags usageFlags,
                                                   VkMemoryPropertyFlags memFlags,
                                                   lvk::Result* outResult,
                                                   const char* debugName) {
#define ENSURE_BUFFER_SIZE(flag, maxSize)                                                             \
  if (usageFlags & flag) {                                                                            \
    if (!LVK_VERIFY(bufferSize <= maxSize)) {                                                         \
      Result::setResult(outResult, Result(Result::Code::RuntimeError, "Buffer size exceeded" #flag)); \
      return {};                                                                                      \
    }                                                                                                 \
  }

  const VkPhysicalDeviceLimits& limits = getVkPhysicalDeviceProperties().limits;

  ENSURE_BUFFER_SIZE(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, limits.maxUniformBufferRange);
  // any buffer
  ENSURE_BUFFER_SIZE(VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM, limits.maxStorageBufferRange);
#undef ENSURE_BUFFER_SIZE

  return buffersPool_.create(VulkanBuffer(this, vkDevice_, bufferSize, usageFlags, memFlags, debugName));
}

std::shared_ptr<lvk::VulkanImage> lvk::VulkanContext::createImage(VkImageType imageType,
                                                                  VkExtent3D extent,
                                                                  VkFormat format,
                                                                  uint32_t numLevels,
                                                                  uint32_t numLayers,
                                                                  VkImageTiling tiling,
                                                                  VkImageUsageFlags usageFlags,
                                                                  VkMemoryPropertyFlags memFlags,
                                                                  VkImageCreateFlags flags,
                                                                  VkSampleCountFlagBits samples,
                                                                  lvk::Result* outResult,
                                                                  const char* debugName) {
  if (!validateImageLimits(imageType, samples, extent, getVkPhysicalDeviceProperties().limits, outResult)) {
    return nullptr;
  }

  return std::make_shared<VulkanImage>(
      *this, vkDevice_, extent, imageType, format, numLevels, numLayers, tiling, usageFlags, memFlags, flags, samples, debugName);
}

void lvk::VulkanContext::bindDefaultDescriptorSets(VkCommandBuffer cmdBuf, VkPipelineBindPoint bindPoint) const {
  LVK_PROFILER_FUNCTION();
  vkCmdBindDescriptorSets(cmdBuf,
                          bindPoint,
                          vkPipelineLayout_,
                          0,
                          (uint32_t)LVK_ARRAY_NUM_ELEMENTS(BindlessDescriptorSet::ds),
                          &bindlessDSets_.ds[0],
                          0,
                          nullptr);
}

void lvk::VulkanContext::checkAndUpdateDescriptorSets() {
  if (!awaitingCreation_) {
    // nothing to update here
    return;
  }

  // newly created resources can be used immediately - make sure they are put into descriptor sets
  LVK_PROFILER_FUNCTION();

  // update Vulkan descriptor set here

  // make sure the guard values are always there
  LVK_ASSERT(texturesPool_.numObjects() >= 1);
  LVK_ASSERT(samplersPool_.numObjects() >= 1);

  uint32_t newMaxTextures = currentMaxTextures_;
  uint32_t newMaxSamplers = currentMaxSamplers_;

  while (texturesPool_.objects_.size() > newMaxTextures) {
    newMaxTextures *= 2;
  }
  while (samplersPool_.objects_.size() > newMaxSamplers) {
    newMaxSamplers *= 2;
  }
  if (newMaxTextures != currentMaxTextures_ || newMaxSamplers != currentMaxSamplers_) {
    growDescriptorPool(newMaxTextures, newMaxSamplers);
  }

  // 1. Sampled and storage images
  std::vector<VkDescriptorImageInfo> infoSampledImages;
  std::vector<VkDescriptorImageInfo> infoStorageImages;

  infoSampledImages.reserve(texturesPool_.numObjects());
  infoStorageImages.reserve(texturesPool_.numObjects());

  // use the dummy texture to avoid sparse array
  VkImageView dummyImageView = texturesPool_.objects_[0].obj_.imageView_;

  for (const auto& obj : texturesPool_.objects_) {
    const VulkanTexture& texture = obj.obj_;
    // multisampled images cannot be directly accessed from shaders
    const bool isTextureAvailable = texture.image_ && ((texture.image_->vkSamples_ & VK_SAMPLE_COUNT_1_BIT) == VK_SAMPLE_COUNT_1_BIT);
    const bool isSampledImage = isTextureAvailable && texture.image_->isSampledImage();
    const bool isStorageImage = isTextureAvailable && texture.image_->isStorageImage();
    infoSampledImages.push_back(
        {samplersPool_.objects_[0].obj_, isSampledImage ? texture.imageView_ : dummyImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    LVK_ASSERT(infoSampledImages.back().imageView != VK_NULL_HANDLE);
    infoStorageImages.push_back(
        VkDescriptorImageInfo{VK_NULL_HANDLE, isStorageImage ? texture.imageView_ : dummyImageView, VK_IMAGE_LAYOUT_GENERAL});
  }

  // 2. Samplers
  std::vector<VkDescriptorImageInfo> infoSamplers;
  infoSamplers.reserve(samplersPool_.objects_.size());

  for (const auto& sampler : samplersPool_.objects_) {
    infoSamplers.push_back({sampler.obj_ ? sampler.obj_ : samplersPool_.objects_[0].obj_, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED});
  }

  VkWriteDescriptorSet write[kBinding_NumBindings * LVK_ARRAY_NUM_ELEMENTS(BindlessDescriptorSet::ds)] = {};
  uint32_t numWrites = 0;

  // we want to update the next available descriptor set
  BindlessDescriptorSet& dsetToUpdate = bindlessDSets_;

  if (!infoSampledImages.empty()) {
    for (uint32_t i = 0; i != LVK_ARRAY_NUM_ELEMENTS(BindlessDescriptorSet::ds); i++) {
      write[numWrites++] = VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = dsetToUpdate.ds[i],
          .dstBinding = kBinding_Textures,
          .dstArrayElement = 0,
          .descriptorCount = (uint32_t)infoSampledImages.size(),
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .pImageInfo = infoSampledImages.data(),
      };
    }
  }

  if (!infoSamplers.empty()) {
    for (uint32_t i = 0; i != LVK_ARRAY_NUM_ELEMENTS(BindlessDescriptorSet::ds); i++) {
      write[numWrites++] = VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = dsetToUpdate.ds[i],
          .dstBinding = kBinding_Samplers,
          .dstArrayElement = 0,
          .descriptorCount = (uint32_t)infoSamplers.size(),
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
          .pImageInfo = infoSamplers.data(),
      };
    }
  }

  if (!infoStorageImages.empty()) {
    for (uint32_t i = 0; i != LVK_ARRAY_NUM_ELEMENTS(BindlessDescriptorSet::ds); i++) {
      write[numWrites++] = VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = dsetToUpdate.ds[i],
          .dstBinding = kBinding_StorageImages,
          .dstArrayElement = 0,
          .descriptorCount = (uint32_t)infoStorageImages.size(),
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .pImageInfo = infoStorageImages.data(),
      };
    }
  }

  // do not switch to the next descriptor set if there is nothing to update
  if (numWrites) {
#if LVK_VULKAN_PRINT_COMMANDS
    LLOGL("vkUpdateDescriptorSets()\n");
#endif // LVK_VULKAN_PRINT_COMMANDS
    immediate_->wait(std::exchange(dsetToUpdate.handle, immediate_->getLastSubmitHandle()));
    vkUpdateDescriptorSets(vkDevice_, numWrites, write, 0, nullptr);
  }

  awaitingCreation_ = false;
}

lvk::SamplerHandle lvk::VulkanContext::createSampler(const VkSamplerCreateInfo& ci, lvk::Result* outResult, const char* debugName) {
  LVK_PROFILER_FUNCTION_COLOR(LVK_PROFILER_COLOR_CREATE);

  VkSampler sampler = VK_NULL_HANDLE;

  VK_ASSERT(vkCreateSampler(vkDevice_, &ci, nullptr, &sampler));
  VK_ASSERT(lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_SAMPLER, (uint64_t)sampler, debugName));

  SamplerHandle handle = samplersPool_.create(VkSampler(sampler));

  awaitingCreation_ = true;

  return handle;
}

void lvk::VulkanContext::querySurfaceCapabilities() {
  // enumerate only the formats we are using
  const VkFormat depthFormats[] = {
      VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM};
  for (const auto& depthFormat : depthFormats) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(vkPhysicalDevice_, depthFormat, &formatProps);

    if (formatProps.optimalTilingFeatures) {
      deviceDepthFormats_.push_back(depthFormat);
    }
  }

  if (vkSurface_ == VK_NULL_HANDLE) {
    return;
  }

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice_, vkSurface_, &deviceSurfaceCaps_);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, vkSurface_, &formatCount, nullptr);

  if (formatCount) {
    deviceSurfaceFormats_.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice_, vkSurface_, &formatCount, deviceSurfaceFormats_.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_, vkSurface_, &presentModeCount, nullptr);

  if (presentModeCount) {
    devicePresentModes_.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice_, vkSurface_, &presentModeCount, devicePresentModes_.data());
  }
}

VkFormat lvk::VulkanContext::getClosestDepthStencilFormat(lvk::Format desiredFormat) const {
  // get a list of compatible depth formats for a given desired format
  // The list will contain depth format that are ordered from most to least closest
  const std::vector<VkFormat> compatibleDepthStencilFormatList = getCompatibleDepthStencilFormats(desiredFormat);

  // Generate a set of device supported formats
  std::set<VkFormat> availableFormats;
  for (auto format : deviceDepthFormats_) {
    availableFormats.insert(format);
  }

  // check if any of the format in compatible list is supported
  for (auto depthStencilFormat : compatibleDepthStencilFormatList) {
    if (availableFormats.count(depthStencilFormat) != 0) {
      return depthStencilFormat;
    }
  }

  // no matching found, choose the first supported format
  return !deviceDepthFormats_.empty() ? deviceDepthFormats_[0] : VK_FORMAT_D24_UNORM_S8_UINT;
}

std::vector<uint8_t> lvk::VulkanContext::getPipelineCacheData() const {
  size_t size = 0;
  vkGetPipelineCacheData(vkDevice_, pipelineCache_, &size, nullptr);

  std::vector<uint8_t> data(size);

  if (size) {
    vkGetPipelineCacheData(vkDevice_, pipelineCache_, &size, data.data());
  }

  return data;
}

void lvk::VulkanContext::deferredTask(std::packaged_task<void()>&& task, SubmitHandle handle) const {
  if (handle.empty()) {
    handle = immediate_->getLastSubmitHandle();
  }
  pimpl_->deferredTasks_.emplace_back(std::move(task), handle);
}

void* lvk::VulkanContext::getVmaAllocator() const {
  return pimpl_->vma_;
}

void lvk::VulkanContext::processDeferredTasks() const {
  while (!pimpl_->deferredTasks_.empty() && immediate_->isReady(pimpl_->deferredTasks_.front().handle_, true)) {
    pimpl_->deferredTasks_.front().task_();
    pimpl_->deferredTasks_.pop_front();
  }
}

void lvk::VulkanContext::waitDeferredTasks() {
  for (auto& task : pimpl_->deferredTasks_) {
    immediate_->wait(task.handle_);
    task.task_();
  }
  pimpl_->deferredTasks_.clear();
}
