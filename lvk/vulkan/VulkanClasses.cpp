/*
 * LightweightVK
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "VulkanClasses.h"
#include "VulkanUtils.h"

#include <igl/vulkan/VulkanContext.h>

#ifndef VK_USE_PLATFORM_WIN32_KHR
#include <unistd.h>
#endif

#include <vector>

uint32_t lvk::VulkanPipelineBuilder::numPipelinesCreated_ = 0;

namespace {

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

lvk::VulkanBuffer::VulkanBuffer(lvk::vulkan::VulkanContext* ctx,
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
    const VkBufferDeviceAddressInfo ai = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, vkBuffer_};
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
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        nullptr,
        vkMemory_,
        offset,
        size,
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

lvk::VulkanImage::VulkanImage(lvk::vulkan::VulkanContext& ctx,
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

lvk::VulkanImage::VulkanImage(lvk::vulkan::VulkanContext& ctx,
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

lvk::VulkanTexture::VulkanTexture(std::shared_ptr<VulkanImage> image, VkImageView imageView) : image_(std::move(image)), imageView_(imageView) {
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

lvk::VulkanSwapchain::VulkanSwapchain(vulkan::VulkanContext& ctx, uint32_t width, uint32_t height) :
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
    VK_ASSERT(vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, acquireSemaphore_, VK_NULL_HANDLE, &currentImageIndex_));
    getNextImage_ = false;
  }

  if (LVK_VERIFY(currentImageIndex_ < numSwapchainImages_)) {
    return swapchainTextures_[currentImageIndex_];
  }

  return {};
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
  VK_ASSERT_RETURN(vkQueuePresentKHR(graphicsQueue_, &pi));
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

lvk::RenderPipelineState::RenderPipelineState(lvk::vulkan::VulkanContext* ctx, const RenderPipelineDesc& desc) : ctx_(ctx), desc_(desc) {
  // Iterate and cache vertex input bindings and attributes
  const lvk::VertexInput& vstate = desc_.vertexInput;

  bool bufferAlreadyBound[VertexInput::LVK_VERTEX_BUFFER_MAX] = {};

  numAttributes_ = vstate.getNumAttributes();

  for (uint32_t i = 0; i != numAttributes_; i++) {
    const auto& attr = vstate.attributes[i];

    vkAttributes_[i] = {
        .location = attr.location, .binding = attr.binding, .format = vertexFormatToVkFormat(attr.format), .offset = (uint32_t)attr.offset};

    if (!bufferAlreadyBound[attr.binding]) {
      bufferAlreadyBound[attr.binding] = true;
      vkBindings_[numBindings_++] = {
          .binding = attr.binding, .stride = vstate.inputBindings[attr.binding].stride, .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    }
  }
}

lvk::RenderPipelineState::~RenderPipelineState() {
  if (!ctx_) {
    return;
  }

  if (!desc_.smVert.empty()) {
    ctx_->destroy(desc_.smVert);
  }
  if (!desc_.smGeom.empty()) {
    ctx_->destroy(desc_.smGeom);
  }
  if (!desc_.smFrag.empty()) {
    ctx_->destroy(desc_.smFrag);
  }

  destroyPipelines();
}

lvk::RenderPipelineState::RenderPipelineState(RenderPipelineState&& other) :
  ctx_(other.ctx_), numBindings_(other.numBindings_), numAttributes_(other.numAttributes_), pipelineLayout_(other.pipelineLayout_) {
  std::swap(desc_, other.desc_);
  std::swap(pipelines_, other.pipelines_);
  for (uint32_t i = 0; i != numBindings_; i++) {
    vkBindings_[i] = other.vkBindings_[i];
  }
  for (uint32_t i = 0; i != numAttributes_; i++) {
    vkAttributes_[i] = other.vkAttributes_[i];
  }
  other.ctx_ = nullptr;
}

lvk::RenderPipelineState& lvk::RenderPipelineState::operator=(RenderPipelineState&& other) {
  std::swap(ctx_, other.ctx_);
  std::swap(desc_, other.desc_);
  std::swap(numBindings_, other.numBindings_);
  std::swap(numAttributes_, other.numAttributes_);
  std::swap(pipelineLayout_, other.pipelineLayout_);
  std::swap(pipelines_, other.pipelines_);
  for (uint32_t i = 0; i != numBindings_; i++) {
    vkBindings_[i] = other.vkBindings_[i];
  }
  for (uint32_t i = 0; i != numAttributes_; i++) {
    vkAttributes_[i] = other.vkAttributes_[i];
  }
  return *this;
}

void lvk::RenderPipelineState::destroyPipelines() {
  for (auto p : pipelines_) {
    if (p.second != VK_NULL_HANDLE) {
      ctx_->deferredTask(std::packaged_task<void()>(
          [device = ctx_->getVkDevice(), pipeline = p.second]() { vkDestroyPipeline(device, pipeline, nullptr); }));
    }
  }
  pipelines_.clear();
}

VkPipeline lvk::RenderPipelineState::getVkPipeline(const RenderPipelineDynamicState& dynamicState) {
  if (pipelineLayout_ != ctx_->vkPipelineLayout_) {
    destroyPipelines();
    pipelineLayout_ = ctx_->vkPipelineLayout_;
  }

  const auto it = pipelines_.find(dynamicState);

  if (it != pipelines_.end()) {
    return it->second;
  }

  // build a new Vulkan pipeline

  VkPipeline pipeline = VK_NULL_HANDLE;

  const uint32_t numColorAttachments = desc_.getNumColorAttachments();

  // Not all attachments are valid. We need to create color blend attachments only for active attachments
  VkPipelineColorBlendAttachmentState colorBlendAttachmentStates[LVK_MAX_COLOR_ATTACHMENTS] = {};
  VkFormat colorAttachmentFormats[LVK_MAX_COLOR_ATTACHMENTS] = {};

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

  const VkShaderModule* vertModule = ctx_->shaderModulesPool_.get(desc_.smVert);
  const VkShaderModule* geomModule = ctx_->shaderModulesPool_.get(desc_.smGeom);
  const VkShaderModule* fragModule = ctx_->shaderModulesPool_.get(desc_.smFrag);

  LVK_ASSERT(vertModule);
  LVK_ASSERT(fragModule);

  const VkPipelineVertexInputStateCreateInfo ciVertexInputState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = numBindings_,
      .pVertexBindingDescriptions = numBindings_ ? vkBindings_ : nullptr,
      .vertexAttributeDescriptionCount = numAttributes_,
      .pVertexAttributeDescriptions = numAttributes_ ? vkAttributes_ : nullptr,
  };

  lvk::VulkanPipelineBuilder()
      // from Vulkan 1.0
      .dynamicState(VK_DYNAMIC_STATE_VIEWPORT)
      .dynamicState(VK_DYNAMIC_STATE_SCISSOR)
      .dynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS)
      .dynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS)
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
      .shaderStage(lvk::getPipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, *vertModule, desc_.entryPointVert))
      .shaderStage(lvk::getPipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, *fragModule, desc_.entryPointFrag))
      .shaderStage(geomModule ? lvk::getPipelineShaderStageCreateInfo(VK_SHADER_STAGE_GEOMETRY_BIT, *geomModule, desc_.entryPointGeom)
                              : VkPipelineShaderStageCreateInfo{.module = VK_NULL_HANDLE})
      .cullMode(cullModeToVkCullMode(desc_.cullMode))
      .frontFace(windingModeToVkFrontFace(desc_.frontFaceWinding))
      .vertexInputState(ciVertexInputState)
      .colorAttachments(colorBlendAttachmentStates, colorAttachmentFormats, numColorAttachments)
      .depthAttachmentFormat(formatToVkFormat(desc_.depthFormat))
      .stencilAttachmentFormat(formatToVkFormat(desc_.stencilFormat))
      .build(ctx_->getVkDevice(), ctx_->pipelineCache_, ctx_->vkPipelineLayout_, &pipeline, desc_.debugName);

  pipelines_[dynamicState] = pipeline;

  return pipeline;
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

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::depthWriteEnable(bool enable) {
  depthStencilState_.depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
  return *this;
}

lvk::VulkanPipelineBuilder& lvk::VulkanPipelineBuilder::depthCompareOp(VkCompareOp compareOp) {
  depthStencilState_.depthTestEnable = compareOp != VK_COMPARE_OP_ALWAYS;
  depthStencilState_.depthCompareOp = compareOp;
  return *this;
}

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

lvk::CommandBuffer::CommandBuffer(vulkan::VulkanContext* ctx) : ctx_(ctx), wrapper_(&ctx_->immediate_->acquire()) {}

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

  const RenderPipelineDesc& desc = rps->getRenderPipelineDesc();

  const bool hasDepthAttachmentPipeline = desc.depthFormat != Format_Invalid;
  const bool hasDepthAttachmentPass = !framebuffer_.depthStencil.texture.empty();

  if (hasDepthAttachmentPipeline != hasDepthAttachmentPass) {
    LVK_ASSERT(false);
    LLOGW("Make sure your render pass and render pipeline both have matching depth attachments");
  }

  lastPipelineBound_ = VK_NULL_HANDLE;
}

void lvk::CommandBuffer::cmdBindDepthState(const DepthState& desc) {
  LVK_PROFILER_FUNCTION();

  dynamicState_.depthWriteEnable_ = desc.isDepthWriteEnabled;
  dynamicState_.setDepthCompareOp(compareOpToVkCompareOp(desc.compareOp));
}

void lvk::CommandBuffer::cmdBindVertexBuffer(uint32_t index, BufferHandle buffer, size_t bufferOffset) {
  LVK_PROFILER_FUNCTION();

  if (!LVK_VERIFY(!buffer.empty())) {
    return;
  }

  lvk::VulkanBuffer* buf = ctx_->buffersPool_.get(buffer);

  VkBuffer vkBuf = buf->vkBuffer_;

  LVK_ASSERT(buf->vkUsageFlags_ & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

  const VkDeviceSize offset = bufferOffset;
  vkCmdBindVertexBuffers(wrapper_->cmdBuf_, index, 1, &vkBuf, &offset);
}

void lvk::CommandBuffer::cmdBindIndexBuffer(BufferHandle indexBuffer, IndexFormat indexFormat, size_t indexBufferOffset) {
  lvk::VulkanBuffer* buf = ctx_->buffersPool_.get(indexBuffer);

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
  lvk::RenderPipelineState* rps = ctx_->renderPipelinesPool_.get(currentPipeline_);

  if (!LVK_VERIFY(rps)) {
    return;
  }

  VkPipeline pipeline = rps->getVkPipeline(dynamicState_);

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

  vkCmdDrawIndexedIndirect(wrapper_->cmdBuf_,
                           bufIndirect->vkBuffer_,
                           indirectBufferOffset,
                           drawCount,
                           stride ? stride : sizeof(VkDrawIndexedIndirectCommand));
}

void lvk::CommandBuffer::cmdSetBlendColor(const float color[4]) {
  vkCmdSetBlendConstants(wrapper_->cmdBuf_, color);
}

void lvk::CommandBuffer::cmdSetDepthBias(float depthBias, float slopeScale, float clamp) {
  dynamicState_.depthBiasEnable_ = true;
  vkCmdSetDepthBias(wrapper_->cmdBuf_, depthBias, clamp, slopeScale);
}
