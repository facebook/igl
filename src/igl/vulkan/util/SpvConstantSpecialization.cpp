/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/util/SpvConstantSpecialization.h>

#include <spirv/unified1/spirv.h>

#define IGL_COMMON_SKIP_CHECK
#include <igl/Common.h>

namespace igl::vulkan::util {
namespace {
uint32_t makeOpCode(uint32_t opCode, uint32_t wordCount) {
  return opCode | (wordCount << SpvWordCountShift);
}
} // namespace

void specializeConstants(uint32_t* spirv, size_t numBytes, const std::vector<uint32_t>& values) {
  const uint32_t bound = spirv[3];
  const size_t size = numBytes / sizeof(uint32_t);

  if (!IGL_DEBUG_VERIFY(bound < 1024 * 1024)) {
    return;
  }

  if (!IGL_DEBUG_VERIFY(spirv[0] == SpvMagicNumber)) {
    return;
  }

  std::vector<uint32_t> idToValue(bound, kNoValue);

  uint32_t* instruction = spirv + 5;
  while (instruction < spirv + size) {
    const uint16_t instructionSize = static_cast<uint16_t>(instruction[0] >> SpvWordCountShift);
    const uint16_t opCode = static_cast<uint16_t>(instruction[0] & SpvOpCodeMask);

    switch (opCode) {
    case SpvOpDecorate: {
      constexpr uint32_t kOpDecorateTargetId = 1;
      constexpr uint32_t kOpDecorateDecoration = 2;
      constexpr uint32_t kOpDecorateOperandIds = 3;

      IGL_DEBUG_ASSERT(instruction + kOpDecorateDecoration <= spirv + size,
                       "OpDecorate out of bounds");

      const uint32_t decoration = instruction[kOpDecorateDecoration];
      const uint32_t targetId = instruction[kOpDecorateTargetId];
      IGL_DEBUG_ASSERT(targetId < bound);

      switch (decoration) {
      case SpvDecorationSpecId: {
        IGL_DEBUG_ASSERT(instruction + kOpDecorateOperandIds <= spirv + size,
                         "OpDecorate out of bounds");
        const uint32_t specId = instruction[kOpDecorateOperandIds];
        idToValue[targetId] = values.size() > specId ? values[specId] : kNoValue;
        break;
      }
      default:
        break;
      }

      break;
    }
    case SpvOpSpecConstantFalse:
    case SpvOpSpecConstantTrue: {
      constexpr uint32_t kOpSpecConstantTrueResultId = 2;

      const uint32_t resultId = instruction[kOpSpecConstantTrueResultId];
      const uint32_t specializedValue = idToValue[resultId];
      if (specializedValue == kNoValue) {
        break;
      }
      instruction[0] =
          makeOpCode(specializedValue ? SpvOpConstantTrue : SpvOpConstantFalse, instructionSize);
      break;
    }

    case SpvOpSpecConstant: {
      constexpr uint32_t kOpSpecConstantResultId = 2;
      constexpr uint32_t kOpSpecConstantValue = 3;

      const uint32_t resultId = instruction[kOpSpecConstantResultId];
      const uint32_t specializedValue = idToValue[resultId];
      if (specializedValue == kNoValue) {
        break;
      }
      instruction[0] = makeOpCode(SpvOpConstant, instructionSize);
      instruction[kOpSpecConstantValue] = specializedValue;
      break;
    }

    default:
      break;
    }

    IGL_DEBUG_ASSERT(instruction + instructionSize <= spirv + size);
    instruction += instructionSize;
  }
}

} // namespace igl::vulkan::util
