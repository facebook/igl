/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/util/SpvReflection.h>

#define IGL_COMMON_SKIP_CHECK
#include <igl/Assert.h>
#include <igl/Macros.h>

#include <unordered_map>
#include <unordered_set>

namespace igl::vulkan::util {
namespace {
constexpr uint32_t kSpvMagicWord = 0x07230203;
constexpr uint32_t kSpvHeaderSize = 5;
constexpr uint32_t kSpvWordCountShift = 16;
constexpr uint32_t kSpvOpCodeMask = 0xFFFF;

constexpr size_t kOpDecorateTargetId = 1;
constexpr size_t kOpDecorateDecoration = 2;
constexpr size_t kOpDecorateOperandIds = 3;
IGL_MAYBE_UNUSED constexpr size_t kOpDecorateMaxUsedIdx = kOpDecorateOperandIds;

constexpr size_t kOpVariableTypeId = 1;
constexpr size_t kOpVariableId = 2;
constexpr size_t kOpVariableStorageClass = 3;
IGL_MAYBE_UNUSED constexpr size_t kOpVariableMaxUsedIdx = kOpVariableStorageClass;

constexpr size_t kOpTypePointerId = 1;
constexpr size_t kOpTypePointerObjectTypeId = 3;
IGL_MAYBE_UNUSED constexpr size_t kOpTypePointerMaxUsedIx = kOpTypePointerObjectTypeId;

constexpr size_t kOpTypeImageTypeId = 1;
constexpr size_t kOpTypeImageDim = 3;
constexpr size_t kOpTypeImageArrayed = 5;
IGL_MAYBE_UNUSED constexpr size_t kOpTypeImageMaxUsedId = kOpTypeImageDim;

constexpr size_t kOpTypeSampledImageTypeId = 1;
constexpr size_t kOpTypeSampledImageImageTypeId = 2;
IGL_MAYBE_UNUSED constexpr size_t kOpTypeSampledImageMaxUsedId = kOpTypeSampledImageImageTypeId;

enum class OpCode : uint32_t {
  OpTypeImage = 25,
  OpTypeSampledImage = 27,
  OpTypePointer = 32,
  OpVariable = 59,
  OpDecorate = 71,
};

struct Decoration {
  enum : uint32_t { Block = 2, Binding = 33 };
};

struct ImageDimensionality {
  enum : uint32_t {
    Dim1d = 0,
    Dim2d = 1,
    Dim3d = 2,
    DimCube = 3,
    DimRect = 4,
    Dim2dExternal = 666, // Doesn't exist in SPIR-V, but needed for Android.
  };
};

struct StorageClass {
  enum : uint32_t { Uniform = 2, StorageBuffer = 12 };
};

using ResultId = uint32_t;

uint16_t getInstructionSize(uint32_t firstWord) {
  return firstWord >> kSpvWordCountShift;
}

OpCode getOpCode(uint32_t firstWord) {
  return static_cast<OpCode>(firstWord & kSpvOpCodeMask);
}

TextureType getTextureType(uint32_t dim, bool isArrayed) {
  switch (dim) {
  case ImageDimensionality::Dim2d:
    return isArrayed ? TextureType::TwoDArray : TextureType::TwoD;
  case ImageDimensionality::Dim3d:
    return TextureType::ThreeD;
  case ImageDimensionality::DimCube:
    return TextureType::Cube;
  case ImageDimensionality::Dim2dExternal:
    return TextureType::ExternalImage;

  case ImageDimensionality::DimRect:
  case ImageDimensionality::Dim1d:
  default:
    return TextureType::Invalid;
  }
}

} // namespace

SpvModuleInfo getReflectionData(const void* spirv, size_t numBytes) {
  // go from bytes to SPIR-V words
  const size_t size = numBytes / sizeof(uint32_t);

  if (size <= kSpvHeaderSize) {
    return {};
  }

  const uint32_t* words = reinterpret_cast<const uint32_t*>(spirv);

  if (words[0] != kSpvMagicWord) {
    IGL_ASSERT_MSG(false, "Invalid SPIR-V magic word");
    return {};
  }

  std::unordered_set<ResultId> interfaceBlockTypeIds;
  std::unordered_set<ResultId> interfaceBlockPointerTypeIds;
  std::unordered_map<ResultId, ResultId> sampledImageTypeIdToImageTypeId;
  std::unordered_map<ResultId, TextureType> sampledImagePointerTypeIds;
  std::unordered_map<ResultId, uint32_t> bindingLocations;
  std::unordered_map<ResultId, TextureType> imageTypes;

  SpvModuleInfo spvModuleInfo;

  for (size_t pos = kSpvHeaderSize; pos < size;) {
    uint16_t instructionSize = getInstructionSize(words[pos]);
    OpCode opCode = getOpCode(words[pos]);

    switch (opCode) {
    case OpCode::OpDecorate: {
      IGL_ASSERT_MSG((pos + kOpDecorateMaxUsedIdx) < size, "OpDecorate out of bounds");

      uint32_t decoration = words[pos + kOpDecorateDecoration];
      uint32_t targetId = words[pos + kOpDecorateTargetId];
      switch (decoration) {
      case Decoration::Block: {
        interfaceBlockTypeIds.insert(targetId);
        break;
      }
      case Decoration::Binding: {
        uint32_t bindingLocation = words[pos + kOpDecorateOperandIds];
        bindingLocations.insert({targetId, bindingLocation});
        break;
      }
      default:
        break;
      }
      break;
    }

    case OpCode::OpTypeImage: {
      IGL_ASSERT_MSG((pos + kOpTypeImageMaxUsedId) < size, "OpTypeImage out of bounds");
      ResultId imageTypeId = words[pos + kOpTypeImageTypeId];
      uint32_t dim = words[pos + kOpTypeImageDim];
      bool isArrayed = words[pos + kOpTypeImageArrayed] == 1u;
      TextureType textureType = getTextureType(dim, isArrayed);
      imageTypes.insert({imageTypeId, textureType});
      break;
    }

    case OpCode::OpTypeSampledImage: {
      IGL_ASSERT_MSG((pos + kOpTypeSampledImageMaxUsedId) < size,
                     "OpTypeSampledImage out of bounds");

      ResultId sampledImageTypeId = words[pos + kOpTypeSampledImageTypeId];
      ResultId imageTypeId = words[pos + kOpTypeSampledImageImageTypeId];
      sampledImageTypeIdToImageTypeId.insert({sampledImageTypeId, imageTypeId});
      break;
    }

    case OpCode::OpTypePointer: {
      IGL_ASSERT_MSG((pos + kOpTypePointerMaxUsedIx) < size, "OpTypePointer out of bounds");

      ResultId objectTypeId = words[pos + kOpTypePointerObjectTypeId];
      ResultId pointerTypeId = words[pos + kOpTypePointerId];
      if (interfaceBlockTypeIds.count(objectTypeId)) {
        interfaceBlockPointerTypeIds.insert(pointerTypeId);
      } else {
        auto sampledImageTypeIdToImageTypeIdIter =
            sampledImageTypeIdToImageTypeId.find(objectTypeId);
        if (sampledImageTypeIdToImageTypeIdIter != sampledImageTypeIdToImageTypeId.end()) {
          auto imageTypeId = sampledImageTypeIdToImageTypeIdIter->second;
          auto imageTypesIter = imageTypes.find(imageTypeId);
          TextureType textureType = TextureType::Invalid;
          if (imageTypesIter != imageTypes.end()) {
            textureType = imageTypesIter->second;
          }
          sampledImagePointerTypeIds.insert({pointerTypeId, textureType});
        }
      }

      break;
    }

    case OpCode::OpVariable: {
      IGL_ASSERT_MSG((pos + kOpVariableMaxUsedIdx) < size, "OpVariable out of bounds");

      ResultId variableTypeId = words[pos + kOpVariableTypeId];
      ResultId variableId = words[pos + kOpVariableId];

      if (interfaceBlockPointerTypeIds.count(variableTypeId)) {
        auto bindingLocationIt = bindingLocations.find(variableId);
        uint32_t bindingLocation = bindingLocationIt != bindingLocations.end()
                                       ? bindingLocationIt->second
                                       : kNoBindingLocation;
        uint32_t storageClass = words[pos + kOpVariableStorageClass];
        if (storageClass == StorageClass::Uniform) {
          spvModuleInfo.uniformBufferBindingLocations.push_back(bindingLocation);
        } else if (storageClass == StorageClass::StorageBuffer) {
          spvModuleInfo.storageBufferBindingLocations.push_back(bindingLocation);
        }
      } else {
        auto sampledImagePointerTypeIdsIter = sampledImagePointerTypeIds.find(variableTypeId);
        if (sampledImagePointerTypeIdsIter != sampledImagePointerTypeIds.end()) {
          auto bindingLocationIt = bindingLocations.find(variableId);
          uint32_t bindingLocation = bindingLocationIt != bindingLocations.end()
                                         ? bindingLocationIt->second
                                         : kNoBindingLocation;

          auto textureType = sampledImagePointerTypeIdsIter->second;
          TextureDescription textureDesc;
          textureDesc.type = textureType;
          textureDesc.bindingLocation = bindingLocation;
          spvModuleInfo.textures.push_back(std::move(textureDesc));
        }
      }

      break;
    }

    default:
      break;
    }

    pos += instructionSize;
  }

  return spvModuleInfo;
}

} // namespace igl::vulkan::util
