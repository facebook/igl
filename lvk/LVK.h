/*
 * LightweightVK
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include <minilog/minilog.h>

// clang-format off
#if defined(LVK_WITH_TRACY)
  #include "tracy/Tracy.hpp"
  // predefined RGB colors for "heavy" point-of-interest operations
  #define IGL_PROFILER_COLOR_WAIT 0xff0000
  #define IGL_PROFILER_COLOR_SUBMIT 0x0000ff
  #define IGL_PROFILER_COLOR_PRESENT 0x00ff00
  #define IGL_PROFILER_COLOR_CREATE 0xff6600
  #define IGL_PROFILER_COLOR_DESTROY 0xffa500
  #define IGL_PROFILER_COLOR_TRANSITION 0xffffff
  //
  #define IGL_PROFILER_FUNCTION() ZoneScoped
  #define IGL_PROFILER_FUNCTION_COLOR(color) ZoneScopedC(color)
  #define IGL_PROFILER_ZONE(name, color) \
    {                                    \
      ZoneScopedC(color);                \
      ZoneName(name, strlen(name))
  #define IGL_PROFILER_ZONE_END() }
  #define IGL_PROFILER_THREAD(name) tracy::SetThreadName(name)
  #define IGL_PROFILER_FRAME(name) FrameMarkNamed(name)
#else
  #define IGL_PROFILER_FUNCTION()
  #define IGL_PROFILER_FUNCTION_COLOR(color)
  #define IGL_PROFILER_ZONE(name, color) {
  #define IGL_PROFILER_ZONE_END() }
  #define IGL_PROFILER_THREAD(name)
  #define IGL_PROFILER_FRAME(name)
#endif // LVK_WITH_TRACY
// clang-format on

#define LVK_ARRAY_NUM_ELEMENTS(x) (sizeof(x) / sizeof((x)[0]))

namespace lvk {

class IDevice;

bool Assert(bool cond, const char* file, int line, const char* format, ...);

// Non-ref counted handles; based on:
// https://enginearchitecture.realtimerendering.com/downloads/reac2023_modern_mobile_rendering_at_hypehype.pdf
template<typename ObjectType>
class Handle final {
 public:
  Handle() = default;

  bool empty() const {
    return gen_ == 0;
  }
  bool valid() const {
    return gen_ != 0;
  }
  uint32_t index() const {
    return index_;
  }
  uint32_t gen() const {
    return gen_;
  }
  void* indexAsVoid() const {
    return reinterpret_cast<void*>(static_cast<ptrdiff_t>(index_));
  }
  bool operator==(const Handle<ObjectType>& other) const {
    return index_ == other.index_ && gen_ == other.gen_;
  }
  bool operator!=(const Handle<ObjectType>& other) const {
    return index_ != other.index_ || gen_ != other.gen_;
  }
  // allow conditions 'if (handle)'
  explicit operator bool() const {
    return gen_ != 0;
  }

 private:
  Handle(uint32_t index, uint32_t gen) : index_(index), gen_(gen){};

  template<typename U, typename V>
  friend class Pool;

  uint32_t index_ = 0;
  uint32_t gen_ = 0;
};

static_assert(sizeof(Handle<class Foo>) == sizeof(uint64_t));

// specialized with dummy structs for type safety
using ComputePipelineHandle = lvk::Handle<struct ComputePipeline>;
using RenderPipelineHandle = lvk::Handle<struct RenderPipeline>;
using ShaderModuleHandle = lvk::Handle<struct ShaderModule>;
using SamplerHandle = lvk::Handle<struct Sampler>;
using BufferHandle = lvk::Handle<struct Buffer>;
using TextureHandle = lvk::Handle<struct Texture>;

// forward declarations to access incomplete type IDevice
void destroy(lvk::IDevice* device, lvk::ComputePipelineHandle handle);
void destroy(lvk::IDevice* device, lvk::RenderPipelineHandle handle);
void destroy(lvk::IDevice* device, lvk::ShaderModuleHandle handle);
void destroy(lvk::IDevice* device, lvk::SamplerHandle handle);
void destroy(lvk::IDevice* device, lvk::BufferHandle handle);
void destroy(lvk::IDevice* device, lvk::TextureHandle handle);

template<typename HandleType>
class Holder final {
 public:
  Holder() = default;
  Holder(lvk::IDevice* device, HandleType handle) : device_(device), handle_(handle) {}
  ~Holder() {
    lvk::destroy(device_, handle_);
  }
  Holder(const Holder&) = delete;
  Holder(Holder&& other) : device_(other.device_), handle_(other.handle_) {
    other.device_ = nullptr;
    other.handle_ = HandleType{};
  }
  Holder& operator=(const Holder&) = delete;
  Holder& operator=(Holder&& other) {
    std::swap(device_, other.device_);
    std::swap(handle_, other.handle_);
    return *this;
  }
  Holder& operator=(std::nullptr_t) {
    this->reset();
    return *this;
  }

  inline operator HandleType() const {
    return handle_;
  }

  bool valid() const {
    return handle_.valid();
  }

  bool empty() const {
    return handle_.empty();
  }

  void reset() {
    lvk::destroy(device_, handle_);
    device_ = nullptr;
    handle_ = HandleType{};
  }

  HandleType release() {
    device_ = nullptr;
    return std::exchange(handle_, HandleType{});
  }

  uint32_t index() const {
    return handle_.index();
  }
  void* indexAsVoid() const {
    return handle_.indexAsVoid();
  }

 private:
  lvk::IDevice* device_ = nullptr;
  HandleType handle_;
};

} // namespace lvk

// clang-format off
#if !defined(NDEBUG) && (defined(DEBUG) || defined(_DEBUG) || defined(__DEBUG))
  #define IGL_VERIFY(cond) ::lvk::Assert((cond), __FILE__, __LINE__, #cond)
  #define IGL_ASSERT(cond) (void)IGL_VERIFY(cond)
  #define IGL_ASSERT_MSG(cond, format, ...) (void)::lvk::Assert((cond), __FILE__, __LINE__, (format), ##__VA_ARGS__)
#else
  #define IGL_VERIFY(cond) (cond)
  #define IGL_ASSERT(cond)
  #define IGL_ASSERT_MSG(cond, format, ...)
#endif
// clang-format on

namespace lvk {

enum { LVK_MAX_COLOR_ATTACHMENTS = 4 };

enum IndexFormat : uint8_t {
  IndexFormat_UI16,
  IndexFormat_UI32,
};

enum PrimitiveType : uint8_t {
  Primitive_Point,
  Primitive_Line,
  Primitive_LineStrip,
  Primitive_Triangle,
  Primitive_TriangleStrip,
};

enum ColorSpace : uint8_t {
  ColorSpace_SRGB_LINEAR,
  ColorSpace_SRGB_NONLINEAR,
};

enum TextureType : uint8_t {
  TextureType_2D,
  TextureType_3D,
  TextureType_Cube,
};

enum SamplerFilter : uint8_t { SamplerFilter_Nearest = 0, SamplerFilter_Linear };
enum SamplerMip : uint8_t { SamplerMip_Disabled = 0, SamplerMip_Nearest, SamplerMip_Linear };
enum SamplerWrap : uint8_t { SamplerWrap_Repeat = 0, SamplerWrap_Clamp, SamplerWrap_MirrorRepeat };

enum HWDeviceType {
  HWDeviceType_Discrete = 1,
  HWDeviceType_External = 2,
  HWDeviceType_Integrated = 3,
  HWDeviceType_Software = 4,
};

struct HWDeviceDesc {
  enum { IGL_MAX_PHYSICAL_DEVICE_NAME_SIZE = 256 };
  uintptr_t guid = 0;
  HWDeviceType type = HWDeviceType_Software;
  char name[IGL_MAX_PHYSICAL_DEVICE_NAME_SIZE] = {0};
};

enum StorageType {
  StorageType_Device,
  StorageType_HostVisible,
  StorageType_Memoryless
};

enum CullMode : uint8_t { CullMode_None, CullMode_Front, CullMode_Back };
enum WindingMode : uint8_t { WindingMode_CCW, WindingMode_CW };

struct Color {
  float r;
  float g;
  float b;
  float a;

  Color(float r, float g, float b) : r(r), g(g), b(b), a(1.0f) {}
  Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}

  const float* toFloatPtr() const {
    return &r;
  }
};

static_assert(sizeof(Color) == 4 * sizeof(float));

struct Result {
  enum class Code {
    Ok,
    ArgumentOutOfRange,
    RuntimeError,
  };

  Code code = Code::Ok;
  const char* message = "";
  explicit Result() = default;
  explicit Result(Code code, const char* message = "") : code(code), message(message) {}

  bool isOk() const {
    return code == Result::Code::Ok;
  }

  static void setResult(Result* outResult, Code code, const char* message = "") {
    if (outResult) {
      outResult->code = code;
      outResult->message = message;
    }
  }

  static void setResult(Result* outResult, const Result& sourceResult) {
    if (outResult) {
      *outResult = sourceResult;
    }
  }
};

struct ScissorRect {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t width = 0;
  uint32_t height = 0;
};

struct Dimensions {
  uint32_t width = 1;
  uint32_t height = 1;
  uint32_t depth = 1;
};

struct Viewport {
  float x = 0.0f;
  float y = 0.0f;
  float width = 1.0f;
  float height = 1.0f;
  float minDepth = 0.0f;
  float maxDepth = 1.0f;
};

enum CompareOp : uint8_t {
  CompareOp_Never = 0,
  CompareOp_Less,
  CompareOp_Equal,
  CompareOp_LessEqual,
  CompareOp_Greater,
  CompareOp_NotEqual,
  CompareOp_GreaterEqual,
  CompareOp_AlwaysPass
};

enum StencilOp : uint8_t {
  StencilOp_Keep = 0,
  StencilOp_Zero,
  StencilOp_Replace,
  StencilOp_IncrementClamp,
  StencilOp_DecrementClamp,
  StencilOp_Invert,
  StencilOp_IncrementWrap,
  StencilOp_DecrementWrap
};

enum BlendOp : uint8_t {
  BlendOp_Add = 0,
  BlendOp_Subtract,
  BlendOp_ReverseSubtract,
  BlendOp_Min,
  BlendOp_Max
};

enum BlendFactor : uint8_t {
  BlendFactor_Zero = 0,
  BlendFactor_One,
  BlendFactor_SrcColor,
  BlendFactor_OneMinusSrcColor,
  BlendFactor_SrcAlpha,
  BlendFactor_OneMinusSrcAlpha,
  BlendFactor_DstColor,
  BlendFactor_OneMinusDstColor,
  BlendFactor_DstAlpha,
  BlendFactor_OneMinusDstAlpha,
  BlendFactor_SrcAlphaSaturated,
  BlendFactor_BlendColor,
  BlendFactor_OneMinusBlendColor,
  BlendFactor_BlendAlpha,
  BlendFactor_OneMinusBlendAlpha,
  BlendFactor_Src1Color,
  BlendFactor_OneMinusSrc1Color,
  BlendFactor_Src1Alpha,
  BlendFactor_OneMinusSrc1Alpha
};

struct SamplerStateDesc {
  SamplerFilter minFilter = SamplerFilter_Linear;
  SamplerFilter magFilter = SamplerFilter_Linear;
  SamplerMip mipMap = SamplerMip_Disabled;
  SamplerWrap wrapU = SamplerWrap_Repeat;
  SamplerWrap wrapV = SamplerWrap_Repeat;
  SamplerWrap wrapW = SamplerWrap_Repeat;
  CompareOp depthCompareOp = CompareOp_LessEqual;
  uint8_t mipLodMin = 0;
  uint8_t mipLodMax = 15;
  uint8_t maxAnisotropic = 1;
  bool depthCompareEnabled = false;
  const char* debugName = "";
};

struct StencilStateDesc {
  StencilOp stencilFailureOp = StencilOp_Keep;
  StencilOp depthFailureOp = StencilOp_Keep;
  StencilOp depthStencilPassOp = StencilOp_Keep;
  CompareOp stencilCompareOp = CompareOp_AlwaysPass;
  uint32_t readMask = (uint32_t)~0;
  uint32_t writeMask = (uint32_t)~0;
};

struct DepthStencilState {
  CompareOp compareOp = CompareOp_AlwaysPass;
  bool isDepthWriteEnabled = false;
  StencilStateDesc backFaceStencil;
  StencilStateDesc frontFaceStencil;
};

enum PolygonMode : uint8_t {
  PolygonMode_Fill = 0,
  PolygonMode_Line = 1,
};

enum class VertexFormat {
  Invalid = 0,

  Float1,
  Float2,
  Float3,
  Float4,

  Byte1,
  Byte2,
  Byte3,
  Byte4,

  UByte1,
  UByte2,
  UByte3,
  UByte4,

  Short1,
  Short2,
  Short3,
  Short4,

  UShort1,
  UShort2,
  UShort3,
  UShort4,

  Byte2Norm,
  Byte4Norm,

  UByte2Norm,
  UByte4Norm,

  Short2Norm,
  Short4Norm,

  UShort2Norm,
  UShort4Norm,

  Int1,
  Int2,
  Int3,
  Int4,

  UInt1,
  UInt2,
  UInt3,
  UInt4,

  HalfFloat1,
  HalfFloat2,
  HalfFloat3,
  HalfFloat4,

  Int_2_10_10_10_REV,
};

enum Format : uint8_t {
  Format_Invalid = 0,

  Format_R_UN8,
  Format_R_UI16,
  Format_R_UN16,
  Format_R_F16,
  Format_R_F32,

  Format_RG_UN8,
  Format_RG_UI16,
  Format_RG_UN16,
  Format_RG_F16,
  Format_RG_F32,

  Format_RGBA_UN8,
  Format_RGBA_UI32,
  Format_RGBA_F16,
  Format_RGBA_F32,
  Format_RGBA_SRGB8,

  Format_BGRA_UN8,
  Format_BGRA_SRGB8,

  Format_ETC2_RGB8,
  Format_ETC2_SRGB8,
  Format_BC7_RGBA,

  Format_Z_UN16,
  Format_Z_UN24,
  Format_Z_F32,
  Format_Z_UN24_S_UI8,
};

enum LoadOp : uint8_t {
  LoadOp_Invalid = 0,
  LoadOp_DontCare,
  LoadOp_Load,
  LoadOp_Clear,
  LoadOp_None,
};

enum StoreOp : uint8_t {
  StoreOp_DontCare = 0,
  StoreOp_Store,
  StoreOp_MsaaResolve,
  StoreOp_None,
};

enum QueueType : uint8_t {
  QueueType_Compute = 0,
  QueueType_Graphics,
  QueueType_Transfer,
};

enum ShaderStage : uint8_t {
  Stage_Vertex = 0,
  Stage_Geometry,
  Stage_Fragment,
  Stage_Compute,
  kNumShaderStages,
};

struct VertexInput final {
  enum { IGL_VERTEX_ATTRIBUTES_MAX = 16 };
  enum { IGL_VERTEX_BUFFER_MAX = 16 };
  struct VertexAttribute final {
    uint32_t location = 0; // a buffer which contains this attribute stream
    uint32_t binding = 0;
    VertexFormat format = VertexFormat::Invalid; // per-element format
    uintptr_t offset = 0; // an offset where the first element of this attribute stream starts
  } attributes[IGL_VERTEX_ATTRIBUTES_MAX];
  struct VertexInputBinding final {
    uint32_t stride = 0;
  } inputBindings[IGL_VERTEX_BUFFER_MAX];

  uint32_t getNumAttributes() const {
    uint32_t n = 0;
    while (n < IGL_VERTEX_ATTRIBUTES_MAX && attributes[n].format != VertexFormat::Invalid) {
      n++;
    }
    return n;
  }
  uint32_t getNumInputBindings() const {
    uint32_t n = 0;
    while (n < IGL_VERTEX_BUFFER_MAX && inputBindings[n].stride) {
      n++;
    }
    return n;
  }
};

struct ColorAttachment {
  Format format = Format_Invalid;
  bool blendEnabled = false;
  BlendOp rgbBlendOp = BlendOp::BlendOp_Add;
  BlendOp alphaBlendOp = BlendOp::BlendOp_Add;
  BlendFactor srcRGBBlendFactor = BlendFactor_One;
  BlendFactor srcAlphaBlendFactor = BlendFactor_One;
  BlendFactor dstRGBBlendFactor = BlendFactor_Zero;
  BlendFactor dstAlphaBlendFactor = BlendFactor_Zero;
};

struct ShaderModuleDesc {
  ShaderStage stage = Stage_Fragment;
  const char* data = nullptr;
  size_t dataSize = 0; // if `dataSize` is non-zero, interpret `data` as binary shader data
  const char* entryPoint = "main";
  const char* debugName = "";

  ShaderModuleDesc(const char* source, lvk::ShaderStage stage, const char* debugName) :
    stage(stage), data(source), debugName(debugName) {}
  ShaderModuleDesc(const void* data,
                   size_t dataLength,
                   lvk::ShaderStage stage,
                   const char* debugName) :
    stage(stage), data(static_cast<const char*>(data)), dataSize(dataLength), debugName(debugName) {
    IGL_ASSERT(dataSize);
  }
};

struct ShaderStages final {
  ShaderStages() = default;
  ShaderStages(lvk::ShaderModuleHandle vertexModule, lvk::ShaderModuleHandle fragmentModule) {
    modules_[Stage_Vertex] = vertexModule;
    modules_[Stage_Fragment] = fragmentModule;
  }
  ShaderStages(lvk::ShaderModuleHandle vertexModule,
               lvk::ShaderModuleHandle geometryModule,
               lvk::ShaderModuleHandle fragmentModule) {
    modules_[Stage_Vertex] = vertexModule;
    modules_[Stage_Geometry] = geometryModule;
    modules_[Stage_Fragment] = fragmentModule;
  }
  explicit ShaderStages(lvk::ShaderModuleHandle computeModule) {
    modules_[Stage_Compute] = std::move(computeModule);
  }

  lvk::ShaderModuleHandle getModule(ShaderStage stage) const {
    IGL_ASSERT(stage < kNumShaderStages);
    return modules_[stage];
  }

  lvk::ShaderModuleHandle modules_[kNumShaderStages] = {};
};

struct RenderPipelineDesc final {
  lvk::VertexInput vertexInput;
  lvk::ShaderStages shaderStages;

  ColorAttachment color[LVK_MAX_COLOR_ATTACHMENTS] = {};
  Format depthFormat = Format_Invalid;
  Format stencilFormat = Format_Invalid;

  CullMode cullMode = lvk::CullMode_None;
  WindingMode frontFaceWinding = lvk::WindingMode_CCW;
  PolygonMode polygonMode = lvk::PolygonMode_Fill;

  uint32_t samplesCount = 1u;

  const char* debugName = "";

  uint32_t getNumColorAttachments() const {
    uint32_t n = 0;
    while (n < LVK_MAX_COLOR_ATTACHMENTS && color[n].format != Format_Invalid) {
      n++;
    }
    return n;
  }
};

struct ComputePipelineDesc final {
  lvk::ShaderStages shaderStages;
  const char* debugName = "";
};

struct RenderPass final {
  struct AttachmentDesc final {
    LoadOp loadOp = LoadOp_Invalid;
    StoreOp storeOp = StoreOp_Store;
    uint8_t layer = 0;
    uint8_t level = 0;
    Color clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    float clearDepth = 1.0f;
    uint32_t clearStencil = 0;
  };

  AttachmentDesc color[LVK_MAX_COLOR_ATTACHMENTS] = {};
  AttachmentDesc depth = {.loadOp = LoadOp_DontCare, .storeOp = StoreOp_DontCare};
  AttachmentDesc stencil = {.loadOp = LoadOp_Invalid, .storeOp = StoreOp_DontCare};

  uint32_t getNumColorAttachments() const {
    uint32_t n = 0;
    while (n < LVK_MAX_COLOR_ATTACHMENTS && color[n].loadOp != LoadOp_Invalid) {
      n++;
    }
    return n;
  }
};

struct Framebuffer final {
  struct AttachmentDesc {
    TextureHandle texture;
    TextureHandle resolveTexture;
  };

  AttachmentDesc color[LVK_MAX_COLOR_ATTACHMENTS] = {};
  AttachmentDesc depthStencil;

  const char* debugName = "";

  uint32_t getNumColorAttachments() const {
    uint32_t n = 0;
    while (n < LVK_MAX_COLOR_ATTACHMENTS && color[n].texture) {
      n++;
    }
    return n;
  }
};

enum BufferUsageBits : uint8_t {
  BufferUsageBits_Index = 1 << 0,
  BufferUsageBits_Vertex = 1 << 1,
  BufferUsageBits_Uniform = 1 << 2,
  BufferUsageBits_Storage = 1 << 3,
  BufferUsageBits_Indirect = 1 << 4,
};

struct BufferDesc final {
  uint8_t usage = 0;
  StorageType storage = StorageType_HostVisible;
  size_t size = 0;
  const void* data = nullptr;
  const char* debugName = "";
};

struct TextureRangeDesc {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t z = 0;
  Dimensions dimensions = {1, 1, 1};
  uint32_t layer = 0;
  uint32_t numLayers = 1;
  uint32_t mipLevel = 0;
  uint32_t numMipLevels = 1;
};

enum TextureUsageBits : uint8_t {
  TextureUsageBits_Sampled = 1 << 0,
  TextureUsageBits_Storage = 1 << 1,
  TextureUsageBits_Attachment = 1 << 2,
};

struct TextureDesc {
  TextureType type = TextureType_2D;
  Format format = Format_Invalid;

  Dimensions dimensions = {1, 1, 1};
  uint32_t numLayers = 1;
  uint32_t numSamples = 1;
  uint8_t usage = TextureUsageBits_Sampled;
  uint32_t numMipLevels = 1;
  StorageType storage = StorageType_Device;
  const void* data = nullptr;
  const char* debugName = "";
};

struct Dependencies {
  enum { IGL_MAX_SUBMIT_DEPENDENCIES = 4 };
  TextureHandle textures[IGL_MAX_SUBMIT_DEPENDENCIES] = {};
};

class ICommandBuffer {
 public:
  virtual ~ICommandBuffer() = default;

  virtual void transitionToShaderReadOnly(TextureHandle surface) const = 0;

  virtual void cmdPushDebugGroupLabel(const char* label,
                                      const lvk::Color& color = lvk::Color(1, 1, 1, 1)) const = 0;
  virtual void cmdInsertDebugEventLabel(const char* label,
                                        const lvk::Color& color = lvk::Color(1, 1, 1, 1)) const = 0;
  virtual void cmdPopDebugGroupLabel() const = 0;

  virtual void cmdBindComputePipeline(lvk::ComputePipelineHandle handle) = 0;
  virtual void cmdDispatchThreadGroups(const Dimensions& threadgroupCount,
                                       const Dependencies& deps = Dependencies()) = 0;

  virtual void cmdBeginRendering(const lvk::RenderPass& renderPass,
                                 const lvk::Framebuffer& desc) = 0;
  virtual void cmdEndRendering() = 0;

  virtual void cmdBindViewport(const Viewport& viewport) = 0;
  virtual void cmdBindScissorRect(const ScissorRect& rect) = 0;

  virtual void cmdBindRenderPipeline(lvk::RenderPipelineHandle handle) = 0;
  virtual void cmdBindDepthStencilState(const DepthStencilState& state) = 0;

  virtual void cmdBindVertexBuffer(uint32_t index,
                                   BufferHandle buffer,
                                   size_t bufferOffset) = 0;
  virtual void cmdPushConstants(const void* data, size_t size, size_t offset = 0) = 0;
  template<typename Struct>
  void cmdPushConstants(const Struct& data) {
    this->cmdPushConstants(&data, sizeof(Struct), 0);
  }

  virtual void cmdDraw(PrimitiveType primitiveType, size_t vertexStart, size_t vertexCount) = 0;
  virtual void cmdDrawIndexed(PrimitiveType primitiveType,
                              size_t indexCount,
                              IndexFormat indexFormat,
                              BufferHandle indexBuffer,
                              size_t indexBufferOffset) = 0;
  virtual void cmdDrawIndirect(PrimitiveType primitiveType,
                               BufferHandle indirectBuffer,
                               size_t indirectBufferOffset,
                               uint32_t drawCount,
                               uint32_t stride = 0) = 0;
  virtual void cmdDrawIndexedIndirect(PrimitiveType primitiveType,
                                      IndexFormat indexFormat,
                                      BufferHandle indexBuffer,
                                      BufferHandle indirectBuffer,
                                      size_t indirectBufferOffset,
                                      uint32_t drawCount,
                                      uint32_t stride = 0) = 0;

  virtual void cmdSetStencilReferenceValues(uint32_t frontValue, uint32_t backValue) = 0;
  virtual void cmdSetBlendColor(Color color) = 0;
  virtual void cmdSetDepthBias(float depthBias, float slopeScale, float clamp) = 0;
};

class IDevice {
 public:
  virtual ~IDevice() = default;

  virtual ICommandBuffer& acquireCommandBuffer() = 0;

  virtual void submit(const ICommandBuffer& commandBuffer,
                      lvk::QueueType queueType = lvk::QueueType_Graphics,
                      TextureHandle present = {}) = 0;

  virtual Holder<BufferHandle> createBuffer(const BufferDesc& desc,
                                            Result* outResult = nullptr) = 0;
  virtual Holder<SamplerHandle> createSampler(const SamplerStateDesc& desc,
                                              Result* outResult = nullptr) = 0;
  virtual Holder<TextureHandle> createTexture(const TextureDesc& desc,
                                              const char* debugName = nullptr,
                                              Result* outResult = nullptr) = 0;

  virtual Holder<ComputePipelineHandle> createComputePipeline(
      const ComputePipelineDesc& desc,
      Result* outResult = nullptr) = 0;
  virtual Holder<RenderPipelineHandle> createRenderPipeline(
      const RenderPipelineDesc& desc,
      Result* outResult = nullptr) = 0;

  virtual Holder<ShaderModuleHandle> createShaderModule(const ShaderModuleDesc& desc,
                                                        Result* outResult = nullptr) = 0;

  virtual void destroy(ComputePipelineHandle handle) = 0;
  virtual void destroy(RenderPipelineHandle handle) = 0;
  virtual void destroy(ShaderModuleHandle handle) = 0;
  virtual void destroy(SamplerHandle handle) = 0;
  virtual void destroy(BufferHandle handle) = 0;
  virtual void destroy(TextureHandle handle) = 0;
  virtual void destroy(Framebuffer& fb) = 0;

#pragma region Buffer functions
  virtual Result upload(BufferHandle handle, const void* data, size_t size, size_t offset = 0) = 0;
  virtual uint8_t* getMappedPtr(BufferHandle handle) const = 0;
  virtual uint64_t gpuAddress(BufferHandle handle, size_t offset = 0) const = 0;
#pragma endregion

#pragma region Texture functions
  // data[] contains per-layer mip-stacks
  virtual Result upload(TextureHandle handle, const TextureRangeDesc& range, const void* data[]) const = 0;
  virtual Dimensions getDimensions(TextureHandle handle) const = 0;
  virtual void generateMipmap(TextureHandle handle) const = 0;
  virtual Format getFormat(TextureHandle handle) const = 0;
#pragma endregion

  virtual TextureHandle getCurrentSwapchainTexture() = 0;
  virtual Format getSwapchainFormat() const = 0;

  ShaderStages createShaderStages(const char* cs,
                                  const char* debugName,
                                  Result* outResult = nullptr) {
    return ShaderStages(
        createShaderModule(ShaderModuleDesc(cs, Stage_Compute, debugName), outResult).release());
  }

  ShaderStages createShaderStages(const char* vs,
                                  const char* debugNameVS,
                                  const char* fs,
                                  const char* debugNameFS,
                                  Result* outResult = nullptr) {
    auto VS = createShaderModule(ShaderModuleDesc(vs, Stage_Vertex, debugNameVS), outResult);
    auto FS = createShaderModule(ShaderModuleDesc(fs, Stage_Fragment, debugNameFS), outResult);
    return ShaderStages(VS.release(), FS.release());
  }
  ShaderStages createShaderStages(const char* vs,
                                  const char* debugNameVS,
                                  const char* gs,
                                  const char* debugNameGS,
                                  const char* fs,
                                  const char* debugNameFS,
                                  Result* outResult = nullptr) {
    auto VS = createShaderModule(ShaderModuleDesc(vs, Stage_Vertex, debugNameVS), outResult);
    auto GS = createShaderModule(ShaderModuleDesc(gs, Stage_Geometry, debugNameGS), outResult);
    auto FS = createShaderModule(ShaderModuleDesc(fs, Stage_Fragment, debugNameFS), outResult);
    return ShaderStages(VS.release(), GS.release(), FS.release());
  }

 protected:
  IDevice() = default;
};

} // namespace lvk

namespace lvk {

bool isDepthOrStencilFormat(lvk::Format format);
uint32_t calcNumMipLevels(uint32_t width, uint32_t height);
uint32_t getTextureBytesPerLayer(uint32_t width, uint32_t height, lvk::Format format, uint32_t level);
void logShaderSource(const char* text);

} // namespace lvk
