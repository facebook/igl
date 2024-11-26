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

#define kVertexUniformBlockIndex 1
#define kFragmentUniformBlockIndex 2

namespace iglu::nanovg {

struct igl_vector_uint2 {
  uint32_t x;
  uint32_t y;
};

typedef enum MNVGvertexInputIndex {
  MNVG_VERTEX_INPUT_INDEX_VERTICES = 0,
  MNVG_VERTEX_INPUT_INDEX_VIEW_SIZE = 1,
} MNVGvertexInputIndex;

typedef enum MNVGshaderType {
  MNVG_SHADER_FILLGRAD,
  MNVG_SHADER_FILLIMG,
  MNVG_SHADER_IMG,
} MNVGshaderType;

enum MNVGcallType {
  MNVG_NONE = 0,
  MNVG_FILL,
  MNVG_CONVEXFILL,
  MNVG_STROKE,
  MNVG_TRIANGLES,
};

struct MNVGblend {
  igl::BlendFactor srcRGB;
  igl::BlendFactor dstRGB;
  igl::BlendFactor srcAlpha;
  igl::BlendFactor dstAlpha;
};

struct MNVGcall {
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
  int uniformOffset;
  MNVGblend blendFunc;
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
  MNVGshaderType type;
};

struct MNVGtexture {
  int _id;
  int type;
  int flags;
  std::shared_ptr<igl::ITexture> tex;
  std::shared_ptr<igl::ISamplerState> sampler;
};

struct MNVGbuffers {
  std::shared_ptr<igl::ICommandBuffer> commandBuffer;
  bool isBusy = false;
  int image = 0;
  std::shared_ptr<igl::IBuffer> vertexUniformBuffer;
  VertexUniforms vertexUniforms;
  std::shared_ptr<igl::ITexture> stencilTexture;
  std::vector<MNVGcall> calls;
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
  std::shared_ptr<igl::IBuffer> fragmentUniformBuffer;
  std::vector<unsigned char> fragmentUniforms;
  int cuniforms = 0;
  int nuniforms = 0;

  MNVGbuffers() {
    vertexUniforms.matrix = iglu::simdtypes::float4x4(1.0f);
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

    if (fragmentUniformBuffer) {
      fragmentUniformBuffer->upload(fragmentUniforms.data(),
                                    igl::BufferRange(fragmentUniforms.size()));
    }
  }
};

const igl::TextureFormat kStencilFormat = igl::TextureFormat::S8_UInt_Z32_UNorm;

static bool mtlnvg_convertBlendFuncFactor(int factor, igl::BlendFactor* result) {
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

static int igl_nanovg__maxi(int a, int b) {
  return a > b ? a : b;
}

static int igl_nanovg__maxVertCount(const NVGpath* paths,
                                int npaths,
                                int* indexCount,
                                int* strokeCount) {
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

static iglu::simdtypes::float4 igl_nanovg__premulColor(NVGcolor c) {
  c.r *= c.a;
  c.g *= c.a;
  c.b *= c.a;
  iglu::simdtypes::float4 ret;
  memcpy(&ret, &c, sizeof(float) * 4);
  return ret;
}

static void igl_nanovg__xformToMat3x3(iglu::simdtypes::float3x3* m3, float* t) {
  float columns_0[3] = {t[0], t[1], 0.0f};
  float columns_1[3] = {t[2], t[3], 0.0f};
  float columns_2[3] = {t[4], t[5], 1.0f};

  memcpy(&m3->columns[0], columns_0, 3 * sizeof(float));
  memcpy(&m3->columns[1], columns_1, 3 * sizeof(float));
  memcpy(&m3->columns[2], columns_2, 3 * sizeof(float));
}

static void igl_nanovg__vset(NVGvertex* vtx, float x, float y, float u, float v) {
  vtx->x = x;
  vtx->y = y;
  vtx->u = u;
  vtx->v = v;
}

class MNVGcontext {
 public:
  igl::IDevice* device_;
  igl::IRenderCommandEncoder* renderEncoder_ = nullptr;

  int fragmentUniformBufferSize_;
  int indexSize_;
  int flags_;
  igl_vector_uint2 viewPortSize_;

  igl::IFramebuffer* framebuffer_ = nullptr;

  // Textures
  std::vector<MNVGtexture*> textures_;
  int textureId_;

  // Per frame buffers
  MNVGbuffers* curBuffers_;
  std::vector<MNVGbuffers*> allBuffers_;
  int maxBuffers_;

  // Cached states.
  MNVGblend* _blendFunc;
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

  MNVGcall* allocCall() {
    MNVGcall* ret = NULL;
    if (curBuffers_->ncalls + 1 > curBuffers_->ccalls) {
      int ccalls = igl_nanovg__maxi(curBuffers_->ncalls + 1, 128) + curBuffers_->ccalls / 2;
      curBuffers_->calls.resize(ccalls);
      curBuffers_->ccalls = ccalls;
    }
    ret = &curBuffers_->calls[curBuffers_->ncalls++];
    memset(ret, 0, sizeof(MNVGcall));
    return ret;
  }

  int allocFragUniforms(int n) {
    int ret = 0;
    if (curBuffers_->nuniforms + n > curBuffers_->cuniforms) {
      int cuniforms = igl_nanovg__maxi(curBuffers_->nuniforms + n, 128) + curBuffers_->cuniforms / 4;
      curBuffers_->fragmentUniforms.resize(fragmentUniformBufferSize_ * cuniforms);

      if (device_->getBackendType() == igl::BackendType::Vulkan) {
        IGL_DEBUG_ASSERT((fragmentUniformBufferSize_ * cuniforms) < 65536); // vulkan max 65536;
      }

      igl::BufferDesc desc(igl::BufferDesc::BufferTypeBits::Uniform,
                           curBuffers_->fragmentUniforms.data(),
                           fragmentUniformBufferSize_ * cuniforms);
      desc.hint = igl::BufferDesc::BufferAPIHintBits::UniformBlock;
      desc.debugName = "fragment_uniform_buffer";
      std::shared_ptr<igl::IBuffer> buffer = device_->createBuffer(desc, NULL);

      curBuffers_->fragmentUniformBuffer = buffer;
      curBuffers_->cuniforms = cuniforms;
    }
    ret = curBuffers_->nuniforms * fragmentUniformBufferSize_;
    curBuffers_->nuniforms += n;
    return ret;
  }

  int allocIndexes(int n) {
    int ret = 0;
    if (curBuffers_->nindexes + n > curBuffers_->cindexes) {
      int cindexes = igl_nanovg__maxi(curBuffers_->nindexes + n, 4096) + curBuffers_->cindexes / 2;
      curBuffers_->indexes.resize(cindexes);

      igl::BufferDesc desc(igl::BufferDesc::BufferTypeBits::Index,
                           curBuffers_->indexes.data(),
                           indexSize_ * cindexes);
      desc.debugName = "index_buffer";
      std::shared_ptr<igl::IBuffer> buffer = device_->createBuffer(desc, NULL);

      curBuffers_->indexBuffer = buffer;
      curBuffers_->cindexes = cindexes;
    }
    ret = curBuffers_->nindexes;
    curBuffers_->nindexes += n;
    return ret;
  }

  MNVGtexture* allocTexture() {
    MNVGtexture* tex = nullptr;

    for (MNVGtexture* texture : textures_) {
      if (texture->_id == 0) {
        tex = texture;
        break;
      }
    }
    if (tex == nullptr) {
      tex = new MNVGtexture();
      textures_.emplace_back(tex);
    }
    tex->_id = ++textureId_;
    return tex;
  }

  int allocVerts(int n) {
    int ret = 0;
    if (curBuffers_->nverts + n > curBuffers_->cverts) {
      int cverts = igl_nanovg__maxi(curBuffers_->nverts + n, 4096) + curBuffers_->cverts / 2;
      curBuffers_->verts.resize(cverts);

      igl::BufferDesc desc(igl::BufferDesc::BufferTypeBits::Vertex,
                           curBuffers_->verts.data(),
                           sizeof(NVGvertex) * cverts);
      desc.debugName = "vertex_buffer";
      std::shared_ptr<igl::IBuffer> buffer = device_->createBuffer(desc, NULL);

      curBuffers_->vertBuffer = buffer;
      curBuffers_->cverts = cverts;
    }
    ret = curBuffers_->nverts;
    curBuffers_->nverts += n;
    return ret;
  }

  MNVGblend blendCompositeOperation(NVGcompositeOperationState op) {
    MNVGblend blend;
    if (!mtlnvg_convertBlendFuncFactor(op.srcRGB, &blend.srcRGB) ||
        !mtlnvg_convertBlendFuncFactor(op.dstRGB, &blend.dstRGB) ||
        !mtlnvg_convertBlendFuncFactor(op.srcAlpha, &blend.srcAlpha) ||
        !mtlnvg_convertBlendFuncFactor(op.dstAlpha, &blend.dstAlpha)) {
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
    MNVGtexture* tex = nullptr;
    float invxform[6];

    memset(frag, 0, sizeof(*frag));

    frag->innerCol = igl_nanovg__premulColor(paint->innerColor);
    frag->outerCol = igl_nanovg__premulColor(paint->outerColor);

    if (scissor->extent[0] < -0.5f || scissor->extent[1] < -0.5f) {
      frag->scissorMat = iglu::simdtypes::float3x3(0);
      frag->scissorExt[0] = 1.0f;
      frag->scissorExt[1] = 1.0f;
      frag->scissorScale[0] = 1.0f;
      frag->scissorScale[1] = 1.0f;
    } else {
      nvgTransformInverse(invxform, scissor->xform);
      igl_nanovg__xformToMat3x3(&frag->scissorMat, invxform);
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

    igl_nanovg__xformToMat3x3(&frag->paintMat, invxform);

    return 1;
  }

  void bindRenderPipeline(const std::shared_ptr<igl::IRenderPipelineState>& pipelineState) {
    renderEncoder_->bindRenderPipelineState(pipelineState);
    renderEncoder_->bindVertexBuffer(MNVG_VERTEX_INPUT_INDEX_VERTICES, *curBuffers_->vertBuffer, 0);
    renderEncoder_->bindBuffer(kVertexUniformBlockIndex, curBuffers_->vertexUniformBuffer.get(), 0);
    renderEncoder_->bindBuffer(
        kFragmentUniformBlockIndex, curBuffers_->fragmentUniformBuffer.get(), 0);
  }

  void convexFill(MNVGcall* call) {
    const int kIndexBufferOffset = call->indexOffset * indexSize_;
    bindRenderPipeline(pipelineState_);
    setUniforms(call->uniformOffset, call->image);
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

  void fill(MNVGcall* call) {
    // Draws shapes.
    const int kIndexBufferOffset = call->indexOffset * indexSize_;
    bindRenderPipeline(stencilOnlyPipelineState_);
    renderEncoder_->bindDepthStencilState(fillShapeStencilState_);
    if (call->indexCount > 0) {
      renderEncoder_->bindIndexBuffer(
          *curBuffers_->indexBuffer, igl::IndexFormat::UInt32, kIndexBufferOffset);
      renderEncoder_->drawIndexed(call->indexCount);
    }

    // Restores states.
    bindRenderPipeline(pipelineStateTriangleStrip_);

    // Draws anti-aliased fragments.
    setUniforms(call->uniformOffset, call->image);
    if (flags_ & NVG_ANTIALIAS && call->strokeCount > 0) {
      renderEncoder_->bindDepthStencilState(fillAntiAliasStencilState_);
      renderEncoder_->draw(call->strokeCount, 1, call->strokeOffset);
    }

    // Draws fill.
    renderEncoder_->bindDepthStencilState(fillStencilState_);
    renderEncoder_->draw(call->triangleCount, 1, call->triangleOffset);
    renderEncoder_->bindDepthStencilState(defaultStencilState_);
  }

  MNVGtexture* findTexture(int _id) {
    for (MNVGtexture* texture : textures_) {
      if (texture->_id == _id)
        return texture;
    }
    return nullptr;
  }

  FragmentUniforms* fragUniformAtIndex(int index) {
    return (FragmentUniforms*)&curBuffers_->fragmentUniforms[index];
  }

  void renderCancel() {
    curBuffers_->image = 0;
    curBuffers_->isBusy = false;
    curBuffers_->nindexes = 0;
    curBuffers_->nverts = 0;
    curBuffers_->ncalls = 0;
    curBuffers_->nuniforms = 0;
  }

  void renderCommandEncoderWithColorTexture() {
    renderEncoder_->setStencilReferenceValue(0);
    renderEncoder_->bindViewport(
        {0.0, 0.0, (float)viewPortSize_.x, (float)viewPortSize_.y, 0.0, 1.0});
    renderEncoder_->bindVertexBuffer(MNVG_VERTEX_INPUT_INDEX_VERTICES, *curBuffers_->vertBuffer, 0);
    renderEncoder_->bindBuffer(kVertexUniformBlockIndex, curBuffers_->vertexUniformBuffer.get(), 0);
    renderEncoder_->bindBuffer(
        kFragmentUniformBlockIndex, curBuffers_->fragmentUniformBuffer.get(), 0);
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
#if IGL_PLATFORM_ANDROID || IGL_PLATFORM_IOS
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
      allBuffers_.emplace_back(new MNVGbuffers());
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
      MNVGtexture* tex = findTexture(kPseudoTextureImage);
      pseudoTexture_ = tex->tex;
    }

    // Initializes default blend states.
    _blendFunc = (MNVGblend*)malloc(sizeof(MNVGblend));
    _blendFunc->srcRGB = igl::BlendFactor::One;
    _blendFunc->dstRGB = igl::BlendFactor::OneMinusSrcAlpha;
    _blendFunc->srcAlpha = igl::BlendFactor::One;
    _blendFunc->dstAlpha = igl::BlendFactor::OneMinusSrcAlpha;

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
    MNVGtexture* tex = allocTexture();

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

    return tex->_id;
  }

  void renderDelete() {
    for (MNVGbuffers* buffers : allBuffers_) {
      buffers->commandBuffer = nullptr;
      buffers->vertexUniformBuffer = nullptr;
      buffers->stencilTexture = nullptr;
      buffers->indexBuffer = nullptr;
      buffers->vertBuffer = nullptr;
      buffers->fragmentUniformBuffer = nullptr;
    }

    for (MNVGtexture* texture : textures_) {
      texture->tex = nullptr;
      texture->sampler = nullptr;
    }

    free(_blendFunc);
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
    for (MNVGtexture* texture : textures_) {
      if (texture->_id == image) {
        if (texture->tex != nullptr && (texture->flags & NVG_IMAGE_NODELETE) == 0) {
          texture->tex = nullptr;
          texture->sampler = nullptr;
        }
        texture->_id = 0;
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
    MNVGcall* call = allocCall();
    NVGvertex* quad;

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
    int maxverts =
        igl_nanovg__maxVertCount(paths, npaths, &indexCount, &strokeCount) + call->triangleCount;
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
      igl_nanovg__vset(&quad[0], bounds[2], bounds[3], 0.5f, 1.0f);
      igl_nanovg__vset(&quad[1], bounds[2], bounds[1], 0.5f, 1.0f);
      igl_nanovg__vset(&quad[2], bounds[0], bounds[3], 0.5f, 1.0f);
      igl_nanovg__vset(&quad[3], bounds[0], bounds[1], 0.5f, 1.0f);
    }

    // Fill shader
    call->uniformOffset = allocFragUniforms(1);
    if (call->uniformOffset == -1) {
      // We get here if call alloc was ok, but something else is not.
      // Roll back the last call to prevent drawing it.
      if (curBuffers_->ncalls > 0)
        curBuffers_->ncalls--;
      return;
    }
    convertPaintForFrag(
        fragUniformAtIndex(call->uniformOffset), paint, scissor, fringe, fringe, -1.0f);
  }

  void renderFlush() {
    // Cancelled if the drawable is invisible.
    if (viewPortSize_.x == 0 || viewPortSize_.y == 0) {
      renderCancel();
      return;
    }

    curBuffers_->uploadToGpu();

    renderCommandEncoderWithColorTexture();

    MNVGcall* call = &curBuffers_->calls[0];
    for (int i = curBuffers_->ncalls; i--; ++call) {
      MNVGblend* blend = &call->blendFunc;

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
    curBuffers_->nuniforms = 0;
  }

  int renderGetTextureSizeForImage(int image, int* width, int* height) {
    MNVGtexture* tex = findTexture(image);
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
    MNVGcall* call = allocCall();

    if (call == NULL)
      return;

    call->type = MNVG_STROKE;
    call->image = paint->image;
    call->blendFunc = blendCompositeOperation(compositeOperation);

    // Allocate vertices for all the paths.
    int strokeCount = 0;
    int maxverts = igl_nanovg__maxVertCount(paths, npaths, NULL, &strokeCount);
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
      call->uniformOffset = allocFragUniforms(2);
      if (call->uniformOffset == -1) {
        // We get here if call alloc was ok, but something else is not.
        // Roll back the last call to prevent drawing it.
        if (curBuffers_->ncalls > 0)
          curBuffers_->ncalls--;
        return;
      }
      convertPaintForFrag(
          fragUniformAtIndex(call->uniformOffset), paint, scissor, strokeWidth, fringe, -1.0f);
      FragmentUniforms* frag = fragUniformAtIndex(call->uniformOffset + fragmentUniformBufferSize_);
      convertPaintForFrag(frag, paint, scissor, strokeWidth, fringe, (1.0f - 0.5f / 255.0f));
    } else {
      // Fill shader
      call->uniformOffset = allocFragUniforms(1);
      if (call->uniformOffset == -1) {
        // We get here if call alloc was ok, but something else is not.
        // Roll back the last call to prevent drawing it.
        if (curBuffers_->ncalls > 0)
          curBuffers_->ncalls--;
        return;
      }
      convertPaintForFrag(
          fragUniformAtIndex(call->uniformOffset), paint, scissor, strokeWidth, fringe, -1.0f);
    }
  }

  void renderTrianglesWithPaint(NVGpaint* paint,
                                NVGcompositeOperationState compositeOperation,
                                NVGscissor* scissor,
                                const NVGvertex* verts,
                                int nverts,
                                float fringe) {
    MNVGcall* call = allocCall();
    FragmentUniforms* frag;

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
    call->uniformOffset = allocFragUniforms(1);
    if (call->uniformOffset == -1) {
      // We get here if call alloc was ok, but something else is not.
      // Roll back the last call to prevent drawing it.
      if (curBuffers_->ncalls > 0)
        curBuffers_->ncalls--;
      return;
    }
    frag = fragUniformAtIndex(call->uniformOffset);
    convertPaintForFrag(frag, paint, scissor, 1.0f, fringe, -1.0f);
    frag->type = MNVG_SHADER_IMG;
  }

  int renderUpdateTextureWithImage(int image,
                                   int x,
                                   int y,
                                   int width,
                                   int height,
                                   const unsigned char* data) {
    MNVGtexture* tex = findTexture(image);

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
                           sizeof(VertexUniforms));
      desc.hint = igl::BufferDesc::BufferAPIHintBits::UniformBlock;
      desc.debugName = "vertex_uniform_buffer";
      curBuffers_->vertexUniformBuffer = device_->createBuffer(desc, NULL);
    }
  }

  void setUniforms(int uniformOffset, int image) {
    renderEncoder_->bindBuffer(kFragmentUniformBlockIndex,
                               curBuffers_->fragmentUniformBuffer.get(),
                               uniformOffset,
                               fragmentUniformBufferSize_);

    MNVGtexture* tex = (image == 0 ? nullptr : findTexture(image));
    if (tex != nullptr) {
      renderEncoder_->bindTexture(0, igl::BindTarget::kFragment, tex->tex.get());
      renderEncoder_->bindSamplerState(0, igl::BindTarget::kFragment, tex->sampler.get());
    } else {
      renderEncoder_->bindTexture(0, igl::BindTarget::kFragment, pseudoTexture_.get());
      renderEncoder_->bindSamplerState(0, igl::BindTarget::kFragment, pseudoSampler_.get());
    }
  }

  void stroke(MNVGcall* call) {
    if (call->strokeCount <= 0) {
      return;
    }

    if (flags_ & NVG_STENCIL_STROKES) {
      // Fills the stroke base without overlap.
      bindRenderPipeline(pipelineStateTriangleStrip_);
      setUniforms(call->uniformOffset + fragmentUniformBufferSize_, call->image);
      renderEncoder_->bindDepthStencilState(strokeShapeStencilState_);

      renderEncoder_->draw(call->strokeCount, 1, call->strokeOffset);

      // Draws anti-aliased fragments.
      setUniforms(call->uniformOffset, call->image);
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
      setUniforms(call->uniformOffset, call->image);
      renderEncoder_->draw(call->strokeCount, 1, call->strokeOffset);
    }
  }

  void triangles(MNVGcall* call) {
    bindRenderPipeline(pipelineState_);
    setUniforms(call->uniformOffset, call->image);
    renderEncoder_->draw(call->triangleCount, 1, call->triangleOffset);
  }

  void updateRenderPipelineStatesForBlend(MNVGblend* blend) {
    if (pipelineState_ != nullptr && stencilOnlyPipelineState_ != nullptr &&
        piplelinePixelFormat_ == framebuffer_->getColorAttachment(0)->getProperties().format &&
        _blendFunc->srcRGB == blend->srcRGB && _blendFunc->dstRGB == blend->dstRGB &&
        _blendFunc->srcAlpha == blend->srcAlpha && _blendFunc->dstAlpha == blend->dstAlpha) {
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
    _blendFunc->srcRGB = blend->srcRGB;
    _blendFunc->dstRGB = blend->dstRGB;
    _blendFunc->srcAlpha = blend->srcAlpha;
    _blendFunc->dstAlpha = blend->dstAlpha;

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

  // Re-creates stencil texture whenever the specified size is bigger.
  void updateStencilTextureToSize(igl_vector_uint2* size) {
    if (curBuffers_->stencilTexture != nullptr &&
        (curBuffers_->stencilTexture->getSize().width < size->x ||
         curBuffers_->stencilTexture->getSize().height < size->y)) {
      curBuffers_->stencilTexture = nullptr;
    }
    if (curBuffers_->stencilTexture == nullptr) {
      igl::TextureDesc stencilTextureDescriptor =
          igl::TextureDesc::new2D(kStencilFormat,
                                  size->x,
                                  size->y,
                                  igl::TextureDesc::TextureUsageBits::Attachment |
                                      igl::TextureDesc::TextureUsageBits::Sampled);
#if TARGET_OS_SIMULATOR
      stencilTextureDescriptor.storage = igl::ResourceStorage::Private;
#endif
      curBuffers_->stencilTexture = device_->createTexture(stencilTextureDescriptor, NULL);
    }
  }
};

static void igl_nanovg__renderCancel(void* uptr) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  mtl->renderCancel();
}

static int igl_nanovg__renderCreateTexture(void* uptr,
                                       int type,
                                       int width,
                                       int height,
                                       int imageFlags,
                                       const unsigned char* data) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  return mtl->renderCreateTextureWithType(type, width, height, imageFlags, data);
}

static int igl_nanovg__renderCreate(void* uptr) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  return mtl->renderCreate();
}

static void igl_nanovg__renderDelete(void* uptr) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  mtl->renderDelete();
}

static int igl_nanovg__renderDeleteTexture(void* uptr, int image) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  return mtl->renderDeleteTexture(image);
}

static void igl_nanovg__renderFill(void* uptr,
                               NVGpaint* paint,
                               NVGcompositeOperationState compositeOperation,
                               NVGscissor* scissor,
                               float fringe,
                               const float* bounds,
                               const NVGpath* paths,
                               int npaths) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  mtl->renderFillWithPaint(paint, compositeOperation, scissor, fringe, bounds, paths, npaths);
}

static void igl_nanovg__renderFlush(void* uptr) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  mtl->renderFlush();
}

static int igl_nanovg__renderGetTextureSize(void* uptr, int image, int* w, int* h) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  return mtl->renderGetTextureSizeForImage(image, w, h);
}

static void igl_nanovg__renderStroke(void* uptr,
                                 NVGpaint* paint,
                                 NVGcompositeOperationState compositeOperation,
                                 NVGscissor* scissor,
                                 float fringe,
                                 float strokeWidth,
                                 const NVGpath* paths,
                                 int npaths) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  mtl->renderStrokeWithPaint(
      paint, compositeOperation, scissor, fringe, strokeWidth, paths, npaths);
}

static void igl_nanovg__renderTriangles(void* uptr,
                                    NVGpaint* paint,
                                    NVGcompositeOperationState compositeOperation,
                                    NVGscissor* scissor,
                                    const NVGvertex* verts,
                                    int nverts,
                                    float fringe) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  mtl->renderTrianglesWithPaint(paint, compositeOperation, scissor, verts, nverts, fringe);
}

static int igl_nanovg__renderUpdateTexture(void* uptr,
                                       int image,
                                       int x,
                                       int y,
                                       int w,
                                       int h,
                                       const unsigned char* data) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  return mtl->renderUpdateTextureWithImage(image, x, y, w, h, data);
}

static void igl_nanovg__renderViewport(void* uptr, float width, float height, float device_PixelRatio) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  mtl->renderViewportWithWidth(width, height, device_PixelRatio);
}

void SetRenderCommandEncoder(NVGcontext* ctx,
                             igl::IFramebuffer* framebuffer,
                             igl::IRenderCommandEncoder* command,
                             float* matrix) {
  MNVGcontext* mtl = (MNVGcontext*)nvgInternalParams(ctx)->userPtr;
  mtl->framebuffer_ = framebuffer;
  mtl->renderEncoder_ = command;
  if (matrix) {
    memcpy(&mtl->curBuffers_->vertexUniforms.matrix, matrix, sizeof(float) * 16);
  }
}

NVGcontext* CreateContext(igl::IDevice* device_, int flags) {
  NVGparams params;
  NVGcontext* ctx = NULL;
  MNVGcontext* mtl = new MNVGcontext();

  memset(&params, 0, sizeof(params));
  params.renderCreate = igl_nanovg__renderCreate;
  params.renderCreateTexture = igl_nanovg__renderCreateTexture;
  params.renderDeleteTexture = igl_nanovg__renderDeleteTexture;
  params.renderUpdateTexture = igl_nanovg__renderUpdateTexture;
  params.renderGetTextureSize = igl_nanovg__renderGetTextureSize;
  params.renderViewport = igl_nanovg__renderViewport;
  params.renderCancel = igl_nanovg__renderCancel;
  params.renderFlush = igl_nanovg__renderFlush;
  params.renderFill = igl_nanovg__renderFill;
  params.renderStroke = igl_nanovg__renderStroke;
  params.renderTriangles = igl_nanovg__renderTriangles;
  params.renderDelete = igl_nanovg__renderDelete;
  params.userPtr = (void*)mtl;
  params.edgeAntiAlias = flags & NVG_ANTIALIAS ? 1 : 0;

  mtl->flags_ = flags;

  // sizeof(MNVGfragUniforms)=176

#if TARGET_OS_OSX || TARGET_OS_SIMULATOR
  mtl->fragmentUniformBufferSize_ = 256;
#else
  static_assert(64 * 3 > sizeof(FragmentUniforms));
  mtl->fragmentUniformBufferSize_ = 64 * 3; // 64 * 3 > 176
#endif // TARGET_OS_OSX
  mtl->indexSize_ = 4; // IndexType::UInt32
  mtl->device_ = device_;

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
  nvgDeleteInternal(ctx);
}

} // namespace iglu::nanovg
