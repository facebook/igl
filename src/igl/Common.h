/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include <igl/Macros.h>

#include <minilog/minilog.h>

namespace igl {

struct ShaderStages;

bool _IGLVerify(bool cond, const char* func, const char* file, int line, const char* format, ...);

} // namespace igl

#if IGL_DEBUG

#define IGL_UNEXPECTED(cond) (!_IGLVerify(0 == !!(cond), IGL_FUNCTION, __FILE__, __LINE__, #cond))
#define IGL_VERIFY(cond) ::igl::_IGLVerify((cond), IGL_FUNCTION, __FILE__, __LINE__, #cond)
#define IGL_ASSERT(cond) (void)IGL_VERIFY(cond)
#define IGL_ASSERT_MSG(cond, format, ...) \
  (void)::igl::_IGLVerify((cond), IGL_FUNCTION, __FILE__, __LINE__, (format), ##__VA_ARGS__)

#else

#define IGL_UNEXPECTED(cond) (cond)
#define IGL_VERIFY(cond) (cond)
#define IGL_ASSERT(cond) static_cast<void>(0)
#define IGL_ASSERT_MSG(cond, format, ...) static_cast<void>(0)

#endif // IGL_DEBUG

#define IGL_ASSERT_NOT_REACHED() IGL_ASSERT_MSG(0, "Code should NOT be reached")

// Undefine macros that are local to this header
#if defined(IGL_CORE_H_COMMON_SKIP_CHECK)
#undef IGL_COMMON_SKIP_CHECK
#undef IGL_CORE_H_COMMON_SKIP_CHECK
#endif // defined(IGL_CORE_H_COMMON_SKIP_CHECK)

/**
 * FB_ANONYMOUS_VARIABLE(str) introduces an identifier starting with
 * str and ending with a number that varies with the line.
 */
#ifndef FB_ANONYMOUS_VARIABLE
#define FB_CONCATENATE_IMPL(s1, s2) s1##s2
#define FB_CONCATENATE(s1, s2) FB_CONCATENATE_IMPL(s1, s2)
#ifdef __COUNTER__
#define FB_ANONYMOUS_VARIABLE(str) FB_CONCATENATE(str, __COUNTER__)
#else
#define FB_ANONYMOUS_VARIABLE(str) FB_CONCATENATE(str, __LINE__)
#endif
#endif // FB_ANONYMOUS_VARIABLE

// a very minimalistic version of folly/ScopeGuard.h
namespace {

enum class ScopeGuardOnExit {};

template<typename T>
class ScopeGuard {
 public:
  explicit ScopeGuard(const T& fn) : fn_(fn) {}
  ~ScopeGuard() {
    fn_();
  }

 private:
  T fn_;
};

template<typename T>
// Ignore readability-named-parameter
// @lint-ignore CLANGTIDY
ScopeGuard<T> operator+(ScopeGuardOnExit, T&& fn) {
  return ScopeGuard<T>(std::forward<T>(fn));
}

} // namespace

#define SCOPE_EXIT \
  auto FB_ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) = ScopeGuardOnExit() + [&]() noexcept

namespace igl {

enum TextureFormat : uint8_t {
  Invalid = 0,

  // 8 bpp
  R_UNorm8,
  // 16 bpp
  R_F16,
  R_UInt16,
  R_UNorm16,
  RG_UNorm8,

  // 32 bpp
  RGBA_UNorm8,
  BGRA_UNorm8,
  RGBA_SRGB,
  BGRA_SRGB,
  RG_F16,
  RG_UInt16,
  RG_UNorm16,
  RGB10_A2_UNorm_Rev,
  RGB10_A2_Uint_Rev,
  BGR10_A2_Unorm,
  R_F32,
  // 64 bpp
  RGBA_F16,
  // 128 bpp
  RGBA_UInt32,
  RGBA_F32,
  // Compressed
  RGB8_ETC2,
  SRGB8_ETC2,
  RGB8_Punchthrough_A1_ETC2,
  SRGB8_Punchthrough_A1_ETC2,
  RG_EAC_UNorm,
  RG_EAC_SNorm,
  R_EAC_UNorm,
  R_EAC_SNorm,
  RGBA_BC7_UNORM_4x4,

  // Depth and Stencil formats
  Z_UNorm16,
  Z_UNorm24,
  Z_UNorm32,
  S8_UInt_Z24_UNorm,
};

/**
 * @brief Represents a type of a physical device for graphics/compute purposes
 */
enum class HWDeviceType {
  /// HW GPU - Discrete
  DiscreteGpu = 1,
  /// HW GPU - External
  ExternalGpu = 2,
  /// HW GPU - Integrated
  IntegratedGpu = 3,
  /// SW GPU
  SoftwareGpu = 4,
};

/**
 * @brief PlatformDeviceType represents the platform and graphics backend that a class implementing
 * the IPlatformDevice interface supports.
 *
 */
enum class PlatformDeviceType {
  Vulkan,
};

class IPlatformDevice {
  friend class IDevice;

 protected:
  IPlatformDevice() = default;
  /**
   * @brief Check the type of an IPlatformDevice.
   * @returns true if the IPlatformDevice is a given PlatformDeviceType t, otherwise false.
   */
  virtual bool isType(PlatformDeviceType t) const noexcept = 0;

 public:
  virtual ~IPlatformDevice() = default;
};

/**
 * @brief  Represents a description of a specific physical device installed in the system
 */
struct HWDeviceDesc {
  /** @brief Implementation-specific identifier of a device */
  uintptr_t guid;
  /** @brief A type of an actual physical device */
  HWDeviceType type;
  /** @brief Implementation-specific name of a device */
  std::string name;
  /** @brief Implementation-specific vendor name */
  std::string vendor;
};

enum class ResourceStorage {
  Private, /// Memory private to GPU access (fastest)
  Shared, /// Memory shared between CPU and GPU
  Memoryless /// Memory can be accessed only by the GPU and only exist temporarily during a render
             /// pass
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

struct Result {
  enum class Code {
    // No error
    Ok,

    /// Bad argument, e.g. invalid buffer/texture/bind type
    ArgumentInvalid,
    /// nullptr input for non-null argument
    ArgumentNull,
    /// Argument out of range, e.g. attachment/mip-level/aniso out of range
    ArgumentOutOfRange,
    /// Cannot execute operation in current state
    InvalidOperation,
    /// Feature is not supported on current hardware or software
    Unsupported,
    /// Feature has not yet been implemented into IGL
    Unimplemented,

    /// Something bad happened internally but we don't know what
    RuntimeError,
  };

  Code code = Code::Ok;
  std::string message;
  explicit Result() = default;
  explicit Result(Code code, const char* message = "") :
    code(code), message(message) {}
  explicit Result(Code code, std::string message) : code(code), message(std::move(message)) {}

  bool isOk() const {
    return code == Result::Code::Ok;
  }

  static void setResult(Result* outResult,
                        Code code,
                        const std::string& message = "") {
    if (outResult != nullptr) {
      outResult->code = code;
      outResult->message = message;
    }
  }

  static void setResult(Result* outResult, const Result& sourceResult) {
    if (outResult != nullptr) {
      *outResult = sourceResult;
    }
  }

  static void setResult(Result* outResult, Result&& sourceResult) {
    if (outResult != nullptr) {
      *outResult = std::move(sourceResult);
    }
  }

  static void setOk(Result* outResult) {
    if (outResult != nullptr) {
      outResult->code = Code::Ok;
      outResult->message = std::string();
    }
  }
};

enum class BackendType {
  Vulkan,
};

///--------------------------------------
/// MARK: - Rect<T>

/// Use value-initialization (i.e. braces) to 0-initialize: `Rect<float> myRect{};`
template<typename T>
struct Rect {
 private:
  static constexpr T kNullValue = std::numeric_limits<T>::has_infinity
                                      ? std::numeric_limits<T>::infinity()
                                      : std::numeric_limits<T>::max();

 public:
  T x = kNullValue;
  T y = kNullValue;
  T width{}; // zero-initialize
  T height{}; // zero-initialize

  bool isNull() const {
    return kNullValue == x && kNullValue == y;
  }
};

using ScissorRect = Rect<uint32_t>;

///--------------------------------------
/// MARK: - Size

struct Size {
  float width = 0.0f;
  float height = 0.0f;

  Size() = default;
  Size(float width, float height) : width(width), height(height) {}
};

struct Dimensions {
  Dimensions() = default;
  Dimensions(size_t w, size_t h, size_t d) {
    width = w;
    height = h;
    depth = d;
  }
  size_t width;
  size_t height;
  size_t depth;
};

///--------------------------------------
/// MARK: - Viewport

/// x, y, width, height in pixels. minDepth, maxDepth are in [0.0, 1.0]
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

using ColorWriteBits = uint8_t;

enum ColorWriteMask : ColorWriteBits {
  ColorWriteMask_Disabled = 0,
  ColorWriteMask_Red = 1 << 0,
  ColorWriteMask_Green = 1 << 1,
  ColorWriteMask_Blue = 1 << 2,
  ColorWriteMask_Alpha = 1 << 3,
  ColorWriteMask_All =
      ColorWriteMask_Red | ColorWriteMask_Green | ColorWriteMask_Blue | ColorWriteMask_Alpha,
};

enum PolygonMode : uint8_t {
  PolygonMode_Fill = 0,
  PolygonMode_Line = 1,
};

enum class VertexAttributeFormat {
  Float1 = 0,
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

  Int_2_10_10_10_REV, // standard format to store normal vectors
};

enum VertexSampleFunction {
  VertexSampleFunction_PerVertex,
  VertexSampleFunction_Instance,
};

struct VertexAttribute {
  /** @brief A buffer which contains this attribute stream */
  uint32_t bufferIndex = 0;
  /** @brief Per-element format */
  VertexAttributeFormat format = VertexAttributeFormat::Float1;
  /** @brief An offset where the first element of this attribute stream starts */
  uintptr_t offset = 0;
  uint32_t location = 0;

  VertexAttribute() = default;
  VertexAttribute(size_t bufferIndex,
                  VertexAttributeFormat format,
                  uintptr_t offset,
                  uint32_t location) :
    bufferIndex(bufferIndex), format(format), offset(offset), location(location) {}
};

struct VertexInputBinding {
  uint32_t stride = 0;
  VertexSampleFunction sampleFunction = VertexSampleFunction_PerVertex;
  size_t sampleRate = 1;
};

struct VertexInputStateDesc {
  enum { IGL_VERTEX_ATTRIBUTES_MAX = 16 };
  enum { IGL_VERTEX_BUFFER_MAX = 16 };
  uint32_t numAttributes = 0;
  VertexAttribute attributes[IGL_VERTEX_ATTRIBUTES_MAX];
  uint32_t numInputBindings = 0;
  VertexInputBinding inputBindings[IGL_VERTEX_BUFFER_MAX];
};

struct ColorAttachment {
  TextureFormat textureFormat = TextureFormat::Invalid;
  ColorWriteBits colorWriteBits = ColorWriteMask_All;
  bool blendEnabled = false;
  BlendOp rgbBlendOp = BlendOp::BlendOp_Add;
  BlendOp alphaBlendOp = BlendOp::BlendOp_Add;
  BlendFactor srcRGBBlendFactor = BlendFactor_One;
  BlendFactor srcAlphaBlendFactor = BlendFactor_One;
  BlendFactor dstRGBBlendFactor = BlendFactor_Zero;
  BlendFactor dstAlphaBlendFactor = BlendFactor_Zero;
};

struct RenderPipelineDesc {
  enum { IGL_COLOR_ATTACHMENTS_MAX = 4 };

  igl::VertexInputStateDesc vertexInputState;
  std::shared_ptr<ShaderStages> shaderStages;

  uint32_t numColorAttachments = 0;
  ColorAttachment colorAttachments[IGL_COLOR_ATTACHMENTS_MAX] = {};
  TextureFormat depthAttachmentFormat = TextureFormat::Invalid;
  TextureFormat stencilAttachmentFormat = TextureFormat::Invalid;

  CullMode cullMode = igl::CullMode_None;
  WindingMode frontFaceWinding = igl::WindingMode_CCW;
  PolygonMode polygonMode = igl::PolygonMode_Fill;

  uint32_t sampleCount = 1u;

  std::string debugName;
};

class IRenderPipelineState {
 public:
  IRenderPipelineState() = default;
  virtual ~IRenderPipelineState() = default;
};

enum class LoadAction : uint8_t {
  DontCare,
  Load,
  Clear,
};

enum class StoreAction : uint8_t {
  DontCare,
  Store,
  MsaaResolve,
};

struct AttachmentDesc {
  LoadAction loadAction = LoadAction::Clear;
  StoreAction storeAction = StoreAction::Store;
  uint8_t slice = 0;
  uint8_t mipmapLevel = 0;
  Color clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
  float clearDepth = 1.0f;
  uint32_t clearStencil = 0;
};

struct RenderPassDesc {
  uint32_t numColorAttachments = 0;
  AttachmentDesc colorAttachments[RenderPipelineDesc::IGL_COLOR_ATTACHMENTS_MAX];
  AttachmentDesc depthStencilAttachment;
};

enum class CommandQueueType {
  Compute, /// Supports Compute commands
  Graphics, /// Supports Graphics commands
  Transfer, /// Supports Memory commands
};

} // namespace igl
