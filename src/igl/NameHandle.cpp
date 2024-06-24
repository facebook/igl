/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/Macros.h>
#include <igl/NameHandle.h>

#include <functional>
#include <string>

#if !defined(__cpp_consteval) && !defined(__cpp_constexpr) || defined(_MSC_VER)
constexpr unsigned long crc_table[256] = {iglCrc256(0)};
namespace {
uint32_t iglCrc32Impl(const char* p, uint32_t crc) {
  while (*p) {
    auto v = (uint8_t)*p++;
    crc = (crc >> 8) ^ crc_table[(crc & 0xFF) ^ v];
  }
  return crc;
}
} // namespace
#else
#define iglCrc32Impl iglCrc32ImplConstExpr
#endif

#if defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>

template<typename T>
bool isAligned(const void* p) {
  return !(reinterpret_cast<uintptr_t>(p) % std::alignment_of<T>::value);
}
namespace {
uint32_t iglCrc32ImplARM8(const char* s, uint32_t crc, size_t length) {
  for (; !isAligned<uint32_t>(s) && length > 0; s++, length--) {
    crc = __crc32b(crc, *s);
  }

  for (; length > 8; s += 8, length -= 8) {
    crc = __crc32d(crc, *(const uint64_t*)(s));
  }

  for (; length > 4; s += 4, length -= 4) {
    crc = __crc32w(crc, *(const uint32_t*)(s));
  }

  for (; length > 0; s++, length--) {
    crc = __crc32b(crc, *s);
  }

  return crc;
}
} // namespace

#if IGL_PLATFORM_ANDROID && defined(__aarch64__)

#include <asm/hwcap.h>
#include <sys/auxv.h>
namespace {
bool detectCrc32() {
  uint64_t hwcaps = getauxval(AT_HWCAP);
  return hwcaps & HWCAP_CRC32 ? true : false;
}
} // namespace
#elif IGL_PLATFORM_APPLE || IGL_PLATFORM_IOS || IGL_PLATFORM_MACOS
namespace {
bool detectCrc32() {
  // All iphones6+ are support it
  return true;
}
} // namespace
#else
namespace {
bool detectCrc32() {
  return false;
}
} // namespace
#endif

uint32_t igl::iglCrc32(const char* data, size_t length) {
  static bool hwSupport = detectCrc32();
  return hwSupport ? ~iglCrc32ImplARM8(data, ~0, length) : ~iglCrc32Impl(data, ~0);
}
#else

uint32_t igl::iglCrc32(const char* data, size_t /*length*/) {
  return ~iglCrc32Impl(data, ~0);
}

#endif

#if IGL_DEBUG
namespace igl {
bool NameHandle::checkIsValidCrcCompare(const NameHandle& nh) const {
  const bool res = nh.crc32_ == crc32_ && nh.name_ != name_;
  IGL_ASSERT_MSG(!res,
                 "NameHandle CRC check fails: name1 (%s %x) name2 (%s %x)\n",
                 name_.c_str(),
                 crc32_,
                 nh.name_.c_str(),
                 nh.crc32_);

  return res;
}
} // namespace igl
#endif // IGL_DEBUG

size_t std::hash<std::vector<igl::NameHandle>>::operator()(
    const std::vector<igl::NameHandle>& key) const {
  size_t hash = 0;
  for (const auto& elem : key) {
    hash ^= std::hash<uint32_t>()(elem.getCrc32());
  }
  return hash;
}
