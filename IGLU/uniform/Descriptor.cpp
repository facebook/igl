/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/uniform/Descriptor.h>

namespace iglu::uniform {

Descriptor::Descriptor(igl::UniformType type) : type_(type) {}

igl::UniformType Descriptor::getType() const noexcept {
  return type_;
}

int Descriptor::getIndex(igl::ShaderStage stage) const noexcept {
  return indices_[EnumToValue(stage)];
}

void Descriptor::setIndex(igl::ShaderStage stage, int newValue) noexcept {
  indices_[EnumToValue(stage)] = newValue;
}

#if IGL_BACKEND_OPENGL
void Descriptor::toUniformDescriptor(int location, igl::UniformDesc& outDescriptor) const noexcept {
  outDescriptor.location = location;
  outDescriptor.offset = 0;
  outDescriptor.type = type_;
  outDescriptor.numElements = size();
  outDescriptor.elementStride = igl::sizeForUniformType(type_);
}
#endif

} // namespace iglu::uniform
