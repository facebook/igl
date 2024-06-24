/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/VertexInputState.h>

using namespace igl;

size_t VertexInputStateDesc::sizeForVertexAttributeFormat(VertexAttributeFormat format) {
  switch (format) {
  case VertexAttributeFormat::Float1:
    return sizeof(float);

  case VertexAttributeFormat::Float2:
    return sizeof(float[2]);

  case VertexAttributeFormat::Float3:
    return sizeof(float[3]);

  case VertexAttributeFormat::Float4:
    return sizeof(float[4]);

  case VertexAttributeFormat::Byte1:
  case VertexAttributeFormat::Byte1Norm:
    return sizeof(int8_t);

  case VertexAttributeFormat::Byte2:
  case VertexAttributeFormat::Byte2Norm:
    return sizeof(int8_t[2]);

  case VertexAttributeFormat::Byte3:
  case VertexAttributeFormat::Byte3Norm:
    return sizeof(int8_t[3]);

  case VertexAttributeFormat::Byte4:
  case VertexAttributeFormat::Byte4Norm:
    return sizeof(int8_t[4]);

  case VertexAttributeFormat::UByte1:
  case VertexAttributeFormat::UByte1Norm:
    return sizeof(uint8_t[1]);

  case VertexAttributeFormat::UByte2:
  case VertexAttributeFormat::UByte2Norm:
    return sizeof(uint8_t[2]);

  case VertexAttributeFormat::UByte3:
  case VertexAttributeFormat::UByte3Norm:
    return sizeof(uint8_t[3]);

  case VertexAttributeFormat::UByte4:
  case VertexAttributeFormat::UByte4Norm:
    return sizeof(uint8_t[4]);

  case VertexAttributeFormat::Short1:
  case VertexAttributeFormat::Short1Norm:
    return sizeof(int16_t);

  case VertexAttributeFormat::Short2:
  case VertexAttributeFormat::Short2Norm:
    return sizeof(int16_t[2]);

  case VertexAttributeFormat::Short3:
  case VertexAttributeFormat::Short3Norm:
    return sizeof(int16_t[3]);

  case VertexAttributeFormat::Short4:
  case VertexAttributeFormat::Short4Norm:
    return sizeof(int16_t[4]);

  case VertexAttributeFormat::UShort1:
  case VertexAttributeFormat::UShort1Norm:
    return sizeof(uint16_t);

  case VertexAttributeFormat::UShort2:
  case VertexAttributeFormat::UShort2Norm:
    return sizeof(uint16_t[2]);

  case VertexAttributeFormat::UShort3:
  case VertexAttributeFormat::UShort3Norm:
    return sizeof(uint16_t[3]);

  case VertexAttributeFormat::UShort4:
  case VertexAttributeFormat::UShort4Norm:
    return sizeof(uint16_t[4]);

  case VertexAttributeFormat::Int1:
  case VertexAttributeFormat::UInt1:
    return sizeof(uint32_t[1]);

  case VertexAttributeFormat::Int2:
  case VertexAttributeFormat::UInt2:
    return sizeof(uint32_t[2]);

  case VertexAttributeFormat::Int3:
  case VertexAttributeFormat::UInt3:
    return sizeof(uint32_t[3]);

  case VertexAttributeFormat::Int4:
  case VertexAttributeFormat::UInt4:
    return sizeof(uint32_t[4]);

  case VertexAttributeFormat::HalfFloat1:
    return sizeof(uint16_t[1]);
  case VertexAttributeFormat::HalfFloat2:
    return sizeof(uint16_t[2]);
  case VertexAttributeFormat::HalfFloat3:
    return sizeof(uint16_t[3]);
  case VertexAttributeFormat::HalfFloat4:
    return sizeof(uint16_t[4]);

  case VertexAttributeFormat::Int_2_10_10_10_REV:
    return sizeof(uint32_t);
  }
  IGL_UNREACHABLE_RETURN(0)
}

bool VertexInputBinding::operator!=(const VertexInputBinding& other) const {
  return !(*this == other);
}

bool VertexInputBinding::operator==(const VertexInputBinding& other) const {
  return other.sampleRate == sampleRate && other.sampleFunction == sampleFunction &&
         other.stride == stride;
}

size_t std::hash<VertexInputBinding>::operator()(const VertexInputBinding& key) const {
  size_t hash = 0;
  hash ^= std::hash<int>()(static_cast<int>(key.sampleRate));
  hash ^= std::hash<int>()(EnumToValue(key.sampleFunction));
  hash ^= std::hash<int>()(static_cast<int>(key.stride));
  return hash;
}

bool VertexAttribute::operator!=(const VertexAttribute& other) const {
  return !(*this == other);
}

size_t std::hash<VertexAttribute>::operator()(const VertexAttribute& key) const {
  size_t hash = 0;
  hash ^= std::hash<std::string>()(key.name);
  hash ^= std::hash<int>()(key.location);
  hash ^= std::hash<uintptr_t>()(key.offset);
  hash ^= std::hash<size_t>()(key.bufferIndex);
  hash ^= std::hash<int>()(static_cast<int>(EnumToValue(key.format)));
  return hash;
}

bool VertexAttribute::operator==(const VertexAttribute& other) const {
  return other.name == name && other.location == location && other.bufferIndex == bufferIndex &&
         other.format == format && other.offset == offset;
}

bool VertexInputStateDesc::operator!=(const VertexInputStateDesc& other) const {
  return !(*this == other);
}

bool VertexInputStateDesc::operator==(const VertexInputStateDesc& other) const {
  if (other.numAttributes != numAttributes || other.numInputBindings != numInputBindings) {
    return false;
  }

  for (auto i = 0; i < numAttributes; i++) {
    if (other.attributes[i] != attributes[i]) {
      return false;
    }
  }

  for (auto i = 0; i < numInputBindings; i++) {
    if (other.inputBindings[i] != inputBindings[i]) {
      return false;
    }
  }

  return true;
}

size_t std::hash<VertexInputStateDesc>::operator()(const VertexInputStateDesc& key) const {
  size_t hash = 0;
  hash ^= std::hash<size_t>()(key.numInputBindings);
  hash ^= std::hash<size_t>()(key.numAttributes);

  for (size_t i = 0; i < key.numAttributes; i++) {
    hash ^= std::hash<VertexAttribute>()(key.attributes[i]);
  }

  for (size_t i = 0; i < key.numInputBindings; i++) {
    hash ^= std::hash<VertexInputBinding>()(key.inputBindings[i]);
  }
  return hash;
}
