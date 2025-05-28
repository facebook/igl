/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/util/SpvReflection.h>

#include <spirv/unified1/spirv.h>

#define IGL_COMMON_SKIP_CHECK
#include <igl/Macros.h>

namespace igl::vulkan::util {
namespace {

struct SpirvId {
  uint32_t opCode = 0;
  uint32_t typeId = 0;
  uint32_t storageClass = 0;
  uint32_t binding = kNoBindingLocation;
  uint32_t dset = kNoDescriptorSet;
  TextureType type = TextureType::Invalid;
  bool isStorageImage = false;
  uint32_t imageFormat = SpvImageFormatUnknown;
};

struct ImageDimensionality {
  enum : uint32_t {
    kDim1d = 0,
    kDim2d = 1,
    kDim3d = 2,
    kDimCube = 3,
    kDimRect = 4,
    kDim2dExternal = 666, // Doesn't exist in SPIR-V, but needed for Android.
    kDim2dExternalYuv = 667, // Doesn't exist in SPIR-V, but needed for Android.
  };
};

TextureType getIGLTextureType(uint32_t dim, bool isArrayed) {
  switch (dim) {
  case ImageDimensionality::kDim2d:
    return isArrayed ? TextureType::TwoDArray : TextureType::TwoD;
  case ImageDimensionality::kDim3d:
    return TextureType::ThreeD;
  case ImageDimensionality::kDimCube:
    return TextureType::Cube;
  case ImageDimensionality::kDim2dExternal:
  case ImageDimensionality::kDim2dExternalYuv:
    return TextureType::ExternalImage;

  case ImageDimensionality::kDimRect:
  case ImageDimensionality::kDim1d:
  default:
    return TextureType::Invalid;
  }
}

} // namespace

SpvModuleInfo getReflectionData(const uint32_t* spirv, size_t numBytes) {
  if (!IGL_DEBUG_VERIFY(spirv)) {
    return {};
  }

  // go from bytes to SPIR-V words
  const size_t size = numBytes / sizeof(uint32_t);

  constexpr uint32_t kSpvBoundOffset = 3;
  constexpr uint32_t kSpvHeaderSize = 5;

  // initial pre-checks
  {
    if (size <= kSpvHeaderSize) {
      return {};
    }

    if (spirv[0] != SpvMagicNumber) {
      IGL_DEBUG_ABORT("Invalid SPIR-V magic word");
      return {};
    }
  }

  // SPIR-V spec: "all <id>s in this module are guaranteed to satisfy: 0 < id < kBound"
  const uint32_t kBound = spirv[kSpvBoundOffset];

  // some reasonable upper bound so that we don't try to allocate a lot of memory in case the SPIR-V
  // header is broken
  if (!IGL_DEBUG_VERIFY(kBound < 1024 * 1024)) {
    return {};
  }

  std::vector<SpirvId> ids(kBound);

  SpvModuleInfo info = {};

  const uint32_t* words = spirv + kSpvHeaderSize;

  while (words < spirv + size) {
    const uint16_t instructionSize = uint16_t(words[0] >> SpvWordCountShift);
    const uint16_t opCode = uint16_t(words[0] & SpvOpCodeMask);

    switch (opCode) {
    case SpvOpDecorate: {
      constexpr uint32_t kOpDecorateTargetId = 1;
      constexpr uint32_t kOpDecorateDecoration = 2;
      constexpr uint32_t kOpDecorateOperandIds = 3;

      IGL_DEBUG_ASSERT(words + kOpDecorateDecoration <= spirv + size, "OpDecorate out of bounds");

      const uint32_t decoration = words[kOpDecorateDecoration];
      const uint32_t targetId = words[kOpDecorateTargetId];
      IGL_DEBUG_ASSERT(targetId < kBound);

      switch (decoration) {
      case SpvDecorationBinding: {
        IGL_DEBUG_ASSERT(words + kOpDecorateOperandIds <= spirv + size, "OpDecorate out of bounds");
        ids[targetId].binding = words[kOpDecorateOperandIds];
        break;
      }
      case SpvDecorationDescriptorSet: {
        IGL_DEBUG_ASSERT(words + kOpDecorateOperandIds <= spirv + size, "OpDecorate out of bounds");
        ids[targetId].dset = words[kOpDecorateOperandIds];
        break;
      }
      default:
        break;
      }
      break;
    }
    case SpvOpTypeStruct:
    case SpvOpTypeImage:
    case SpvOpTypeSampler:
    case SpvOpTypeSampledImage: {
      constexpr uint32_t kOpTypeResultId = 1;
      IGL_DEBUG_ASSERT(words + kOpTypeResultId <= spirv + size, "OpTypeImage out of bounds");
      const uint32_t targetId = words[kOpTypeResultId];
      IGL_DEBUG_ASSERT(targetId < kBound);
      IGL_DEBUG_ASSERT(ids[targetId].opCode == 0);
      ids[targetId].opCode = opCode;
      if (opCode == SpvOpTypeSampledImage) {
        constexpr uint32_t kOpTypeSampledImageImageTypeId = 2;
        ids[targetId].typeId = words[kOpTypeSampledImageImageTypeId];
      } else if (opCode == SpvOpTypeImage) {
        constexpr uint32_t kOpTypeImageTypeId = 1;
        constexpr uint32_t kOpTypeImageDim = 3;
        constexpr uint32_t kOpTypeImageArrayed = 5;
        IGL_DEBUG_ASSERT(words + kOpTypeImageArrayed <= spirv + size, "OpTypeImage out of bounds");
        const uint32_t imageTypeId = words[kOpTypeImageTypeId];
        const uint32_t dim = words[kOpTypeImageDim];
        const bool isArray = words[kOpTypeImageArrayed] == 1u;
        const TextureType textureType = getIGLTextureType(dim, isArray);
        ids[imageTypeId].type = textureType;
        // The spec: https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#OpTypeImage
        constexpr uint32_t kOpTypeImageSampledField = 7;
        constexpr uint32_t kOpTypeImageFormatField = 8;
        const uint32_t sampledValue = (instructionSize > kOpTypeImageSampledField)
                                          ? words[kOpTypeImageSampledField]
                                          : 0; // default if missing
        // If sampledValue == 2, this is a storage image
        ids[targetId].isStorageImage = sampledValue == 2;
        if (instructionSize > kOpTypeImageFormatField) {
          ids[targetId].imageFormat = words[kOpTypeImageFormatField];
        } else {
          ids[targetId].imageFormat = SpvImageFormatUnknown;
        }
      }
      break;
    }
    case SpvOpTypePointer: {
      constexpr uint32_t kOpTypePointerTargetId = 1;
      constexpr uint32_t kOpTypePointerStorageClassId = 2;
      constexpr uint32_t kOpTypePointerObjectTypeId = 3;
      IGL_DEBUG_ASSERT(words + kOpTypePointerObjectTypeId <= spirv + size,
                       "OpTypePointer out of bounds");
      const uint32_t targetId = words[kOpTypePointerTargetId];
      IGL_DEBUG_ASSERT(targetId < kBound);
      IGL_DEBUG_ASSERT(ids[targetId].opCode == 0);
      ids[targetId].opCode = opCode;
      ids[targetId].typeId = words[kOpTypePointerObjectTypeId];
      ids[targetId].storageClass = words[kOpTypePointerStorageClassId];
      break;
    }
    case SpvOpConstant: {
      constexpr uint32_t kOpConstantTypeId = 1;
      constexpr uint32_t kOpConstantTargetId = 2;
      IGL_DEBUG_ASSERT(words + kOpConstantTargetId <= spirv + size, "OpTypePointer out of bounds");
      const uint32_t targetId = words[kOpConstantTargetId];
      IGL_DEBUG_ASSERT(targetId < kBound);
      IGL_DEBUG_ASSERT(ids[targetId].opCode == 0);
      ids[targetId].opCode = opCode;
      ids[targetId].typeId = words[kOpConstantTypeId];
      break;
    }
    case SpvOpVariable: {
      constexpr uint32_t kOpVariableTypeId = 1;
      constexpr uint32_t kOpVariableTargetId = 2;
      constexpr uint32_t kOpVariableStorageClass = 3;
      IGL_DEBUG_ASSERT(words + kOpVariableStorageClass <= spirv + size, "OpVariable out of bounds");
      const uint32_t targetId = words[kOpVariableTargetId];
      IGL_DEBUG_ASSERT(targetId < kBound);
      IGL_DEBUG_ASSERT(ids[targetId].opCode == 0);
      ids[targetId].opCode = opCode;
      ids[targetId].typeId = words[kOpVariableTypeId];
      ids[targetId].storageClass = words[kOpVariableStorageClass];
      break;
    }
    default:
      break;
    }

    IGL_DEBUG_ASSERT(words + instructionSize <= spirv + size);
    words += instructionSize;
  }

  for (auto& id : ids) {
    const bool isStorage = id.storageClass == SpvStorageClassStorageBuffer;
    const bool isUniform = id.storageClass == SpvStorageClassUniform ||
                           id.storageClass == SpvStorageClassUniformConstant;
    if (id.opCode == SpvOpVariable && (isStorage || isUniform)) {
      IGL_DEBUG_ASSERT(ids[id.typeId].opCode == SpvOpTypePointer);
      IGL_DEBUG_ASSERT(ids[id.typeId].typeId < kBound);

      const uint32_t opCode = ids[ids[id.typeId].typeId].opCode;

      switch (SpvOp(opCode)) {
      case SpvOpTypeStruct:
        info.buffers.push_back({id.binding, id.dset, isStorage});
        break;
      case SpvOpTypeImage: {
        const TextureType tt = ids[ids[id.typeId].typeId].type; // dimension
        IGL_DEBUG_ASSERT(tt != TextureType::Invalid);
        const bool isStorageImage = ids[ids[id.typeId].typeId].isStorageImage;
        if (isStorageImage) {
          uint32_t imgFmt = ids[ids[id.typeId].typeId].imageFormat;
          info.images.push_back({id.binding, id.dset, tt, imgFmt});
        } else {
          info.textures.push_back({id.binding, id.dset, tt});
        }
        break;
      }
      case SpvOpTypeSampler:
        break;
      case SpvOpTypeSampledImage: {
        IGL_DEBUG_ASSERT(ids[ids[id.typeId].typeId].typeId < kBound);
        IGL_DEBUG_ASSERT(ids[ids[ids[id.typeId].typeId].typeId].opCode == SpvOpTypeImage);
        const TextureType tt = ids[ids[ids[id.typeId].typeId].typeId].type;
        IGL_DEBUG_ASSERT(tt != TextureType::Invalid);
        info.textures.push_back({id.binding, id.dset, tt});
        break;
      }
      default:
        break;
      }
    }
    if (id.opCode == SpvOpVariable && id.storageClass == SpvStorageClassPushConstant) {
      info.hasPushConstants = true;
    }
  }

  for (const auto& desc : info.buffers) {
    if (desc.bindingLocation != kNoBindingLocation) {
      info.usageMaskBuffers |= 1ul << desc.bindingLocation;
    }
  }
  for (const auto& desc : info.textures) {
    if (desc.bindingLocation != kNoBindingLocation) {
      info.usageMaskTextures |= 1ul << desc.bindingLocation;
    }
  }

  return info;
}

namespace {

template<typename T>
void combineDescriptions(std::vector<T>& out, const std::vector<T>& c1, const std::vector<T>& c2) {
  out = c1;

  for (const auto& desc : c2) {
    auto it = std::find_if(out.begin(), out.end(), [loc = desc.bindingLocation](const auto& d) {
      return d.bindingLocation == loc;
    });
    if (it == out.end()) {
      out.emplace_back(desc);
    } else {
      IGL_DEBUG_ASSERT(desc.descriptorSet == it->descriptorSet);
    }
  }
}

} // namespace

SpvModuleInfo mergeReflectionData(const SpvModuleInfo& info1, const SpvModuleInfo& info2) {
  SpvModuleInfo result;

  combineDescriptions(result.buffers, info1.buffers, info2.buffers);
  combineDescriptions(result.textures, info1.textures, info2.textures);

  result.hasPushConstants = info1.hasPushConstants || info2.hasPushConstants;
  result.usageMaskBuffers = info1.usageMaskBuffers | info2.usageMaskBuffers;
  result.usageMaskTextures = info1.usageMaskTextures | info2.usageMaskTextures;

  return result;
}

} // namespace igl::vulkan::util
