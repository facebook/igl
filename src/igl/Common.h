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

#include <igl/Assert.h>
#include <igl/Macros.h>

// Undefine macros that are local to this header
#if defined(IGL_CORE_H_COMMON_SKIP_CHECK)
#undef IGL_COMMON_SKIP_CHECK
#undef IGL_CORE_H_COMMON_SKIP_CHECK
#endif // defined(IGL_CORE_H_COMMON_SKIP_CHECK)

#include <utility>

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

constexpr size_t IGL_VERTEX_ATTRIBUTES_MAX = 16;
constexpr size_t IGL_VERTEX_BUFFER_MAX = 16;

// See GL_MAX_COLOR_ATTACHMENTS in
// https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glGet.xhtml
// and see maximum number of color render targets in
// https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
constexpr size_t IGL_COLOR_ATTACHMENTS_MAX = 4;

/**
 * @brief Represents a type of a physical device for graphics/compute purposes
 */
enum class HWDeviceType {
  /// Unknown
  Unknown = 0,
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
 * @brief Represents a query of a physical device to be requested from the underlying IGL
 * implementation
 */
struct HWDeviceQueryDesc {
  /** @brief Desired hardware type */
  HWDeviceType hardwareType;
  /** @brief If set, ignores hardwareType and returns device assigned to displayId */
  uintptr_t displayId;
  /** @brief Reserved */
  uint32_t flags;
};

/**
 * @brief PlatformDeviceType represents the platform and graphics backend that a class implementing
 * the IPlatformDevice interface supports.
 *
 */
enum class PlatformDeviceType {
  Unknown = 0,
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

  bool operator!=(const Viewport other) const {
    return x != other.x || y != other.y || width != other.width || height != other.height ||
           minDepth != other.minDepth || maxDepth != other.maxDepth;
  }
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

} // namespace igl
