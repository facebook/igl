#include "nanovg_mtl.h"

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <igl/IGL.h>
//#import <QuartzCore/QuartzCore.h>
#import <simd/simd.h>
//#include <TargetConditionals.h>

#include "nanovg.h"
#include "shader_metal.h"

//#if TARGET_OS_SIMULATOR
//#  include "mnvg_bitcode/simulator.h"
//#elif TARGET_OS_IOS
//#  include "mnvg_bitcode/ios.h"
//#elif TARGET_OS_OSX
//#  include "mnvg_bitcode/macos.h"
//#elif TARGET_OS_TV
//#  include "mnvg_bitcode/tvos.h"
//#else
//#  define MNVG_INVALID_TARGET
//#endif

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
  MNVG_falseNE = 0,
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
  matrix_float3x3 scissorMat;
  matrix_float3x3 paintMat;
  vector_float4 innerCol;
  vector_float4 outerCol;
  vector_float2 scissorExt;
  vector_float2 scissorScale;
  vector_float2 extent;
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

struct MNVGbuffers{
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
    
    void uploadToGpu(){
        if (vertBuffer){
            vertBuffer->upload(verts.data(), igl::BufferRange(verts.size() * sizeof(NVGvertex)));
        }
        
        if (indexBuffer){
            indexBuffer->upload(indexes.data(), igl::BufferRange(indexes.size() * sizeof(uint32_t)));
        }
        
        if (uniformBuffer){
            uniformBuffer->upload(uniforms.data(), igl::BufferRange(uniforms.size()));
        }
    }
};

// Keeps the weak reference to the currently binded framebuffer.
MNVGframebuffer* s_framebuffer = NULL;

//const MTLResourceOptions kMetalBufferOptions = \
//    (MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeShared);

#if TARGET_OS_SIMULATOR
const igl::TextureFormat kStencilFormat = igl::TextureFormat::S8_UInt_Z32_UNorm;
#else
const igl::TextureFormat kStencilFormat = igl::TextureFormat::S8_UInt_Z32_UNorm;
#endif  // TARGET_OS_SIMULATOR

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

static int mtlnvg__maxi(int a, int b) { return a > b ? a : b; }

static int mtlnvg__maxVertCount(const NVGpath* paths, int npaths,
                                int* indexCount, int* strokeCount) {
  int count = 0;
  if (indexCount != NULL) *indexCount = 0;
  if (strokeCount != NULL) *strokeCount = 0;
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
      if (strokeCount != NULL) *strokeCount += nstroke;
    }
  }
  return count;
}

static vector_float4 mtlnvg__premulColor(NVGcolor c) {
  c.r *= c.a;
  c.g *= c.a;
  c.b *= c.a;
  return (vector_float4){c.r, c.g, c.b, c.a};
}

static void mtlnvg__xformToMat3x3(matrix_float3x3* m3, float* t) {
  *m3 = matrix_from_columns((vector_float3){t[0], t[1], 0.0f},
                            (vector_float3){t[2], t[3], 0.0f},
                            (vector_float3){t[4], t[5], 1.0f});
}

static void mtlnvg__vset(NVGvertex* vtx, float x, float y, float u, float v) {
  vtx->x = x;
  vtx->y = y;
  vtx->u = u;
  vtx->v = v;
}

void nvgDeleteMTL(NVGcontext* ctx) {
  nvgDeleteInternal(ctx);
}

void mnvgBindFramebuffer(MNVGframebuffer* framebuffer) {
  s_framebuffer = framebuffer;
}

MNVGframebuffer* mnvgCreateFramebuffer(NVGcontext* ctx, int width,
                                       int height, int imageFlags) {
  MNVGframebuffer* framebuffer = \
      (MNVGframebuffer*)malloc(sizeof(MNVGframebuffer));
  if (framebuffer == NULL)
    return NULL;

  memset(framebuffer, 0, sizeof(MNVGframebuffer));
  framebuffer->image = nvgCreateImageRGBA(ctx, width, height,
                                          imageFlags | NVG_IMAGE_PREMULTIPLIED,
                                          NULL);
  framebuffer->ctx = ctx;
  return framebuffer;
}

void mnvgDeleteFramebuffer(MNVGframebuffer* framebuffer) {
  if (framebuffer == NULL) return;
  if (framebuffer->image > 0) {
    nvgDeleteImage(framebuffer->ctx, framebuffer->image);
  }
  free(framebuffer);
}



class MNVGcontext{
public:
    igl::IDevice * device;
    std::shared_ptr<igl::ICommandQueue> _commandQueue;
    std::shared_ptr<igl::IRenderCommandEncoder> _renderEncoder;
    
    int _fragSize;
    int _indexSize;
    int _flags;
    vector_uint2 viewPortSize;
    igl::Color clearColor{1,1,1};
    bool clearBufferOnFlush;
    
    std::shared_ptr<igl::ITexture> colorTexture;
    std::shared_ptr<igl::ITexture> stencilTexture;
    
    // Textures
    std::vector<MNVGtexture*> _textures;
    int _textureId;
    
    // Per frame buffers
    MNVGbuffers* _buffers;
    std::vector<MNVGbuffers*> _cbuffers;
    int _maxBuffers;
//    dispatch_semaphore_t _semaphore;
    
    // Cached states.
    MNVGblend* _blendFunc;
    std::shared_ptr<igl::IDepthStencilState> _defaultStencilState;
    std::shared_ptr<igl::IDepthStencilState> _fillShapeStencilState;
    std::shared_ptr<igl::IDepthStencilState>
    _fillAntiAliasStencilState;
    std::shared_ptr<igl::IDepthStencilState> _fillStencilState;
    std::shared_ptr<igl::IDepthStencilState> _strokeShapeStencilState;
    std::shared_ptr<igl::IDepthStencilState>
    _strokeAntiAliasStencilState;
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
    
    MNVGcall* allocCall(){
        MNVGcall* ret = NULL;
        if (_buffers->ncalls + 1 > _buffers->ccalls) {
//            MNVGcall* calls;
            int ccalls = mtlnvg__maxi(_buffers->ncalls + 1, 128) + _buffers->ccalls / 2;
//            calls = (MNVGcall*)realloc(_buffers->calls, sizeof(MNVGcall) * ccalls);
//            calls.resize(ccalls);
//            if (calls == NULL) return NULL;
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
            int cuniforms = mtlnvg__maxi(_buffers->nuniforms + n, 128) + _buffers->cuniforms / 2;
            //todo:
//            std::shared_ptr<igl::IBuffer> buffer = [_metalLayer.device
//                                                    newBufferWithLength:(_fragSize * cuniforms)
//                                                    options:kMetalBufferOptions];
            
            igl::BufferDesc desc(igl::BufferDesc::BufferTypeBits::Uniform, nullptr, _fragSize * cuniforms);
            std::shared_ptr<igl::IBuffer> buffer = device->createBuffer(desc, NULL);
            
//            unsigned char* uniforms = NULL;// [buffer contents];
            if (_buffers->uniformBuffer != nullptr) {
//                memcpy(uniforms, _buffers->uniforms.data(),
//                       _fragSize * _buffers->nuniforms);
                
                buffer->upload(_buffers->uniforms.data(), igl::BufferRange(_fragSize * _buffers->nuniforms));
            }
            _buffers->uniformBuffer = buffer;
            _buffers->uniforms.resize(_fragSize * cuniforms);
            _buffers->cuniforms = cuniforms;
        }
        ret = _buffers->nuniforms * _fragSize;
        _buffers->nuniforms += n;
        return ret;
    }
    
    int allocIndexes(int n){
        int ret = 0;
        if (_buffers->nindexes + n > _buffers->cindexes) {
            int cindexes = mtlnvg__maxi(_buffers->nindexes + n, 4096) + _buffers->cindexes / 2;
//            std::shared_ptr<igl::IBuffer> buffer ; //= [_metalLayer.device
                                                    //newBufferWithLength:(_indexSize * cindexes)
                                                    //options:kMetalBufferOptions];
            
            igl::BufferDesc desc(igl::BufferDesc::BufferTypeBits::Index, nullptr, _indexSize * cindexes);
            std::shared_ptr<igl::IBuffer> buffer = device->createBuffer(desc, NULL);
            
//            uint32_t* indexes ;// = [buffer contents];
            if (_buffers->indexBuffer != nullptr) {
//                memcpy(indexes, _buffers->indexes.data(), _indexSize * _buffers->nindexes);
                buffer->upload(_buffers->indexes.data(), igl::BufferRange(_indexSize * _buffers->nindexes));
            }
            _buffers->indexBuffer = buffer;
            _buffers->indexes.resize(cindexes);
            _buffers->cindexes = cindexes;
        }
        ret = _buffers->nindexes;
        _buffers->nindexes += n;
        return ret;
    }
    
    MNVGtexture* allocTexture(){
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
    
    int allocVerts(int n){
        int ret = 0;
        if (_buffers->nverts + n > _buffers->cverts) {
            int cverts = mtlnvg__maxi(_buffers->nverts + n, 4096) + _buffers->cverts / 2;
//            std::shared_ptr<igl::IBuffer> buffer = ;//[_metalLayer.device
                                                    //newBufferWithLength:(sizeof(NVGvertex) * cverts)
                                                    //options:kMetalBufferOptions];
            
            igl::BufferDesc desc(igl::BufferDesc::BufferTypeBits::Vertex, nullptr, sizeof(NVGvertex) * cverts);
            std::shared_ptr<igl::IBuffer> buffer = device->createBuffer(desc, NULL);
            
//            NVGvertex* verts ;//= [buffer contents];
            if (_buffers->vertBuffer != nullptr) {
//                memcpy(verts, _buffers->verts, sizeof(NVGvertex) * _buffers->nverts);
                buffer->upload(_buffers->verts.data(), igl::BufferRange(sizeof(NVGvertex) * _buffers->nverts));
            }
            _buffers->vertBuffer = buffer;
            _buffers->verts.resize(cverts);
            _buffers->cverts = cverts;
        }
        ret = _buffers->nverts;
        _buffers->nverts += n;
        return ret;
    }
    
    MNVGblend blendCompositeOperation(NVGcompositeOperationState op){
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
    
//    void checkError(NSError* erro, const char* message) {
//        if ((_flags & NVG_DEBUG) == 0) return;
////        if (error) {
////            printf("Error occurs after %s: %s\n",
////                   message, [[error localizedDescription] UTF8String]);
////        }
//    }
    
    int convertPaintForFrag(MNVGfragUniforms*frag,
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
            frag->scissorMat = matrix_from_rows((vector_float3){0, 0, 0},
                                                (vector_float3){0, 0, 0},
                                                (vector_float3){0, 0, 0});
            frag->scissorExt.x = 1.0f;
            frag->scissorExt.y = 1.0f;
            frag->scissorScale.x = 1.0f;
            frag->scissorScale.y = 1.0f;
        } else {
            nvgTransformInverse(invxform, scissor->xform);
            mtlnvg__xformToMat3x3(&frag->scissorMat, invxform);
            frag->scissorExt.x = scissor->extent[0];
            frag->scissorExt.y = scissor->extent[1];
            frag->scissorScale.x = sqrtf(scissor->xform[0] * scissor->xform[0] + scissor->xform[2] * scissor->xform[2]) / fringe;
            frag->scissorScale.y = sqrtf(scissor->xform[1] * scissor->xform[1] + scissor->xform[3] * scissor->xform[3]) / fringe;
        }
        
        frag->extent = (vector_float2){paint->extent[0], paint->extent[1]};
        frag->strokeMult = (width * 0.5f + fringe * 0.5f) / fringe;
        frag->strokeThr = strokeThr;
        
        if (paint->image != 0) {
            tex = findTexture(paint->image);
            if (tex == nullptr) return 0;
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
    
    void convexFill(MNVGcall*call) {
        const int kIndexBufferOffset = call->indexOffset * _indexSize;
        setUniforms(call->uniformOffset,call->image);
        _renderEncoder->bindRenderPipelineState(_pipelineState);
        if (call->indexCount > 0) {
//            [_renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
//                                       indexCount:call->indexCount
//                                        indexType:MTLIndexTypeUInt32
//                                      indexBuffer:_buffers->indexBuffer
//                                indexBufferOffset:kIndexBufferOffset];
            _renderEncoder->bindIndexBuffer(*_buffers->indexBuffer, igl::IndexFormat::UInt32, kIndexBufferOffset);
            _renderEncoder->drawIndexed(call->indexCount);
        }
        
        // Draw fringes
        if (call->strokeCount > 0) {
//            [_renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
//                               vertexStart:call->strokeOffset
//                               vertexCount:call->strokeCount];
            _renderEncoder->bindRenderPipelineState(_pipelineStateTriangleStrip);
            _renderEncoder->draw(call->strokeCount, 1, call->strokeOffset);
        }
    }
    
    void fill(MNVGcall*call) {
        // Draws shapes.
        const int kIndexBufferOffset = call->indexOffset * _indexSize;
        //todo
//        [_renderEncoder setCullMode:MTLCullModeNone];
        _renderEncoder->bindDepthStencilState(_fillShapeStencilState);
        _renderEncoder->bindRenderPipelineState(_stencilOnlyPipelineState);
        if (call->indexCount > 0) {
//            [_renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
//                                       indexCount:call->indexCount
//                                        indexType:MTLIndexTypeUInt32
//                                      indexBuffer:_buffers->indexBuffer
//                                indexBufferOffset:kIndexBufferOffset];
            _renderEncoder->bindIndexBuffer(*_buffers->indexBuffer, igl::IndexFormat::UInt32, kIndexBufferOffset);
            _renderEncoder->drawIndexed(call->indexCount);
        }
        
        // Restores states.
        //todo
//        [_renderEncoder setCullMode:MTLCullModeBack];
        _renderEncoder->bindRenderPipelineState(_pipelineStateTriangleStrip);
        
        // Draws anti-aliased fragments.
        setUniforms(call->uniformOffset ,call->image);
        if (_flags & NVG_ANTIALIAS && call->strokeCount > 0) {
            _renderEncoder->bindDepthStencilState(_fillAntiAliasStencilState);
//            [_renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
//                               vertexStart:call->strokeOffset
//                               vertexCount:call->strokeCount];
            _renderEncoder->draw(call->strokeCount, 1, call->strokeOffset);
        }
        
        // Draws fill.
        _renderEncoder->bindDepthStencilState(_fillStencilState);
//        [_renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
//                           vertexStart:call->triangleOffset
//                           vertexCount:call->triangleCount];
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
    
    void renderCancel(){
        _buffers->image = 0;
        _buffers->isBusy = false;
        _buffers->nindexes = 0;
        _buffers->nverts = 0;
        _buffers->ncalls = 0;
        _buffers->nuniforms = 0;
//        dispatch_semaphore_signal(_semaphore);
    }
    
    std::shared_ptr<igl::IRenderCommandEncoder> renderCommandEncoderWithColorTexture() {
        igl::RenderPassDesc descriptor;
        
        descriptor.colorAttachments.resize(1);
        descriptor.colorAttachments[0].clearColor = clearColor;
        descriptor.colorAttachments[0].loadAction = \
        clearBufferOnFlush ? igl::LoadAction::Clear : igl::LoadAction::Load;
        descriptor.colorAttachments[0].storeAction = igl::StoreAction::Store;
//        descriptor.colorAttachments[0].texture = colorTexture;
        clearBufferOnFlush = false;
        
        descriptor.stencilAttachment.clearStencil = 0;
        descriptor.stencilAttachment.loadAction = igl::LoadAction::Clear;
        descriptor.stencilAttachment.storeAction = igl::StoreAction::DontCare;
//        descriptor.stencilAttachment.texture = _buffers->stencilTexture;
        
        igl::FramebufferDesc framebuffer_desc;
        framebuffer_desc.colorAttachments[0].texture = colorTexture;
        framebuffer_desc.stencilAttachment.texture = stencilTexture;
        
        std::shared_ptr<igl::IFramebuffer> framebuffer =device->createFramebuffer(framebuffer_desc, NULL);
        
        std::shared_ptr<igl::ICommandBuffer> commandBuffer = _buffers->commandBuffer;
        std::unique_ptr<igl::IRenderCommandEncoder> encoder = commandBuffer->createRenderCommandEncoder(descriptor, framebuffer);
        
        //todo:
        //[encoder setCullMode:MTLCullModeBack];
        //[encoder setFrontFacingWinding:MTLWindingCounterClockwise];
        encoder->setStencilReferenceValue(0);
        encoder->bindViewport({0.0, 0.0, (float)viewPortSize.x, (float)viewPortSize.y, 0.0, 1.0});
        
        encoder->bindVertexBuffer(MNVG_VERTEX_INPUT_INDEX_VERTICES, *_buffers->vertBuffer, 0);
        
        encoder->bindVertexBuffer(MNVG_VERTEX_INPUT_INDEX_VIEW_SIZE, *_buffers->viewSizeBuffer, 0);
        
        encoder->bindBuffer(2, _buffers->uniformBuffer.get(), 0);
        return encoder;
    }

    int renderCreate() {
//        if (_metalLayer.device == nullptr) {
//            id<MTLDevice> device = MTLCreateSystemDefaultDevice();
//            _metalLayer.device = device;
//        }
//#if TARGET_OS_OSX
//        _metalLayer.opaque = false;
//#endif  // TARGET_OS_OSX
//        
//        // Loads shaders from pre-compiled metal library..
//        NSError* error;
//        id<MTLDevice> device = _metalLayer.device;
//#ifdef MNVG_INVALID_TARGET
//        id<MTLLibrary> library = nullptr;
//        return 0;
//#endif
        
        bool creates_pseudo_texture = false;
        unsigned char* metal_library_bitcode;
        unsigned int metal_library_bitcode_len;
//#if TARGET_OS_SIMULATOR
//        metal_library_bitcode = mnvg_bitcode_simulator;
//        metal_library_bitcode_len = mnvg_bitcode_simulator_len;
//#elif TARGET_OS_IOS
//        if (@available(iOS 10, *)) {
//        } else if (@available(iOS 8, *)) {
//            creates_pseudo_texture = true;
//        } else {
//            return 0;
//        }
//        metal_library_bitcode = mnvg_bitcode_ios;
//        metal_library_bitcode_len = mnvg_bitcode_ios_len;
//#elif TARGET_OS_OSX
//        if (@available(macOS 10.11, *)) {
//            metal_library_bitcode = mnvg_bitcode_macos;
//            metal_library_bitcode_len = mnvg_bitcode_macos_len;
//        } else {
//            return 0;
//        }
//#elif TARGET_OS_TV
//        metal_library_bitcode = mnvg_bitcode_tvos;
//        metal_library_bitcode_len = mnvg_bitcode_tvos_len;
//#endif
        
//        dispatch_data_t data = dispatch_data_create(metal_library_bitcode,
//                                                    metal_library_bitcode_len,
//                                                    NULL,
//                                                    DISPATCH_DATA_DESTRUCTOR_DEFAULT);
//        id<MTLLibrary> library = [device newLibraryWithData:data error:&error];
//        
//        [self checkError:error withMessage:"init library"];
//        if (library == nullptr) {
//            return 0;
//        }
        
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

        std::unique_ptr<igl::IShaderLibrary> shader_library = igl::ShaderLibraryCreator::fromStringInput(*device, igl::nanovg::metal_shdader.c_str(), vertexFunction,fragmentFunction, "",
                                                         &result);
        
        _vertexFunction = shader_library->getShaderModule(vertexFunction);
        _fragmentFunction = shader_library->getShaderModule(fragmentFunction);
        //shaderStages_ = igl::ShaderStagesCreator::fromRenderModules(*device, vertex_shader_module, fragment_shader_module, NULL);
        
        igl::CommandQueueDesc queue_desc;
        queue_desc.type = igl::CommandQueueType::Graphics;
        _commandQueue = device->createCommandQueue(queue_desc, &result);
        
        // Initializes the number of available buffers.
        if (_flags & NVG_TRIPLE_BUFFER) {
            _maxBuffers = 3;
        } else if (_flags & NVG_DOUBLE_BUFFER) {
            _maxBuffers = 2;
        } else {
            _maxBuffers = 1;
        }
        //_cbuffers = [NSMutableArray arrayWithCapacity:_maxBuffers];
        for (int i = _maxBuffers; i--;) {
            _cbuffers.emplace_back( new MNVGbuffers() );
        }
        clearBufferOnFlush = false;
//        _semaphore = dispatch_semaphore_create(_maxBuffers);
        
        // Initializes vertex descriptor.
//        _vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
        _vertexDescriptor.numAttributes = 2;
        _vertexDescriptor.attributes[0].format = igl::VertexAttributeFormat::Float2;
        _vertexDescriptor.attributes[0].bufferIndex = 0;
        _vertexDescriptor.attributes[0].offset = offsetof(NVGvertex, x);
        _vertexDescriptor.attributes[0].location = 0;
        
        _vertexDescriptor.attributes[1].format = igl::VertexAttributeFormat::Float2;
        _vertexDescriptor.attributes[1].bufferIndex = 0;
        _vertexDescriptor.attributes[1].offset = offsetof(NVGvertex, u);
        _vertexDescriptor.attributes[1].location = 1;
        
        _vertexDescriptor.numInputBindings = 1;
        _vertexDescriptor.inputBindings[0].stride = sizeof(NVGvertex);
        _vertexDescriptor.inputBindings[0].sampleFunction = igl::VertexSampleFunction::PerVertex;
        
        // Initialzes textures.
        _textureId = 0;
//        _textures = [NSMutableArray array];
        
        // Initializes default sampler descriptor.
//        MTLSamplerDescriptor* samplerDescriptor = [MTLSamplerDescriptor new];
        igl::SamplerStateDesc samplerDescriptor;
        samplerDescriptor.debugName = "pseudoSampler";
        _pseudoSampler = device->createSamplerState(samplerDescriptor, &result);
        
        // Initializes pseudo texture for macOS.
        if (creates_pseudo_texture) {
            const int kPseudoTextureImage = renderCreateTextureWithType(NVG_TEXTURE_ALPHA,
                                             1,
                                             1,
                                             0,
                                             NULL);
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
        igl::StencilStateDesc frontFaceStencilDescriptor ;
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
        frontFaceStencilDescriptor.depthStencilPassOperation = \
        igl::StencilOperation::Zero;
        
        stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
        stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
        stencilDescriptor.debugName = "fillAntiAliasStencilState";
        _fillAntiAliasStencilState = device->createDepthStencilState(stencilDescriptor, &result);
        
        // Fill stencil.
        frontFaceStencilDescriptor.stencilCompareFunction = \
        igl::CompareFunction::NotEqual;
        frontFaceStencilDescriptor.stencilFailureOperation = igl::StencilOperation::Zero;
        frontFaceStencilDescriptor.depthFailureOperation = igl::StencilOperation::Zero;
        frontFaceStencilDescriptor.depthStencilPassOperation = \
        igl::StencilOperation::Zero;
        
        stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
        stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
        stencilDescriptor.debugName = "fillStencilState";
        _fillStencilState = device->createDepthStencilState(stencilDescriptor, &result);
        
        // Stroke shape stencil.
        frontFaceStencilDescriptor.stencilCompareFunction = igl::CompareFunction::Equal;
        frontFaceStencilDescriptor.stencilFailureOperation = igl::StencilOperation::Keep;
        frontFaceStencilDescriptor.depthFailureOperation = igl::StencilOperation::Keep;
        frontFaceStencilDescriptor.depthStencilPassOperation = \
        igl::StencilOperation::IncrementClamp;
        
        stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
        stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
        stencilDescriptor.debugName = "strokeShapeStencilState";
        _strokeShapeStencilState = device->createDepthStencilState(stencilDescriptor, &result);
        
        // Stroke anti-aliased stencil.
        frontFaceStencilDescriptor.depthStencilPassOperation = \
        igl::StencilOperation::Keep;
        
        stencilDescriptor.backFaceStencil = igl::StencilStateDesc();
        stencilDescriptor.frontFaceStencil = frontFaceStencilDescriptor;
        stencilDescriptor.debugName = "strokeAntiAliasStencilState";
        _strokeAntiAliasStencilState = device->createDepthStencilState(stencilDescriptor, &result);
        
        // Stroke clear stencil.
        frontFaceStencilDescriptor.stencilCompareFunction = igl::CompareFunction::AlwaysPass;
        frontFaceStencilDescriptor.stencilFailureOperation = igl::StencilOperation::Zero;
        frontFaceStencilDescriptor.depthFailureOperation = igl::StencilOperation::Zero;
        frontFaceStencilDescriptor.depthStencilPassOperation = \
        igl::StencilOperation::Zero;
        
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
        
        if (tex == nullptr) return 0;
        
        igl::TextureFormat pixelFormat = igl::TextureFormat::RGBA_UNorm8;
        if (type == NVG_TEXTURE_ALPHA) {
            pixelFormat = igl::TextureFormat::R_UNorm8;
        }
        
        tex->type = type;
        tex->flags = imageFlags;
        
        //(imageFlags & NVG_IMAGE_GENERATE_MIPMAPS ? true : false)
        
        igl::TextureDesc textureDescriptor = igl::TextureDesc::new2D(pixelFormat,width,height, igl::TextureDesc::TextureUsageBits::Sampled);
        
//        textureDescriptor.usage = MTLTextureUsageShaderRead
//        | MTLTextureUsageRenderTarget
//        | MTLTextureUsageShaderWrite;
        
#if TARGET_OS_SIMULATOR
        textureDescriptor.storageMode = MTLStorageModePrivate;
#endif  // TARGET_OS_SIMULATOR
        tex->tex = device->createTexture(textureDescriptor, NULL);
        
        if (data != NULL) {
            int bytesPerRow = 0;
            if (tex->type == NVG_TEXTURE_RGBA) {
                bytesPerRow = width * 4;
            } else {
                bytesPerRow = width;
            }
            
            tex->tex->upload(igl::TextureRangeDesc::new2D(0, 0, width, height), data);
            
#if 0 //没用了
            if (textureDescriptor.storageMode == MTLStorageModePrivate) {
                const NSUInteger kBufferSize = bytesPerRow * height;
                std::shared_ptr<igl::IBuffer> buffer = [_metalLayer.device
                                                        newBufferWithLength:kBufferSize
                                                        options:MTLResourceStorageModeShared];
                memcpy([buffer contents], data, kBufferSize);
                
                id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
                id<MTLBlitCommandEncoder> blitCommandEncoder = [commandBuffer
                                                                blitCommandEncoder];
                [blitCommandEncoder copyFromBuffer:buffer
                                      sourceOffset:0
                                 sourceBytesPerRow:bytesPerRow
                               sourceBytesPerImage:kBufferSize
                                        sourceSize:MTLSizeMake(width, height, 1)
                                         toTexture:tex->tex
                                  destinationSlice:0
                                  destinationLevel:0
                                 destinationOrigin:MTLOriginMake(0, 0, 0)];
                
                [blitCommandEncoder endEncoding];
                [commandBuffer commit];
                [commandBuffer waitUntilCompleted];
            } else {
                [tex->tex replaceRegion:MTLRegionMake2D(0, 0, width, height)
                            mipmapLevel:0
                              withBytes:data
                            bytesPerRow:bytesPerRow];
            }
            
            if (imageFlags & NVG_IMAGE_GENERATE_MIPMAPS) {
                id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
                id<MTLBlitCommandEncoder> encoder = [commandBuffer blitCommandEncoder];
                [encoder generateMipmapsForTexture:tex->tex];
                [encoder endEncoding];
                [commandBuffer commit];
                [commandBuffer waitUntilCompleted];
            }
#endif
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

    
    void renderDelete(){
        for (MNVGbuffers* buffers : _cbuffers) {
            buffers->commandBuffer = nullptr;
            buffers->viewSizeBuffer = nullptr;
            buffers->stencilTexture = nullptr;
            buffers->indexBuffer = nullptr;
            buffers->vertBuffer = nullptr;
            buffers->uniformBuffer = nullptr;
//            free(buffers->calls);
        }
        
        for (MNVGtexture* texture : _textures) {
            texture->tex = nullptr;
            texture->sampler = nullptr;
        }
        
        free(_blendFunc);
        _commandQueue = nullptr;
        _renderEncoder = nullptr;
        _textures.clear();
        _cbuffers.clear();
        _defaultStencilState = nullptr;
        _fillShapeStencilState = nullptr;
        _fillAntiAliasStencilState = nullptr;
        _strokeShapeStencilState = nullptr;
        _strokeAntiAliasStencilState = nullptr;
        _strokeClearStencilState = nullptr;
//        _fragmentFunction = nullptr;
//        _vertexFunction = nullptr;
        _pipelineState = nullptr;
        _stencilOnlyPipelineState = nullptr;
        _pseudoSampler = nullptr;
        _pseudoTexture = nullptr;
//        _vertexDescriptor = nullptr;
        device = nullptr;
    }
    
    int renderDeleteTexture(int image) {
        for (MNVGtexture* texture : _textures) {
            if (texture->_id == image) {
                if (texture->tex != nullptr &&
                    (texture->flags & NVG_IMAGE_NODELETE) == 0) {
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
    
    void renderFillWithPaint(NVGpaint*paint,
    NVGcompositeOperationState compositeOperation,
    NVGscissor* scissor,
    float fringe,
    const float* bounds,
    const NVGpath* paths,
    int npaths) {
        MNVGcall* call = allocCall();
        NVGvertex* quad;
        
        if (call == NULL) return;
        
        call->type = MNVG_FILL;
        call->triangleCount = 4;
        call->image = paint->image;
        call->blendFunc = blendCompositeOperation(compositeOperation);
        
        if (npaths == 1 && paths[0].convex) {
            call->type = MNVG_CONVEXFILL;
            call->triangleCount = 0;  // Bounding box fill quad not needed for convex fill
        }
        
        // Allocate vertices for all the paths.
        int indexCount, strokeCount = 0;
        int maxverts = mtlnvg__maxVertCount(paths, npaths, &indexCount, &strokeCount)
        + call->triangleCount;
        int vertOffset = allocVerts(maxverts);
        if (vertOffset == -1) {
            // We get here if call alloc was ok, but something else is not.
            // Roll back the last call to prevent drawing it.
            if (_buffers->ncalls > 0) _buffers->ncalls--;
            return;
        }
        
        int indexOffset = allocIndexes(indexCount);
        if (indexOffset == -1) {
            // We get here if call alloc was ok, but something else is not.
            // Roll back the last call to prevent drawing it.
            if (_buffers->ncalls > 0) _buffers->ncalls--;
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
                memcpy(&_buffers->verts[vertOffset], path->fill,
                       sizeof(NVGvertex) * path->nfill);
                
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
            if (_buffers->ncalls > 0) _buffers->ncalls--;
            return;
        }
        convertPaintForFrag(fragUniformAtIndex(call->uniformOffset),
                            paint,
                          scissor,
                            fringe,
                           fringe,
                        -1.0f);
    }
    
    void renderFlush(){
        // Cancelled if the drawable is invisible.
        if (viewPortSize.x == 0 || viewPortSize.y == 0) {
            renderCancel();
            return;
        }
        
        igl::CommandBufferDesc commandBufferDesc;
        commandBufferDesc.debugName = "iglNanoVG";
        std::shared_ptr<igl::ICommandBuffer> commandBuffer = _commandQueue->createCommandBuffer(commandBufferDesc, NULL);
//        std::shared_ptr<igl::ITexture> colorTexture = nullptr;;
        vector_uint2 textureSize;
        
        _buffers->commandBuffer = commandBuffer;
        __block MNVGbuffers* buffers = _buffers;
//        [commandBuffer enqueue];
//        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
//            buffers.isBusy = false;
//            buffers.commandBuffer = nullptr;
//            buffers.image = 0;
//            buffers.nindexes = 0;
//            buffers.nverts = 0;
//            buffers.ncalls = 0;
//            buffers.nuniforms = 0;
//            dispatch_semaphore_signal(self.semaphore);
//        }];
        
        if (s_framebuffer == NULL ||
            nvgInternalParams(s_framebuffer->ctx)->userPtr != ( void*)this) {
            textureSize = viewPortSize;
        } else {  // renders in framebuffer
            buffers->image = s_framebuffer->image;
            MNVGtexture* tex = findTexture(s_framebuffer->image);
            colorTexture = tex->tex;
            textureSize = (vector_uint2){(uint)colorTexture->getSize().width,
                (uint)colorTexture->getSize().height};
        }
        if (textureSize.x == 0 || textureSize.y == 0) return;
        updateStencilTextureToSize(&textureSize);
        
        _buffers->uploadToGpu();
        
//        id<CAMetalDrawable> drawable = nullptr;
//        if (colorTexture == nullptr) {
            //todo:
//            drawable = _metalLayer.nextDrawable;
//            colorTexture = drawable.texture;
//        }
        _renderEncoder = renderCommandEncoderWithColorTexture();
        if (_renderEncoder == nullptr) {
            return;
        }
        
        MNVGcall* call = &buffers->calls[0];
        for (int i = buffers->ncalls; i--; ++call) {
            MNVGblend* blend = &call->blendFunc;
            updateRenderPipelineStatesForBlend(blend,colorTexture->getProperties().format);
            
            if (call->type == MNVG_FILL){
                _renderEncoder->pushDebugGroupLabel("fill");
                fill(call);
            }
            else if (call->type == MNVG_CONVEXFILL){
                _renderEncoder->pushDebugGroupLabel("convexFill");
                convexFill(call);
            }
            else if (call->type == MNVG_STROKE){
                _renderEncoder->pushDebugGroupLabel("stroke");
                stroke(call);
            }
            else if (call->type == MNVG_TRIANGLES){
                _renderEncoder->pushDebugGroupLabel("triangles");
                triangles(call);
            }
            
            _renderEncoder->popDebugGroupLabel();
        }
        
        _renderEncoder->endEncoding();
        
        commandBuffer->present(colorTexture);
        _commandQueue->submit(*commandBuffer, true);
        
        {
            buffers->isBusy = false;
            buffers->commandBuffer = nullptr;
            buffers->image = 0;
            buffers->nindexes = 0;
            buffers->nverts = 0;
            buffers->ncalls = 0;
            buffers->nuniforms = 0;
            //            dispatch_semaphore_signal(self.semaphore);
        }
        
        _renderEncoder = nullptr;
        
//        if (drawable && !_metalLayer.presentsWithTransaction) {
//            [_buffers->commandBuffer presentDrawable:drawable];
//        }
//        
//#if TARGET_OS_OSX
//        // Makes mnvgReadPixels() work as expected on Mac.
//        if (s_framebuffer != NULL) {
//            id<MTLBlitCommandEncoder> blitCommandEncoder = [_buffers->commandBuffer
//                                                            blitCommandEncoder];
//            [blitCommandEncoder synchronizeResource:colorTexture];
//            [blitCommandEncoder endEncoding];
//        }
//#endif  // TARGET_OS_OSX
//        
//        [_buffers->commandBuffer commit];
//        
//        if (drawable && _metalLayer.presentsWithTransaction) {
//            [_buffers->commandBuffer waitUntilScheduled];
//            [drawable present];
//        }
    }
    
    int renderGetTextureSizeForImage(int image,
    int*width,
    int*height) {
        MNVGtexture* tex = findTexture(image);
        if (tex == nullptr) return 0;
        *width = (int)tex->tex->getSize().width;
        *height = (int)tex->tex->getSize().height;
        return 1;
    }
    
    void renderStrokeWithPaint(NVGpaint*paint,
    NVGcompositeOperationState compositeOperation,
    NVGscissor* scissor,
    float fringe,
    float strokeWidth,
    const NVGpath* paths,
    int npaths){
        MNVGcall* call = allocCall();
        
        if (call == NULL) return;
        
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
            if (_buffers->ncalls > 0) _buffers->ncalls--;
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
                if (_buffers->ncalls > 0) _buffers->ncalls--;
                return;
            }
            convertPaintForFrag(fragUniformAtIndex(call->uniformOffset),
                                paint,
                              scissor,
                                strokeWidth,
                               fringe,
                            -1.0f);
            MNVGfragUniforms* frag = fragUniformAtIndex(call->uniformOffset + _fragSize);
            convertPaintForFrag(frag,
                                paint,
                              scissor,
                                strokeWidth,
                               fringe,
                            (1.0f - 0.5f / 255.0f));
        } else {
            // Fill shader
            call->uniformOffset = allocFragUniforms(1);
            if (call->uniformOffset == -1) {
                // We get here if call alloc was ok, but something else is not.
                // Roll back the last call to prevent drawing it.
                if (_buffers->ncalls > 0) _buffers->ncalls--;
                return;
            }
            convertPaintForFrag(fragUniformAtIndex(call->uniformOffset),
                                paint,
                              scissor,
                                strokeWidth,
                               fringe,
                            -1.0f);
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
        
        if (call == NULL) return;
        
        call->type = MNVG_TRIANGLES;
        call->image = paint->image;
        call->blendFunc = blendCompositeOperation(compositeOperation);
        
        // Allocate vertices for all the paths.
        call->triangleOffset = allocVerts(nverts);
        if (call->triangleOffset == -1) {
            // We get here if call alloc was ok, but something else is not.
            // Roll back the last call to prevent drawing it.
            if (_buffers->ncalls > 0) _buffers->ncalls--;
            return;
        }
        call->triangleCount = nverts;
        
        memcpy(&_buffers->verts[call->triangleOffset], verts,
               sizeof(NVGvertex) * nverts);
        
        // Fill shader
        call->uniformOffset = allocFragUniforms(1);
        if (call->uniformOffset == -1) {
            // We get here if call alloc was ok, but something else is not.
            // Roll back the last call to prevent drawing it.
            if (_buffers->ncalls > 0) _buffers->ncalls--;
            return;
        }
        frag = fragUniformAtIndex(call->uniformOffset);
        convertPaintForFrag(frag,
                            paint,
                          scissor,
                            1.0f,
                           fringe,
                        -1.0f);
        frag->type = MNVG_SHADER_IMG;
    }
    
    int renderUpdateTextureWithImage(int image,
    int x,
    int y,
    int width,
    int height,
    const unsigned char* data) {
        MNVGtexture* tex = findTexture(image);
        
        if (tex == nullptr) return 0;
        
        unsigned char* bytes = NULL;
        int bytesPerRow = 0;
        if (tex->type == NVG_TEXTURE_RGBA) {
            bytesPerRow = tex->tex->getSize().width * 4;
            bytes = (unsigned char*)data + y * bytesPerRow + x * 4;
        } else {
            bytesPerRow = tex->tex->getSize().width;
            bytes = (unsigned char*)data + y * bytesPerRow + x;
        }
        
#if TARGET_OS_SIMULATOR
        const NSUInteger kBufferSize = bytesPerRow * height;
        std::shared_ptr<igl::IBuffer> buffer = [_metalLayer.device
                                                newBufferWithLength:kBufferSize
                                                options:MTLResourceStorageModeShared];
        memcpy([buffer contents], bytes, kBufferSize);
        
        id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
        id<MTLBlitCommandEncoder> blitCommandEncoder = [commandBuffer
                                                        blitCommandEncoder];
        [blitCommandEncoder copyFromBuffer:buffer
                              sourceOffset:0
                         sourceBytesPerRow:bytesPerRow
                       sourceBytesPerImage:kBufferSize
                                sourceSize:MTLSizeMake(width, height, 1)
                                 toTexture:tex->tex
                          destinationSlice:0
                          destinationLevel:0
                         destinationOrigin:MTLOriginMake(x, y, 0)];
        
        [blitCommandEncoder endEncoding];
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];
#else
        std::shared_ptr<igl::ITexture> texture = tex->tex;
//        [texture replaceRegion:MTLRegionMake2D(x, y, width, height)
//                   mipmapLevel:0
//                     withBytes:bytes
//                   bytesPerRow:bytesPerRow];
        
        igl::TextureRangeDesc desc = igl::TextureRangeDesc::new2D(x, y, width, height);
        texture->upload(desc, bytes, bytesPerRow);
#endif
        
        return 1;
    }
    
    void renderViewportWithWidth(float width,
    float height,
    float devicePixelRatio) {
        viewPortSize = (vector_uint2){(unsigned int)(width * devicePixelRatio),
            (unsigned int)(height * devicePixelRatio)};
        
        //dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER);
        for (MNVGbuffers* buffers : _cbuffers) {
            if (!buffers->isBusy) {
                buffers->isBusy = true;
                _buffers = buffers;
                break;
            }
        }
        
        // Initializes view size buffer for vertex function.
        if (_buffers->viewSizeBuffer == nullptr) {
//            _buffers->viewSizeBuffer = [_metalLayer.device
//                                       newBufferWithLength:sizeof(vector_float2)
//                                       options:kMetalBufferOptions];
            igl::BufferDesc desc(igl::BufferDesc::BufferTypeBits::Uniform, NULL, sizeof(vector_float2));
            _buffers->viewSizeBuffer = device->createBuffer(desc, NULL);
        }
        float viewSize[2] ;//= (float*)[_buffers->viewSizeBuffer contents];
        viewSize[0] = width;
        viewSize[1] = height;
        _buffers->viewSizeBuffer->upload(viewSize, igl::BufferRange(sizeof(vector_float2)));
    }
    
    void setUniforms(int uniformOffset ,int image) {
        //[_renderEncoder setFragmentBufferOffset:uniformOffset atIndex:0];
        _renderEncoder->bindBuffer(2, _buffers->uniformBuffer.get(), uniformOffset);
        
        MNVGtexture* tex = (image == 0 ? nullptr : findTexture(image));
        if (tex != nullptr) {
            _renderEncoder->bindTexture(0, igl::BindTarget::kFragment, tex->tex.get());
            _renderEncoder->bindSamplerState(0, igl::BindTarget::kFragment, tex->sampler.get());
        } else {
            _renderEncoder->bindTexture(0, igl::BindTarget::kFragment, _pseudoTexture.get());
            _renderEncoder->bindSamplerState(0,  igl::BindTarget::kFragment, _pseudoSampler.get());
        }
    }
    
    void stroke(MNVGcall* call) {
        if (call->strokeCount <= 0) {
            return;
        }
        
        if (_flags & NVG_STENCIL_STROKES) {
            // Fills the stroke base without overlap.
            setUniforms(call->uniformOffset + _fragSize, call->image);
            _renderEncoder->bindDepthStencilState(_strokeShapeStencilState);
            _renderEncoder->bindRenderPipelineState(_pipelineStateTriangleStrip);
//            _renderEncoder->drawPrimitives:MTLPrimitiveTypeTriangleStrip
//                               vertexStart:call->strokeOffset
//                               vertexCount:call->strokeCount];
            _renderEncoder->draw(call->strokeCount, 1, call->strokeOffset);
            
            // Draws anti-aliased fragments.
            setUniforms(call->uniformOffset ,call->image);
            _renderEncoder->bindDepthStencilState(_strokeAntiAliasStencilState);
//            _renderEncoder->drawPrimitives:MTLPrimitiveTypeTriangleStrip
//                               vertexStart:call->strokeOffset
//                               vertexCount:call->strokeCount];
            _renderEncoder->draw(call->strokeCount, 1, call->strokeOffset);
            
            // Clears stencil buffer.
            _renderEncoder->bindDepthStencilState(_strokeClearStencilState);
            _renderEncoder->bindRenderPipelineState(_stencilOnlyPipelineStateTriangleStrip);
//            _renderEncoder->drawPrimitives:MTLPrimitiveTypeTriangleStrip
//                               vertexStart:call->strokeOffset
//                               vertexCount:call->strokeCount];
            _renderEncoder->draw(call->strokeCount, 1, call->strokeOffset);
            _renderEncoder->bindDepthStencilState(_defaultStencilState);
        } else {
            // Draws strokes.
            setUniforms(call->uniformOffset ,call->image);
            _renderEncoder->bindRenderPipelineState(_pipelineStateTriangleStrip);
//            _renderEncoder->drawPrimitives:MTLPrimitiveTypeTriangleStrip
//                               vertexStart:call->strokeOffset
//                               vertexCount:call->strokeCount];
            _renderEncoder->draw(call->strokeCount, 1, call->strokeOffset);
        }
    }
    
    void triangles(MNVGcall*call) {
        setUniforms(call->uniformOffset, call->image);
        _renderEncoder->bindRenderPipelineState(_pipelineState);
//        _renderEncoder->drawPrimitives:MTLPrimitiveTypeTriangle
//                           vertexStart:call->triangleOffset
//                           vertexCount:call->triangleCount];
        
        _renderEncoder->draw(call->triangleCount, 1, call->triangleOffset);
    }
    
    void updateRenderPipelineStatesForBlend(MNVGblend* blend,
    igl::TextureFormat pixelFormat ){
        if (_pipelineState != nullptr &&
            _stencilOnlyPipelineState != nullptr &&
            piplelinePixelFormat == pixelFormat &&
            _blendFunc->srcRGB == blend->srcRGB &&
            _blendFunc->dstRGB == blend->dstRGB &&
            _blendFunc->srcAlpha == blend->srcAlpha &&
            _blendFunc->dstAlpha == blend->dstAlpha) {
            return;
        }
        
        igl::Result result;
        
        igl::RenderPipelineDesc pipelineStateDescriptor;
        
        pipelineStateDescriptor.targetDesc.colorAttachments.resize(1);
        igl::RenderPipelineDesc::TargetDesc::ColorAttachment & colorAttachmentDescriptor = pipelineStateDescriptor.targetDesc.colorAttachments[0];
        colorAttachmentDescriptor.textureFormat = pixelFormat;
        pipelineStateDescriptor.targetDesc.stencilAttachmentFormat = kStencilFormat;
//        pipelineStateDescriptor.fragmentFunction = _fragmentFunction;
//        pipelineStateDescriptor.vertexFunction = _vertexFunction;
        pipelineStateDescriptor.shaderStages = igl::ShaderStagesCreator::fromRenderModules(*device, _vertexFunction, _fragmentFunction, &result);
        IGL_DEBUG_ASSERT(result.isOk());
        
        pipelineStateDescriptor.vertexInputState = device->createVertexInputState(_vertexDescriptor, &result);
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
        _pipelineState = device->createRenderPipeline(pipelineStateDescriptor,&result);
        
        pipelineStateDescriptor.topology = igl::PrimitiveType::TriangleStrip;
        pipelineStateDescriptor.cullMode = igl::CullMode::Back;
        pipelineStateDescriptor.debugName = igl::genNameHandle("TriangleStripe_CullBack");
        _pipelineStateTriangleStrip = device->createRenderPipeline(pipelineStateDescriptor,&result);
        IGL_DEBUG_ASSERT(result.isOk());
//        checkError:error withMessage:"init pipeline state"];
        
        pipelineStateDescriptor.shaderStages = igl::ShaderStagesCreator::fromRenderModules(*device, _vertexFunction, nullptr, &result);
        IGL_DEBUG_ASSERT(result.isOk());
        colorAttachmentDescriptor.colorWriteMask = igl::ColorWriteBits::ColorWriteBitsDisabled;
        pipelineStateDescriptor.cullMode = igl::CullMode::Disabled;
        pipelineStateDescriptor.topology = igl::PrimitiveType::Triangle;
        pipelineStateDescriptor.debugName = igl::genNameHandle("stencilOnlyPipelineState");
        _stencilOnlyPipelineState = device->createRenderPipeline(pipelineStateDescriptor,&result);
        IGL_DEBUG_ASSERT(result.isOk());
        
        pipelineStateDescriptor.debugName = igl::genNameHandle("stencilOnlyPipelineStateTriangleStrip");
        pipelineStateDescriptor.topology = igl::PrimitiveType::TriangleStrip;
        _stencilOnlyPipelineStateTriangleStrip = device->createRenderPipeline(pipelineStateDescriptor,&result);
        IGL_DEBUG_ASSERT(result.isOk());
//        [self checkError:error withMessage:"init pipeline stencil only state"];
        
        piplelinePixelFormat = pixelFormat;
    }
    
    // Re-creates stencil texture whenever the specified size is bigger.
    void updateStencilTextureToSize(vector_uint2* size) {
        if (_buffers->stencilTexture != nullptr &&
            (_buffers->stencilTexture->getSize().width < size->x ||
             _buffers->stencilTexture->getSize().height < size->y)) {
            _buffers->stencilTexture = nullptr;
        }
        if (_buffers->stencilTexture == nullptr) {
            igl::TextureDesc stencilTextureDescriptor = igl::TextureDesc::new2D(kStencilFormat,size->x,size->y ,
                                                                                igl::TextureDesc::TextureUsageBits::Attachment | igl::TextureDesc::TextureUsageBits::Sampled);
#if TARGET_OS_SIMULATOR
            stencilTextureDescriptor.storage = igl::ResourceStorage::Private;
#endif
            _buffers->stencilTexture = device->createTexture(stencilTextureDescriptor, NULL);
            
//            stencilTextureDescriptor.usage = MTLTextureUsageRenderTarget;
//#if TARGET_OS_OSX || TARGET_OS_SIMULATOR || TARGET_OS_MACCATALYST
//            stencilTextureDescriptor.storageMode = MTLStorageModePrivate;
//#endif  // TARGET_OS_OSX || TARGET_OS_SIMULATOR || TARGET_OS_MACCATALYST
            
        }
    }
};

static void mtlnvg__renderCancel(void* uptr) {
  MNVGcontext* mtl = ( MNVGcontext*)uptr;
  mtl->renderCancel();
}

static int mtlnvg__renderCreateTexture(void* uptr, int type, int width,
                                       int height, int imageFlags,
                                       const unsigned char* data) {
  MNVGcontext* mtl = ( MNVGcontext*)uptr;
  return mtl->renderCreateTextureWithType(type,
                                    width,
                                   height,
                               imageFlags,
                                     data);
}

static int mtlnvg__renderCreate(void* uptr) {
  MNVGcontext* mtl = ( MNVGcontext*)uptr;
  return mtl->renderCreate();
}

static void mtlnvg__renderDelete(void* uptr) {
  MNVGcontext* mtl = ( MNVGcontext*)uptr;
  mtl->renderDelete();
}

static int mtlnvg__renderDeleteTexture(void* uptr, int image) {
  MNVGcontext* mtl = ( MNVGcontext*)uptr;
  return mtl->renderDeleteTexture(image);
}

static void mtlnvg__renderFill(void* uptr, NVGpaint* paint,
                              NVGcompositeOperationState compositeOperation,
                              NVGscissor* scissor, float fringe,
                              const float* bounds, const NVGpath* paths,
                              int npaths) {
  MNVGcontext* mtl = ( MNVGcontext*)uptr;
  mtl->renderFillWithPaint(paint,
        compositeOperation,
                   scissor,
                    fringe,
                    bounds,
                     paths,
                    npaths);
}

static void mtlnvg__renderFlush(void* uptr) {
  MNVGcontext* mtl = ( MNVGcontext*)uptr;
  mtl->renderFlush();
}

static int mtlnvg__renderGetTextureSize(void* uptr, int image, int* w, int* h) {
  MNVGcontext* mtl = (MNVGcontext*)uptr;
  return mtl->renderGetTextureSizeForImage(image ,w ,h);
}

static void mtlnvg__renderStroke(void* uptr, NVGpaint* paint,
                                 NVGcompositeOperationState compositeOperation,
                                 NVGscissor* scissor, float fringe,
                                 float strokeWidth, const NVGpath* paths,
                                 int npaths) {
  MNVGcontext* mtl = ( MNVGcontext*)uptr;
  mtl->renderStrokeWithPaint(paint,
          compositeOperation,
                     scissor,
                      fringe,
                 strokeWidth,
                       paths,
                      npaths);
}

static void mtlnvg__renderTriangles(
    void* uptr, NVGpaint* paint, NVGcompositeOperationState compositeOperation,
    NVGscissor* scissor, const NVGvertex* verts, int nverts, float fringe) {
  MNVGcontext* mtl = ( MNVGcontext*)uptr;
  mtl->renderTrianglesWithPaint(paint,
             compositeOperation,
                        scissor,
                          verts,
                         nverts,
                         fringe);
}

static int mtlnvg__renderUpdateTexture(void* uptr, int image, int x, int y,
                                       int w, int h,
                                       const unsigned char* data) {
  MNVGcontext* mtl = ( MNVGcontext*)uptr;
  return mtl->renderUpdateTextureWithImage(image,
                                         x,
                                         y,
                                     w,
                                    h,
                                      data);
}

static void mtlnvg__renderViewport(void* uptr, float width, float height,
                                   float devicePixelRatio) {
  MNVGcontext* mtl = ( MNVGcontext*)uptr;
  mtl->renderViewportWithWidth(width,
                        height,
              devicePixelRatio);
}

void nvgSetColorTexture(NVGcontext* ctx, std::shared_ptr<igl::ITexture> color, std::shared_ptr<igl::ITexture> stencil){
    MNVGcontext* mtl = (MNVGcontext*)nvgInternalParams(ctx)->userPtr;
    mtl->colorTexture = color;
    mtl->stencilTexture = stencil;
}

void mnvgClearWithColor(NVGcontext* ctx, NVGcolor color) {
  MNVGcontext* mtl = (MNVGcontext*)nvgInternalParams(ctx)->userPtr;
  float alpha = (float)color.a;
  mtl->clearColor = igl::Color((float)color.r * alpha,
                                     (float)color.g * alpha,
                                     (float)color.b * alpha,
                                     (float)color.a);
  mtl->clearBufferOnFlush = true;
}

void* mnvgCommandQueue(NVGcontext* ctx) {
  MNVGcontext* mtl = ( MNVGcontext*)nvgInternalParams(ctx)->userPtr;
  return (void*)mtl->_commandQueue.get();
}

int mnvgCreateImageFromHandle(NVGcontext* ctx, void* textureId, int imageFlags) {
    return 0;
#if 0
  MNVGcontext* mtl = (MNVGcontext*)nvgInternalParams(ctx)->userPtr;
  MNVGtexture* tex = mtl->allocTexture();

  if (tex == NULL) return 0;

  tex->type = NVG_TEXTURE_RGBA;
  tex->tex = (std::shared_ptr<igl::ITexture>)textureId;
  tex->flags = imageFlags;

  return tex->_id;
#endif
}

void* mnvgDevice(NVGcontext* ctx) {
  MNVGcontext* mtl = ( MNVGcontext*)nvgInternalParams(ctx)->userPtr;
  return ( void*)mtl->device;
}

void* mnvgImageHandle(NVGcontext* ctx, int image) {
  MNVGcontext* mtl = ( MNVGcontext*)nvgInternalParams(ctx)->userPtr;
  MNVGtexture* tex = mtl->findTexture(image);
  if (tex == nullptr) return NULL;

  // Makes sure the command execution for the image has been done.
  for (MNVGbuffers* buffers : mtl->_cbuffers) {
    if (buffers->isBusy && buffers->image == image && buffers->commandBuffer) {
      std::shared_ptr<igl::ICommandBuffer> commandBuffer = buffers->commandBuffer;
      commandBuffer->waitUntilCompleted();
      break;
    }
  }

  return ( void*)tex->tex.get();
}

void mnvgReadPixels(NVGcontext* ctx, int image, int x, int y, int width,
                    int height, void* data) {
  MNVGcontext* mtl = ( MNVGcontext*)nvgInternalParams(ctx)->userPtr;
  MNVGtexture* tex = mtl->findTexture(image);
  if (tex == nullptr) return;

  int bytesPerRow = 0;
  if (tex->type == NVG_TEXTURE_RGBA) {
      bytesPerRow = tex->tex->getSize().width * 4;
  } else {
      bytesPerRow = tex->tex->getSize().width;
  }

  // Makes sure the command execution for the image has been done.
  for (MNVGbuffers* buffers : mtl->_cbuffers) {
    if (buffers->isBusy && buffers->image == image && buffers->commandBuffer) {
        std::shared_ptr<igl::ICommandBuffer> commandBuffer = buffers->commandBuffer;
      commandBuffer->waitUntilCompleted();
      break;
    }
  }

#if TARGET_OS_SIMULATOR
  CAMetalLayer* metalLayer = mtl.metalLayer;
  const NSUInteger kBufferSize = bytesPerRow * height;
  std::shared_ptr<igl::IBuffer> buffer = [metalLayer.device
      newBufferWithLength:kBufferSize
      options:MTLResourceStorageModeShared];

  id<MTLCommandBuffer> commandBuffer = [mtl.commandQueue commandBuffer];
  id<MTLBlitCommandEncoder> blitCommandEncoder = [commandBuffer
      blitCommandEncoder];
  [blitCommandEncoder copyFromTexture:tex->tex
      sourceSlice:0
      sourceLevel:0
      sourceOrigin:MTLOriginMake(x, y, 0)
      sourceSize:MTLSizeMake(width, height, 1)
      toBuffer:buffer
      destinationOffset:0
      destinationBytesPerRow:bytesPerRow
      destinationBytesPerImage:kBufferSize];

  [blitCommandEncoder endEncoding];
  [commandBuffer commit];
  [commandBuffer waitUntilCompleted];
  memcpy(data, [buffer contents], kBufferSize);
#else
    //todo:
//  [tex->tex getBytes:data
//         bytesPerRow:bytesPerRow
//          fromRegion:MTLRegionMake2D(x, y, width, height)
//         mipmapLevel:0];
#endif  // TARGET_OS_SIMULATOR
}

enum MNVGTarget mnvgTarget() {
#if TARGET_OS_SIMULATOR
  return MNVG_SIMULATOR;
#elif TARGET_OS_IOS
  return MNVG_IOS;
#elif TARGET_OS_OSX
  return MNVG_MACOS;
#elif TARGET_OS_TV
  return MNVG_TVOS;
#else
  return MNVG_UNKfalseWN;
#endif
}

NVGcontext* nvgCreateMTL(igl::IDevice * device, int flags) {
#ifdef MNVG_INVALID_TARGET
  printf("Metal is only supported on iOS, macOS, and tvOS.\n");
  return NULL;
#endif  // MNVG_INVALID_TARGET

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
#if TARGET_OS_OSX || TARGET_OS_SIMULATOR
  mtl->_fragSize = 256;
#else
  mtl->_fragSize = sizeof(MNVGfragUniforms);
#endif  // TARGET_OS_OSX
  mtl->_indexSize = 4;  // MTLIndexTypeUInt32
  mtl->device = device;

  ctx = nvgCreateInternal(&params);
  if (ctx == NULL) goto error;
  return ctx;

error:
  // 'mtl' is freed by nvgDeleteInternal.
  if (ctx != NULL) nvgDeleteInternal(ctx);
  return NULL;
}