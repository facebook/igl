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

static void setValue(iglu::simdtypes::float2 * dst, float x, float y){
    float src[2] = {x, y};
    memcpy(dst, src, sizeof(float)*2);
}

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
typedef struct MNVGblend MNVGblend;

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
typedef struct MNVGcall MNVGcall;

struct MNVGfragUniforms {
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

typedef struct MNVGfragUniforms MNVGfragUniforms;

struct MNVGtexture {
  int _id;
  int type;
  int flags;
  std::shared_ptr<igl::ITexture> tex;
  std::shared_ptr<igl::ISamplerState> sampler;
};

struct MNVGbuffers {
  std::shared_ptr<igl::ICommandBuffer> commandBuffer;
  bool isBusy;
  int image;
  std::shared_ptr<igl::IBuffer> viewSizeBuffer;
  std::shared_ptr<igl::ITexture> stencilTexture;
  std::vector<MNVGcall> calls;
  int ccalls;
  int ncalls;
  std::shared_ptr<igl::IBuffer> indexBuffer;
  std::vector<uint32_t> indexes;
  int cindexes;
  int nindexes;
  std::shared_ptr<igl::IBuffer> vertBuffer;
  std::vector<NVGvertex> verts;
  int cverts;
  int nverts;
  std::shared_ptr<igl::IBuffer> uniformBuffer;
  std::vector<unsigned char> uniforms;
  int cuniforms;
  int nuniforms;

  void uploadToGpu() {
    if (vertBuffer) {
      vertBuffer->upload(verts.data(), igl::BufferRange(verts.size() * sizeof(NVGvertex)));
    }

    if (indexBuffer) {
      indexBuffer->upload(indexes.data(), igl::BufferRange(indexes.size() * sizeof(uint32_t)));
    }

    if (uniformBuffer) {
      uniformBuffer->upload(uniforms.data(), igl::BufferRange(uniforms.size()));
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

static int mtlnvg__maxi(int a, int b) {
  return a > b ? a : b;
}

static int mtlnvg__maxVertCount(const NVGpath* paths,
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

static iglu::simdtypes::float4 mtlnvg__premulColor(NVGcolor c) {
  c.r *= c.a;
  c.g *= c.a;
  c.b *= c.a;
  iglu::simdtypes::float4 ret;
  memcpy(&ret, &c, sizeof(float) * 4);
  return ret;
}

static void mtlnvg__xformToMat3x3(iglu::simdtypes::float3x3* m3, float* t) {
  float columns_0[3] = {t[0], t[1], 0.0f};
  float columns_1[3] = {t[2], t[3], 0.0f};
  float columns_2[3] = {t[4], t[5], 1.0f};

  memcpy(&m3->columns[0], columns_0, 3 * sizeof(float));
  memcpy(&m3->columns[1], columns_1, 3 * sizeof(float));
  memcpy(&m3->columns[2], columns_2, 3 * sizeof(float));
}

static void mtlnvg__vset(NVGvertex* vtx, float x, float y, float u, float v) {
  vtx->x = x;
  vtx->y = y;
  vtx->u = u;
  vtx->v = v;
}

class MNVGcontext {
 public:
  igl::IDevice* device;
  std::shared_ptr<igl::IRenderCommandEncoder> _renderEncoder;

  int _fragSize;
  int _indexSize;
  int _flags;
  igl_vector_uint2 viewPortSize;
  igl::Color clearColor{1, 1, 1};
  bool clearBufferOnFlush;

  std::shared_ptr<igl::IFramebuffer> framebuffer;

  // Textures
  std::vector<MNVGtexture*> _textures;
  int _textureId;

  // Per frame buffers
  MNVGbuffers* _buffers;
  std::vector<MNVGbuffers*> _cbuffers;
  int _maxBuffers;

  // Cached states.
  MNVGblend* _blendFunc;
  std::shared_ptr<igl::IDepthStencilState> _defaultStencilState;
  std::shared_ptr<igl::IDepthStencilState> _fillShapeStencilState;
  std::shared_ptr<igl::IDepthStencilState> _fillAntiAliasStencilState;
  std::shared_ptr<igl::IDepthStencilState> _fillStencilState;
  std::shared_ptr<igl::IDepthStencilState> _strokeShapeStencilState;
  std::shared_ptr<igl::IDepthStencilState> _strokeAntiAliasStencilState;
  std::shared_ptr<igl::IDepthStencilState> _strokeClearStencilState;
  std::shared_ptr<igl::IShaderModule> _fragmentFunction;
  std::shared_ptr<igl::IShaderModule> _vertexFunction;
  igl::TextureFormat piplelinePixelFormat;
  std::shared_ptr<igl::IRenderPipelineState> _pipelineState;
  std::shared_ptr<igl::IRenderPipelineState> _pipelineStateTriangleStrip;
  std::shared_ptr<igl::IRenderPipelineState> _stencilOnlyPipelineState;
  std::shared_ptr<igl::IRenderPipelineState> _stencilOnlyPipelineStateTriangleStrip;
  std::shared_ptr<igl::ISamplerState> _pseudoSampler;
  std::shared_ptr<igl::ITexture> _pseudoTexture;
  igl::VertexInputStateDesc _vertexDescriptor;

  MNVGcall* allocCall() {
    MNVGcall* ret = NULL;
    if (_buffers->ncalls + 1 > _buffers->ccalls) {
      int ccalls = mtlnvg__maxi(_buffers->ncalls + 1, 128) + _buffers->ccalls / 2;
      _buffers->calls.resize(ccalls);
      _buffers->ccalls = ccalls;
    }
    ret = &_buffers->calls[_buffers->ncalls++];
    memset(ret, 0, sizeof(MNVGcall));
    return ret;
  }

  int allocFragUniforms(int n) {
    int ret = 0;
    if (_buffers->nuniforms + n > _buffers->cuniforms) {
      int cuniforms = mtlnvg__maxi(_buffers->nuniforms + n, 128) + _buffers->cuniforms / 4;
      _buffers->uniforms.resize(_fragSize * cuniforms);

      if (device->getBackendType() == igl::BackendType::Vulkan) {
        IGL_DEBUG_ASSERT((_fragSize * cuniforms) < 65536); // vulkan max 65536;
      }

      igl::BufferDesc desc(igl::BufferDesc::BufferTypeBits::Uniform,
                           _buffers->uniforms.data(),
                           _fragSize * cuniforms);
      desc.hint = igl::BufferDesc::BufferAPIHintBits::UniformBlock;
      desc.debugName = "fragment_uniform_buffer";
      std::shared_ptr<igl::IBuffer> buffer = device->createBuffer(desc, NULL);

      _buffers->uniformBuffer = buffer;
      _buffers->cuniforms = cuniforms;
    }
    ret = _buffers->nuniforms * _fragSize;
    _buffers->nuniforms += n;
    return ret;
  }

  int allocIndexes(int n) {
    int ret = 0;
    if (_buffers->nindexes + n > _buffers->cindexes) {
      int cindexes = mtlnvg__maxi(_buffers->nindexes + n, 4096) + _buffers->cindexes / 2;
      _buffers->indexes.resize(cindexes);

      igl::BufferDesc desc(
          igl::BufferDesc::BufferTypeBits::Index, _buffers->indexes.data(), _indexSize * cindexes);
      desc.debugName = "index_buffer";
      std::shared_ptr<igl::IBuffer> buffer = device->createBuffer(desc, NULL);

      _buffers->indexBuffer = buffer;
      _buffers->cindexes = cindexes;
    }
    ret = _buffers->nindexes;
    _buffers->nindexes += n;
    return ret;
  }

  MNVGtexture* allocTexture() {
    MNVGtexture* tex = nullptr;

    for (MNVGtexture* texture : _textures) {
      if (texture->_id == 0) {
        tex = texture;
        break;
      }
    }
    if (tex == nullptr) {
      tex = new MNVGtexture();
      _textures.emplace_back(tex);
    }
    tex->_id = ++_textureId;
    return tex;
  }

  int allocVerts(int n) {
    int ret = 0;
    if (_buffers->nverts + n > _buffers->cverts) {
      int cverts = mtlnvg__maxi(_buffers->nverts + n, 4096) + _buffers->cverts / 2;
      _buffers->verts.resize(cverts);

      igl::BufferDesc desc(igl::BufferDesc::BufferTypeBits::Vertex,
                           _buffers->verts.data(),
                           sizeof(NVGvertex) * cverts);
      desc.debugName = "vertex_buffer";
      std::shared_ptr<igl::IBuffer> buffer = device->createBuffer(desc, NULL);

      _buffers->vertBuffer = buffer;
      _buffers->cverts = cverts;
    }
    ret = _buffers->nverts;
    _buffers->nverts += n;
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

  int convertPaintForFrag(MNVGfragUniforms* frag,
                          NVGpaint* paint,
                          NVGscissor* scissor,
                          float width,
                          float fringe,
                          float strokeThr) {
    MNVGtexture* tex = nullptr;
    float invxform[6];

    memset(frag, 0, sizeof(*frag));

    frag->innerCol = mtlnvg__premulColor(paint->innerColor);
    frag->outerCol = mtlnvg__premulColor(paint->outerColor);

    if (scissor->extent[0] < -0.5f || scissor->extent[1] < -0.5f) {
      frag->scissorMat = iglu::simdtypes::float3x3(0);
        setValue(&frag->scissorExt, 1.0f, 1.0f);
        setValue(&frag->scissorScale, 1.0f, 1.0f);
    } else {
      nvgTransformInverse(invxform, scissor->xform);
      mtlnvg__xformToMat3x3(&frag->scissorMat, invxform);
        setValue(&frag->scissorExt, scissor->extent[0], scissor->extent[1]);
        setValue(&frag->scissorScale,
                 sqrtf(scissor->xform[0] * scissor->xform[0] + scissor->xform[2] * scissor->xform[2]) /
                 fringe,
                 sqrtf(scissor->xform[1] * scissor->xform[1] + scissor->xform[3] * scissor->xform[3]) /
                 fringe);
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
        nvgTransformTranslate(m1, 0.0f, frag->extent.y * 0.5f);
        nvgTransformMultiply(m1, paint->xform);
        nvgTransformScale(m2, 1.0f, -1.0f);
        nvgTransformMultiply(m2, m1);
        nvgTransformTranslate(m1, 0.0f, -frag->extent.y * 0.5f);
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

    mtlnvg__xformToMat3x3(&frag->paintMat, invxform);

    return 1;
  }

  void bindRenderPipeline(const std::shared_ptr<igl::IRenderPipelineState>& pipelineState) {
    _renderEncoder->bindRenderPipelineState(pipelineState);
    _renderEncoder->bindVertexBuffer(MNVG_VERTEX_INPUT_INDEX_VERTICES, *_buffers->vertBuffer, 0);
    _renderEncoder->bindBuffer(kVertexUniformBlockIndex, _buffers->viewSizeBuffer.get(), 0);
    _renderEncoder->bindBuffer(kFragmentUniformBlockIndex, _buffers->uniformBuffer.get(), 0);
  }

  void convexFill(MNVGcall* call) {
    const int kIndexBufferOffset = call->indexOffset * _indexSize;
    bindRenderPipeline(_pipelineState);
    setUniforms(call->uniformOffset, call->image);
    if (call->indexCount > 0) {
      _renderEncoder->bindIndexBuffer(
          *_buffers->indexBuffer, igl::IndexFormat::UInt32, kIndexBufferOffset);
      _renderEncoder->drawIndexed(call->indexCount);
    }

    // Draw fringes
    if (call->strokeCount > 0) {
      bindRenderPipeline(_pipelineStateTriangleStrip);
      _renderEncoder->draw(call->strokeCount, 1, call->strokeOffset);
    }
  }

  void fill(MNVGcall* call) {
    // Draws shapes.
    const int kIndexBufferOffset = call->indexOffset * _indexSize;
    bindRenderPipeline(_stencilOnlyPipelineState);
    _renderEncoder->bindDepthStencilState(_fillShapeStencilState);
    if (call->indexCount > 0) {
      _renderEncoder->bindIndexBuffer(
          *_buffers->indexBuffer, igl::IndexFormat::UInt32, kIndexBufferOffset);
      _renderEncoder->drawIndexed(call->indexCount);
    }

    // Restores states.
    bindRenderPipeline(_pipelineStateTriangleStrip);

    // Draws anti-aliased fragments.
    setUniforms(call->uniformOffset, call->image);
    if (_flags & NVG_ANTIALIAS && call->strokeCount > 0) {
      _renderEncoder->bindDepthStencilState(_fillAntiAliasStencilState);
      _renderEncoder->draw(call->strokeCount, 1, call->strokeOffset);
    }

    // Draws fill.
    _renderEncoder->bindDepthStencilState(_fillStencilState);
    _renderEncoder->draw(call->triangleCount, 1, call->triangleOffset);
    _renderEncoder->bindDepthStencilState(_defaultStencilState);
  }

  MNVGtexture* findTexture(int _id) {
    for (MNVGtexture* texture : _textures) {
      if (texture->_id == _id)
        return texture;
    }
    return nullptr;
  }

  MNVGfragUniforms* fragUniformAtIndex(int index) {
    return (MNVGfragUniforms*)&_buffers->uniforms[index];
  }

  void renderCancel() {
    _buffers->image = 0;
    _buffers->isBusy = false;
    _buffers->nindexes = 0;
    _buffers->nverts = 0;
    _buffers->ncalls = 0;
    _buffers->nuniforms = 0;
  }

  void renderCommandEncoderWithColorTexture() {
    clearBufferOnFlush = false;

    _renderEncoder->setStencilReferenceValue(0);
    _renderEncoder->bindViewport(
        {0.0, 0.0, (float)viewPortSize.x, (float)viewPortSize.y, 0.0, 1.0});
    _renderEncoder->bindVertexBuffer(MNVG_VERTEX_INPUT_INDEX_VERTICES, *_buffers->vertBuffer, 0);
    _renderEncoder->bindBuffer(kVertexUniformBlockIndex, _buffers->viewSizeBuffer.get(), 0);
    _renderEncoder->bindBuffer(kFragmentUniformBlockIndex, _buffers->uniformBuffer.get(), 0);
  }

  int renderCreate() {
    bool creates_pseudo_texture = false;

    const std::string vertexFunction = "vertexShader";
    std::string fragmentFunction;
    if (_flags & NVG_ANTIALIAS) {
      fragmentFunction = "fragmentShaderAA";
    } else {
      fragmentFunction = "fragmentShader";
    }

    igl::Result result;

    unsigned char* shader_source = NULL;
    unsigned int shader_length = 0;

    if (device->getBackendType() == igl::BackendType::Metal) {
      std::unique_ptr<igl::IShaderLibrary> shader_library =
          igl::ShaderLibraryCreator::fromStringInput(*device,
                                                     iglu::nanovg::metal_shader.c_str(),
                                                     vertexFunction,
                                                     fragmentFunction,
                                                     "",
                                                     &result);

      _vertexFunction = shader_library->getShaderModule(vertexFunction);
      _fragmentFunction = shader_library->getShaderModule(fragmentFunction);
    } else if (device->getBackendType() == igl::BackendType::OpenGL) {
#if IGL_PLATFORM_ANDROID || IGL_PLATFORM_IOS
      auto codeVS = std::regex_replace(iglu::nanovg::opengl_410_vertex_shader_header,
                                       std::regex("#version 410"),
                                       "#version 300 es");
      auto codeFS = std::regex_replace(iglu::nanovg::opengl_410_fragment_shader_header,
                                       std::regex("#version 410"),
                                       "#version 300 es");

      codeVS += iglu::nanovg::opengl_vertex_shader_body;
      codeFS += iglu::nanovg::opengl_fragment_shader_body;
#else
      auto codeVS =
          iglu::nanovg::opengl_410_vertex_shader_header + iglu::nanovg::opengl_vertex_shader_body;
      auto codeFS = iglu::nanovg::opengl_410_fragment_shader_header +
                    iglu::nanovg::opengl_fragment_shader_body;

#endif

      std::unique_ptr<igl::IShaderStages> shader_stages =
          igl::ShaderStagesCreator::fromModuleStringInput(
              *device, codeVS.c_str(), "main", "", codeFS.c_str(), "main", "", nullptr);

      _vertexFunction = shader_stages->getVertexModule();
      _fragmentFunction = shader_stages->getFragmentModule();
    } else if (device->getBackendType() == igl::BackendType::Vulkan) {
      auto codeVS =
          iglu::nanovg::opengl_460_vertex_shader_header + iglu::nanovg::opengl_vertex_shader_body;
      auto codeFS = iglu::nanovg::opengl_460_fragment_shader_header +
                    iglu::nanovg::opengl_fragment_shader_body;

      std::unique_ptr<igl::IShaderStages> shader_stages =
          igl::ShaderStagesCreator::fromModuleStringInput(
              *device, codeVS.c_str(), "main", "", codeFS.c_str(), "main", "", nullptr);

      _vertexFunction = shader_stages->getVertexModule();
      _fragmentFunction = shader_stages->getFragmentModule();
    }

    _maxBuffers = 3;

    for (int i = _maxBuffers; i--;) {
      _cbuffers.emplace_back(new MNVGbuffers());
    }
    clearBufferOnFlush = false;

    // Initializes vertex descriptor.
    _vertexDescriptor.numAttributes = 2;
    _vertexDescriptor.attributes[0].format = igl::VertexAttributeFormat::Float2;
    _vertexDescriptor.attributes[0].name = "pos";
    _vertexDescriptor.attributes[0].bufferIndex = 0;
    _vertexDescriptor.attributes[0].offset = offsetof(NVGvertex, x);
    _vertexDescriptor.attributes[0].location = 0;

    _vertexDescriptor.attributes[1].format = igl::VertexAttributeFormat::Float2;
    _vertexDescriptor.attributes[1].name = "tcoord";
    _vertexDescriptor.attributes[1].bufferIndex = 0;
    _vertexDescriptor.attributes[1].offset = offsetof(NVGvertex, u);
    _vertexDescriptor.attributes[1].location = 1;

    _vertexDescriptor.numInputBindings = 1;
    _vertexDescriptor.inputBindings[0].stride = sizeof(NVGvertex);
    _vertexDescriptor.inputBindings[0].sampleFunction = igl::VertexSampleFunction::PerVertex;

    // Initialzes textures.
    _textureId = 0;

    // Initializes default sampler descriptor.
    igl::SamplerStateDesc samplerDescriptor;
    samplerDescriptor.debugName = "pseudoSampler";
    _pseudoSampler = device->createSamplerState(samplerDescriptor, &result);

    // Initializes pseudo texture for macOS.
    if (creates_pseudo_texture) {
      const int kPseudoTextureImage = renderCreateTextureWithType(NVG_TEXTURE_ALPHA, 1, 1, 0, NULL);
      MNVGtexture* tex = findTexture(kPseudoTextureImage);
      _pseudoTexture = tex->tex;
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
    _defaultStencilState = device->createDepthStencilState(stencilDescriptor, &result);

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
    _fillShapeStencilState = device->createDepthStencilState(stencilDescriptor, &result);

    // Fill anti-aliased stencil.
    frontFaceStencilDescriptor.stencilCompareFunction = igl::CompareFunction::Equal;
    frontFaceStencilDescriptor.stencilFailureOperation = igl::StencilOperation::Keep;
    frontFaceStencilDescriptor.depthFailureOperation = igl::StencilOperation::Keep;
    frontFaceStencilDescriptor.depthStencilPassOperation = igl::StencilOperation::Zero;

    stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
    stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
    stencilDescriptor.debugName = "fillAntiAliasStencilState";
    _fillAntiAliasStencilState = device->createDepthStencilState(stencilDescriptor, &result);

    // Fill stencil.
    frontFaceStencilDescriptor.stencilCompareFunction = igl::CompareFunction::NotEqual;
    frontFaceStencilDescriptor.stencilFailureOperation = igl::StencilOperation::Zero;
    frontFaceStencilDescriptor.depthFailureOperation = igl::StencilOperation::Zero;
    frontFaceStencilDescriptor.depthStencilPassOperation = igl::StencilOperation::Zero;

    stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
    stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
    stencilDescriptor.debugName = "fillStencilState";
    _fillStencilState = device->createDepthStencilState(stencilDescriptor, &result);

    // Stroke shape stencil.
    frontFaceStencilDescriptor.stencilCompareFunction = igl::CompareFunction::Equal;
    frontFaceStencilDescriptor.stencilFailureOperation = igl::StencilOperation::Keep;
    frontFaceStencilDescriptor.depthFailureOperation = igl::StencilOperation::Keep;
    frontFaceStencilDescriptor.depthStencilPassOperation = igl::StencilOperation::IncrementClamp;

    stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
    stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
    stencilDescriptor.debugName = "strokeShapeStencilState";
    _strokeShapeStencilState = device->createDepthStencilState(stencilDescriptor, &result);

    // Stroke anti-aliased stencil.
    frontFaceStencilDescriptor.depthStencilPassOperation = igl::StencilOperation::Keep;

    stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
    stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
    stencilDescriptor.debugName = "strokeAntiAliasStencilState";
    _strokeAntiAliasStencilState = device->createDepthStencilState(stencilDescriptor, &result);

    // Stroke clear stencil.
    frontFaceStencilDescriptor.stencilCompareFunction = igl::CompareFunction::AlwaysPass;
    frontFaceStencilDescriptor.stencilFailureOperation = igl::StencilOperation::Zero;
    frontFaceStencilDescriptor.depthFailureOperation = igl::StencilOperation::Zero;
    frontFaceStencilDescriptor.depthStencilPassOperation = igl::StencilOperation::Zero;

    stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
    stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
    stencilDescriptor.debugName = "strokeClearStencilState";
    _strokeClearStencilState = device->createDepthStencilState(stencilDescriptor, &result);
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

    tex->tex = device->createTexture(textureDescriptor, NULL);

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
    tex->sampler = device->createSamplerState(samplerDescriptor, NULL);

    return tex->_id;
  }

  void renderDelete() {
    for (MNVGbuffers* buffers : _cbuffers) {
      buffers->commandBuffer = nullptr;
      buffers->viewSizeBuffer = nullptr;
      buffers->stencilTexture = nullptr;
      buffers->indexBuffer = nullptr;
      buffers->vertBuffer = nullptr;
      buffers->uniformBuffer = nullptr;
    }

    for (MNVGtexture* texture : _textures) {
      texture->tex = nullptr;
      texture->sampler = nullptr;
    }

    free(_blendFunc);
    _renderEncoder = nullptr;
    _textures.clear();
    _cbuffers.clear();
    _defaultStencilState = nullptr;
    _fillShapeStencilState = nullptr;
    _fillAntiAliasStencilState = nullptr;
    _strokeShapeStencilState = nullptr;
    _strokeAntiAliasStencilState = nullptr;
    _strokeClearStencilState = nullptr;
    _pipelineState = nullptr;
    _stencilOnlyPipelineState = nullptr;
    _pseudoSampler = nullptr;
    _pseudoTexture = nullptr;
    device = nullptr;
  }

  int renderDeleteTexture(int image) {
    for (MNVGtexture* texture : _textures) {
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
        mtlnvg__maxVertCount(paths, npaths, &indexCount, &strokeCount) + call->triangleCount;
    int vertOffset = allocVerts(maxverts);
    if (vertOffset == -1) {
      // We get here if call alloc was ok, but something else is not.
      // Roll back the last call to prevent drawing it.
      if (_buffers->ncalls > 0)
        _buffers->ncalls--;
      return;
    }

    int indexOffset = allocIndexes(indexCount);
    if (indexOffset == -1) {
      // We get here if call alloc was ok, but something else is not.
      // Roll back the last call to prevent drawing it.
      if (_buffers->ncalls > 0)
        _buffers->ncalls--;
      return;
    }
    call->indexOffset = indexOffset;
    call->indexCount = indexCount;
    uint32_t* index = &_buffers->indexes[indexOffset];

    int strokeVertOffset = vertOffset + (maxverts - strokeCount);
    call->strokeOffset = strokeVertOffset + 1;
    call->strokeCount = strokeCount - 2;
    NVGvertex* strokeVert = _buffers->verts.data() + strokeVertOffset;

    NVGpath* path = (NVGpath*)&paths[0];
    for (int i = npaths; i--; ++path) {
      if (path->nfill > 2) {
        memcpy(&_buffers->verts[vertOffset], path->fill, sizeof(NVGvertex) * path->nfill);

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
      quad = &_buffers->verts[call->triangleOffset];
      mtlnvg__vset(&quad[0], bounds[2], bounds[3], 0.5f, 1.0f);
      mtlnvg__vset(&quad[1], bounds[2], bounds[1], 0.5f, 1.0f);
      mtlnvg__vset(&quad[2], bounds[0], bounds[3], 0.5f, 1.0f);
      mtlnvg__vset(&quad[3], bounds[0], bounds[1], 0.5f, 1.0f);
    }

    // Fill shader
    call->uniformOffset = allocFragUniforms(1);
    if (call->uniformOffset == -1) {
      // We get here if call alloc was ok, but something else is not.
      // Roll back the last call to prevent drawing it.
      if (_buffers->ncalls > 0)
        _buffers->ncalls--;
      return;
    }
    convertPaintForFrag(
        fragUniformAtIndex(call->uniformOffset), paint, scissor, fringe, fringe, -1.0f);
  }

  void renderFlush() {
    // Cancelled if the drawable is invisible.
    if (viewPortSize.x == 0 || viewPortSize.y == 0) {
      renderCancel();
      return;
    }

    _buffers->uploadToGpu();

    renderCommandEncoderWithColorTexture();

    MNVGcall* call = &_buffers->calls[0];
    for (int i = _buffers->ncalls; i--; ++call) {
      MNVGblend* blend = &call->blendFunc;

      updateRenderPipelineStatesForBlend(blend);

      if (call->type == MNVG_FILL) {
        _renderEncoder->pushDebugGroupLabel("fill");
        fill(call);
      } else if (call->type == MNVG_CONVEXFILL) {
        _renderEncoder->pushDebugGroupLabel("convexFill");
        convexFill(call);
      } else if (call->type == MNVG_STROKE) {
        _renderEncoder->pushDebugGroupLabel("stroke");
        stroke(call);
      } else if (call->type == MNVG_TRIANGLES) {
        _renderEncoder->pushDebugGroupLabel("triangles");
        triangles(call);
      }

      _renderEncoder->popDebugGroupLabel();
    }

        _buffers->isBusy = false;
        _buffers->commandBuffer = nullptr;
        _buffers->image = 0;
        _buffers->nindexes = 0;
        _buffers->nverts = 0;
        _buffers->ncalls = 0;
        _buffers->nuniforms = 0;
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
    int maxverts = mtlnvg__maxVertCount(paths, npaths, NULL, &strokeCount);
    int offset = allocVerts(maxverts);
    if (offset == -1) {
      // We get here if call alloc was ok, but something else is not.
      // Roll back the last call to prevent drawing it.
      if (_buffers->ncalls > 0)
        _buffers->ncalls--;
      return;
    }

    call->strokeOffset = offset + 1;
    call->strokeCount = strokeCount - 2;
    NVGvertex* strokeVert = _buffers->verts.data() + offset;

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

    if (_flags & NVG_STENCIL_STROKES) {
      // Fill shader
      call->uniformOffset = allocFragUniforms(2);
      if (call->uniformOffset == -1) {
        // We get here if call alloc was ok, but something else is not.
        // Roll back the last call to prevent drawing it.
        if (_buffers->ncalls > 0)
          _buffers->ncalls--;
        return;
      }
      convertPaintForFrag(
          fragUniformAtIndex(call->uniformOffset), paint, scissor, strokeWidth, fringe, -1.0f);
      MNVGfragUniforms* frag = fragUniformAtIndex(call->uniformOffset + _fragSize);
      convertPaintForFrag(frag, paint, scissor, strokeWidth, fringe, (1.0f - 0.5f / 255.0f));
    } else {
      // Fill shader
      call->uniformOffset = allocFragUniforms(1);
      if (call->uniformOffset == -1) {
        // We get here if call alloc was ok, but something else is not.
        // Roll back the last call to prevent drawing it.
        if (_buffers->ncalls > 0)
          _buffers->ncalls--;
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
    MNVGfragUniforms* frag;

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
      if (_buffers->ncalls > 0)
        _buffers->ncalls--;
      return;
    }
    call->triangleCount = nverts;

    memcpy(&_buffers->verts[call->triangleOffset], verts, sizeof(NVGvertex) * nverts);

    // Fill shader
    call->uniformOffset = allocFragUniforms(1);
    if (call->uniformOffset == -1) {
      // We get here if call alloc was ok, but something else is not.
      // Roll back the last call to prevent drawing it.
      if (_buffers->ncalls > 0)
        _buffers->ncalls--;
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

  void renderViewportWithWidth(float width, float height, float devicePixelRatio) {
    viewPortSize.x = (uint32_t)(width * devicePixelRatio);
    viewPortSize.y = (uint32_t)(height * devicePixelRatio);

    bufferIndex = (bufferIndex + 1) % 3;
    _buffers = _cbuffers[bufferIndex];

    float viewSize[2];
    viewSize[0] = width;
    viewSize[1] = height;

    // Initializes view size buffer for vertex function.
    if (_buffers->viewSizeBuffer == nullptr) {
      igl::BufferDesc desc(
          igl::BufferDesc::BufferTypeBits::Uniform, viewSize, sizeof(iglu::simdtypes::float2));
      desc.hint = igl::BufferDesc::BufferAPIHintBits::UniformBlock;
      desc.debugName = "vertex_viewSize_buffer";
      _buffers->viewSizeBuffer = device->createBuffer(desc, NULL);
    }

    _buffers->viewSizeBuffer->upload(viewSize, igl::BufferRange(sizeof(iglu::simdtypes::float2)));
  }

  void setUniforms(int uniformOffset, int image) {
    _renderEncoder->bindBuffer(
        kFragmentUniformBlockIndex, _buffers->uniformBuffer.get(), uniformOffset, _fragSize);

    MNVGtexture* tex = (image == 0 ? nullptr : findTexture(image));
    if (tex != nullptr) {
      _renderEncoder->bindTexture(0, igl::BindTarget::kFragment, tex->tex.get());
      _renderEncoder->bindSamplerState(0, igl::BindTarget::kFragment, tex->sampler.get());
    } else {
      _renderEncoder->bindTexture(0, igl::BindTarget::kFragment, _pseudoTexture.get());
      _renderEncoder->bindSamplerState(0, igl::BindTarget::kFragment, _pseudoSampler.get());
    }
  }

  void stroke(MNVGcall* call) {
    if (call->strokeCount <= 0) {
      return;
    }

    if (_flags & NVG_STENCIL_STROKES) {
      // Fills the stroke base without overlap.
      bindRenderPipeline(_pipelineStateTriangleStrip);
      setUniforms(call->uniformOffset + _fragSize, call->image);
      _renderEncoder->bindDepthStencilState(_strokeShapeStencilState);

      _renderEncoder->draw(call->strokeCount, 1, call->strokeOffset);

      // Draws anti-aliased fragments.
      setUniforms(call->uniformOffset, call->image);
      _renderEncoder->bindDepthStencilState(_strokeAntiAliasStencilState);
      _renderEncoder->draw(call->strokeCount, 1, call->strokeOffset);

      // Clears stencil buffer.
      bindRenderPipeline(_stencilOnlyPipelineStateTriangleStrip);
      _renderEncoder->bindDepthStencilState(_strokeClearStencilState);
      _renderEncoder->draw(call->strokeCount, 1, call->strokeOffset);
      _renderEncoder->bindDepthStencilState(_defaultStencilState);
    } else {
      // Draws strokes.
      bindRenderPipeline(_pipelineStateTriangleStrip);
      setUniforms(call->uniformOffset, call->image);
      _renderEncoder->draw(call->strokeCount, 1, call->strokeOffset);
    }
  }

  void triangles(MNVGcall* call) {
    bindRenderPipeline(_pipelineState);
    setUniforms(call->uniformOffset, call->image);
    _renderEncoder->draw(call->triangleCount, 1, call->triangleOffset);
  }

  void updateRenderPipelineStatesForBlend(MNVGblend* blend) {
    if (_pipelineState != nullptr && _stencilOnlyPipelineState != nullptr &&
        piplelinePixelFormat == framebuffer->getColorAttachment(0)->getProperties().format &&
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
        framebuffer->getColorAttachment(0)->getProperties().format;
    pipelineStateDescriptor.targetDesc.stencilAttachmentFormat =
        framebuffer->getStencilAttachment()->getProperties().format;
    pipelineStateDescriptor.targetDesc.depthAttachmentFormat =
        framebuffer->getDepthAttachment()->getProperties().format;
    pipelineStateDescriptor.shaderStages = igl::ShaderStagesCreator::fromRenderModules(
        *device, _vertexFunction, _fragmentFunction, &result);
    IGL_DEBUG_ASSERT(result.isOk());

    pipelineStateDescriptor.vertexInputState =
        device->createVertexInputState(_vertexDescriptor, &result);
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
    _pipelineState = device->createRenderPipeline(pipelineStateDescriptor, &result);

    pipelineStateDescriptor.topology = igl::PrimitiveType::TriangleStrip;
    pipelineStateDescriptor.cullMode = igl::CullMode::Back;
    pipelineStateDescriptor.debugName = igl::genNameHandle("TriangleStripe_CullBack");
    _pipelineStateTriangleStrip = device->createRenderPipeline(pipelineStateDescriptor, &result);
    IGL_DEBUG_ASSERT(result.isOk());

    auto fragmentFunction = device->getBackendType() == igl::BackendType::Metal ? nullptr
                                                                                : _fragmentFunction;
    pipelineStateDescriptor.shaderStages = igl::ShaderStagesCreator::fromRenderModules(
        *device, _vertexFunction, fragmentFunction, &result);

    IGL_DEBUG_ASSERT(result.isOk());
    colorAttachmentDescriptor.colorWriteMask = igl::ColorWriteBits::ColorWriteBitsDisabled;
    pipelineStateDescriptor.cullMode = igl::CullMode::Disabled;
    pipelineStateDescriptor.topology = igl::PrimitiveType::Triangle;
    pipelineStateDescriptor.debugName = igl::genNameHandle("stencilOnlyPipelineState");
    _stencilOnlyPipelineState = device->createRenderPipeline(pipelineStateDescriptor, &result);
    IGL_DEBUG_ASSERT(result.isOk());

    pipelineStateDescriptor.debugName = igl::genNameHandle("stencilOnlyPipelineStateTriangleStrip");
    pipelineStateDescriptor.topology = igl::PrimitiveType::TriangleStrip;
    _stencilOnlyPipelineStateTriangleStrip =
        device->createRenderPipeline(pipelineStateDescriptor, &result);
    IGL_DEBUG_ASSERT(result.isOk());

    piplelinePixelFormat = framebuffer->getColorAttachment(0)->getProperties().format;
  }

  // Re-creates stencil texture whenever the specified size is bigger.
  void updateStencilTextureToSize(igl_vector_uint2* size) {
    if (_buffers->stencilTexture != nullptr &&
        (_buffers->stencilTexture->getSize().width < size->x ||
         _buffers->stencilTexture->getSize().height < size->y)) {
      _buffers->stencilTexture = nullptr;
    }
    if (_buffers->stencilTexture == nullptr) {
      igl::TextureDesc stencilTextureDescriptor =
          igl::TextureDesc::new2D(kStencilFormat,
                                  size->x,
                                  size->y,
                                  igl::TextureDesc::TextureUsageBits::Attachment |
                                      igl::TextureDesc::TextureUsageBits::Sampled);
#if TARGET_OS_SIMULATOR
      stencilTextureDescriptor.storage = igl::ResourceStorage::Private;
#endif
      _buffers->stencilTexture = device->createTexture(stencilTextureDescriptor, NULL);
    }
  }
};

static void mtlnvg__renderCancel(void* uptr) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  mtl->renderCancel();
}

static int mtlnvg__renderCreateTexture(void* uptr,
                                       int type,
                                       int width,
                                       int height,
                                       int imageFlags,
                                       const unsigned char* data) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  return mtl->renderCreateTextureWithType(type, width, height, imageFlags, data);
}

static int mtlnvg__renderCreate(void* uptr) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  return mtl->renderCreate();
}

static void mtlnvg__renderDelete(void* uptr) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  mtl->renderDelete();
}

static int mtlnvg__renderDeleteTexture(void* uptr, int image) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  return mtl->renderDeleteTexture(image);
}

static void mtlnvg__renderFill(void* uptr,
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

static void mtlnvg__renderFlush(void* uptr) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  mtl->renderFlush();
}

static int mtlnvg__renderGetTextureSize(void* uptr, int image, int* w, int* h) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  return mtl->renderGetTextureSizeForImage(image, w, h);
}

static void mtlnvg__renderStroke(void* uptr,
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

static void mtlnvg__renderTriangles(void* uptr,
                                    NVGpaint* paint,
                                    NVGcompositeOperationState compositeOperation,
                                    NVGscissor* scissor,
                                    const NVGvertex* verts,
                                    int nverts,
                                    float fringe) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  mtl->renderTrianglesWithPaint(paint, compositeOperation, scissor, verts, nverts, fringe);
}

static int mtlnvg__renderUpdateTexture(void* uptr,
                                       int image,
                                       int x,
                                       int y,
                                       int w,
                                       int h,
                                       const unsigned char* data) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  return mtl->renderUpdateTextureWithImage(image, x, y, w, h, data);
}

static void mtlnvg__renderViewport(void* uptr, float width, float height, float devicePixelRatio) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  mtl->renderViewportWithWidth(width, height, devicePixelRatio);
}

void SetRenderCommandEncoder(NVGcontext* ctx,
                             std::shared_ptr<igl::IFramebuffer> framebuffer,
                             std::shared_ptr<igl::IRenderCommandEncoder> command) {
  MNVGcontext* mtl = (MNVGcontext*)nvgInternalParams(ctx)->userPtr;
  mtl->framebuffer = framebuffer;
  mtl->_renderEncoder = command;
}

NVGcontext* CreateContext(igl::IDevice* device, int flags) {
  NVGparams params;
  NVGcontext* ctx = NULL;
  MNVGcontext* mtl = new MNVGcontext();

  memset(&params, 0, sizeof(params));
  params.renderCreate = mtlnvg__renderCreate;
  params.renderCreateTexture = mtlnvg__renderCreateTexture;
  params.renderDeleteTexture = mtlnvg__renderDeleteTexture;
  params.renderUpdateTexture = mtlnvg__renderUpdateTexture;
  params.renderGetTextureSize = mtlnvg__renderGetTextureSize;
  params.renderViewport = mtlnvg__renderViewport;
  params.renderCancel = mtlnvg__renderCancel;
  params.renderFlush = mtlnvg__renderFlush;
  params.renderFill = mtlnvg__renderFill;
  params.renderStroke = mtlnvg__renderStroke;
  params.renderTriangles = mtlnvg__renderTriangles;
  params.renderDelete = mtlnvg__renderDelete;
  params.userPtr = (void*)mtl;
  params.edgeAntiAlias = flags & NVG_ANTIALIAS ? 1 : 0;

  mtl->_flags = flags;

  auto aaa = sizeof(MNVGfragUniforms);
  // sizeof(MNVGfragUniforms)=176

#if TARGET_OS_OSX || TARGET_OS_SIMULATOR
  mtl->_fragSize = 256;
#else
  static_assert(64 * 3 > sizeof(MNVGfragUniforms));
  mtl->_fragSize = 64 * 3;
#endif // TARGET_OS_OSX
  mtl->_indexSize = 4; // MTLIndexTypeUInt32
  mtl->device = device;

  ctx = nvgCreateInternal(&params);
  if (ctx == NULL)
    goto error;
  return ctx;

error:
  if (ctx != NULL)
    nvgDeleteInternal(ctx);
  return NULL;
}

void DeleteContext(NVGcontext* ctx) {
  nvgDeleteInternal(ctx);
}

} // namespace iglu::nanovg
