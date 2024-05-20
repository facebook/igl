/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

#include "VertexData.h"

#include <utility>

namespace iglu {
namespace vertexdata {

VertexData::VertexData(std::shared_ptr<igl::IVertexInputState> vis,
                       std::shared_ptr<igl::IBuffer> vertexBuffer,
                       std::shared_ptr<igl::IBuffer> indexBuffer,
                       igl::IndexFormat indexBufferFormat,
                       const PrimitiveDesc& primitiveDesc) :
  vis_(std::move(vis)),
  vb_(std::move(vertexBuffer)),
  ib_(std::move(indexBuffer)),
  ibFormat_(indexBufferFormat),
  primitiveDesc_(primitiveDesc),
  usedBytes_(vb_->getSizeInBytes()) {}

VertexData::VertexData(igl::IDevice& device,
                       const std::shared_ptr<igl::IVertexInputState>& vis,
                       size_t bufferSize) :
  VertexData(vis,
             device.createBuffer(igl::BufferDesc(igl::BufferDesc::BufferTypeBits::Vertex,
                                                 nullptr,
                                                 bufferSize,
                                                 igl::ResourceStorage::Shared),
                                 nullptr),
             nullptr,
             igl::IndexFormat::UInt16,
             {}) {
  primitiveDesc_.type = igl::PrimitiveType::Point;
  usedBytes_ = 0;
}

void VertexData::populatePipelineDescriptor(igl::RenderPipelineDesc& pipelineDesc) const {
  pipelineDesc.vertexInputState = vis_;
  pipelineDesc.frontFaceWinding = primitiveDesc_.frontFaceWinding;
}

bool VertexData::appendData(const void* data, size_t size, size_t numPrimitives) {
  IGL_ASSERT(vb_);

  if (!vb_) {
    return false;
  }

  if (usedBytes_ + size > vb_->getSizeInBytes()) {
    return false;
  }

  vb_->upload(data, {size, usedBytes_});

  primitiveDesc_.numEntries += numPrimitives;
  usedBytes_ += size;

  return true;
}

void VertexData::draw(igl::IRenderCommandEncoder& commandEncoder) {
  if (primitiveDesc_.numEntries == 0) {
    return;
  }
  // Assumption: we don't need buffer offset
  if (vb_) {
    commandEncoder.bindVertexBuffer(0, *vb_);
  }

  if (ib_) {
    commandEncoder.bindIndexBuffer(*ib_, ibFormat_, primitiveDesc_.offset);
    commandEncoder.drawIndexed(primitiveDesc_.type, primitiveDesc_.numEntries);
  } else {
    commandEncoder.draw(primitiveDesc_.type, primitiveDesc_.offset, primitiveDesc_.numEntries);
  }
}

PrimitiveDesc& VertexData::primitiveDesc() {
  return primitiveDesc_;
}

std::shared_ptr<igl::IVertexInputState> VertexData::vertexInputState() {
  return vis_;
}

} // namespace vertexdata
} // namespace iglu
