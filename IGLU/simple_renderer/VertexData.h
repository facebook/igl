/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#pragma once

#include <igl/IGL.h>
#include <memory>
#include <vector>

namespace iglu {
namespace vertexdata {

/// Describes how the underlying APIs should interpret the buffers when drawing.
struct PrimitiveDesc {
  size_t numEntries = 0;
  size_t offset = 0;
  igl::WindingMode frontFaceWinding = igl::WindingMode::CounterClockwise;
};

/// Consolidates all vertex data input in a single place. Also handles binding and drawing.
class VertexData final {
 public:
  /// Preparates some of the rendering pipeline descriptors for this vertex data. Must be called
  /// before draw().
  void populatePipelineDescriptor(igl::RenderPipelineDesc& pipelineDesc) const;

  /// Invokes the draw command of the lower level APIs.
  void draw(igl::IRenderCommandEncoder& commandEncoder);

  PrimitiveDesc& primitiveDesc();
  std::shared_ptr<igl::IVertexInputState> vertexInputState();

  /// The arguments fully describe the vertex data and how various aspects of
  /// the rendering pipeline should interpret that data.
  /// @param vis Describes the layout of all the buffers
  /// @param vertexBuffer Buffer that contains the actual vertex information.
  /// @param indexBuffer Optional.
  /// @param indexBufferFormat 16 or 32-bit format. Ignored when indexBuffer is nullptr.
  /// @param primitiveDesc Additional information for the internal draw commands.
  VertexData(std::shared_ptr<igl::IVertexInputState> vis,
             std::shared_ptr<igl::IBuffer> vertexBuffer,
             std::shared_ptr<igl::IBuffer> indexBuffer,
             igl::IndexFormat indexBufferFormat,
             const PrimitiveDesc& primitiveDesc,
             igl::PrimitiveType topology = igl::PrimitiveType::Triangle);
  VertexData(igl::IDevice& device,
             const std::shared_ptr<igl::IVertexInputState>& vis,
             size_t bufferSize);
  ~VertexData() = default;

  /// Appends data to the vertex buffer.
  /// @param data Pointer to the new data. The memory will be copied.
  /// @param size Size of the new data.
  /// @param numPrimitives The number of draw primitives the new data
  /// corresponds to. The internal PrimitiveDesc will be updated.
  bool appendData(const void* data, size_t size, size_t numPrimitives);

  igl::IBuffer& indexBuffer() {
    IGL_ASSERT(ib_);
    return *ib_.get();
  }

  igl::IBuffer& vertexBuffer() {
    IGL_ASSERT(vb_);
    return *vb_.get();
  }

 protected:
  std::shared_ptr<igl::IVertexInputState> vis_;
  std::shared_ptr<igl::IBuffer> vb_;
  std::shared_ptr<igl::IBuffer> ib_;
  igl::IndexFormat ibFormat_ = igl::IndexFormat::UInt16;
  PrimitiveDesc primitiveDesc_;
  size_t usedBytes_ = 0;
  const igl::PrimitiveType topology_ = igl::PrimitiveType::Triangle;
};

} // namespace vertexdata
} // namespace iglu
