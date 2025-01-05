/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

/*
 * Base on https://github.com/ollix/MetalNanoVG
 */

#include "nanovg_igl.h"
#include "nanovg.h"
#include "shader_metal.h"
#include "shader_opengl.h"
#include <IGLU/simdtypes/SimdTypes.h>
#include <igl/IGL.h>
#include <math.h>
#include <regex>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define kVertexInputIndex 0
#define kVertexUniformBlockIndex 1
#define kFragmentUniformBlockIndex 2

namespace iglu::nanovg {

struct igl_vector_uint2 {
  uint32_t x;
  uint32_t y;
};

enum ShaderType {
  MNVG_SHADER_FILLGRAD,
  MNVG_SHADER_FILLIMG,
  MNVG_SHADER_IMG,
};

enum CallType {
  MNVG_NONE = 0,
  MNVG_FILL,
  MNVG_CONVEXFILL,
  MNVG_STROKE,
  MNVG_TRIANGLES,
};

struct Blend {
  igl::BlendFactor srcRGB;
  igl::BlendFactor dstRGB;
  igl::BlendFactor srcAlpha;
  igl::BlendFactor dstAlpha;
};

struct UniformBufferIndex {
  igl::IBuffer* buffer = nullptr;
  void* data = nullptr;
  size_t offset = 0;
};

struct Call {
  int type;
  int image;
  int pathOffset;
  int pathCount;
  int triangleOffset;
  int triangleCount;
  int indexOffset;
  int indexCount;
  int strokeOffset;
  int strokeCount;
  UniformBufferIndex uboIndex;
  UniformBufferIndex uboIndex2;
  Blend blendFunc;
};

struct VertexUniforms {
  iglu::simdtypes::float4x4 matrix;
  iglu::simdtypes::float2 viewSize;
};

struct FragmentUniforms {
  iglu::simdtypes::float3x3 scissorMat;
  iglu::simdtypes::float3x3 paintMat;
  iglu::simdtypes::float4 innerCol;
  iglu::simdtypes::float4 outerCol;
  iglu::simdtypes::float2 scissorExt;
  iglu::simdtypes::float2 scissorScale;
  iglu::simdtypes::float2 extent;
  float radius;
  float feather;
  float strokeMult;
  float strokeThr;
  int texType;
  ShaderType type;
};

struct Texture {
  int Id;
  int type;
  int flags;
  std::shared_ptr<igl::ITexture> tex;
  std::shared_ptr<igl::ISamplerState> sampler;
};

class UniformBufferBlock {
 public:
  UniformBufferBlock(igl::IDevice* device, size_t blockSize) : blockSize_(blockSize) {
    data_.resize(blockSize);
    igl::BufferDesc desc(igl::BufferDesc::BufferTypeBits::Uniform,
                         data_.data(),
                         blockSize,
                         igl::ResourceStorage::Shared);
    desc.hint = igl::BufferDesc::BufferAPIHintBits::UniformBlock;
    desc.debugName = "fragment_uniform_buffer";
    buffer_ = device->createBuffer(desc, NULL);
  }

  ~UniformBufferBlock() {
    IGL_LOG_DEBUG("iglu::nanovg::UniformBufferBlock::~UniformBufferBlock()\n");
  }

  bool checkLeftSpace(size_t dataSize) {
    return current_ + dataSize <= blockSize_;
  }

  UniformBufferIndex allocData(size_t dataSize) {
    assert(checkLeftSpace(dataSize));

    UniformBufferIndex index{buffer_.get(), data_.data() + current_, current_};

    current_ += dataSize;

    return index;
  }

  void uploadToGpu() {
    buffer_->upload(data_.data(), igl::BufferRange(data_.size()));
  }

  void reset() {
    current_ = 0;
  }

 private:
  std::shared_ptr<igl::IBuffer> buffer_;
  std::vector<unsigned char> data_;
  size_t blockSize_ = 0;
  size_t current_ = 0;
};

class UniformBufferPool {
 public:
  UniformBufferPool(igl::IDevice* device, size_t blockSize) :
    device_(device), blockSize_(blockSize) {
    allocNewBlock();
  }

  UniformBufferIndex allocData(size_t dataSize) {
    if (!bufferBlocks_[currentBlockIndex]->checkLeftSpace(dataSize)) {
      currentBlockIndex++;
      if (bufferBlocks_.size() <= currentBlockIndex) {
        allocNewBlock();
      }
    }

    return bufferBlocks_[currentBlockIndex]->allocData(dataSize);
  }

  void uploadToGpu() {
    for (auto& block : bufferBlocks_) {
      block->uploadToGpu();
    }
  }

  void reset() {
    currentBlockIndex = 0;
    for (auto& block : bufferBlocks_) {
      block->reset();
    }
  }

 private:
  void allocNewBlock() {
    bufferBlocks_.emplace_back(std::make_shared<UniformBufferBlock>(device_, blockSize_));
  }

 private:
  std::vector<std::shared_ptr<UniformBufferBlock>> bufferBlocks_;
  size_t blockSize_ = 0;
  igl::IDevice* device_ = nullptr;
  size_t currentBlockIndex = 0;
};

struct Buffers {
  std::shared_ptr<igl::ICommandBuffer> commandBuffer;
  bool isBusy = false;
  int image = 0;
  std::shared_ptr<igl::IBuffer> vertexUniformBuffer;
  VertexUniforms vertexUniforms;
  std::shared_ptr<igl::ITexture> stencilTexture;
  std::vector<Call> calls;
  int ccalls = 0;
  int ncalls = 0;
  std::shared_ptr<igl::IBuffer> indexBuffer;
  std::vector<uint32_t> indexes;
  int cindexes = 0;
  int nindexes = 0;
  std::shared_ptr<igl::IBuffer> vertBuffer;
  std::vector<NVGvertex> verts;
  int cverts = 0;
  int nverts = 0;
  std::shared_ptr<UniformBufferPool> uniformBufferPool;

  Buffers(igl::IDevice* device, size_t uniformBufferBlockSize) {
    vertexUniforms.matrix = iglu::simdtypes::float4x4(1.0f);
    uniformBufferPool = std::make_shared<UniformBufferPool>(device, uniformBufferBlockSize);
  }

  ~Buffers() {
    IGL_LOG_DEBUG("iglu::nanovg::Buffers::~Buffers()\n");
  }

  void uploadToGpu() {
    if (vertBuffer) {
      vertBuffer->upload(verts.data(), igl::BufferRange(verts.size() * sizeof(NVGvertex)));
    }

    if (indexBuffer) {
      indexBuffer->upload(indexes.data(), igl::BufferRange(indexes.size() * sizeof(uint32_t)));
    }

    if (vertexUniformBuffer) {
      vertexUniformBuffer->upload(&vertexUniforms, igl::BufferRange(sizeof(VertexUniforms)));
    }

    uniformBufferPool->uploadToGpu();
  }
};

static bool convertBlendFuncFactor(int factor, igl::BlendFactor* result) {
  if (factor == NVG_ZERO)
    *result = igl::BlendFactor::Zero;
  else if (factor == NVG_ONE)
    *result = igl::BlendFactor::One;
  else if (factor == NVG_SRC_COLOR)
    *result = igl::BlendFactor::SrcColor;
  else if (factor == NVG_ONE_MINUS_SRC_COLOR)
    *result = igl::BlendFactor::OneMinusSrcColor;
  else if (factor == NVG_DST_COLOR)
    *result = igl::BlendFactor::DstColor;
  else if (factor == NVG_ONE_MINUS_DST_COLOR)
    *result = igl::BlendFactor::OneMinusDstColor;
  else if (factor == NVG_SRC_ALPHA)
    *result = igl::BlendFactor::SrcAlpha;
  else if (factor == NVG_ONE_MINUS_SRC_ALPHA)
    *result = igl::BlendFactor::OneMinusSrcAlpha;
  else if (factor == NVG_DST_ALPHA)
    *result = igl::BlendFactor::DstAlpha;
  else if (factor == NVG_ONE_MINUS_DST_ALPHA)
    *result = igl::BlendFactor::OneMinusDstAlpha;
  else if (factor == NVG_SRC_ALPHA_SATURATE)
    *result = igl::BlendFactor::SrcAlphaSaturated;
  else
    return false;
  return true;
}

static int MAXINT(int a, int b) {
  return a > b ? a : b;
}

static int maxVertexCount(const NVGpath* paths, int npaths, int* indexCount, int* strokeCount) {
  int count = 0;
  if (indexCount != NULL)
    *indexCount = 0;
  if (strokeCount != NULL)
    *strokeCount = 0;
  NVGpath* path = (NVGpath*)&paths[0];
  for (int i = npaths; i--; ++path) {
    const int nfill = path->nfill;
    if (nfill > 2) {
      count += nfill;
      if (indexCount != NULL)
        *indexCount += (nfill - 2) * 3;
    }
    if (path->nstroke > 0) {
      const int nstroke = path->nstroke + 2;
      count += nstroke;
      if (strokeCount != NULL)
        *strokeCount += nstroke;
    }
  }
  return count;
}

static iglu::simdtypes::float4 preMultiplyColor(NVGcolor c) {
  c.r *= c.a;
  c.g *= c.a;
  c.b *= c.a;
  iglu::simdtypes::float4 ret;
  memcpy(&ret, &c, sizeof(float) * 4);
  return ret;
}

static void transformToMat3x3(iglu::simdtypes::float3x3* m3, float* t) {
  float columns_0[3] = {t[0], t[1], 0.0f};
  float columns_1[3] = {t[2], t[3], 0.0f};
  float columns_2[3] = {t[4], t[5], 1.0f};

  memcpy(&m3->columns[0], columns_0, 3 * sizeof(float));
  memcpy(&m3->columns[1], columns_1, 3 * sizeof(float));
  memcpy(&m3->columns[2], columns_2, 3 * sizeof(float));
}

static void setVertextData(NVGvertex* vtx, float x, float y, float u, float v) {
  vtx->x = x;
  vtx->y = y;
  vtx->u = u;
  vtx->v = v;
}

class Context {
 public:
  igl::IDevice* device_ = nullptr;
  igl::IRenderCommandEncoder* renderEncoder_ = nullptr;

  size_t fragmentUniformBufferSize_;
  size_t maxUniformBufferSize_;
  int indexSize_;
  int flags_;
  igl_vector_uint2 viewPortSize_;

  igl::IFramebuffer* framebuffer_ = nullptr;

  // Textures
  std::vector<std::shared_ptr<Texture>> textures_;
  int textureId_;

  // Per frame buffers
  std::shared_ptr<Buffers> curBuffers_ = nullptr;
  std::vector<std::shared_ptr<Buffers>> allBuffers_;
  int maxBuffers_;

  // Cached states.
  Blend* blendFunc_ = nullptr;
  std::shared_ptr<igl::IDepthStencilState> defaultStencilState_;
  std::shared_ptr<igl::IDepthStencilState> fillShapeStencilState_;
  std::shared_ptr<igl::IDepthStencilState> fillAntiAliasStencilState_;
  std::shared_ptr<igl::IDepthStencilState> fillStencilState_;
  std::shared_ptr<igl::IDepthStencilState> strokeShapeStencilState_;
  std::shared_ptr<igl::IDepthStencilState> strokeAntiAliasStencilState_;
  std::shared_ptr<igl::IDepthStencilState> strokeClearStencilState_;
  std::shared_ptr<igl::IShaderModule> fragmentFunction_;
  std::shared_ptr<igl::IShaderModule> vertexFunction_;
  igl::TextureFormat piplelinePixelFormat_;
  std::shared_ptr<igl::IRenderPipelineState> pipelineState_;
  std::shared_ptr<igl::IRenderPipelineState> pipelineStateTriangleStrip_;
  std::shared_ptr<igl::IRenderPipelineState> stencilOnlyPipelineState_;
  std::shared_ptr<igl::IRenderPipelineState> stencilOnlyPipelineStateTriangleStrip_;
  std::shared_ptr<igl::ISamplerState> pseudoSampler_;
  std::shared_ptr<igl::ITexture> pseudoTexture_;
  igl::VertexInputStateDesc vertexDescriptor_;

  Context() {
    IGL_LOG_DEBUG("iglu::nanovg::Context::Context()\n");
  }

  ~Context() {
    IGL_LOG_DEBUG("iglu::nanovg::Context::~Context()\n");
  }

  Call* allocCall() {
    Call* ret = NULL;
    if (curBuffers_->ncalls + 1 > curBuffers_->ccalls) {
      int ccalls = MAXINT(curBuffers_->ncalls + 1, 128) + curBuffers_->ccalls / 2;
      curBuffers_->calls.resize(ccalls);
      curBuffers_->ccalls = ccalls;
    }
    ret = &curBuffers_->calls[curBuffers_->ncalls++];
    memset(ret, 0, sizeof(Call));
    return ret;
  }

  UniformBufferIndex allocFragUniforms(size_t dataSize) {
    return curBuffers_->uniformBufferPool->allocData(dataSize);
  }

  int allocIndexes(int n) {
    int ret = 0;
    if (curBuffers_->nindexes + n > curBuffers_->cindexes) {
      int cindexes = MAXINT(curBuffers_->nindexes + n, 4096) + curBuffers_->cindexes / 2;
      curBuffers_->indexes.resize(cindexes);

      igl::BufferDesc desc(igl::BufferDesc::BufferTypeBits::Index,
                           curBuffers_->indexes.data(),
                           indexSize_ * cindexes,
                           igl::ResourceStorage::Shared);
      desc.debugName = "index_buffer";
      std::shared_ptr<igl::IBuffer> buffer = device_->createBuffer(desc, NULL);

      curBuffers_->indexBuffer = buffer;
      curBuffers_->cindexes = cindexes;
    }
    ret = curBuffers_->nindexes;
    curBuffers_->nindexes += n;
    return ret;
  }

  std::shared_ptr<Texture> allocTexture() {
    std::shared_ptr<Texture> tex = nullptr;

    for (auto& texture : textures_) {
      if (texture->Id == 0) {
        tex = texture;
        break;
      }
    }
    if (tex == nullptr) {
      tex = std::make_shared<Texture>();
      textures_.emplace_back(tex);
    }
    tex->Id = ++textureId_;
    return tex;
  }

  int allocVerts(int n) {
    int ret = 0;
    if (curBuffers_->nverts + n > curBuffers_->cverts) {
      int cverts = MAXINT(curBuffers_->nverts + n, 4096) + curBuffers_->cverts / 2;
      curBuffers_->verts.resize(cverts);

      igl::BufferDesc desc(igl::BufferDesc::BufferTypeBits::Vertex,
                           curBuffers_->verts.data(),
                           sizeof(NVGvertex) * cverts,
                           igl::ResourceStorage::Shared);
      desc.debugName = "vertex_buffer";
      std::shared_ptr<igl::IBuffer> buffer = device_->createBuffer(desc, NULL);

      curBuffers_->vertBuffer = buffer;
      curBuffers_->cverts = cverts;
    }
    ret = curBuffers_->nverts;
    curBuffers_->nverts += n;
    return ret;
  }

  Blend blendCompositeOperation(NVGcompositeOperationState op) {
    Blend blend;
    if (!convertBlendFuncFactor(op.srcRGB, &blend.srcRGB) ||
        !convertBlendFuncFactor(op.dstRGB, &blend.dstRGB) ||
        !convertBlendFuncFactor(op.srcAlpha, &blend.srcAlpha) ||
        !convertBlendFuncFactor(op.dstAlpha, &blend.dstAlpha)) {
      blend.srcRGB = igl::BlendFactor::One;
      blend.dstRGB = igl::BlendFactor::OneMinusSrcAlpha;
      blend.srcAlpha = igl::BlendFactor::One;
      blend.dstAlpha = igl::BlendFactor::OneMinusSrcAlpha;
    }
    return blend;
  }

  int convertPaintForFrag(FragmentUniforms* frag,
                          NVGpaint* paint,
                          NVGscissor* scissor,
                          float width,
                          float fringe,
                          float strokeThr) {
    std::shared_ptr<Texture> tex = nullptr;
    float invxform[6];

    memset(frag, 0, sizeof(*frag));

    frag->innerCol = preMultiplyColor(paint->innerColor);
    frag->outerCol = preMultiplyColor(paint->outerColor);

    if (scissor->extent[0] < -0.5f || scissor->extent[1] < -0.5f) {
      frag->scissorMat = iglu::simdtypes::float3x3(0);
      frag->scissorExt[0] = 1.0f;
      frag->scissorExt[1] = 1.0f;
      frag->scissorScale[0] = 1.0f;
      frag->scissorScale[1] = 1.0f;
    } else {
      nvgTransformInverse(invxform, scissor->xform);
      transformToMat3x3(&frag->scissorMat, invxform);
      frag->scissorExt[0] = scissor->extent[0];
      frag->scissorExt[1] = scissor->extent[1];
      frag->scissorScale[0] =
          sqrtf(scissor->xform[0] * scissor->xform[0] + scissor->xform[2] * scissor->xform[2]) /
          fringe;
      frag->scissorScale[1] =
          sqrtf(scissor->xform[1] * scissor->xform[1] + scissor->xform[3] * scissor->xform[3]) /
          fringe;
    }

    frag->extent = iglu::simdtypes::float2{paint->extent[0], paint->extent[1]};
    frag->strokeMult = (width * 0.5f + fringe * 0.5f) / fringe;
    frag->strokeThr = strokeThr;

    if (paint->image != 0) {
      tex = findTexture(paint->image);
      if (tex == nullptr)
        return 0;
      if (tex->flags & NVG_IMAGE_FLIPY) {
        float m1[6], m2[6];
        nvgTransformTranslate(m1, 0.0f, frag->extent[0] * 0.5f);
        nvgTransformMultiply(m1, paint->xform);
        nvgTransformScale(m2, 1.0f, -1.0f);
        nvgTransformMultiply(m2, m1);
        nvgTransformTranslate(m1, 0.0f, -frag->extent[1] * 0.5f);
        nvgTransformMultiply(m1, m2);
        nvgTransformInverse(invxform, m1);
      } else {
        nvgTransformInverse(invxform, paint->xform);
      }
      frag->type = MNVG_SHADER_FILLIMG;

      if (tex->type == NVG_TEXTURE_RGBA)
        frag->texType = (tex->flags & NVG_IMAGE_PREMULTIPLIED) ? 0 : 1;
      else
        frag->texType = 2;
    } else {
      frag->type = MNVG_SHADER_FILLGRAD;
      frag->radius = paint->radius;
      frag->feather = paint->feather;
      nvgTransformInverse(invxform, paint->xform);
    }

    transformToMat3x3(&frag->paintMat, invxform);

    return 1;
  }

  void bindRenderPipeline(const std::shared_ptr<igl::IRenderPipelineState>& pipelineState,
                          const UniformBufferIndex* uboIndex = nullptr) {
    renderEncoder_->bindRenderPipelineState(pipelineState);
    renderEncoder_->bindVertexBuffer(kVertexInputIndex, *curBuffers_->vertBuffer, 0);
    renderEncoder_->bindBuffer(kVertexUniformBlockIndex, curBuffers_->vertexUniformBuffer.get(), 0);
    if (uboIndex) {
      renderEncoder_->bindBuffer(kFragmentUniformBlockIndex, uboIndex->buffer, uboIndex->offset);
    }
  }

  void convexFill(Call* call) {
    const int kIndexBufferOffset = call->indexOffset * indexSize_;
    bindRenderPipeline(pipelineState_);
    setUniforms(call->uboIndex, call->image);
    if (call->indexCount > 0) {
      renderEncoder_->bindIndexBuffer(
          *curBuffers_->indexBuffer, igl::IndexFormat::UInt32, kIndexBufferOffset);
      renderEncoder_->drawIndexed(call->indexCount);
    }

    // Draw fringes
    if (call->strokeCount > 0) {
      bindRenderPipeline(pipelineStateTriangleStrip_);
      renderEncoder_->draw(call->strokeCount, 1, call->strokeOffset);
    }
  }

  void fill(Call* call) {
    // Draws shapes.
    const int kIndexBufferOffset = call->indexOffset * indexSize_;
    bindRenderPipeline(stencilOnlyPipelineState_, &call->uboIndex);
    renderEncoder_->bindDepthStencilState(fillShapeStencilState_);
    if (call->indexCount > 0) {
      renderEncoder_->bindIndexBuffer(
          *curBuffers_->indexBuffer, igl::IndexFormat::UInt32, kIndexBufferOffset);
      renderEncoder_->drawIndexed(call->indexCount);
    }

    // Restores states.
    bindRenderPipeline(pipelineStateTriangleStrip_);

    // Draws anti-aliased fragments.
    setUniforms(call->uboIndex, call->image);
    if (flags_ & NVG_ANTIALIAS && call->strokeCount > 0) {
      renderEncoder_->bindDepthStencilState(fillAntiAliasStencilState_);
      renderEncoder_->draw(call->strokeCount, 1, call->strokeOffset);
    }

    // Draws fill.
    renderEncoder_->bindDepthStencilState(fillStencilState_);
    renderEncoder_->draw(call->triangleCount, 1, call->triangleOffset);
    renderEncoder_->bindDepthStencilState(defaultStencilState_);
  }

  std::shared_ptr<Texture> findTexture(int _id) {
    for (auto& texture : textures_) {
      if (texture->Id == _id)
        return texture;
    }
    return nullptr;
  }

  void renderCancel() {
    curBuffers_->image = 0;
    curBuffers_->isBusy = false;
    curBuffers_->nindexes = 0;
    curBuffers_->nverts = 0;
    curBuffers_->ncalls = 0;
    curBuffers_->uniformBufferPool->reset();
  }

  void renderCommandEncoderWithColorTexture() {
    renderEncoder_->setStencilReferenceValue(0);
    renderEncoder_->bindViewport(
        {0.0, 0.0, (float)viewPortSize_.x, (float)viewPortSize_.y, 0.0, 1.0});
    renderEncoder_->bindVertexBuffer(kVertexInputIndex, *curBuffers_->vertBuffer, 0);
    renderEncoder_->bindBuffer(kVertexUniformBlockIndex, curBuffers_->vertexUniformBuffer.get(), 0);
  }

  int renderCreate() {
    bool creates_pseudo_texture = false;

    igl::Result result;

    if (device_->getBackendType() == igl::BackendType::Metal) {
      const std::string vertexEntryPoint = "vertexShader";
      std::string fragmentEntryPoint = (flags_ & NVG_ANTIALIAS) ? "fragmentShaderAA"
                                                                : "fragmentShader";

      std::unique_ptr<igl::IShaderLibrary> shader_library =
          igl::ShaderLibraryCreator::fromStringInput(
              *device_, metalShader.c_str(), vertexEntryPoint, fragmentEntryPoint, "", &result);

      vertexFunction_ = shader_library->getShaderModule(vertexEntryPoint);
      fragmentFunction_ = shader_library->getShaderModule(fragmentEntryPoint);
    } else if (device_->getBackendType() == igl::BackendType::OpenGL) {
#if IGL_PLATFORM_ANDROID || IGL_PLATFORM_IOS || IGL_PLATFORM_LINUX
      auto codeVS = std::regex_replace(
          openglVertexShaderHeader410, std::regex("#version 410"), "#version 300 es");
      auto codeFS = std::regex_replace(
          openglFragmentShaderHeader410, std::regex("#version 410"), "#version 300 es");

      codeVS += openglVertexShaderBody;
      codeFS += ((flags_ & NVG_ANTIALIAS) ? openglAntiAliasingFragmentShaderBody
                                          : openglNoAntiAliasingFragmentShaderBody);
#else
      auto codeVS = openglVertexShaderHeader410 + openglVertexShaderBody;
      auto codeFS = openglFragmentShaderHeader410 + ((flags_ & NVG_ANTIALIAS)
                                                         ? openglAntiAliasingFragmentShaderBody
                                                         : openglNoAntiAliasingFragmentShaderBody);

#endif

      std::unique_ptr<igl::IShaderStages> shader_stages =
          igl::ShaderStagesCreator::fromModuleStringInput(
              *device_, codeVS.c_str(), "main", "", codeFS.c_str(), "main", "", nullptr);

      vertexFunction_ = shader_stages->getVertexModule();
      fragmentFunction_ = shader_stages->getFragmentModule();
    } else if (device_->getBackendType() == igl::BackendType::Vulkan) {
      auto codeVS = openglVertexShaderHeader460 + openglVertexShaderBody;
      auto codeFS = openglFragmentShaderHeader460 + ((flags_ & NVG_ANTIALIAS)
                                                         ? openglAntiAliasingFragmentShaderBody
                                                         : openglNoAntiAliasingFragmentShaderBody);

      std::unique_ptr<igl::IShaderStages> shader_stages =
          igl::ShaderStagesCreator::fromModuleStringInput(
              *device_, codeVS.c_str(), "main", "", codeFS.c_str(), "main", "", nullptr);

      vertexFunction_ = shader_stages->getVertexModule();
      fragmentFunction_ = shader_stages->getFragmentModule();
    }

    maxBuffers_ = 3;

    for (int i = maxBuffers_; i--;) {
      allBuffers_.emplace_back(std::make_shared<Buffers>(device_, maxUniformBufferSize_));
    }

    // Initializes vertex descriptor.
    vertexDescriptor_.numAttributes = 2;
    vertexDescriptor_.attributes[0].format = igl::VertexAttributeFormat::Float2;
    vertexDescriptor_.attributes[0].name = "pos";
    vertexDescriptor_.attributes[0].bufferIndex = 0;
    vertexDescriptor_.attributes[0].offset = offsetof(NVGvertex, x);
    vertexDescriptor_.attributes[0].location = 0;

    vertexDescriptor_.attributes[1].format = igl::VertexAttributeFormat::Float2;
    vertexDescriptor_.attributes[1].name = "tcoord";
    vertexDescriptor_.attributes[1].bufferIndex = 0;
    vertexDescriptor_.attributes[1].offset = offsetof(NVGvertex, u);
    vertexDescriptor_.attributes[1].location = 1;

    vertexDescriptor_.numInputBindings = 1;
    vertexDescriptor_.inputBindings[0].stride = sizeof(NVGvertex);
    vertexDescriptor_.inputBindings[0].sampleFunction = igl::VertexSampleFunction::PerVertex;

    // Initialzes textures.
    textureId_ = 0;

    // Initializes default sampler descriptor.
    igl::SamplerStateDesc samplerDescriptor;
    samplerDescriptor.debugName = "pseudoSampler";
    pseudoSampler_ = device_->createSamplerState(samplerDescriptor, &result);

    // Initializes pseudo texture for macOS.
    if (creates_pseudo_texture) {
      const int kPseudoTextureImage = renderCreateTextureWithType(NVG_TEXTURE_ALPHA, 1, 1, 0, NULL);
      std::shared_ptr<Texture> tex = findTexture(kPseudoTextureImage);
      pseudoTexture_ = tex->tex;
    }

    // Initializes default blend states.
    blendFunc_ = (Blend*)malloc(sizeof(Blend));
    blendFunc_->srcRGB = igl::BlendFactor::One;
    blendFunc_->dstRGB = igl::BlendFactor::OneMinusSrcAlpha;
    blendFunc_->srcAlpha = igl::BlendFactor::One;
    blendFunc_->dstAlpha = igl::BlendFactor::OneMinusSrcAlpha;

    // Initializes stencil states.
    igl::DepthStencilStateDesc stencilDescriptor;

    // Default stencil state.
    stencilDescriptor.debugName = "defaultStencilState";
    defaultStencilState_ = device_->createDepthStencilState(stencilDescriptor, &result);

    // Fill shape stencil.
    igl::StencilStateDesc frontFaceStencilDescriptor;
    frontFaceStencilDescriptor.stencilCompareFunction = igl::CompareFunction::AlwaysPass;
    frontFaceStencilDescriptor.depthStencilPassOperation = igl::StencilOperation::IncrementWrap;

    igl::StencilStateDesc backFaceStencilDescriptor;
    backFaceStencilDescriptor.stencilCompareFunction = igl::CompareFunction::AlwaysPass;
    backFaceStencilDescriptor.depthStencilPassOperation = igl::StencilOperation::DecrementWrap;

    stencilDescriptor.compareFunction = igl::CompareFunction::AlwaysPass;
    stencilDescriptor.backFaceStencil = backFaceStencilDescriptor;
    stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
    stencilDescriptor.debugName = "fillShapeStencilState";
    fillShapeStencilState_ = device_->createDepthStencilState(stencilDescriptor, &result);

    // Fill anti-aliased stencil.
    frontFaceStencilDescriptor.stencilCompareFunction = igl::CompareFunction::Equal;
    frontFaceStencilDescriptor.stencilFailureOperation = igl::StencilOperation::Keep;
    frontFaceStencilDescriptor.depthFailureOperation = igl::StencilOperation::Keep;
    frontFaceStencilDescriptor.depthStencilPassOperation = igl::StencilOperation::Zero;

    stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
    stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
    stencilDescriptor.debugName = "fillAntiAliasStencilState";
    fillAntiAliasStencilState_ = device_->createDepthStencilState(stencilDescriptor, &result);

    // Fill stencil.
    frontFaceStencilDescriptor.stencilCompareFunction = igl::CompareFunction::NotEqual;
    frontFaceStencilDescriptor.stencilFailureOperation = igl::StencilOperation::Zero;
    frontFaceStencilDescriptor.depthFailureOperation = igl::StencilOperation::Zero;
    frontFaceStencilDescriptor.depthStencilPassOperation = igl::StencilOperation::Zero;

    stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
    stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
    stencilDescriptor.debugName = "fillStencilState";
    fillStencilState_ = device_->createDepthStencilState(stencilDescriptor, &result);

    // Stroke shape stencil.
    frontFaceStencilDescriptor.stencilCompareFunction = igl::CompareFunction::Equal;
    frontFaceStencilDescriptor.stencilFailureOperation = igl::StencilOperation::Keep;
    frontFaceStencilDescriptor.depthFailureOperation = igl::StencilOperation::Keep;
    frontFaceStencilDescriptor.depthStencilPassOperation = igl::StencilOperation::IncrementClamp;

    stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
    stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
    stencilDescriptor.debugName = "strokeShapeStencilState";
    strokeShapeStencilState_ = device_->createDepthStencilState(stencilDescriptor, &result);

    // Stroke anti-aliased stencil.
    frontFaceStencilDescriptor.depthStencilPassOperation = igl::StencilOperation::Keep;

    stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
    stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
    stencilDescriptor.debugName = "strokeAntiAliasStencilState";
    strokeAntiAliasStencilState_ = device_->createDepthStencilState(stencilDescriptor, &result);

    // Stroke clear stencil.
    frontFaceStencilDescriptor.stencilCompareFunction = igl::CompareFunction::AlwaysPass;
    frontFaceStencilDescriptor.stencilFailureOperation = igl::StencilOperation::Zero;
    frontFaceStencilDescriptor.depthFailureOperation = igl::StencilOperation::Zero;
    frontFaceStencilDescriptor.depthStencilPassOperation = igl::StencilOperation::Zero;

    stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
    stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
    stencilDescriptor.debugName = "strokeClearStencilState";
    strokeClearStencilState_ = device_->createDepthStencilState(stencilDescriptor, &result);
    return 1;
  }

  int renderCreateTextureWithType(int type,
                                  int width,
                                  int height,
                                  int imageFlags,
                                  const unsigned char* data) {
    std::shared_ptr<Texture> tex = allocTexture();

    if (tex == nullptr)
      return 0;

    igl::TextureFormat pixelFormat = igl::TextureFormat::RGBA_UNorm8;
    if (type == NVG_TEXTURE_ALPHA) {
      pixelFormat = igl::TextureFormat::R_UNorm8;
    }

    tex->type = type;
    tex->flags = imageFlags;

    // todo:
    //(imageFlags & NVG_IMAGE_GENERATE_MIPMAPS ? true : false)

    igl::TextureDesc textureDescriptor = igl::TextureDesc::new2D(
        pixelFormat, width, height, igl::TextureDesc::TextureUsageBits::Sampled);

    tex->tex = device_->createTexture(textureDescriptor, NULL);

    if (data != NULL) {
      int bytesPerRow = 0;
      if (tex->type == NVG_TEXTURE_RGBA) {
        bytesPerRow = width * 4;
      } else {
        bytesPerRow = width;
      }

      tex->tex->upload(igl::TextureRangeDesc::new2D(0, 0, width, height), data);
    }

    igl::SamplerStateDesc samplerDescriptor;
    if (imageFlags & NVG_IMAGE_NEAREST) {
      samplerDescriptor.minFilter = igl::SamplerMinMagFilter::Nearest;
      samplerDescriptor.magFilter = igl::SamplerMinMagFilter::Nearest;
      if (imageFlags & NVG_IMAGE_GENERATE_MIPMAPS)
        samplerDescriptor.mipFilter = igl::SamplerMipFilter::Nearest;
    } else {
      samplerDescriptor.minFilter = igl::SamplerMinMagFilter::Linear;
      samplerDescriptor.magFilter = igl::SamplerMinMagFilter::Linear;
      if (imageFlags & NVG_IMAGE_GENERATE_MIPMAPS)
        samplerDescriptor.mipFilter = igl::SamplerMipFilter::Linear;
    }

    if (imageFlags & NVG_IMAGE_REPEATX) {
      samplerDescriptor.addressModeU = igl::SamplerAddressMode::Repeat;
    } else {
      samplerDescriptor.addressModeU = igl::SamplerAddressMode::Clamp;
    }

    if (imageFlags & NVG_IMAGE_REPEATY) {
      samplerDescriptor.addressModeV = igl::SamplerAddressMode::Repeat;
    } else {
      samplerDescriptor.addressModeV = igl::SamplerAddressMode::Clamp;
    }

    samplerDescriptor.debugName = "textureSampler";
    tex->sampler = device_->createSamplerState(samplerDescriptor, NULL);

    return tex->Id;
  }

  void renderDelete() {
    for (auto& buffers : allBuffers_) {
      buffers->commandBuffer = nullptr;
      buffers->vertexUniformBuffer = nullptr;
      buffers->stencilTexture = nullptr;
      buffers->indexBuffer = nullptr;
      buffers->vertBuffer = nullptr;
      buffers->uniformBufferPool = nullptr;
    }

    for (auto& texture : textures_) {
      texture->tex = nullptr;
      texture->sampler = nullptr;
    }

    free(blendFunc_);
    renderEncoder_ = nullptr;
    textures_.clear();
    allBuffers_.clear();
    defaultStencilState_ = nullptr;
    fillShapeStencilState_ = nullptr;
    fillAntiAliasStencilState_ = nullptr;
    strokeShapeStencilState_ = nullptr;
    strokeAntiAliasStencilState_ = nullptr;
    strokeClearStencilState_ = nullptr;
    pipelineState_ = nullptr;
    stencilOnlyPipelineState_ = nullptr;
    pseudoSampler_ = nullptr;
    pseudoTexture_ = nullptr;
    device_ = nullptr;
  }

  int renderDeleteTexture(int image) {
    for (auto& texture : textures_) {
      if (texture->Id == image) {
        if (texture->tex != nullptr && (texture->flags & NVG_IMAGE_NODELETE) == 0) {
          texture->tex = nullptr;
          texture->sampler = nullptr;
        }
        texture->Id = 0;
        texture->flags = 0;
        return 1;
      }
    }
    return 0;
  }

  void renderFillWithPaint(NVGpaint* paint,
                           NVGcompositeOperationState compositeOperation,
                           NVGscissor* scissor,
                           float fringe,
                           const float* bounds,
                           const NVGpath* paths,
                           int npaths) {
    Call* call = allocCall();
    NVGvertex* quad = nullptr;

    if (call == NULL)
      return;

    call->type = MNVG_FILL;
    call->triangleCount = 4;
    call->image = paint->image;
    call->blendFunc = blendCompositeOperation(compositeOperation);

    if (npaths == 1 && paths[0].convex) {
      call->type = MNVG_CONVEXFILL;
      call->triangleCount = 0; // Bounding box fill quad not needed for convex fill
    }

    // Allocate vertices for all the paths.
    int indexCount, strokeCount = 0;
    int maxverts = maxVertexCount(paths, npaths, &indexCount, &strokeCount) + call->triangleCount;
    int vertOffset = allocVerts(maxverts);
    if (vertOffset == -1) {
      // We get here if call alloc was ok, but something else is not.
      // Roll back the last call to prevent drawing it.
      if (curBuffers_->ncalls > 0)
        curBuffers_->ncalls--;
      return;
    }

    int indexOffset = allocIndexes(indexCount);
    if (indexOffset == -1) {
      // We get here if call alloc was ok, but something else is not.
      // Roll back the last call to prevent drawing it.
      if (curBuffers_->ncalls > 0)
        curBuffers_->ncalls--;
      return;
    }
    call->indexOffset = indexOffset;
    call->indexCount = indexCount;
    uint32_t* index = &curBuffers_->indexes[indexOffset];

    int strokeVertOffset = vertOffset + (maxverts - strokeCount);
    call->strokeOffset = strokeVertOffset + 1;
    call->strokeCount = strokeCount - 2;
    NVGvertex* strokeVert = curBuffers_->verts.data() + strokeVertOffset;

    NVGpath* path = (NVGpath*)&paths[0];
    for (int i = npaths; i--; ++path) {
      if (path->nfill > 2) {
        memcpy(&curBuffers_->verts[vertOffset], path->fill, sizeof(NVGvertex) * path->nfill);

        int hubVertOffset = vertOffset++;
        for (int j = 2; j < path->nfill; j++) {
          *index++ = hubVertOffset;
          *index++ = vertOffset++;
          *index++ = vertOffset;
        }
        vertOffset++;
      }
      if (path->nstroke > 0) {
        memcpy(strokeVert, path->stroke, sizeof(NVGvertex));
        ++strokeVert;
        memcpy(strokeVert, path->stroke, sizeof(NVGvertex) * path->nstroke);
        strokeVert += path->nstroke;
        memcpy(strokeVert, path->stroke + path->nstroke - 1, sizeof(NVGvertex));
        ++strokeVert;
      }
    }

    // Setup uniforms for draw calls
    if (call->type == MNVG_FILL) {
      // Quad
      call->triangleOffset = vertOffset;
      quad = &curBuffers_->verts[call->triangleOffset];
      setVertextData(&quad[0], bounds[2], bounds[3], 0.5f, 1.0f);
      setVertextData(&quad[1], bounds[2], bounds[1], 0.5f, 1.0f);
      setVertextData(&quad[2], bounds[0], bounds[3], 0.5f, 1.0f);
      setVertextData(&quad[3], bounds[0], bounds[1], 0.5f, 1.0f);
    }

    // Fill shader
    call->uboIndex = allocFragUniforms(fragmentUniformBufferSize_);
    convertPaintForFrag(
        (FragmentUniforms*)call->uboIndex.data, paint, scissor, fringe, fringe, -1.0f);
  }

  void renderFlush() {
    // Cancelled if the drawable is invisible.
    if (viewPortSize_.x == 0 || viewPortSize_.y == 0) {
      renderCancel();
      return;
    }

    curBuffers_->uploadToGpu();

    renderCommandEncoderWithColorTexture();

    Call* call = &curBuffers_->calls[0];
    for (int i = curBuffers_->ncalls; i--; ++call) {
      Blend* blend = &call->blendFunc;

      updateRenderPipelineStatesForBlend(blend);

      if (call->type == MNVG_FILL) {
        renderEncoder_->pushDebugGroupLabel("fill");
        fill(call);
      } else if (call->type == MNVG_CONVEXFILL) {
        renderEncoder_->pushDebugGroupLabel("convexFill");
        convexFill(call);
      } else if (call->type == MNVG_STROKE) {
        renderEncoder_->pushDebugGroupLabel("stroke");
        stroke(call);
      } else if (call->type == MNVG_TRIANGLES) {
        renderEncoder_->pushDebugGroupLabel("triangles");
        triangles(call);
      }

      renderEncoder_->popDebugGroupLabel();
    }

    curBuffers_->isBusy = false;
    curBuffers_->commandBuffer = nullptr;
    curBuffers_->image = 0;
    curBuffers_->nindexes = 0;
    curBuffers_->nverts = 0;
    curBuffers_->ncalls = 0;
    curBuffers_->uniformBufferPool->reset();
  }

  int renderGetTextureSizeForImage(int image, int* width, int* height) {
    std::shared_ptr<Texture> tex = findTexture(image);
    if (tex == nullptr)
      return 0;
    *width = (int)tex->tex->getSize().width;
    *height = (int)tex->tex->getSize().height;
    return 1;
  }

  void renderStrokeWithPaint(NVGpaint* paint,
                             NVGcompositeOperationState compositeOperation,
                             NVGscissor* scissor,
                             float fringe,
                             float strokeWidth,
                             const NVGpath* paths,
                             int npaths) {
    Call* call = allocCall();

    if (call == NULL)
      return;

    call->type = MNVG_STROKE;
    call->image = paint->image;
    call->blendFunc = blendCompositeOperation(compositeOperation);

    // Allocate vertices for all the paths.
    int strokeCount = 0;
    int maxverts = maxVertexCount(paths, npaths, NULL, &strokeCount);
    int offset = allocVerts(maxverts);
    if (offset == -1) {
      // We get here if call alloc was ok, but something else is not.
      // Roll back the last call to prevent drawing it.
      if (curBuffers_->ncalls > 0)
        curBuffers_->ncalls--;
      return;
    }

    call->strokeOffset = offset + 1;
    call->strokeCount = strokeCount - 2;
    NVGvertex* strokeVert = curBuffers_->verts.data() + offset;

    NVGpath* path = (NVGpath*)&paths[0];
    for (int i = npaths; i--; ++path) {
      if (path->nstroke > 0) {
        memcpy(strokeVert, path->stroke, sizeof(NVGvertex));
        ++strokeVert;
        memcpy(strokeVert, path->stroke, sizeof(NVGvertex) * path->nstroke);
        strokeVert += path->nstroke;
        memcpy(strokeVert, path->stroke + path->nstroke - 1, sizeof(NVGvertex));
        ++strokeVert;
      }
    }

    if (flags_ & NVG_STENCIL_STROKES) {
      // Fill shader
      call->uboIndex = allocFragUniforms(fragmentUniformBufferSize_);
      convertPaintForFrag(
          (FragmentUniforms*)call->uboIndex.data, paint, scissor, strokeWidth, fringe, -1.0f);
      call->uboIndex2 = allocFragUniforms(fragmentUniformBufferSize_);
      convertPaintForFrag((FragmentUniforms*)call->uboIndex2.data,
                          paint,
                          scissor,
                          strokeWidth,
                          fringe,
                          (1.0f - 0.5f / 255.0f));
    } else {
      // Fill shader
      call->uboIndex = allocFragUniforms(fragmentUniformBufferSize_);
      convertPaintForFrag(
          (FragmentUniforms*)call->uboIndex.data, paint, scissor, strokeWidth, fringe, -1.0f);
    }
  }

  void renderTrianglesWithPaint(NVGpaint* paint,
                                NVGcompositeOperationState compositeOperation,
                                NVGscissor* scissor,
                                const NVGvertex* verts,
                                int nverts,
                                float fringe) {
    Call* call = allocCall();

    if (call == NULL)
      return;

    call->type = MNVG_TRIANGLES;
    call->image = paint->image;
    call->blendFunc = blendCompositeOperation(compositeOperation);

    // Allocate vertices for all the paths.
    call->triangleOffset = allocVerts(nverts);
    if (call->triangleOffset == -1) {
      // We get here if call alloc was ok, but something else is not.
      // Roll back the last call to prevent drawing it.
      if (curBuffers_->ncalls > 0)
        curBuffers_->ncalls--;
      return;
    }
    call->triangleCount = nverts;

    memcpy(&curBuffers_->verts[call->triangleOffset], verts, sizeof(NVGvertex) * nverts);

    // Fill shader
    call->uboIndex = allocFragUniforms(fragmentUniformBufferSize_);
    convertPaintForFrag(
        (FragmentUniforms*)call->uboIndex.data, paint, scissor, 1.0f, fringe, -1.0f);
    ((FragmentUniforms*)call->uboIndex.data)->type = MNVG_SHADER_IMG;
  }

  int renderUpdateTextureWithImage(int image,
                                   int x,
                                   int y,
                                   int width,
                                   int height,
                                   const unsigned char* data) {
    std::shared_ptr<Texture> tex = findTexture(image);

    if (tex == nullptr)
      return 0;

    unsigned char* bytes = NULL;
    int bytesPerRow = 0;
    if (tex->type == NVG_TEXTURE_RGBA) {
      bytesPerRow = tex->tex->getSize().width * 4;
      bytes = (unsigned char*)data + y * bytesPerRow + x * 4;
    } else {
      bytesPerRow = tex->tex->getSize().width;
      bytes = (unsigned char*)data + y * bytesPerRow + x;
    }

    std::shared_ptr<igl::ITexture> texture = tex->tex;
    igl::TextureRangeDesc desc = igl::TextureRangeDesc::new2D(x, y, width, height);
    texture->upload(desc, bytes, bytesPerRow);

    return 1;
  }

  int bufferIndex = 0;

  void renderViewportWithWidth(float width, float height, float device_PixelRatio) {
    viewPortSize_.x = (uint32_t)(width * device_PixelRatio);
    viewPortSize_.y = (uint32_t)(height * device_PixelRatio);

    bufferIndex = (bufferIndex + 1) % 3;
    curBuffers_ = allBuffers_[bufferIndex];

    curBuffers_->vertexUniforms.viewSize[0] = width;
    curBuffers_->vertexUniforms.viewSize[1] = height;

    // Initializes view size buffer for vertex function.
    if (curBuffers_->vertexUniformBuffer == nullptr) {
      igl::BufferDesc desc(igl::BufferDesc::BufferTypeBits::Uniform,
                           &curBuffers_->vertexUniforms,
                           sizeof(VertexUniforms),
                           igl::ResourceStorage::Shared);
      desc.hint = igl::BufferDesc::BufferAPIHintBits::UniformBlock;
      desc.debugName = "vertex_uniform_buffer";
      curBuffers_->vertexUniformBuffer = device_->createBuffer(desc, NULL);
    }
  }

  void setUniforms(const UniformBufferIndex& uboIndex, int image) {
    renderEncoder_->bindBuffer(
        kFragmentUniformBlockIndex, uboIndex.buffer, uboIndex.offset, fragmentUniformBufferSize_);

    std::shared_ptr<Texture> tex = (image == 0 ? nullptr : findTexture(image));
    if (tex != nullptr) {
      renderEncoder_->bindTexture(0, igl::BindTarget::kFragment, tex->tex.get());
      renderEncoder_->bindSamplerState(0, igl::BindTarget::kFragment, tex->sampler.get());
    } else {
      renderEncoder_->bindTexture(0, igl::BindTarget::kFragment, pseudoTexture_.get());
      renderEncoder_->bindSamplerState(0, igl::BindTarget::kFragment, pseudoSampler_.get());
    }
  }

  void stroke(Call* call) {
    if (call->strokeCount <= 0) {
      return;
    }

    if (flags_ & NVG_STENCIL_STROKES) {
      // Fills the stroke base without overlap.
      bindRenderPipeline(pipelineStateTriangleStrip_);
      setUniforms(call->uboIndex2, call->image);
      renderEncoder_->bindDepthStencilState(strokeShapeStencilState_);

      renderEncoder_->draw(call->strokeCount, 1, call->strokeOffset);

      // Draws anti-aliased fragments.
      setUniforms(call->uboIndex, call->image);
      renderEncoder_->bindDepthStencilState(strokeAntiAliasStencilState_);
      renderEncoder_->draw(call->strokeCount, 1, call->strokeOffset);

      // Clears stencil buffer.
      bindRenderPipeline(stencilOnlyPipelineStateTriangleStrip_);
      renderEncoder_->bindDepthStencilState(strokeClearStencilState_);
      renderEncoder_->draw(call->strokeCount, 1, call->strokeOffset);
      renderEncoder_->bindDepthStencilState(defaultStencilState_);
    } else {
      // Draws strokes.
      bindRenderPipeline(pipelineStateTriangleStrip_);
      setUniforms(call->uboIndex, call->image);
      renderEncoder_->draw(call->strokeCount, 1, call->strokeOffset);
    }
  }

  void triangles(Call* call) {
    bindRenderPipeline(pipelineState_);
    setUniforms(call->uboIndex, call->image);
    renderEncoder_->draw(call->triangleCount, 1, call->triangleOffset);
  }

  void updateRenderPipelineStatesForBlend(Blend* blend) {
    if (pipelineState_ != nullptr && stencilOnlyPipelineState_ != nullptr &&
        piplelinePixelFormat_ == framebuffer_->getColorAttachment(0)->getProperties().format &&
        blendFunc_->srcRGB == blend->srcRGB && blendFunc_->dstRGB == blend->dstRGB &&
        blendFunc_->srcAlpha == blend->srcAlpha && blendFunc_->dstAlpha == blend->dstAlpha) {
      return;
    }

    igl::Result result;

    igl::RenderPipelineDesc pipelineStateDescriptor;

    pipelineStateDescriptor.fragmentUnitSamplerMap[0] = IGL_NAMEHANDLE("textureUnit");
    pipelineStateDescriptor.uniformBlockBindingMap[kVertexUniformBlockIndex] = {
        std::make_pair(IGL_NAMEHANDLE("VertexUniformBlock"), igl::NameHandle{})};
    pipelineStateDescriptor.uniformBlockBindingMap[kFragmentUniformBlockIndex] = {
        std::make_pair(IGL_NAMEHANDLE("FragmentUniformBlock"), igl::NameHandle{})};

    pipelineStateDescriptor.targetDesc.colorAttachments.resize(1);
    igl::RenderPipelineDesc::TargetDesc::ColorAttachment& colorAttachmentDescriptor =
        pipelineStateDescriptor.targetDesc.colorAttachments[0];
    colorAttachmentDescriptor.textureFormat =
        framebuffer_->getColorAttachment(0)->getProperties().format;
    pipelineStateDescriptor.targetDesc.stencilAttachmentFormat =
        framebuffer_->getStencilAttachment()->getProperties().format;
    pipelineStateDescriptor.targetDesc.depthAttachmentFormat =
        framebuffer_->getDepthAttachment()->getProperties().format;
    pipelineStateDescriptor.shaderStages = igl::ShaderStagesCreator::fromRenderModules(
        *device_, vertexFunction_, fragmentFunction_, &result);
    IGL_DEBUG_ASSERT(result.isOk());

    pipelineStateDescriptor.vertexInputState =
        device_->createVertexInputState(vertexDescriptor_, &result);
    IGL_DEBUG_ASSERT(result.isOk());

    // Sets blending states.
    colorAttachmentDescriptor.blendEnabled = true;
    colorAttachmentDescriptor.srcRGBBlendFactor = blend->srcRGB;
    colorAttachmentDescriptor.srcAlphaBlendFactor = blend->srcAlpha;
    colorAttachmentDescriptor.dstRGBBlendFactor = blend->dstRGB;
    colorAttachmentDescriptor.dstAlphaBlendFactor = blend->dstAlpha;
    blendFunc_->srcRGB = blend->srcRGB;
    blendFunc_->dstRGB = blend->dstRGB;
    blendFunc_->srcAlpha = blend->srcAlpha;
    blendFunc_->dstAlpha = blend->dstAlpha;

    pipelineStateDescriptor.topology = igl::PrimitiveType::Triangle;
    pipelineStateDescriptor.cullMode = igl::CullMode::Disabled;
    pipelineStateDescriptor.debugName = igl::genNameHandle("Triangle_CullNone");
    pipelineState_ = device_->createRenderPipeline(pipelineStateDescriptor, &result);

    pipelineStateDescriptor.topology = igl::PrimitiveType::TriangleStrip;
    pipelineStateDescriptor.cullMode = igl::CullMode::Back;
    pipelineStateDescriptor.debugName = igl::genNameHandle("TriangleStripe_CullBack");
    pipelineStateTriangleStrip_ = device_->createRenderPipeline(pipelineStateDescriptor, &result);
    IGL_DEBUG_ASSERT(result.isOk());

    auto fragmentFunction =
        device_->getBackendType() == igl::BackendType::Metal ? nullptr : fragmentFunction_;
    pipelineStateDescriptor.shaderStages = igl::ShaderStagesCreator::fromRenderModules(
        *device_, vertexFunction_, fragmentFunction, &result);

    IGL_DEBUG_ASSERT(result.isOk());
    colorAttachmentDescriptor.colorWriteMask = igl::ColorWriteBits::ColorWriteBitsDisabled;
    pipelineStateDescriptor.cullMode = igl::CullMode::Disabled;
    pipelineStateDescriptor.topology = igl::PrimitiveType::Triangle;
    pipelineStateDescriptor.debugName = igl::genNameHandle("stencilOnlyPipelineState");
    stencilOnlyPipelineState_ = device_->createRenderPipeline(pipelineStateDescriptor, &result);
    IGL_DEBUG_ASSERT(result.isOk());

    pipelineStateDescriptor.debugName = igl::genNameHandle("stencilOnlyPipelineStateTriangleStrip");
    pipelineStateDescriptor.topology = igl::PrimitiveType::TriangleStrip;
    stencilOnlyPipelineStateTriangleStrip_ =
        device_->createRenderPipeline(pipelineStateDescriptor, &result);
    IGL_DEBUG_ASSERT(result.isOk());

    piplelinePixelFormat_ = framebuffer_->getColorAttachment(0)->getProperties().format;
  }
};

static void callback__renderCancel(void* uptr) {
  Context* mtl = (Context*)uptr;
  mtl->renderCancel();
}

static int callback__renderCreateTexture(void* uptr,
                                         int type,
                                         int width,
                                         int height,
                                         int imageFlags,
                                         const unsigned char* data) {
  Context* mtl = (Context*)uptr;
  return mtl->renderCreateTextureWithType(type, width, height, imageFlags, data);
}

static int callback__renderCreate(void* uptr) {
  Context* mtl = (Context*)uptr;
  return mtl->renderCreate();
}

static void callback__renderDelete(void* uptr) {
  Context* mtl = (Context*)uptr;
  mtl->renderDelete();
}

static int callback__renderDeleteTexture(void* uptr, int image) {
  Context* mtl = (Context*)uptr;
  return mtl->renderDeleteTexture(image);
}

static void callback__renderFill(void* uptr,
                                 NVGpaint* paint,
                                 NVGcompositeOperationState compositeOperation,
                                 NVGscissor* scissor,
                                 float fringe,
                                 const float* bounds,
                                 const NVGpath* paths,
                                 int npaths) {
  Context* mtl = (Context*)uptr;
  mtl->renderFillWithPaint(paint, compositeOperation, scissor, fringe, bounds, paths, npaths);
}

static void callback__renderFlush(void* uptr) {
  Context* mtl = (Context*)uptr;
  mtl->renderFlush();
}

static int callback__renderGetTextureSize(void* uptr, int image, int* w, int* h) {
  Context* mtl = (Context*)uptr;
  return mtl->renderGetTextureSizeForImage(image, w, h);
}

static void callback__renderStroke(void* uptr,
                                   NVGpaint* paint,
                                   NVGcompositeOperationState compositeOperation,
                                   NVGscissor* scissor,
                                   float fringe,
                                   float strokeWidth,
                                   const NVGpath* paths,
                                   int npaths) {
  Context* mtl = (Context*)uptr;
  mtl->renderStrokeWithPaint(
      paint, compositeOperation, scissor, fringe, strokeWidth, paths, npaths);
}

static void callback__renderTriangles(void* uptr,
                                      NVGpaint* paint,
                                      NVGcompositeOperationState compositeOperation,
                                      NVGscissor* scissor,
                                      const NVGvertex* verts,
                                      int nverts,
                                      float fringe) {
  Context* mtl = (Context*)uptr;
  mtl->renderTrianglesWithPaint(paint, compositeOperation, scissor, verts, nverts, fringe);
}

static int callback__renderUpdateTexture(void* uptr,
                                         int image,
                                         int x,
                                         int y,
                                         int w,
                                         int h,
                                         const unsigned char* data) {
  Context* mtl = (Context*)uptr;
  return mtl->renderUpdateTextureWithImage(image, x, y, w, h, data);
}

static void callback__renderViewport(void* uptr,
                                     float width,
                                     float height,
                                     float device_PixelRatio) {
  Context* mtl = (Context*)uptr;
  mtl->renderViewportWithWidth(width, height, device_PixelRatio);
}

void SetRenderCommandEncoder(NVGcontext* ctx,
                             igl::IFramebuffer* framebuffer,
                             igl::IRenderCommandEncoder* command,
                             float* matrix) {
  Context* mtl = (Context*)nvgInternalParams(ctx)->userPtr;
  mtl->framebuffer_ = framebuffer;
  mtl->renderEncoder_ = command;
  if (matrix) {
    memcpy(&mtl->curBuffers_->vertexUniforms.matrix, matrix, sizeof(float) * 16);
  }
}

NVGcontext* CreateContext(igl::IDevice* device, int flags) {
  NVGparams params;
  NVGcontext* ctx = NULL;
  Context* mtl = new Context();

  memset(&params, 0, sizeof(params));
  params.renderCreate = callback__renderCreate;
  params.renderCreateTexture = callback__renderCreateTexture;
  params.renderDeleteTexture = callback__renderDeleteTexture;
  params.renderUpdateTexture = callback__renderUpdateTexture;
  params.renderGetTextureSize = callback__renderGetTextureSize;
  params.renderViewport = callback__renderViewport;
  params.renderCancel = callback__renderCancel;
  params.renderFlush = callback__renderFlush;
  params.renderFill = callback__renderFill;
  params.renderStroke = callback__renderStroke;
  params.renderTriangles = callback__renderTriangles;
  params.renderDelete = callback__renderDelete;
  params.userPtr = (void*)mtl;
  params.edgeAntiAlias = flags & NVG_ANTIALIAS ? 1 : 0;

  mtl->flags_ = flags;

  device->getFeatureLimits(igl::DeviceFeatureLimits::MaxUniformBufferBytes,
                           mtl->maxUniformBufferSize_);

  mtl->maxUniformBufferSize_ = std::min(mtl->maxUniformBufferSize_, (size_t)512 * 1024);

  size_t uniformBufferAlignment = 16;
  device->getFeatureLimits(igl::DeviceFeatureLimits::BufferAlignment, uniformBufferAlignment);
  // sizeof(MNVGfragUniforms)= 176
  // 64 * 3 > 176
  mtl->fragmentUniformBufferSize_ = std::max(64 * 3, (int)uniformBufferAlignment);

  mtl->indexSize_ = 4; // IndexType::UInt32
  mtl->device_ = device;

  ctx = nvgCreateInternal(&params);
  if (ctx == NULL)
    goto error;
  return ctx;

error:
  if (ctx != NULL)
    nvgDeleteInternal(ctx);
  return NULL;
}

void DestroyContext(NVGcontext* ctx) {
  if (!ctx)
    return;
  nvgDeleteInternal(ctx);

  if (auto param = nvgInternalParams(ctx)) {
    if (param->userPtr) {
      delete (Context*)param->userPtr;
    }
  }
}

} // namespace iglu::nanovg
