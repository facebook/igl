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
#include <vector>

#define IGL_NULLABLE FOLLY_NULLABLE
#define IGL_NONNULL FOLLY_NONNULL

namespace igl {

// Callback to delete and/or release a pointer
using Deleter = void (*)(void* IGL_NULLABLE);

/// Device Capabilities or Metal Features
constexpr uint32_t IGL_TEXTURE_SAMPLERS_MAX = 16;
constexpr uint32_t IGL_VERTEX_ATTRIBUTES_MAX = 24;
constexpr uint32_t IGL_VERTEX_BUFFER_MAX = 128;
constexpr uint32_t IGL_VERTEX_BINDINGS_MAX = 24;
constexpr uint32_t IGL_UNIFORM_BLOCKS_BINDING_MAX = 16;

// See GL_MAX_COLOR_ATTACHMENTS in
// https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glGet.xhtml
// and see maximum number of color render targets in
// https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
constexpr uint32_t IGL_COLOR_ATTACHMENTS_MAX = 4;

// See https://www.khronos.org/registry/OpenGL-Refpages/es3.1/html/glDrawElementsIndirect.xhtml,
// https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/VkDrawIndexedIndirectCommand.html,
// or https://developer.apple.com/documentation/metal/mtldrawindexedprimitivesindirectarguments.
constexpr uint32_t IGL_DRAW_ELEMENTS_INDIRECT_COMMAND_SIZE = 4 * 5;

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

enum class PrimitiveType : uint8_t {
  Point,
  Line,
  LineStrip,
  Triangle,
  TriangleStrip,
};

struct Color {
  float r;
  float g;
  float b;
  float a;

  constexpr Color(float r, float g, float b) : r(r), g(g), b(b), a(1.0f) {}
  constexpr Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}

  [[nodiscard]] const float* IGL_NONNULL toFloatPtr() const {
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

  [[nodiscard]] bool isOk() const {
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
  Invalid,
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

  [[nodiscard]] bool isNull() const {
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
  Dimensions(uint32_t w, uint32_t h, uint32_t d) : width(w), height(h), depth(d) {}
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t depth = 0;
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
};

inline bool operator==(const Viewport& lhs, const Viewport& rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width && lhs.height == rhs.height &&
         lhs.minDepth == rhs.minDepth && lhs.maxDepth == rhs.maxDepth;
}

inline bool operator!=(const Viewport& lhs, const Viewport& rhs) {
  return !operator==(lhs, rhs);
}

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

///--------------------------------------
/// MARK: - optimizedMemcpy

// Optimized version of memcpy() that allows to copy smallest uniforms in the most efficient way.
// Other sizes utilize a libc memcpy() implementation. It's not a universal function and expects to
// have a proper alignment for data!
void optimizedMemcpy(void* IGL_NULLABLE dst, const void* IGL_NULLABLE src, size_t size);

///--------------------------------------
/// MARK: - Handle

// Non-ref counted handles; based on:
// https://enginearchitecture.realtimerendering.com/downloads/reac2023_modern_mobile_rendering_at_hypehype.pdf
// https://github.com/corporateshark/lightweightvk/blob/main/lvk/LVK.h
template<typename ObjectType>
class Handle final {
 public:
  Handle() noexcept = default;

  [[nodiscard]] bool empty() const noexcept {
    return gen_ == 0;
  }
  [[nodiscard]] bool valid() const noexcept {
    return gen_ != 0;
  }
  [[nodiscard]] uint32_t index() const noexcept {
    return index_;
  }
  [[nodiscard]] uint32_t gen() const noexcept {
    return gen_;
  }
  [[nodiscard]] void* IGL_NULLABLE indexAsVoid() const noexcept {
    return reinterpret_cast<void*>(static_cast<ptrdiff_t>(index_));
  }
  bool operator==(const Handle<ObjectType>& other) const noexcept {
    return index_ == other.index_ && gen_ == other.gen_;
  }
  bool operator!=(const Handle<ObjectType>& other) const noexcept {
    return index_ != other.index_ || gen_ != other.gen_;
  }
  // allow conditions 'if (handle)'
  explicit operator bool() const noexcept {
    return gen_ != 0;
  }

 private:
  Handle(uint32_t index, uint32_t gen) noexcept : index_(index), gen_(gen) {}

  template<typename ObjectType_, typename ImplObjectType>
  friend class Pool;

  uint32_t index_ = 0; // the index of this handle within a Pool
  uint32_t gen_ = 0; // the generation of this handle to prevent the ABA Problem
};

static_assert(sizeof(Handle<class Foo>) == sizeof(uint64_t));

// specialized with dummy structs for type safety
using BindGroupTextureHandle = igl::Handle<struct BindGroupTextureTag>;
using BindGroupBufferHandle = igl::Handle<struct BindGroupBufferTag>;
using TextureHandle = igl::Handle<struct TextureTag>;
using SamplerHandle = igl::Handle<struct SamplerTag>;

class IDevice;

// forward declarations to access incomplete type IDevice
void destroy(igl::IDevice* IGL_NULLABLE device, igl::BindGroupTextureHandle handle);
void destroy(igl::IDevice* IGL_NULLABLE device, igl::BindGroupBufferHandle handle);
void destroy(igl::IDevice* IGL_NULLABLE device, igl::TextureHandle handle);
void destroy(igl::IDevice* IGL_NULLABLE device, igl::SamplerHandle handle);

///--------------------------------------
/// MARK: - Holder

// RAII wrapper around Handle<>; based on:
// https://github.com/corporateshark/lightweightvk/blob/main/lvk/LVK.h
template<typename HandleType>
class Holder final {
 public:
  Holder() noexcept = default;
  Holder(igl::IDevice* IGL_NULLABLE device, HandleType handle) noexcept :
    device_(device), handle_(handle) {}
  ~Holder() {
    igl::destroy(device_, handle_);
  }
  Holder(const Holder&) = delete;
  Holder(Holder&& other) noexcept : device_(other.device_), handle_(other.handle_) {
    other.device_ = nullptr;
    other.handle_ = HandleType{};
  }
  Holder& operator=(const Holder&) = delete;
  Holder& operator=(Holder&& other) noexcept {
    std::swap(device_, other.device_);
    std::swap(handle_, other.handle_);
    return *this;
  }
  Holder& operator=(std::nullptr_t) {
    this->reset();
    return *this;
  }

  [[nodiscard]] operator HandleType() const noexcept {
    return handle_;
  }

  [[nodiscard]] bool valid() const noexcept {
    return handle_.valid();
  }

  [[nodiscard]] bool empty() const noexcept {
    return handle_.empty();
  }

  void reset() {
    igl::destroy(device_, handle_);
    device_ = nullptr;
    handle_ = HandleType{};
  }

  [[nodiscard]] HandleType release() noexcept {
    device_ = nullptr;
    return std::exchange(handle_, HandleType{});
  }

  [[nodiscard]] uint32_t gen() const noexcept {
    return handle_.gen();
  }
  [[nodiscard]] uint32_t index() const noexcept {
    return handle_.index();
  }
  [[nodiscard]] void* IGL_NULLABLE indexAsVoid() const noexcept {
    return handle_.indexAsVoid();
  }

 private:
  igl::IDevice* IGL_NULLABLE device_ = nullptr;
  HandleType handle_ = {};
};

///--------------------------------------
/// MARK: - Pool

// A Pool of objects which is compatible with the abovementioned Handle<> types; based on:
// https://enginearchitecture.realtimerendering.com/downloads/reac2023_modern_mobile_rendering_at_hypehype.pdf
// https://github.com/corporateshark/lightweightvk/blob/main/lvk/Pool.h
template<typename ObjectType, typename ImplObjectType>
class Pool {
  static constexpr uint32_t kListEndSentinel = 0xffffffff;
  struct PoolEntry {
    explicit PoolEntry(ImplObjectType& obj) noexcept : obj_(std::move(obj)) {}
    ImplObjectType obj_ = {};
    uint32_t gen_ = 1;
    uint32_t nextFree_ = kListEndSentinel;
  };
  uint32_t freeListHead_ = kListEndSentinel;
  uint32_t numObjects_ = 0;

 public:
  std::vector<PoolEntry> objects_;

  [[nodiscard]] Handle<ObjectType> create(ImplObjectType&& obj) {
    uint32_t idx = 0;
    if (freeListHead_ != kListEndSentinel) {
      idx = freeListHead_;
      freeListHead_ = objects_[idx].nextFree_;
      objects_[idx].obj_ = std::move(obj);
    } else {
      idx = (uint32_t)objects_.size();
      objects_.emplace_back(obj);
    }
    numObjects_++;
    return Handle<ObjectType>(idx, objects_[idx].gen_);
  }
  void destroy(Handle<ObjectType> handle) noexcept {
    if (handle.empty()) {
      return;
    }
    IGL_ASSERT_MSG(numObjects_ > 0, "Double deletion");
    const uint32_t index = handle.index();
    IGL_ASSERT(index < objects_.size());
    IGL_ASSERT_MSG(handle.gen() == objects_[index].gen_, "Double deletion");
    objects_[index].obj_ = ImplObjectType{};
    objects_[index].gen_++;
    objects_[index].nextFree_ = freeListHead_;
    freeListHead_ = index;
    numObjects_--;
  }
  // this is a helper function to simplify migration to handles (should be deprecated after the
  // migration is completed)
  void destroy(uint32_t index) noexcept {
    IGL_ASSERT_MSG(numObjects_ > 0, "Double deletion");
    IGL_ASSERT(index < objects_.size());
    objects_[index].obj_ = ImplObjectType{};
    objects_[index].gen_++;
    objects_[index].nextFree_ = freeListHead_;
    freeListHead_ = index;
    numObjects_--;
  }
  [[nodiscard]] const ImplObjectType* IGL_NULLABLE get(Handle<ObjectType> handle) const noexcept {
    if (handle.empty()) {
      return nullptr;
    }

    const uint32_t index = handle.index();
    IGL_ASSERT(index < objects_.size());
    IGL_ASSERT_MSG(handle.gen() == objects_[index].gen_, "Accessing a deleted object");
    return &objects_[index].obj_;
  }
  [[nodiscard]] ImplObjectType* IGL_NULLABLE get(Handle<ObjectType> handle) noexcept {
    if (handle.empty()) {
      return nullptr;
    }

    const uint32_t index = handle.index();
    IGL_ASSERT(index < objects_.size());
    IGL_ASSERT_MSG(handle.gen() == objects_[index].gen_, "Accessing a deleted object");
    return &objects_[index].obj_;
  }
  [[nodiscard]] Handle<ObjectType> findObject(const ImplObjectType* IGL_NULLABLE obj) noexcept {
    if (!obj) {
      return {};
    }

    for (size_t idx = 0; idx != objects_.size(); idx++) {
      if (objects_[idx].obj_ == *obj) {
        return Handle<ObjectType>((uint32_t)idx, objects_[idx].gen_);
      }
    }

    return {};
  }
  void clear() noexcept {
    objects_.clear();
    freeListHead_ = kListEndSentinel;
    numObjects_ = 0;
  }
  [[nodiscard]] uint32_t numObjects() const noexcept {
    return numObjects_;
  }
};

} // namespace igl

#endif // IGL_COMMON_H
