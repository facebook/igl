/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/uniform/Descriptor.h>
#include <IGLU/uniform/Encoder.h>
#include <igl/IGL.h>

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

namespace iglu::uniform {

// ----------------------------------------------------------------------------

namespace {

#if IGL_BACKEND_OPENGL
void bindRenderUniform(igl::IRenderCommandEncoder& encoder,
                       int bufferIndex,
                       const Descriptor& uniform) {
  // The openGL backend shaders use uniforms instead of uniformBlocks
  igl::UniformDesc descriptor;
  uniform.toUniformDescriptor(bufferIndex, descriptor);

  const void* data = uniform.data(Alignment::Packed);
  encoder.bindUniform(descriptor, data);
}

void bindComputeUniform(igl::IComputeCommandEncoder& encoder,
                        int bufferIndex,
                        const Descriptor& uniform) {
  // The openGL backend shaders use uniforms instead of uniformBlocks
  igl::UniformDesc descriptor;
  uniform.toUniformDescriptor(bufferIndex, descriptor);

  const void* data = uniform.data(Alignment::Packed);
  encoder.bindUniform(descriptor, data);
}
#endif

void encodeRenderUniform(igl::IRenderCommandEncoder& encoder,
                         int bufferIndex,
                         uint8_t bindTarget,
                         const Descriptor& uniform,
                         Alignment alignment) {
  const void* data = uniform.data(alignment);
  const size_t numBytes = uniform.numBytes(alignment);
  IGL_ASSERT_MSG(numBytes <= 4 * 1024,
                 "bindBytes should only be used for uniforms smaller than 4kb");
  encoder.bindBytes(bufferIndex, bindTarget, data, static_cast<int>(numBytes));
}

void encodeAlignedCompute(igl::IComputeCommandEncoder& encoder,
                          int bufferIndex,
                          const Descriptor& uniform) {
  const void* data = uniform.data(Alignment::Aligned);
  const size_t numBytes = uniform.numBytes(Alignment::Aligned);
  IGL_ASSERT_MSG(numBytes <= 4 * 1024,
                 "bindBytes should only be used for uniforms smaller than 4kb");
  encoder.bindBytes(bufferIndex, data, static_cast<int>(numBytes));
}

} // namespace

// ----------------------------------------------------------------------------

Encoder::Encoder(igl::BackendType backendType) : backendType_(backendType) {}

void Encoder::operator()(igl::IRenderCommandEncoder& encoder,
                         uint8_t bindTarget,
                         const Descriptor& uniform) const noexcept {
  const int bufferIndex =
      uniform.getIndex(bindTarget == igl::BindTarget::kVertex ? igl::ShaderStage::Vertex
                                                              : igl::ShaderStage::Fragment);
  if (!IGL_VERIFY(bufferIndex >= 0)) {
    return;
  }

  if (backendType_ == igl::BackendType::OpenGL) {
#if IGL_BACKEND_OPENGL
    bindRenderUniform(encoder, bufferIndex, uniform);
#else
    IGL_ASSERT_NOT_REACHED();
#endif
  } else if (backendType_ == igl::BackendType::Metal) {
    encodeRenderUniform(encoder, bufferIndex, bindTarget, uniform, Alignment::Aligned);
  } else if (backendType_ == igl::BackendType::Vulkan) {
    IGL_ASSERT_NOT_IMPLEMENTED();
  // @fb-only
    // @fb-only
  } else {
    IGL_ASSERT_NOT_REACHED();
  }
}

void Encoder::operator()(igl::IComputeCommandEncoder& encoder,
                         const Descriptor& uniform) const noexcept {
  const int bufferIndex = uniform.getIndex(igl::ShaderStage::Compute);
  if (!IGL_VERIFY(bufferIndex >= 0)) {
    return;
  }

  if (backendType_ == igl::BackendType::OpenGL) {
#if IGL_BACKEND_OPENGL
    bindComputeUniform(encoder, bufferIndex, uniform);
#else
    IGL_ASSERT_NOT_REACHED();
#endif
  } else if (backendType_ == igl::BackendType::Metal) {
    encodeAlignedCompute(encoder, bufferIndex, uniform);
  } else if (backendType_ == igl::BackendType::Vulkan) {
    IGL_ASSERT_NOT_IMPLEMENTED();
  // @fb-only
    // @fb-only
  }
}

} // namespace iglu::uniform
