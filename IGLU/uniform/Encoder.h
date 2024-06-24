/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <igl/CommandBuffer.h>
#include <igl/Common.h>

namespace igl {
class IRenderCommandEncoder;
class IComputeCommandEncoder;
} // namespace igl

namespace iglu::uniform {

struct Descriptor;

// Encoder submits an uniform described by Descriptor.
//
// It handles backend-specific details:
// * For Metal, it calls igl::IRenderCommandEncoder::bindBytes() or
// igl::IComputeCommandEncoder::bindBytes()
// * For OpenGL, it calls igl::RenderCommandEncoder::bindUniform() or
// igl::IComputeCommandEncoder::bindUniform()
class Encoder {
 public:
  explicit Encoder(igl::BackendType backendType);
  void operator()(igl::IRenderCommandEncoder& encoder,
                  uint8_t bindTarget,
                  const Descriptor& uniform) const noexcept;

  void operator()(igl::IComputeCommandEncoder& encoder, const Descriptor& uniform) const noexcept;

 private:
  igl::BackendType backendType_;
};

} // namespace iglu::uniform
