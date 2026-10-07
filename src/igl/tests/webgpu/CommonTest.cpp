/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/Common.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <igl/Texture.h>
#include <igl/webgpu/WebGPUCompat.h>

namespace igl::tests {
namespace {

struct FakeObject {
  int refCount = 1;
  int numAddRefs = 0;
  int numReleases = 0;
};

} // namespace
} // namespace igl::tests

namespace igl::webgpu {

template<>
struct HandleTraits<tests::FakeObject*> {
  static void addRef(tests::FakeObject* object) noexcept {
    ++object->refCount;
    ++object->numAddRefs;
  }
  static void release(tests::FakeObject* object) noexcept {
    --object->refCount;
    ++object->numReleases;
  }
};

} // namespace igl::webgpu

namespace igl::tests {
namespace {

using FakeHandle = webgpu::Handle<FakeObject*>;

// TextureFormats appended after this bound are not caught: textureFormatToWGPUTextureFormat() has
// a default case, so they silently map to std::nullopt. Extend the bound when the enum grows.
constexpr TextureFormat kLastTextureFormat = TextureFormat::B10G11R11_UFloat;
constexpr VertexAttributeFormat kLastVertexAttributeFormat =
    VertexAttributeFormat::Int_2_10_10_10_REV;

constexpr std::optional<WGPUFeatureName> kNoFeature = std::nullopt;
constexpr WGPUFeatureName kUnorm16 = webgpu::compat::kUnorm16TextureFormatsFeature;
constexpr WGPUFeatureName kASTC = WGPUFeatureName_TextureCompressionASTC;
constexpr WGPUFeatureName kETC2 = WGPUFeatureName_TextureCompressionETC2;
constexpr WGPUFeatureName kBC = WGPUFeatureName_TextureCompressionBC;
constexpr WGPUFeatureName kDepth32Stencil8 = WGPUFeatureName_Depth32FloatStencil8;

using MappedTextureFormat =
    std::tuple<TextureFormat, WGPUTextureFormat, std::optional<WGPUFeatureName>>;

// Every TextureFormat the backend maps, with the feature it requires; all others must map to
// std::nullopt.
constexpr MappedTextureFormat kMappedTextureFormats[] = {
    {TextureFormat::R_UNorm8, WGPUTextureFormat_R8Unorm, kNoFeature},
    {TextureFormat::R_F16, WGPUTextureFormat_R16Float, kNoFeature},
    {TextureFormat::R_UInt16, WGPUTextureFormat_R16Uint, kNoFeature},
    {TextureFormat::R_UNorm16, WGPUTextureFormat_R16Unorm, kUnorm16},
    {TextureFormat::RG_UNorm8, WGPUTextureFormat_RG8Unorm, kNoFeature},
    {TextureFormat::RGBX_UNorm8, WGPUTextureFormat_RGBA8Unorm, kNoFeature},
    {TextureFormat::RGBA_UNorm8, WGPUTextureFormat_RGBA8Unorm, kNoFeature},
    {TextureFormat::BGRA_UNorm8, WGPUTextureFormat_BGRA8Unorm, kNoFeature},
    {TextureFormat::RGBA_SRGB, WGPUTextureFormat_RGBA8UnormSrgb, kNoFeature},
    {TextureFormat::BGRA_SRGB, WGPUTextureFormat_BGRA8UnormSrgb, kNoFeature},
    {TextureFormat::RG_F16, WGPUTextureFormat_RG16Float, kNoFeature},
    {TextureFormat::RG_UInt16, WGPUTextureFormat_RG16Uint, kNoFeature},
    {TextureFormat::RG_UNorm16, WGPUTextureFormat_RG16Unorm, kUnorm16},
    {TextureFormat::RGB10_A2_UNorm_Rev, WGPUTextureFormat_RGB10A2Unorm, kNoFeature},
    {TextureFormat::RGB10_A2_Uint_Rev, WGPUTextureFormat_RGB10A2Uint, kNoFeature},
    {TextureFormat::R_F32, WGPUTextureFormat_R32Float, kNoFeature},
    {TextureFormat::R_UInt32, WGPUTextureFormat_R32Uint, kNoFeature},
    {TextureFormat::RGBA_UNorm16, WGPUTextureFormat_RGBA16Unorm, kUnorm16},
    {TextureFormat::RGBA_F16, WGPUTextureFormat_RGBA16Float, kNoFeature},
    {TextureFormat::RG_F32, WGPUTextureFormat_RG32Float, kNoFeature},
    {TextureFormat::RGBA_UInt32, WGPUTextureFormat_RGBA32Uint, kNoFeature},
    {TextureFormat::RGBA_F32, WGPUTextureFormat_RGBA32Float, kNoFeature},
    {TextureFormat::RGBA_ASTC_4x4, WGPUTextureFormat_ASTC4x4Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_4x4, WGPUTextureFormat_ASTC4x4UnormSrgb, kASTC},
    {TextureFormat::RGBA_ASTC_5x4, WGPUTextureFormat_ASTC5x4Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_5x4, WGPUTextureFormat_ASTC5x4UnormSrgb, kASTC},
    {TextureFormat::RGBA_ASTC_5x5, WGPUTextureFormat_ASTC5x5Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_5x5, WGPUTextureFormat_ASTC5x5UnormSrgb, kASTC},
    {TextureFormat::RGBA_ASTC_6x5, WGPUTextureFormat_ASTC6x5Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_6x5, WGPUTextureFormat_ASTC6x5UnormSrgb, kASTC},
    {TextureFormat::RGBA_ASTC_6x6, WGPUTextureFormat_ASTC6x6Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_6x6, WGPUTextureFormat_ASTC6x6UnormSrgb, kASTC},
    {TextureFormat::RGBA_ASTC_8x5, WGPUTextureFormat_ASTC8x5Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_8x5, WGPUTextureFormat_ASTC8x5UnormSrgb, kASTC},
    {TextureFormat::RGBA_ASTC_8x6, WGPUTextureFormat_ASTC8x6Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_8x6, WGPUTextureFormat_ASTC8x6UnormSrgb, kASTC},
    {TextureFormat::RGBA_ASTC_8x8, WGPUTextureFormat_ASTC8x8Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_8x8, WGPUTextureFormat_ASTC8x8UnormSrgb, kASTC},
    {TextureFormat::RGBA_ASTC_10x5, WGPUTextureFormat_ASTC10x5Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_10x5, WGPUTextureFormat_ASTC10x5UnormSrgb, kASTC},
    {TextureFormat::RGBA_ASTC_10x6, WGPUTextureFormat_ASTC10x6Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_10x6, WGPUTextureFormat_ASTC10x6UnormSrgb, kASTC},
    {TextureFormat::RGBA_ASTC_10x8, WGPUTextureFormat_ASTC10x8Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_10x8, WGPUTextureFormat_ASTC10x8UnormSrgb, kASTC},
    {TextureFormat::RGBA_ASTC_10x10, WGPUTextureFormat_ASTC10x10Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_10x10, WGPUTextureFormat_ASTC10x10UnormSrgb, kASTC},
    {TextureFormat::RGBA_ASTC_12x10, WGPUTextureFormat_ASTC12x10Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_12x10, WGPUTextureFormat_ASTC12x10UnormSrgb, kASTC},
    {TextureFormat::RGBA_ASTC_12x12, WGPUTextureFormat_ASTC12x12Unorm, kASTC},
    {TextureFormat::SRGB8_A8_ASTC_12x12, WGPUTextureFormat_ASTC12x12UnormSrgb, kASTC},
    {TextureFormat::RGB8_ETC1, WGPUTextureFormat_ETC2RGB8Unorm, kETC2},
    {TextureFormat::RGB8_ETC2, WGPUTextureFormat_ETC2RGB8Unorm, kETC2},
    {TextureFormat::SRGB8_ETC2, WGPUTextureFormat_ETC2RGB8UnormSrgb, kETC2},
    {TextureFormat::RGB8_Punchthrough_A1_ETC2, WGPUTextureFormat_ETC2RGB8A1Unorm, kETC2},
    {TextureFormat::SRGB8_Punchthrough_A1_ETC2, WGPUTextureFormat_ETC2RGB8A1UnormSrgb, kETC2},
    {TextureFormat::RGBA8_EAC_ETC2, WGPUTextureFormat_ETC2RGBA8Unorm, kETC2},
    {TextureFormat::SRGB8_A8_EAC_ETC2, WGPUTextureFormat_ETC2RGBA8UnormSrgb, kETC2},
    {TextureFormat::RG_EAC_UNorm, WGPUTextureFormat_EACRG11Unorm, kETC2},
    {TextureFormat::RG_EAC_SNorm, WGPUTextureFormat_EACRG11Snorm, kETC2},
    {TextureFormat::R_EAC_UNorm, WGPUTextureFormat_EACR11Unorm, kETC2},
    {TextureFormat::R_EAC_SNorm, WGPUTextureFormat_EACR11Snorm, kETC2},
    {TextureFormat::RGBA_BC7_UNORM_4x4, WGPUTextureFormat_BC7RGBAUnorm, kBC},
    {TextureFormat::RGBA_BC7_SRGB_4x4, WGPUTextureFormat_BC7RGBAUnormSrgb, kBC},
    {TextureFormat::Z_UNorm16, WGPUTextureFormat_Depth16Unorm, kNoFeature},
    {TextureFormat::Z_UNorm24, WGPUTextureFormat_Depth24Plus, kNoFeature},
    {TextureFormat::Z_UNorm32, WGPUTextureFormat_Depth32Float, kNoFeature},
    {TextureFormat::S8_UInt_Z24_UNorm, WGPUTextureFormat_Depth24PlusStencil8, kNoFeature},
    {TextureFormat::S8_UInt_Z32_UNorm, WGPUTextureFormat_Depth32FloatStencil8, kDepth32Stencil8},
    {TextureFormat::S_UInt8, WGPUTextureFormat_Stencil8, kNoFeature},
    {TextureFormat::B10G11R11_UFloat, WGPUTextureFormat_RG11B10Ufloat, kNoFeature},
};

// Formats that alias another IGL format with the same layout; the reverse mapping returns the
// canonical one.
const std::pair<TextureFormat, TextureFormat> kAliasedTextureFormats[] = {
    {TextureFormat::RGBX_UNorm8, TextureFormat::RGBA_UNorm8},
    {TextureFormat::RGB8_ETC1, TextureFormat::RGB8_ETC2},
};

TextureFormat canonical(TextureFormat format) {
  for (const auto& [alias, target] : kAliasedTextureFormats) {
    if (alias == format) {
      return target;
    }
  }
  return format;
}

template<typename T>
concept ConvertibleToWGPUStringView =
    requires(T&& str) { webgpu::toWGPUStringView(std::forward<T>(str)); };

void onQueueWorkDone(WGPUQueueWorkDoneStatus status, void* userdata1, void* userdata2) {
  *static_cast<WGPUQueueWorkDoneStatus*>(userdata1) = status;
  ++*static_cast<int*>(userdata2);
}

} // namespace

// compat *************************************************************************************
TEST(WebGPUCompatTest, QueueWorkDoneCallbackForwardsStatusAndUserdata) {
  WGPUQueueWorkDoneStatus status = WGPUQueueWorkDoneStatus_Error;
  int numCalls = 0;
  const WGPUQueueWorkDoneCallbackInfo info =
      webgpu::compat::queueWorkDoneCallbackInfo<onQueueWorkDone>(
          WGPUCallbackMode_AllowProcessEvents, &status, &numCalls);
  EXPECT_EQ(info.nextInChain, nullptr);
  EXPECT_EQ(info.mode, WGPUCallbackMode_AllowProcessEvents);
  EXPECT_EQ(info.userdata1, &status);
  EXPECT_EQ(info.userdata2, &numCalls);
  ASSERT_NE(info.callback, nullptr);
#if IGL_WEBGPU_HEADER_V2
  info.callback(WGPUQueueWorkDoneStatus_Success,
                webgpu::toWGPUStringView("done"),
                info.userdata1,
                info.userdata2);
#else
  info.callback(WGPUQueueWorkDoneStatus_Success, info.userdata1, info.userdata2);
#endif
  EXPECT_EQ(status, WGPUQueueWorkDoneStatus_Success);
  EXPECT_EQ(numCalls, 1);
}

// Handle *************************************************************************************
TEST(WebGPUHandleTest, DefaultIsEmpty) {
  const FakeHandle handle;
  EXPECT_FALSE(handle);
  EXPECT_EQ(handle.get(), nullptr);
}

TEST(WebGPUHandleTest, AdoptsWithoutAddRefAndReleasesOnce) {
  FakeObject object;
  {
    const FakeHandle handle(&object);
    EXPECT_TRUE(handle);
    EXPECT_EQ(handle.get(), &object);
    EXPECT_EQ(object.numAddRefs, 0);
  }
  EXPECT_EQ(object.numReleases, 1);
  EXPECT_EQ(object.refCount, 0);
}

TEST(WebGPUHandleTest, RetainAddsReference) {
  FakeObject object;
  {
    const FakeHandle handle = FakeHandle::retain(&object);
    EXPECT_EQ(handle.get(), &object);
    EXPECT_EQ(object.refCount, 2);
  }
  EXPECT_EQ(object.refCount, 1);
  EXPECT_EQ(object.numAddRefs, 1);
  EXPECT_EQ(object.numReleases, 1);
}

TEST(WebGPUHandleTest, RetainNullIsEmpty) {
  const FakeHandle handle = FakeHandle::retain(nullptr);
  EXPECT_FALSE(handle);
}

TEST(WebGPUHandleTest, CopyAddsReference) {
  FakeObject object;
  {
    const FakeHandle a(&object);
    const FakeHandle b(a); // NOLINT(performance-unnecessary-copy-initialization)
    EXPECT_EQ(a.get(), &object);
    EXPECT_EQ(b.get(), &object);
    EXPECT_EQ(object.refCount, 2);
  }
  EXPECT_EQ(object.refCount, 0);
  EXPECT_EQ(object.numAddRefs, 1);
  EXPECT_EQ(object.numReleases, 2);
}

TEST(WebGPUHandleTest, CopyAssignReleasesPreviousObject) {
  FakeObject first;
  FakeObject second;
  FakeHandle a(&first);
  const FakeHandle b(&second);
  a = b;
  EXPECT_EQ(a.get(), &second);
  EXPECT_EQ(first.refCount, 0);
  EXPECT_EQ(second.refCount, 2);
}

TEST(WebGPUHandleTest, SelfCopyAssignKeepsReference) {
  FakeObject object;
  FakeHandle a(&object);
  const FakeHandle& alias = a;
  a = alias;
  EXPECT_EQ(a.get(), &object);
  EXPECT_EQ(object.refCount, 1);
}

TEST(WebGPUHandleTest, MoveTransfersWithoutRefCounting) {
  FakeObject object;
  FakeHandle a(&object);
  const FakeHandle b(std::move(a));
  EXPECT_FALSE(a); // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(b.get(), &object);
  EXPECT_EQ(object.numAddRefs, 0);
  EXPECT_EQ(object.numReleases, 0);
}

TEST(WebGPUHandleTest, MoveAssignReleasesPreviousObject) {
  FakeObject first;
  FakeObject second;
  FakeHandle a(&first);
  FakeHandle b(&second);
  a = std::move(b);
  EXPECT_EQ(a.get(), &second);
  EXPECT_FALSE(b); // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(first.refCount, 0);
  EXPECT_EQ(second.refCount, 1);
  EXPECT_EQ(second.numAddRefs, 0);
}

TEST(WebGPUHandleTest, AssignNullReleases) {
  FakeObject object;
  FakeHandle handle(&object);
  handle = nullptr;
  EXPECT_FALSE(handle);
  EXPECT_EQ(object.refCount, 0);
}

TEST(WebGPUHandleTest, ResetAdoptsNewObject) {
  FakeObject first;
  FakeObject second;
  FakeHandle handle(&first);
  handle.reset(&second);
  EXPECT_EQ(handle.get(), &second);
  EXPECT_EQ(first.refCount, 0);
  EXPECT_EQ(second.refCount, 1);
  EXPECT_EQ(second.numAddRefs, 0);
  handle.reset();
  EXPECT_FALSE(handle);
  EXPECT_EQ(second.refCount, 0);
}

TEST(WebGPUHandleTest, ReleaseGivesUpOwnership) {
  FakeObject object;
  {
    FakeHandle handle(&object);
    const FakeObject* raw = handle.release();
    EXPECT_EQ(raw, &object);
    EXPECT_FALSE(handle);
  }
  EXPECT_EQ(object.refCount, 1);
  EXPECT_EQ(object.numReleases, 0);
}

TEST(WebGPUHandleTest, SwapExchangesWithoutRefCounting) {
  FakeObject first;
  FakeObject second;
  FakeHandle a(&first);
  FakeHandle b(&second);
  a.swap(b);
  EXPECT_EQ(a.get(), &second);
  EXPECT_EQ(b.get(), &first);
  EXPECT_EQ(first.numAddRefs + first.numReleases, 0);
  EXPECT_EQ(second.numAddRefs + second.numReleases, 0);
}

// WGPUStringView ******************************************************************************
TEST(WebGPUCommonTest, ToWGPUStringView) {
  const WGPUStringView fromNull = webgpu::toWGPUStringView(nullptr);
  EXPECT_EQ(fromNull.data, nullptr);
  EXPECT_EQ(fromNull.length, WGPU_STRLEN);

  const char* literal = "label";
  const WGPUStringView fromLiteral = webgpu::toWGPUStringView(literal);
  EXPECT_EQ(fromLiteral.data, literal);
  EXPECT_EQ(fromLiteral.length, WGPU_STRLEN);

  const std::string str("abc");
  const WGPUStringView fromString = webgpu::toWGPUStringView(str);
  EXPECT_EQ(fromString.data, str.data());
  EXPECT_EQ(fromString.length, 3u);

  static_assert(ConvertibleToWGPUStringView<const std::string&>);
  static_assert(!ConvertibleToWGPUStringView<std::string>);
  static_assert(!ConvertibleToWGPUStringView<const std::string>);
}

TEST(WebGPUCommonTest, ToStdString) {
  EXPECT_EQ(webgpu::toStdString({.data = nullptr, .length = WGPU_STRLEN}), "");
  EXPECT_EQ(webgpu::toStdString({.data = nullptr, .length = 0}), "");
  EXPECT_EQ(webgpu::toStdString({.data = "abc", .length = WGPU_STRLEN}), "abc");
  // Explicit lengths are not null-terminated and may cut a longer buffer.
  EXPECT_EQ(webgpu::toStdString({.data = "abcdef", .length = 3}), "abc");
  EXPECT_EQ(webgpu::toStdString({.data = "abc", .length = 0}), "");
}

// Texture formats ****************************************************************************
TEST(WebGPUCommonTest, TextureFormats) {
  size_t numMapped = 0;
  for (uint8_t i = 0; i <= static_cast<uint8_t>(kLastTextureFormat); ++i) {
    const auto format = static_cast<TextureFormat>(i);
    std::optional<WGPUTextureFormat> expectedFormat;
    std::optional<WGPUFeatureName> expectedFeature;
    for (const auto& [iglFormat, wgpuFormat, requiredFeature] : kMappedTextureFormats) {
      if (iglFormat == format) {
        expectedFormat = wgpuFormat;
        expectedFeature = requiredFeature;
        ++numMapped;
      }
    }
    const char* name = TextureFormatProperties::fromTextureFormat(format).name;
    EXPECT_EQ(webgpu::textureFormatToWGPUTextureFormat(format), expectedFormat) << name;
    EXPECT_EQ(webgpu::getRequiredWGPUFeature(format), expectedFeature) << name;
  }
  EXPECT_EQ(numMapped, IGL_ARRAY_NUM_ELEMENTS(kMappedTextureFormats));
}

// The reverse mapping returns the canonical format, which must have the same memory layout.
TEST(WebGPUCommonTest, WGPUTextureFormatToTextureFormat) {
  for (const auto& [format, wgpuFormat, requiredFeature] : kMappedTextureFormats) {
    const auto props = TextureFormatProperties::fromTextureFormat(format);
    const TextureFormat reverse = webgpu::wgpuTextureFormatToTextureFormat(wgpuFormat);
    EXPECT_EQ(reverse, canonical(format)) << props.name;
    const auto reverseProps = TextureFormatProperties::fromTextureFormat(reverse);
    EXPECT_EQ(reverseProps.flags, props.flags) << props.name;
    EXPECT_EQ(reverseProps.bytesPerBlock, props.bytesPerBlock) << props.name;
    EXPECT_EQ(reverseProps.blockWidth, props.blockWidth) << props.name;
    EXPECT_EQ(reverseProps.blockHeight, props.blockHeight) << props.name;
  }
}

TEST(WebGPUCommonTest, UnsupportedTextureFormats) {
  const TextureFormat unsupported[] = {
      TextureFormat::Invalid,
      TextureFormat::A_UNorm8,
      TextureFormat::L_UNorm8,
      TextureFormat::LA_UNorm8,
      TextureFormat::B5G5R5A1_UNorm,
      TextureFormat::B5G6R5_UNorm,
      TextureFormat::ABGR_UNorm4,
      TextureFormat::R4G2B2_UNorm_Apple,
      TextureFormat::R4G2B2_UNorm_Rev_Apple,
      TextureFormat::R5G5B5A1_UNorm,
      TextureFormat::R5G6B5_UNorm,
      TextureFormat::BGRA_UNorm8_Rev,
      TextureFormat::BGR10_A2_Unorm,
      TextureFormat::RGB_F16,
      TextureFormat::RGB_F32,
      TextureFormat::RGBA_PVRTC_2BPPV1,
      TextureFormat::RGB_PVRTC_2BPPV1,
      TextureFormat::RGBA_PVRTC_4BPPV1,
      TextureFormat::RGB_PVRTC_4BPPV1,
      TextureFormat::YUV_NV12,
      TextureFormat::YUV_420p,
  };
  for (const TextureFormat format : unsupported) {
    EXPECT_EQ(webgpu::textureFormatToWGPUTextureFormat(format), std::nullopt)
        << TextureFormatProperties::fromTextureFormat(format).name;
  }
}

TEST(WebGPUCommonTest, UnmappedWGPUTextureFormatsAreInvalid) {
  const WGPUTextureFormat unmapped[] = {
      WGPUTextureFormat_Undefined,
      WGPUTextureFormat_R8Snorm,
      WGPUTextureFormat_R8Uint,
      WGPUTextureFormat_RGBA8Snorm,
      WGPUTextureFormat_RGB9E5Ufloat,
      WGPUTextureFormat_RGBA16Uint,
      WGPUTextureFormat_BC1RGBAUnorm,
      WGPUTextureFormat_BC6HRGBFloat,
      WGPUTextureFormat_R16Snorm,
  };
  for (const WGPUTextureFormat format : unmapped) {
    EXPECT_EQ(webgpu::wgpuTextureFormatToTextureFormat(format), TextureFormat::Invalid) << format;
  }
}

// Vertex formats *****************************************************************************
TEST(WebGPUCommonTest, VertexAttributeFormats) {
  const std::pair<VertexAttributeFormat, WGPUVertexFormat> mapped[] = {
      {VertexAttributeFormat::Float1, WGPUVertexFormat_Float32},
      {VertexAttributeFormat::Float2, WGPUVertexFormat_Float32x2},
      {VertexAttributeFormat::Float3, WGPUVertexFormat_Float32x3},
      {VertexAttributeFormat::Float4, WGPUVertexFormat_Float32x4},
      {VertexAttributeFormat::Byte1, WGPUVertexFormat_Sint8},
      {VertexAttributeFormat::Byte2, WGPUVertexFormat_Sint8x2},
      {VertexAttributeFormat::Byte4, WGPUVertexFormat_Sint8x4},
      {VertexAttributeFormat::UByte1, WGPUVertexFormat_Uint8},
      {VertexAttributeFormat::UByte2, WGPUVertexFormat_Uint8x2},
      {VertexAttributeFormat::UByte4, WGPUVertexFormat_Uint8x4},
      {VertexAttributeFormat::Short1, WGPUVertexFormat_Sint16},
      {VertexAttributeFormat::Short2, WGPUVertexFormat_Sint16x2},
      {VertexAttributeFormat::Short4, WGPUVertexFormat_Sint16x4},
      {VertexAttributeFormat::UShort1, WGPUVertexFormat_Uint16},
      {VertexAttributeFormat::UShort2, WGPUVertexFormat_Uint16x2},
      {VertexAttributeFormat::UShort4, WGPUVertexFormat_Uint16x4},
      {VertexAttributeFormat::Byte1Norm, WGPUVertexFormat_Snorm8},
      {VertexAttributeFormat::Byte2Norm, WGPUVertexFormat_Snorm8x2},
      {VertexAttributeFormat::Byte4Norm, WGPUVertexFormat_Snorm8x4},
      {VertexAttributeFormat::UByte1Norm, WGPUVertexFormat_Unorm8},
      {VertexAttributeFormat::UByte2Norm, WGPUVertexFormat_Unorm8x2},
      {VertexAttributeFormat::UByte4Norm, WGPUVertexFormat_Unorm8x4},
      {VertexAttributeFormat::Short1Norm, WGPUVertexFormat_Snorm16},
      {VertexAttributeFormat::Short2Norm, WGPUVertexFormat_Snorm16x2},
      {VertexAttributeFormat::Short4Norm, WGPUVertexFormat_Snorm16x4},
      {VertexAttributeFormat::UShort1Norm, WGPUVertexFormat_Unorm16},
      {VertexAttributeFormat::UShort2Norm, WGPUVertexFormat_Unorm16x2},
      {VertexAttributeFormat::UShort4Norm, WGPUVertexFormat_Unorm16x4},
      {VertexAttributeFormat::Int1, WGPUVertexFormat_Sint32},
      {VertexAttributeFormat::Int2, WGPUVertexFormat_Sint32x2},
      {VertexAttributeFormat::Int3, WGPUVertexFormat_Sint32x3},
      {VertexAttributeFormat::Int4, WGPUVertexFormat_Sint32x4},
      {VertexAttributeFormat::UInt1, WGPUVertexFormat_Uint32},
      {VertexAttributeFormat::UInt2, WGPUVertexFormat_Uint32x2},
      {VertexAttributeFormat::UInt3, WGPUVertexFormat_Uint32x3},
      {VertexAttributeFormat::UInt4, WGPUVertexFormat_Uint32x4},
      {VertexAttributeFormat::HalfFloat1, WGPUVertexFormat_Float16},
      {VertexAttributeFormat::HalfFloat2, WGPUVertexFormat_Float16x2},
      {VertexAttributeFormat::HalfFloat4, WGPUVertexFormat_Float16x4},
  };
  const VertexAttributeFormat unsupported[] = {
      VertexAttributeFormat::Byte3,
      VertexAttributeFormat::UByte3,
      VertexAttributeFormat::Short3,
      VertexAttributeFormat::UShort3,
      VertexAttributeFormat::Byte3Norm,
      VertexAttributeFormat::UByte3Norm,
      VertexAttributeFormat::Short3Norm,
      VertexAttributeFormat::UShort3Norm,
      VertexAttributeFormat::HalfFloat3,
      VertexAttributeFormat::Int_2_10_10_10_REV,
  };
  for (const auto& [iglFormat, wgpuFormat] : mapped) {
    EXPECT_EQ(webgpu::vertexAttributeFormatToWGPUVertexFormat(iglFormat), wgpuFormat)
        << static_cast<int>(iglFormat);
  }
  for (const VertexAttributeFormat format : unsupported) {
    EXPECT_EQ(webgpu::vertexAttributeFormatToWGPUVertexFormat(format), std::nullopt)
        << static_cast<int>(format);
  }
  // Together the two lists cover the whole enum.
  EXPECT_EQ(IGL_ARRAY_NUM_ELEMENTS(mapped) + IGL_ARRAY_NUM_ELEMENTS(unsupported),
            static_cast<size_t>(kLastVertexAttributeFormat) + 1);
}

// Blending ***********************************************************************************
TEST(WebGPUCommonTest, BlendFactors) {
  const std::pair<BlendFactor, std::optional<WGPUBlendFactor>> factors[] = {
      {BlendFactor::Zero, WGPUBlendFactor_Zero},
      {BlendFactor::One, WGPUBlendFactor_One},
      {BlendFactor::SrcColor, WGPUBlendFactor_Src},
      {BlendFactor::OneMinusSrcColor, WGPUBlendFactor_OneMinusSrc},
      {BlendFactor::SrcAlpha, WGPUBlendFactor_SrcAlpha},
      {BlendFactor::OneMinusSrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha},
      {BlendFactor::DstColor, WGPUBlendFactor_Dst},
      {BlendFactor::OneMinusDstColor, WGPUBlendFactor_OneMinusDst},
      {BlendFactor::DstAlpha, WGPUBlendFactor_DstAlpha},
      {BlendFactor::OneMinusDstAlpha, WGPUBlendFactor_OneMinusDstAlpha},
      {BlendFactor::SrcAlphaSaturated, WGPUBlendFactor_SrcAlphaSaturated},
      {BlendFactor::BlendColor, WGPUBlendFactor_Constant},
      {BlendFactor::OneMinusBlendColor, WGPUBlendFactor_OneMinusConstant},
      {BlendFactor::BlendAlpha, std::nullopt},
      {BlendFactor::OneMinusBlendAlpha, std::nullopt},
      {BlendFactor::Src1Color, WGPUBlendFactor_Src1},
      {BlendFactor::OneMinusSrc1Color, WGPUBlendFactor_OneMinusSrc1},
      {BlendFactor::Src1Alpha, WGPUBlendFactor_Src1Alpha},
      {BlendFactor::OneMinusSrc1Alpha, WGPUBlendFactor_OneMinusSrc1Alpha},
  };
  for (const auto& [iglFactor, wgpuFactor] : factors) {
    EXPECT_EQ(webgpu::blendFactorToWGPUBlendFactor(iglFactor), wgpuFactor)
        << static_cast<int>(iglFactor);
  }
  EXPECT_EQ(IGL_ARRAY_NUM_ELEMENTS(factors),
            static_cast<size_t>(BlendFactor::OneMinusSrc1Alpha) + 1);
}

TEST(WebGPUCommonTest, BlendOps) {
  EXPECT_EQ(webgpu::blendOpToWGPUBlendOperation(BlendOp::Add), WGPUBlendOperation_Add);
  EXPECT_EQ(webgpu::blendOpToWGPUBlendOperation(BlendOp::Subtract), WGPUBlendOperation_Subtract);
  EXPECT_EQ(webgpu::blendOpToWGPUBlendOperation(BlendOp::ReverseSubtract),
            WGPUBlendOperation_ReverseSubtract);
  EXPECT_EQ(webgpu::blendOpToWGPUBlendOperation(BlendOp::Min), WGPUBlendOperation_Min);
  EXPECT_EQ(webgpu::blendOpToWGPUBlendOperation(BlendOp::Max), WGPUBlendOperation_Max);
}

TEST(WebGPUCommonTest, ColorWriteMask) {
  EXPECT_EQ(webgpu::colorWriteMaskToWGPUColorWriteMask(kColorWriteBitsDisabled),
            WGPUColorWriteMask_None);
  EXPECT_EQ(webgpu::colorWriteMaskToWGPUColorWriteMask(kColorWriteBitsAll), WGPUColorWriteMask_All);
  EXPECT_EQ(webgpu::colorWriteMaskToWGPUColorWriteMask(kColorWriteBitsRed), WGPUColorWriteMask_Red);
  EXPECT_EQ(webgpu::colorWriteMaskToWGPUColorWriteMask(kColorWriteBitsGreen),
            WGPUColorWriteMask_Green);
  EXPECT_EQ(webgpu::colorWriteMaskToWGPUColorWriteMask(kColorWriteBitsBlue),
            WGPUColorWriteMask_Blue);
  EXPECT_EQ(webgpu::colorWriteMaskToWGPUColorWriteMask(kColorWriteBitsAlpha),
            WGPUColorWriteMask_Alpha);
  EXPECT_EQ(webgpu::colorWriteMaskToWGPUColorWriteMask(kColorWriteBitsRed | kColorWriteBitsAlpha),
            WGPUColorWriteMask_Red | WGPUColorWriteMask_Alpha);
}

// Depth and stencil **************************************************************************
TEST(WebGPUCommonTest, CompareFunctions) {
  const std::pair<CompareFunction, WGPUCompareFunction> funcs[] = {
      {CompareFunction::Never, WGPUCompareFunction_Never},
      {CompareFunction::Less, WGPUCompareFunction_Less},
      {CompareFunction::Equal, WGPUCompareFunction_Equal},
      {CompareFunction::LessEqual, WGPUCompareFunction_LessEqual},
      {CompareFunction::Greater, WGPUCompareFunction_Greater},
      {CompareFunction::NotEqual, WGPUCompareFunction_NotEqual},
      {CompareFunction::GreaterEqual, WGPUCompareFunction_GreaterEqual},
      {CompareFunction::AlwaysPass, WGPUCompareFunction_Always},
  };
  for (const auto& [iglFunc, wgpuFunc] : funcs) {
    EXPECT_EQ(webgpu::compareFunctionToWGPUCompareFunction(iglFunc), wgpuFunc)
        << static_cast<int>(iglFunc);
  }
}

TEST(WebGPUCommonTest, StencilOperations) {
  const std::pair<StencilOperation, WGPUStencilOperation> ops[] = {
      {StencilOperation::Keep, WGPUStencilOperation_Keep},
      {StencilOperation::Zero, WGPUStencilOperation_Zero},
      {StencilOperation::Replace, WGPUStencilOperation_Replace},
      {StencilOperation::IncrementClamp, WGPUStencilOperation_IncrementClamp},
      {StencilOperation::DecrementClamp, WGPUStencilOperation_DecrementClamp},
      {StencilOperation::Invert, WGPUStencilOperation_Invert},
      {StencilOperation::IncrementWrap, WGPUStencilOperation_IncrementWrap},
      {StencilOperation::DecrementWrap, WGPUStencilOperation_DecrementWrap},
  };
  for (const auto& [iglOp, wgpuOp] : ops) {
    EXPECT_EQ(webgpu::stencilOperationToWGPUStencilOperation(iglOp), wgpuOp)
        << static_cast<int>(iglOp);
  }
}

// Samplers ***********************************************************************************
TEST(WebGPUCommonTest, SamplerAddressModes) {
  EXPECT_EQ(webgpu::samplerAddressModeToWGPUAddressMode(SamplerAddressMode::Repeat),
            WGPUAddressMode_Repeat);
  EXPECT_EQ(webgpu::samplerAddressModeToWGPUAddressMode(SamplerAddressMode::Clamp),
            WGPUAddressMode_ClampToEdge);
  EXPECT_EQ(webgpu::samplerAddressModeToWGPUAddressMode(SamplerAddressMode::MirrorRepeat),
            WGPUAddressMode_MirrorRepeat);
  EXPECT_EQ(webgpu::samplerAddressModeToWGPUAddressMode(SamplerAddressMode::ClampToBorder),
            std::nullopt);
}

TEST(WebGPUCommonTest, SamplerFilters) {
  EXPECT_EQ(webgpu::samplerMinMagFilterToWGPUFilterMode(SamplerMinMagFilter::Nearest),
            WGPUFilterMode_Nearest);
  EXPECT_EQ(webgpu::samplerMinMagFilterToWGPUFilterMode(SamplerMinMagFilter::Linear),
            WGPUFilterMode_Linear);
  EXPECT_EQ(webgpu::samplerMipFilterToWGPUMipmapFilterMode(SamplerMipFilter::Disabled),
            WGPUMipmapFilterMode_Nearest);
  EXPECT_EQ(webgpu::samplerMipFilterToWGPUMipmapFilterMode(SamplerMipFilter::Nearest),
            WGPUMipmapFilterMode_Nearest);
  EXPECT_EQ(webgpu::samplerMipFilterToWGPUMipmapFilterMode(SamplerMipFilter::Linear),
            WGPUMipmapFilterMode_Linear);
}

// Rasterization ******************************************************************************
TEST(WebGPUCommonTest, PrimitiveTopologies) {
  EXPECT_EQ(webgpu::primitiveTypeToWGPUPrimitiveTopology(PrimitiveType::Point),
            WGPUPrimitiveTopology_PointList);
  EXPECT_EQ(webgpu::primitiveTypeToWGPUPrimitiveTopology(PrimitiveType::Line),
            WGPUPrimitiveTopology_LineList);
  EXPECT_EQ(webgpu::primitiveTypeToWGPUPrimitiveTopology(PrimitiveType::LineStrip),
            WGPUPrimitiveTopology_LineStrip);
  EXPECT_EQ(webgpu::primitiveTypeToWGPUPrimitiveTopology(PrimitiveType::Triangle),
            WGPUPrimitiveTopology_TriangleList);
  EXPECT_EQ(webgpu::primitiveTypeToWGPUPrimitiveTopology(PrimitiveType::TriangleStrip),
            WGPUPrimitiveTopology_TriangleStrip);
}

TEST(WebGPUCommonTest, CullModeAndWinding) {
  EXPECT_EQ(webgpu::cullModeToWGPUCullMode(CullMode::Disabled), WGPUCullMode_None);
  EXPECT_EQ(webgpu::cullModeToWGPUCullMode(CullMode::Front), WGPUCullMode_Front);
  EXPECT_EQ(webgpu::cullModeToWGPUCullMode(CullMode::Back), WGPUCullMode_Back);
  EXPECT_EQ(webgpu::windingModeToWGPUFrontFace(WindingMode::Clockwise), WGPUFrontFace_CW);
  EXPECT_EQ(webgpu::windingModeToWGPUFrontFace(WindingMode::CounterClockwise), WGPUFrontFace_CCW);
}

TEST(WebGPUCommonTest, IndexFormats) {
  EXPECT_EQ(webgpu::indexFormatToWGPUIndexFormat(IndexFormat::UInt8), std::nullopt);
  EXPECT_EQ(webgpu::indexFormatToWGPUIndexFormat(IndexFormat::UInt16), WGPUIndexFormat_Uint16);
  EXPECT_EQ(webgpu::indexFormatToWGPUIndexFormat(IndexFormat::UInt32), WGPUIndexFormat_Uint32);
}

// Textures and passes ************************************************************************
TEST(WebGPUCommonTest, TextureDimensions) {
  const TextureType types[] = {
      TextureType::Invalid,
      TextureType::TwoD,
      TextureType::TwoDArray,
      TextureType::ThreeD,
      TextureType::Cube,
      TextureType::ExternalImage,
  };
  const std::optional<WGPUTextureDimension> dimensions[] = {
      std::nullopt,
      WGPUTextureDimension_2D,
      WGPUTextureDimension_2D,
      WGPUTextureDimension_3D,
      WGPUTextureDimension_2D,
      std::nullopt,
  };
  const std::optional<WGPUTextureViewDimension> viewDimensions[] = {
      std::nullopt,
      WGPUTextureViewDimension_2D,
      WGPUTextureViewDimension_2DArray,
      WGPUTextureViewDimension_3D,
      WGPUTextureViewDimension_Cube,
      std::nullopt,
  };
  for (size_t i = 0; i < IGL_ARRAY_NUM_ELEMENTS(types); ++i) {
    EXPECT_EQ(webgpu::textureTypeToWGPUTextureDimension(types[i]), dimensions[i]) << i;
    EXPECT_EQ(webgpu::textureTypeToWGPUTextureViewDimension(types[i]), viewDimensions[i]) << i;
  }
}

TEST(WebGPUCommonTest, LoadAndStoreActions) {
  EXPECT_EQ(webgpu::loadActionToWGPULoadOp(LoadAction::DontCare), WGPULoadOp_Clear);
  EXPECT_EQ(webgpu::loadActionToWGPULoadOp(LoadAction::Load), WGPULoadOp_Load);
  EXPECT_EQ(webgpu::loadActionToWGPULoadOp(LoadAction::Clear), WGPULoadOp_Clear);
  EXPECT_EQ(webgpu::storeActionToWGPUStoreOp(StoreAction::DontCare), WGPUStoreOp_Discard);
  EXPECT_EQ(webgpu::storeActionToWGPUStoreOp(StoreAction::Store), WGPUStoreOp_Store);
  EXPECT_EQ(webgpu::storeActionToWGPUStoreOp(StoreAction::MsaaResolve), WGPUStoreOp_Discard);
}

// Errors *************************************************************************************
TEST(WebGPUCommonTest, ErrorTypeToResultCode) {
  EXPECT_EQ(webgpu::wgpuErrorTypeToResultCode(WGPUErrorType_NoError), Result::Code::Ok);
  EXPECT_EQ(webgpu::wgpuErrorTypeToResultCode(WGPUErrorType_Validation),
            Result::Code::ArgumentInvalid);
  EXPECT_EQ(webgpu::wgpuErrorTypeToResultCode(WGPUErrorType_OutOfMemory),
            Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(webgpu::wgpuErrorTypeToResultCode(WGPUErrorType_Internal), Result::Code::RuntimeError);
  EXPECT_EQ(webgpu::wgpuErrorTypeToResultCode(WGPUErrorType_Unknown), Result::Code::RuntimeError);
  EXPECT_EQ(webgpu::wgpuErrorTypeToResultCode(static_cast<WGPUErrorType>(0x100)),
            Result::Code::RuntimeError);
}

TEST(WebGPUCommonTest, GetResultFromWGPUError) {
  const Result ok =
      webgpu::getResultFromWGPUError(WGPUErrorType_NoError, webgpu::toWGPUStringView("ignored"));
  EXPECT_TRUE(ok.isOk());
  EXPECT_TRUE(ok.message.empty());

  const Result validation = webgpu::getResultFromWGPUError(
      WGPUErrorType_Validation, webgpu::toWGPUStringView("bad descriptor"));
  EXPECT_EQ(validation.code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(validation.message, "bad descriptor");

  const std::pair<WGPUErrorType, const char*> fallbacks[] = {
      {WGPUErrorType_Validation, "WebGPU validation error"},
      {WGPUErrorType_OutOfMemory, "WebGPU out of memory"},
      {WGPUErrorType_Internal, "WebGPU internal error"},
      {WGPUErrorType_Unknown, "WebGPU unknown error"},
      {static_cast<WGPUErrorType>(0x100), "WebGPU error"},
  };
  for (const auto& [type, message] : fallbacks) {
    const Result fromNull = webgpu::getResultFromWGPUError(type, {.data = nullptr, .length = 0});
    EXPECT_EQ(fromNull.code, webgpu::wgpuErrorTypeToResultCode(type)) << message;
    EXPECT_EQ(fromNull.message, message);
    const Result fromEmpty = webgpu::getResultFromWGPUError(type, webgpu::toWGPUStringView(""));
    EXPECT_EQ(fromEmpty.message, message);
  }
}

TEST(WebGPUCommonTest, SetResultFrom) {
  webgpu::setResultFrom(nullptr, WGPUErrorType_Validation, webgpu::toWGPUStringView("unused"));

  Result result(Result::Code::RuntimeError, "stale");
  webgpu::setResultFrom(&result, WGPUErrorType_NoError, webgpu::toWGPUStringView(nullptr));
  EXPECT_TRUE(result.isOk());
  EXPECT_TRUE(result.message.empty());

  webgpu::setResultFrom(&result, WGPUErrorType_OutOfMemory, webgpu::toWGPUStringView("oom"));
  EXPECT_EQ(result.code, Result::Code::ArgumentOutOfRange);
  EXPECT_EQ(result.message, "oom");
}

} // namespace igl::tests
