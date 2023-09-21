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

enum class OpCode : uint32_t {
  OpTypePointer = 32,
  OpVariable = 59,
  OpDecorate = 71,
};

struct Decoration {
  enum : uint32_t { Block = 2, Binding = 33 };
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

} // namespace

SpvModuleInfo getReflectionData(const uint32_t* words, size_t size) {
  SpvModuleInfo spvModuleInfo;
  if (size < kSpvHeaderSize) {
    return spvModuleInfo;
  }
  if (words[0] != kSpvMagicWord) {
    IGL_ASSERT_MSG(false, "Invalid SPIR-V magic word");
    return spvModuleInfo;
  }

  std::unordered_set<ResultId> interfaceBlockTypeIds;
  std::unordered_set<ResultId> interfaceBlockPointerTypeIds;
  std::unordered_map<ResultId, uint32_t> bindingLocations;

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

    case OpCode::OpTypePointer: {
      IGL_ASSERT_MSG((pos + kOpTypePointerMaxUsedIx) < size, "OpTypePointer out of bounds");

      ResultId objectTypeId = words[pos + kOpTypePointerObjectTypeId];
      if (interfaceBlockTypeIds.count(objectTypeId)) {
        ResultId pointerTypeId = words[pos + kOpTypePointerId];
        interfaceBlockPointerTypeIds.insert(pointerTypeId);
      }

      break;
    }

    case OpCode::OpVariable: {
      IGL_ASSERT_MSG((pos + kOpVariableMaxUsedIdx) < size, "OpVariable out of bounds");

      ResultId variableTypeId = words[pos + kOpVariableTypeId];
      if (interfaceBlockPointerTypeIds.count(variableTypeId)) {
        ResultId variableId = words[pos + kOpVariableId];
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
