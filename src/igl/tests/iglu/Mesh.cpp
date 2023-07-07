/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "../data/ShaderData.h"
#include "../util/Common.h"
#include "../util/TestDevice.h"

#include <gtest/gtest.h>
#include <igl/IGL.h>
#include <iglu/mesh/ElementStream.h>
#include <iglu/mesh/IndexStream.h>
#include <iglu/mesh/LayoutField.h>
#include <iglu/mesh/MeshIR.h>
#include <iglu/mesh/VertexStream.h>
#include <iglu/state_pool/RenderPipelineStatePool.h>
#include <string>

namespace igl {
namespace tests {

//
// MeshTest
//
class MeshTest : public ::testing::Test {
 private:
 public:
  MeshTest() = default;
  ~MeshTest() override = default;

  //
  // SetUp()
  //
  void SetUp() override {
    setDebugBreakEnabled(false);
  }

  void TearDown() override {}

  // Member variables
 public:
  const std::string backend_ = IGL_BACKEND_TYPE;
};

TEST_F(MeshTest, givenEmptyVertexStream_whenConstructed_GetElementCount) {
  Result ret;

  iglu::mesh::ElementStream::ElementLayout layout;
  layout.add(iglu::mesh::VertexStream::PosField());
  layout.add(iglu::mesh::VertexStream::NormalField());
  iglu::mesh::VertexStream stream(layout, igl::ResourceStorage::Managed);

  EXPECT_EQ(stream.getElementCount(), 0);
}

TEST_F(MeshTest, givenEmptyVertexStream_whenAdd10Elements_getElementCountEqual10) {
  Result ret;

  iglu::mesh::ElementStream::ElementLayout layout;
  layout.add(iglu::mesh::VertexStream::PosField());
  layout.add(iglu::mesh::VertexStream::NormalField());
  iglu::mesh::VertexStream stream(layout, igl::ResourceStorage::Managed);

  float pos[30] = {0.0f};
  stream.add(iglu::mesh::LayoutField::Semantic::Position, pos, sizeof(pos), 0, 10);

  EXPECT_EQ(stream.getElementCount(), 10);
}

const std::array<float, 12> positions{10, 10, 10, 40, 20, 30, 20, 40, 20, 60, 30, 20};
const std::array<float, 8> uvs{0, 0, 0, 1, 1, 0, 1, 1};
const std::array<uint16_t, 4> indices{0, 1, 2, 3};

struct VertexPosTex0 {
  float pos[3];
  float tex0[2];
};

const VertexPosTex0 vertexPosTex0[] = {{{10, 10, 10}, {0, 0}},
                                       {{40, 20, 30}, {0, 1}},
                                       {{20, 40, 20}, {1, 0}},
                                       {{60, 30, 20}, {1, 1}}};

TEST_F(MeshTest, fillIndexStream) {
  igl::Result ret;

  iglu::mesh::ElementStream::ElementLayout indexLayout;
  indexLayout.add(iglu::mesh::IndexStream::IndexField());

  iglu::mesh::IndexStream indexStream(indexLayout, igl::ResourceStorage::Managed);
  auto size = positions.size() / 3;
  indexStream.resize(static_cast<int>(size));

  indexStream.put(iglu::mesh::LayoutField::Semantic::Index,
                  indices.data(),
                  indices.size() * sizeof(uint16_t),
                  0,
                  0,
                  static_cast<int>(size));
  auto rawBuffer = indexStream.getRawBuffer();
  uint16_t* rawData = reinterpret_cast<uint16_t*>(rawBuffer.data());
  for (size_t i = 0; i < indices.size(); ++i) {
    ASSERT_EQ(indices[i], rawData[i]);
  }
}

TEST_F(MeshTest, fillVertexStream) {
  igl::Result ret;

  iglu::mesh::ElementStream::ElementLayout vertexLayout;
  vertexLayout.add(iglu::mesh::VertexStream::PosField());
  vertexLayout.add(iglu::mesh::VertexStream::TexCoordField());

  iglu::mesh::VertexStream vertexStream(vertexLayout, igl::ResourceStorage::Managed);
  auto size = positions.size() / 3;
  vertexStream.resize(static_cast<int>(size));

  vertexStream.put(iglu::mesh::LayoutField::Semantic::Position,
                   positions.data(),
                   positions.size() * sizeof(float),
                   0,
                   0,
                   static_cast<int>(size));
  vertexStream.put(iglu::mesh::LayoutField::Semantic::TexCoords0,
                   uvs.data(),
                   uvs.size() * sizeof(float),
                   0,
                   0,
                   static_cast<int>(size));

  auto rawBuffer = vertexStream.getRawBuffer();
  float* rawData = reinterpret_cast<float*>(rawBuffer.data());
  int posOffset = vertexStream.getElementLayout().getField(0).getFieldOffset() / sizeof(float);
  float* rawPosData = rawData + posOffset;
  int uvOffset = vertexStream.getElementLayout().getField(1).getFieldOffset() / sizeof(float);
  float* rawUVData = rawData + uvOffset;

  int stride = vertexStream.getElementLayout().getElementSize() / sizeof(float);

  for (size_t i = 0; i < indices.size(); ++i) {
    ASSERT_EQ(positions[i * 3 + 0], rawPosData[i * stride + 0]);
    ASSERT_EQ(positions[i * 3 + 1], rawPosData[i * stride + 1]);
    ASSERT_EQ(positions[i * 3 + 2], rawPosData[i * stride + 2]);

    ASSERT_EQ(uvs[i * 2 + 0], rawUVData[i * stride + 0]);
    ASSERT_EQ(uvs[i * 2 + 1], rawUVData[i * stride + 1]);
  }
}

TEST_F(MeshTest, constructBuilder1Buffer) {
  igl::Result ret;

  auto size = positions.size() / 3;

  iglu::mesh::MeshIR meshIR = iglu::mesh::MeshIR::Builder()
                                  .bufferCount(1)
                                  .vertexCount(static_cast<uint32_t>(size))
                                  .indexCount(static_cast<uint32_t>(size))
                                  .attribute(0, iglu::mesh::VertexStream::PosField())
                                  .attribute(0, iglu::mesh::VertexStream::TexCoordField())
                                  .build(ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  auto ptrStream1 = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::Position).get();
  auto ptrStream2 = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::TexCoords0).get();
  ASSERT_EQ(ptrStream1, ptrStream2);
}

TEST_F(MeshTest, constructBuilderFailChecks) {
  igl::Result ret;

  auto size = positions.size() / 3;

  {
    iglu::mesh::MeshIR meshIR = iglu::mesh::MeshIR::Builder()
                                    .bufferCount(0)
                                    .vertexCount(static_cast<uint32_t>(size))
                                    .indexCount(static_cast<uint32_t>(size))
                                    .attribute(0, iglu::mesh::VertexStream::PosField())
                                    .attribute(0, iglu::mesh::VertexStream::TexCoordField())
                                    .build(ret);

    ASSERT_EQ(ret.code, Result::Code::InvalidOperation);
  }

  {
    iglu::mesh::MeshIR meshIR = iglu::mesh::MeshIR::Builder()
                                    .bufferCount(1)
                                    .vertexCount(static_cast<uint32_t>(size))
                                    .indexCount(0)
                                    .attribute(0, iglu::mesh::VertexStream::PosField())
                                    .attribute(0, iglu::mesh::VertexStream::TexCoordField())
                                    .build(ret);

    ASSERT_EQ(ret.code, Result::Code::InvalidOperation);
  }

  {
    iglu::mesh::MeshIR meshIR = iglu::mesh::MeshIR::Builder()
                                    .bufferCount(1)
                                    .vertexCount(0)
                                    .indexCount(0)
                                    .attribute(0, iglu::mesh::VertexStream::PosField())
                                    .attribute(0, iglu::mesh::VertexStream::TexCoordField())
                                    .build(ret);

    ASSERT_EQ(ret.code, Result::Code::InvalidOperation);
  }

  {
    iglu::mesh::MeshIR meshIR = iglu::mesh::MeshIR::Builder()
                                    .bufferCount(1)
                                    .vertexCount(10)
                                    .indexCount(10)
                                    .attribute(1, iglu::mesh::VertexStream::PosField())
                                    .attribute(0, iglu::mesh::VertexStream::TexCoordField())
                                    .build(ret);

    ASSERT_EQ(ret.code, Result::Code::ArgumentOutOfRange);
  }
}

TEST_F(MeshTest, constructBuilder2Buffers) {
  igl::Result ret;

  auto size = positions.size() / 3;

  iglu::mesh::MeshIR meshIR = iglu::mesh::MeshIR::Builder()
                                  .bufferCount(2)
                                  .vertexCount(static_cast<uint32_t>(size))
                                  .indexCount(static_cast<uint32_t>(size))
                                  .attribute(0, iglu::mesh::VertexStream::PosField())
                                  .attribute(1, iglu::mesh::VertexStream::TexCoordField())
                                  .build(ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  auto ptrStream1 = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::Position).get();
  auto ptrStream2 = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::TexCoords0).get();
  ASSERT_NE(ptrStream1, ptrStream2);
}

TEST_F(MeshTest, constructAndFillBuilder1Buffer) {
  igl::Result ret;

  auto size = positions.size() / 3;

  iglu::mesh::MeshIR meshIR = iglu::mesh::MeshIR::Builder()
                                  .bufferCount(1)
                                  .vertexCount(static_cast<uint32_t>(size))
                                  .indexCount(static_cast<uint32_t>(size))
                                  .attribute(0, iglu::mesh::VertexStream::PosField())
                                  .attribute(0, iglu::mesh::VertexStream::TexCoordField())
                                  .build(ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  auto ptrStream1 = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::Position).get();
  auto ptrStream2 = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::TexCoords0).get();
  ASSERT_EQ(ptrStream1, ptrStream2);
  {
    auto indexStream = meshIR.getIndexStream();
    indexStream->put(iglu::mesh::LayoutField::Semantic::Index,
                     indices.data(),
                     indices.size() * sizeof(uint16_t),
                     0,
                     0,
                     static_cast<int>(size));
    auto rawBuffer = indexStream->getRawBuffer();
    uint16_t* rawData = reinterpret_cast<uint16_t*>(rawBuffer.data());
    for (size_t i = 0; i < indices.size(); ++i) {
      ASSERT_EQ(indices[i], rawData[i]);
    }
  }

  {
    auto vertexStream = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::Position);
    vertexStream->put(iglu::mesh::LayoutField::Semantic::Position,
                      positions.data(),
                      positions.size() * sizeof(float),
                      0,
                      0,
                      static_cast<int>(size));

    vertexStream = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::TexCoords0);
    vertexStream->put(iglu::mesh::LayoutField::Semantic::TexCoords0,
                      uvs.data(),
                      uvs.size() * sizeof(float),
                      0,
                      0,
                      static_cast<int>(size));

    auto rawBuffer = vertexStream->getRawBuffer();
    float* rawData = reinterpret_cast<float*>(rawBuffer.data());
    int posOffset = vertexStream->getElementLayout().getField(0).getFieldOffset() / sizeof(float);
    float* rawPosData = rawData + posOffset;
    int uvOffset = vertexStream->getElementLayout().getField(1).getFieldOffset() / sizeof(float);
    float* rawUVData = rawData + uvOffset;

    int stride = vertexStream->getElementLayout().getElementSize() / sizeof(float);

    for (size_t i = 0; i < indices.size(); ++i) {
      ASSERT_EQ(positions[i * 3 + 0], rawPosData[i * stride + 0]);
      ASSERT_EQ(positions[i * 3 + 1], rawPosData[i * stride + 1]);
      ASSERT_EQ(positions[i * 3 + 2], rawPosData[i * stride + 2]);

      ASSERT_EQ(uvs[i * 2 + 0], rawUVData[i * stride + 0]);
      ASSERT_EQ(uvs[i * 2 + 1], rawUVData[i * stride + 1]);
    }
  }
}

TEST_F(MeshTest, constructAndFillBuilder2Buffers) {
  igl::Result ret;

  auto size = positions.size() / 3;

  iglu::mesh::MeshIR meshIR = iglu::mesh::MeshIR::Builder()
                                  .bufferCount(2)
                                  .vertexCount(static_cast<uint32_t>(size))
                                  .indexCount(static_cast<uint32_t>(size))
                                  .attribute(0, iglu::mesh::VertexStream::PosField())
                                  .attribute(1, iglu::mesh::VertexStream::TexCoordField())
                                  .build(ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  auto ptrStream1 = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::Position).get();
  auto ptrStream2 = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::TexCoords0).get();
  ASSERT_NE(ptrStream1, ptrStream2);
  {
    auto indexStream = meshIR.getIndexStream();
    indexStream->put(iglu::mesh::LayoutField::Semantic::Index,
                     indices.data(),
                     indices.size() * sizeof(uint16_t),
                     0,
                     0,
                     static_cast<int>(size));
    auto rawBuffer = indexStream->getRawBuffer();
    uint16_t* rawData = reinterpret_cast<uint16_t*>(rawBuffer.data());
    for (size_t i = 0; i < indices.size(); ++i) {
      ASSERT_EQ(indices[i], rawData[i]);
    }
  }

  {
    auto vertexStreamPos = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::Position);
    vertexStreamPos->put(iglu::mesh::LayoutField::Semantic::Position,
                         positions.data(),
                         positions.size() * sizeof(float),
                         0,
                         0,
                         static_cast<int>(size));

    auto rawBuffer = vertexStreamPos->getRawBuffer();
    float* rawData = reinterpret_cast<float*>(rawBuffer.data());
    int posOffset =
        vertexStreamPos->getElementLayout().getField(0).getFieldOffset() / sizeof(float);
    float* rawPosData = rawData + posOffset;

    int stride = vertexStreamPos->getElementLayout().getElementSize() / sizeof(float);

    for (size_t i = 0; i < indices.size(); ++i) {
      ASSERT_EQ(positions[i * 3 + 0], rawPosData[i * stride + 0]);
      ASSERT_EQ(positions[i * 3 + 1], rawPosData[i * stride + 1]);
      ASSERT_EQ(positions[i * 3 + 2], rawPosData[i * stride + 2]);
    }
  }

  {
    auto vertexStreamTex = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::TexCoords0);
    vertexStreamTex->put(iglu::mesh::LayoutField::Semantic::TexCoords0,
                         uvs.data(),
                         uvs.size() * sizeof(float),
                         0,
                         0,
                         static_cast<int>(size));

    auto rawBuffer = vertexStreamTex->getRawBuffer();
    float* rawData = reinterpret_cast<float*>(rawBuffer.data());
    int uvOffset = vertexStreamTex->getElementLayout().getField(0).getFieldOffset() / sizeof(float);
    float* rawUVData = rawData + uvOffset;

    int stride = vertexStreamTex->getElementLayout().getElementSize() / sizeof(float);

    for (size_t i = 0; i < indices.size(); ++i) {
      ASSERT_EQ(uvs[i * 2 + 0], rawUVData[i * stride + 0]);
      ASSERT_EQ(uvs[i * 2 + 1], rawUVData[i * stride + 1]);
    }
  }
}

TEST_F(MeshTest, constructAndFillBuilder1BufferWithStruct) {
  igl::Result ret;

  auto size = positions.size() / 3;

  iglu::mesh::MeshIR meshIR = iglu::mesh::MeshIR::Builder()
                                  .bufferCount(1)
                                  .vertexCount(static_cast<uint32_t>(size))
                                  .indexCount(static_cast<uint32_t>(size))
                                  .attribute(0, iglu::mesh::VertexStream::PosField())
                                  .attribute(0, iglu::mesh::VertexStream::TexCoordField())
                                  .build(ret);

  ASSERT_EQ(ret.code, Result::Code::Ok);
  auto ptrStream1 = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::Position).get();
  auto ptrStream2 = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::TexCoords0).get();
  ASSERT_EQ(ptrStream1, ptrStream2);
  {
    auto indexStream = meshIR.getIndexStream();
    indexStream->put(iglu::mesh::LayoutField::Semantic::Index,
                     indices.data(),
                     indices.size() * sizeof(uint16_t),
                     0,
                     0,
                     static_cast<int>(size));
    auto rawBuffer = indexStream->getRawBuffer();
    uint16_t* rawData = reinterpret_cast<uint16_t*>(rawBuffer.data());
    for (size_t i = 0; i < indices.size(); ++i) {
      ASSERT_EQ(indices[i], rawData[i]);
    }
  }

  {
    auto vertexStream = meshIR.getVertexStream(iglu::mesh::LayoutField::Semantic::Attribute0);
    vertexStream->putRaw((uint8_t*)vertexPosTex0,
                         sizeof(vertexPosTex0),
                         0,
                         0,
                         sizeof(VertexPosTex0),
                         static_cast<int>(size));

    auto rawBuffer = vertexStream->getRawBuffer();
    float* rawData = reinterpret_cast<float*>(rawBuffer.data());
    int posOffset = vertexStream->getElementLayout().getField(0).getFieldOffset() / sizeof(float);
    float* rawPosData = rawData + posOffset;
    int uvOffset = vertexStream->getElementLayout().getField(1).getFieldOffset() / sizeof(float);
    float* rawUVData = rawData + uvOffset;

    int stride = vertexStream->getElementLayout().getElementSize() / sizeof(float);

    for (size_t i = 0; i < 4; ++i) {
      ASSERT_EQ(vertexPosTex0[i].pos[0], rawPosData[i * stride + 0]);
      ASSERT_EQ(vertexPosTex0[i].pos[1], rawPosData[i * stride + 1]);
      ASSERT_EQ(vertexPosTex0[i].pos[2], rawPosData[i * stride + 2]);

      ASSERT_EQ(vertexPosTex0[i].tex0[0], rawUVData[i * stride + 0]);
      ASSERT_EQ(vertexPosTex0[i].tex0[1], rawUVData[i * stride + 1]);
    }
  }
}

} // namespace tests
} // namespace igl
