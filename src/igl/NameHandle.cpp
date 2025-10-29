/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/NameHandle.h>

#include <functional>
#include <string>
#include <igl/Macros.h>

namespace igl {
namespace {
#if !defined(__cpp_constexpr)
static const CRC_TABLE;
uint32_t iglCrc32Fallback(const char* p) {
  uint32_t crc = ~0u;
  while (*p) {
    auto v = (uint8_t)*p++;
    crc = (crc >> 8) ^ kCrcTable[(crc & 0xFF) ^ v];
  }
  return ~crc;
}
#endif

uint32_t iglCrc32Impl(const char* p) {
#if defined(__cpp_constexpr)
  return iglCrc32ConstExpr(p);
#else
  return iglCrc32Fallback(p);
#endif
}

} // namespace

#if defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>

namespace {
template<typename T>
bool isAligned(const void* p) {
  return !(reinterpret_cast<uintptr_t>(p) % std::alignment_of<T>::value);
}

uint32_t iglCrc32ImplARM8(const char* s, size_t length) {
  uint32_t crc = ~0u;
  for (; !isAligned<uint64_t>(s) && length > 0; s++, length--) {
    crc = __crc32b(crc, *s);
  }

  for (; length >= 8; s += 8, length -= 8) {
    crc = __crc32d(crc, *(const uint64_t*)(s));
  }

  for (; length >= 4; s += 4, length -= 4) {
    crc = __crc32w(crc, *(const uint32_t*)(s));
  }

  for (; length > 0; s++, length--) {
    crc = __crc32b(crc, *s);
  }

  return ~crc;
}

#if IGL_PLATFORM_ANDROID && defined(__aarch64__)

#include <asm/hwcap.h>
#include <sys/auxv.h>
bool detectCrc32() {
  uint64_t hwcaps = getauxval(AT_HWCAP);
  return hwcaps & HWCAP_CRC32 ? true : false;
}
#elif IGL_PLATFORM_APPLE || IGL_PLATFORM_IOS || IGL_PLATFORM_MACOSX
bool detectCrc32() {
  // All iphones6+ are support it
  return true;
}
#else
bool detectCrc32() {
  return false;
}
#endif
} // namespace

uint32_t iglCrc32(const char* data, size_t length) {
  static bool hwSupport = detectCrc32();
  return hwSupport ? iglCrc32ImplARM8(data, length) : iglCrc32Impl(data);
}
#else

uint32_t iglCrc32(const char* data, size_t /*length*/) {
  return iglCrc32Impl(data);
}

#endif

#if IGL_DEBUG_ABORT_ENABLED
bool NameHandle::checkIsValidCrcCompare(const NameHandle& nh) const {
  const bool res = nh.crc32_ == crc32_ && nh.name_ != name_;
  IGL_DEBUG_ASSERT(!res,
                   "NameHandle CRC check fails: name1 (%s %x) name2 (%s %x)\n",
                   name_.c_str(),
                   crc32_,
                   nh.name_.c_str(),
                   nh.crc32_);

  return res;
}
#endif // IGL_DEBUG_ABORT_ENABLED
} // namespace igl

size_t std::hash<std::vector<igl::NameHandle>>::operator()(
    const std::vector<igl::NameHandle>& key) const {
  size_t hash = 0;
  for (const auto& elem : key) {
    hash ^= std::hash<uint32_t>()(elem.getCrc32());
  }
  return hash;
}
