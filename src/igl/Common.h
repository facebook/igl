/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#ifndef IGL_COMMON_H
#define IGL_COMMON_H

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <igl/Core.h>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

using ulong_t = unsigned long;
using long_t = long;

#define IGL_NULLABLE FOLLY_NULLABLE
#define IGL_NONNULL FOLLY_NONNULL

namespace igl {

// Callback to delete and/or release a pointer
using Deleter = void (*)(void* IGL_NULLABLE);

/// Device Capabilities or Metal Features
constexpr size_t IGL_TEXTURE_SAMPLERS_MAX = 16;
constexpr size_t IGL_VERTEX_ATTRIBUTES_MAX = 24;
constexpr size_t IGL_VERTEX_BUFFER_MAX = 24;
constexpr size_t IGL_VERTEX_BINDINGS_MAX = 24;
constexpr size_t IGL_UNIFORM_BLOCKS_BINDING_MAX = 16;

// See GL_MAX_COLOR_ATTACHMENTS in
// https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glGet.xhtml
// and see maximum number of color render targets in
// https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
constexpr size_t IGL_COLOR_ATTACHMENTS_MAX = 4;

// See https://www.khronos.org/registry/OpenGL-Refpages/es3.1/html/glDrawElementsIndirect.xhtml,
// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkDrawIndexedIndirectCommand.html,
// or https://developer.apple.com/documentation/metal/mtldrawindexedprimitivesindirectarguments.
constexpr size_t IGL_DRAW_ELEMENTS_INDIRECT_COMMAND_SIZE = 4 * 5;

enum class ResourceStorage {
  Invalid, /// Invalid sharing mode
  Private, /// Memory private to GPU access (fastest)
  Shared, /// Memory shared between CPU and GPU
  Managed, /// Memory pair synchronized between CPU and GPU
  Memoryless /// Memory can be accessed only by the GPU and only exist temporarily during a render
             /// pass
};

enum class CullMode : uint8_t { Disabled, Front, Back };
enum class WindingMode : uint8_t { Clockwise, CounterClockwise };
enum class NormalizedZRange : uint8_t { NegOneToOne, ZeroToOne };

struct Color {
  float r;
  float g;
  float b;
  float a;

  Color(float r, float g, float b) : r(r), g(g), b(b), a(1.0f) {}
  Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}

  const float* IGL_NONNULL toFloatPtr() const {
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
  explicit Result(Code code, const char* IGL_NULLABLE message = "") :
    code(code), message(message) {}
  explicit Result(Code code, std::string message) : code(code), message(std::move(message)) {}

  bool isOk() const {
    return code == Result::Code::Ok;
  }

  static void setResult(Result* IGL_NULLABLE outResult,
                        Code code,
                        const std::string& message = "") {
    if (outResult != nullptr) {
      outResult->code = code;
      outResult->message = message;
    }
  }

  static void setResult(Result* IGL_NULLABLE outResult, const Result& sourceResult) {
    if (outResult != nullptr) {
      *outResult = sourceResult;
    }
  }

  static void setResult(Result* IGL_NULLABLE outResult, Result&& sourceResult) {
    if (outResult != nullptr) {
      *outResult = std::move(sourceResult);
    }
  }

  static void setOk(Result* IGL_NULLABLE outResult) {
    if (outResult != nullptr) {
      outResult->code = Code::Ok;
      outResult->message = std::string();
    }
  }
};

enum class BackendType {
  OpenGL,
  Metal,
  Vulkan,
  // @fb-only
};
std::string BackendTypeToString(BackendType backendType);

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

inline bool operator==(const Dimensions& lhs, const Dimensions& rhs) {
  return (lhs.width == rhs.width && lhs.height == rhs.height && lhs.depth == rhs.depth);
}

inline bool operator!=(const Dimensions& lhs, const Dimensions& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator==(const Size& lhs, const Size& rhs) {
  return (lhs.width == rhs.width && lhs.height == rhs.height);
}

inline bool operator!=(const Size& lhs, const Size& rhs) {
  return !operator==(lhs, rhs);
}

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

const Viewport kInvalidViewport = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};

///--------------------------------------
/// MARK: - Enum utilities

// Get value of enum by stripping enum class type
template<typename E>
constexpr typename std::underlying_type<E>::type EnumToValue(E enumerator) noexcept {
  return static_cast<typename std::underlying_type<E>::type>(enumerator);
}

///--------------------------------------
/// MARK: - ScopeGuard

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

#define IGL_SCOPE_EXIT \
  auto FB_ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) = ScopeGuardOnExit() + [&]() noexcept
} // namespace igl

#endif // IGL_COMMON_H
