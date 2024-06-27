/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>
#include <igl/Common.h>
#include <string>
#include <utility>
#include <vector>

namespace igl {
// Constexpr as constexpr. In mostcases all hashes from string would be generated in compile-time
// crc32 generation is based on gcc implementation

#define iglCRC(c) ((c) >> 1 ^ ((c & 1) ? 0xedb88320 : 0))
#define iglCrc1(c) (iglCRC(iglCRC(iglCRC(iglCRC(iglCRC(iglCRC(iglCRC(iglCRC(c)))))))))
#define iglCrc4(x) iglCrc1(x), iglCrc1(x + 1), iglCrc1(x + 2), iglCrc1(x + 3)
#define iglCrc16(x) iglCrc4(x), iglCrc4(x + 4), iglCrc4(x + 8), iglCrc4(x + 12)
#define iglCrc64(x) iglCrc16(x), iglCrc16(x + 16), iglCrc16(x + 32), iglCrc16(x + 48)
#define iglCrc256(x) iglCrc64(x), iglCrc64(x + 64), iglCrc64(x + 128), iglCrc64(x + 192)

#if defined(__cpp_consteval) && !defined(_MSC_VER) && !defined(__GNUC__)
consteval uint32_t iglCrc32ImplConstExpr(const char* p, uint32_t crc) {
  consteval uint32_t crc_table[256] = {iglCrc256(0)};
  if (*p) {
    uint8_t v = (uint8_t)*p;
    return iglCrc32ImplConstExpr(p + 1, (crc >> 8) ^ crc_table[(crc & 0xFF) ^ v]);
  }
  return crc;
}

/**
 * @brief Calculates CRC32 for the incoming character array.
 * @param data null terminated character string
 * @returns CRC32 representation of data
 */
consteval uint32_t iglCrc32ConstExpr(const char* data) {
  return ~iglCrc32ImplConstExpr(data, ~0);
}

#elif defined(__cpp_constexpr) && !defined(_MSC_VER)

template<int N>
constexpr uint32_t iglCrc32ImplConstExprImpl(const char* p, uint32_t crc) {
  constexpr uint32_t crc_table[256] = {iglCrc256(0)};
  if (*p) {
    const uint8_t v = (uint8_t)*p;
    return iglCrc32ImplConstExprImpl<N>(p + 1, (crc >> 8) ^ crc_table[(crc & 0xFF) ^ v]);
  }
  return crc;
}

constexpr uint32_t iglCrc32ImplConstExpr(const char* p, uint32_t crc) {
  // Trick to enforce to use compile-time
  // https://stackoverflow.com/questions/39889604/how-to-force-a-constexpr-function-to-be-evaluated-at-compile-time
  return iglCrc32ImplConstExprImpl<0>(p, crc);
}
/**
 * @brief Calculates CRC32 for the incoming character array.
 * @param data null terminated character string
 * @returns CRC32 representation of data
 */
constexpr uint32_t iglCrc32ConstExpr(const char* data) {
  return ~iglCrc32ImplConstExpr(data, ~0u);
}
#endif // defined(__cpp_constexpr)

/**
 * @brief Calculates CRC32 for the incoming character array.
 * @param data null terminated character string
 * @returns CRC32 representation of data
 */
uint32_t iglCrc32(const char* data, size_t length);

///--------------------------------------
/// MARK: - NameHandle

#if IGL_DEBUG
#define CHECK_VALID_CRC(a) (void)checkIsValidCrcCompare(a)
#else
#define CHECK_VALID_CRC(a)
#endif

/**
 * @brief Creates a mapping between a string and its equivalent CRC32 handle
 * This way when we need to check if a uniform exists or if it matches another
 * uniform, we can do an integer comparison rather than a string comparison.
 */
class NameHandle {
 public:
  NameHandle() = default;
  ~NameHandle() = default;

  NameHandle(const NameHandle& other) = default;
  NameHandle(NameHandle&& other) noexcept = default;

  NameHandle(std::string name, uint32_t crc32) : crc32_(crc32), name_(std::move(name)) {}

  /**
   * @brief Returns a null terminated character array version of the name
   * @returns null terminated character array
   */
  [[nodiscard]] const char* c_str() const {
    return name_.c_str();
  }

  /**
   * @brief Returns a reference to the actual name string
   * @returns Reference to the actual name string
   */
  [[nodiscard]] const std::string& toString() const {
    return name_;
  }

  /**
   * @brief Returns crc32 handle for the name string
   * @returns crc32 handle
   */
  [[nodiscard]] uint32_t getCrc32() const {
    return crc32_;
  }

  bool operator==(const NameHandle& other) const {
    CHECK_VALID_CRC(other);
    return crc32_ == other.crc32_;
  }

  bool operator!=(const NameHandle& other) const {
    return !(*this == other);
  }

  bool operator<(const NameHandle& other) const {
    CHECK_VALID_CRC(other);
    return (crc32_ < other.crc32_);
  }

  bool operator>=(const NameHandle& other) const {
    return !(*this < other);
  }

  bool operator>(const NameHandle& other) const {
    CHECK_VALID_CRC(other);
    return (crc32_ > other.crc32_);
  }

  bool operator<=(const NameHandle& other) const {
    return !(*this > other);
  }

  NameHandle& operator=(const NameHandle& other) {
    if (crc32_ == other.crc32_) {
      return *this;
    }

    crc32_ = other.crc32_;
    name_ = other.name_;
    return *this;
  }

  NameHandle& operator=(NameHandle&& other) noexcept = default;

  operator const char*() const {
    return name_.c_str();
  }

 private:
#if IGL_DEBUG
  [[nodiscard]] bool checkIsValidCrcCompare(const NameHandle& nh) const;
#endif
  uint32_t crc32_ = 0;
  std::string name_;
};

/**
 * @brief Helper function to convert a string to a NameHandle
 * @param name String to convert
 * @return NameHandle that includes the name and its CRC32.
 */
inline NameHandle genNameHandle(const std::string& name) {
  return {name, iglCrc32(name.c_str(), name.length())};
}
} // namespace igl

#if (defined(__cpp_consteval) || defined(__cpp_constexpr)) && !defined(_MSC_VER)
/// @def IGL_DEFINE_NAMEHANDLE_CONST(name, str)
/// @brief Declares and assigns a named igl::NameHandle variable.
/// @param name The name of the NameHandle variable.
/// @param str The name the handle represents (e.g., uniform name). Must be a const char*.
#define IGL_DEFINE_NAMEHANDLE_CONST(name, str) \
  const igl::NameHandle name(str, igl::iglCrc32ConstExpr(str))

/// @def IGL_NAMEHANDLE(str)
/// @brief Creates an igl::NameHandle instance.
/// @param str The name the handle represents (e.g., uniform name). Must be a const char*.
#define IGL_NAMEHANDLE(str) igl::NameHandle(str, igl::iglCrc32ConstExpr(str))
#else
// To avoid stlib include, this function is trivial
inline size_t iglStringlength(const char* str) {
  size_t r = 0;
  while (*str++) {
    r++;
  }
  return r;
}
#define IGL_DEFINE_NAMEHANDLE_CONST(name, str) \
  const igl::NameHandle name(str, igl::iglCrc32(str, iglStringlength(str)))
#define IGL_NAMEHANDLE(str) igl::NameHandle(str, igl::iglCrc32(str, iglStringlength(str)))
#endif

/// @def IGL_NAMEHANDLE_ACCESSOR(name)
/// @brief Declares a function returning a const igl::NameHandle& instance.
/// @param name The name of the NameHandle function to create.
///
/// Use IGL_NAMEHANDLE_ACCESSOR_IMPL to define the function body.
/// Use these to avoid compiler warnings with file-scoped const NameHandle instances.
/// TODO: Remove once C++20 can be used as NameHandle can be constexpr.
#define IGL_NAMEHANDLE_ACCESSOR(name) const ::igl::NameHandle& name();

/// @def IGL_NAMEHANDLE_ACCESSOR_IMPL(name, str)
/// @brief Defines a function returning a const ref to a local static igl::NameHandle instance.
/// @param name The name of the NameHandle method to create.
/// @param str The name the handle represents (e.g., uniform name). Must be a const char*.
///
/// Use IGL_NAMEHANDLE_ACCESSOR to declare the function prototype.
/// Use these to avoid compiler warnings with file-scoped const NameHandle instances.
/// TODO: Remove once C++20 can be used as NameHandle can be constexpr.
#define IGL_NAMEHANDLE_ACCESSOR_IMPL(name, str)    \
  const ::igl::NameHandle& name() {                \
    static IGL_DEFINE_NAMEHANDLE_CONST(name, str); \
    return name;                                   \
  }

namespace std {

template<>
struct hash<igl::NameHandle> {
  size_t operator()(const igl::NameHandle& key) const {
    return std::hash<uint32_t>()(key.getCrc32());
  }
};

template<>
struct hash<std::pair<igl::NameHandle, igl::NameHandle>> {
  size_t operator()(const std::pair<igl::NameHandle, igl::NameHandle>& key) const {
    return std::hash<uint32_t>()(key.first.getCrc32()) ^
           std::hash<uint32_t>()(key.second.getCrc32());
  }
};

template<>
struct hash<std::vector<igl::NameHandle>> {
  size_t operator()(const std::vector<igl::NameHandle>& key) const;
};

} // namespace std
