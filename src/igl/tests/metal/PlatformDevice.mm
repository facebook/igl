/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/metal/PlatformDevice.h>

#include "../util/Common.h"

#import <Metal/Metal.h>
#include <array>
#include <cstring>
#include <igl/IGL.h>
#include <igl/metal/Device.h>
#include <igl/metal/Shader.h>

namespace igl::tests {

class PlatformDeviceMetalTest : public ::testing::Test {
 public:
  PlatformDeviceMetalTest() = default;

  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);

    ASSERT_NE(iglDev_, nullptr);
    ASSERT_NE(cmdQueue_, nullptr);
  }
  void TearDown() override {}

 protected:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

// Test Cases
TEST_F(PlatformDeviceMetalTest, GetPlatformDeviceParentCls) {
  auto* pd = iglDev_->getPlatformDevice<metal::PlatformDevice>();
  ASSERT_NE(pd, nullptr);
}

TEST_F(PlatformDeviceMetalTest, DynamicLibrarySupportFollowsTheDevice) {
  const auto* pd = iglDev_->getPlatformDevice<metal::PlatformDevice>();
  ASSERT_NE(pd, nullptr);
  bool expected = false;
  if (@available(macOS 12.0, iOS 15.0, *)) {
    id<MTLDevice> device = static_cast<metal::Device&>(*iglDev_).get();
    expected = device.supportsDynamicLibraries && device.supportsRenderDynamicLibraries;
  }
  EXPECT_EQ(pd->supportsDynamicLibraries(), expected);
}

TEST_F(PlatformDeviceMetalTest, StagesFromTheDeviceLinkNoLibraries) {
  Result res;
  auto stages = iglDev_->createShaderStages(ShaderStagesDesc::fromComputeModule(nullptr), &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(stages, nullptr);
  EXPECT_TRUE(static_cast<const metal::ShaderStages&>(*stages).getLinkedLibraries().empty());
}

namespace {

// tint() returns (red, green, 0, 1), scaled by the input's alpha.
NSString* tintSource(int red, int green) {
  return [NSString stringWithFormat:
                       @"#include <metal_stdlib>\n"
                        "using namespace metal;\n"
                        "float4 tint(float4 c) { return float4(%d.0, %d.0, 0.0, 1.0) "
                        "* c.a; }\n",
                       red,
                       green];
}

// fill() and tintFragment() call tint() from whichever dynamic library their pipeline links.
NSString* const kFillSource =
    @"#include <metal_stdlib>\n"
     "using namespace metal;\n"
     "extern float4 tint(float4 c);\n"
     "kernel void fill(device float4* out [[buffer(0)]], uint id [[thread_position_in_grid]]) {\n"
     "  out[id] = tint(float4(1.0));\n"
     "}\n";

NSString* const kCoverSource =
    @"#include <metal_stdlib>\n"
     "using namespace metal;\n"
     "extern float4 tint(float4 c);\n"
     "struct Cover { float4 position [[position]]; };\n"
     "vertex Cover coverVertex(uint id [[vertex_id]]) {\n"
     "  const float2 uv = float2((id << 1) & 2, id & 2);\n"
     "  return Cover{float4(uv * 2.0 - 1.0, 0.0, 1.0)};\n"
     "}\n"
     "fragment float4 tintFragment() {\n"
     "  return tint(float4(1.0));\n"
     "}\n";

} // namespace

//
// MetalDynamicLibraryTest
//
// Covers metal::PlatformDevice's dynamic libraries. The libraries are compiled from source at run
// time with the install name of a file in a temporary directory, because Metal links a pipeline
// only when a library exists at the install name its functions recorded.
//
class MetalDynamicLibraryTest : public PlatformDeviceMetalTest {
 public:
  void SetUp() override {
    PlatformDeviceMetalTest::SetUp();
    platformDevice_ = iglDev_->getPlatformDevice<metal::PlatformDevice>();
    ASSERT_NE(platformDevice_, nullptr);
    if (!platformDevice_->supportsDynamicLibraries()) {
      GTEST_SKIP() << "This device cannot link Metal dynamic libraries";
    }
    directory_ = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSUUID UUID].UUIDString];
    ASSERT_TRUE([[NSFileManager defaultManager] createDirectoryAtPath:directory_
                                          withIntermediateDirectories:YES
                                                           attributes:nil
                                                                error:nil]);
    installName_ = [directory_ stringByAppendingPathComponent:@"libtint.metallib"];
  }

  void TearDown() override {
    if (directory_ != nil) {
      [[NSFileManager defaultManager] removeItemAtPath:directory_ error:nil];
    }
  }

 protected:
  // Compiles tint() into a dynamic library with installName_, writes it to `path`, and returns the
  // file's bytes: a dynamic .metallib, as `metal -dynamiclib` builds one.
  [[nodiscard]] NSData* buildTint(int red, int green, NSString* path) const {
    if (@available(macOS 12.0, iOS 15.0, *)) {
      id<MTLDevice> device = static_cast<metal::Device&>(*iglDev_).get();
      MTLCompileOptions* options = [MTLCompileOptions new];
      options.libraryType = MTLLibraryTypeDynamic;
      options.installName = installName_;
      NSError* error = nil;
      id<MTLLibrary> library = [device newLibraryWithSource:tintSource(red, green)
                                                    options:options
                                                      error:&error];
      if (library == nil) {
        return nil;
      }
      id<MTLDynamicLibrary> dynamicLibrary = [device newDynamicLibrary:library error:&error];
      if (![dynamicLibrary serializeToURL:[NSURL fileURLWithPath:path] error:&error]) {
        return nil;
      }
      return [NSData dataWithContentsOfFile:path];
    }
    return nil;
  }

  [[nodiscard]] std::shared_ptr<metal::DynamicLibrary> createTint(int red,
                                                                  int green,
                                                                  NSString* path) const {
    NSData* bytes = buildTint(red, green, path);
    if (bytes == nil) {
      return nullptr;
    }
    Result res;
    auto library = platformDevice_->createDynamicLibrary(bytes.bytes, bytes.length, "tint", &res);
    return res.isOk() ? library : nullptr;
  }

  // `source`, compiled against `library`, so that its functions record the library's install name.
  [[nodiscard]] id<MTLLibrary> compileAgainst(const metal::DynamicLibrary& library,
                                              NSString* source) const {
    if (@available(macOS 12.0, iOS 15.0, *)) {
      id<MTLDevice> device = static_cast<metal::Device&>(*iglDev_).get();
      MTLCompileOptions* options = [MTLCompileOptions new];
      options.libraries = @[ library.get() ];
      return [device newLibraryWithSource:source options:options error:nil];
    }
    return nil;
  }

  [[nodiscard]] static std::shared_ptr<IShaderModule> createModule(id<MTLLibrary> library,
                                                                   const std::string& entryPoint,
                                                                   ShaderStage stage) {
    NSString* name = [NSString stringWithUTF8String:entryPoint.c_str()];
    if (name == nil) {
      return nullptr;
    }
    id<MTLFunction> function = [library newFunctionWithName:name];
    if (function == nil) {
      return nullptr;
    }
    return std::make_shared<metal::ShaderModule>(
        ShaderModuleInfo{.stage = stage, .entryPoint = entryPoint, .debugName = entryPoint},
        function);
  }

  // Runs fill() on one thread through a pipeline created from `stages`, and returns its output.
  [[nodiscard]] std::array<float, 4> runFill(std::shared_ptr<IShaderStages> stages) const {
    std::array<float, 4> output{};
    Result res;
    auto pipeline = iglDev_->createComputePipeline({.shaderStages = std::move(stages)}, &res);
    EXPECT_TRUE(res.isOk()) << res.message;
    if (pipeline == nullptr) {
      return output;
    }
    auto buffer = iglDev_->createBuffer(
        BufferDesc(
            BufferDesc::BufferTypeBits::Storage, nullptr, sizeof(output), ResourceStorage::Shared),
        &res);
    EXPECT_TRUE(res.isOk()) << res.message;
    auto commandBuffer = cmdQueue_->createCommandBuffer({}, &res);
    EXPECT_TRUE(res.isOk()) << res.message;
    if (buffer == nullptr || commandBuffer == nullptr) {
      return output;
    }
    auto encoder = commandBuffer->createComputeCommandEncoder();
    encoder->bindComputePipelineState(pipeline);
    encoder->bindBuffer(0, buffer.get(), 0, sizeof(output));
    encoder->dispatchThreadGroups(Dimensions(1, 1, 1), Dimensions(1, 1, 1), Dependencies());
    encoder->endEncoding();
    cmdQueue_->submit(*commandBuffer);
    commandBuffer->waitUntilCompleted();
    const void* data = buffer->map(BufferRange(sizeof(output), 0), &res);
    EXPECT_TRUE(res.isOk()) << res.message;
    if (data != nullptr) {
      std::memcpy(output.data(), data, sizeof(output));
    }
    buffer->unmap();
    return output;
  }

  // Draws a triangle that covers a 1x1 framebuffer with a pipeline created from `stages`, and
  // returns the pixel.
  [[nodiscard]] std::array<uint8_t, 4> runCover(std::shared_ptr<IShaderStages> stages) const {
    std::array<uint8_t, 4> pixel{};
    Result res;
    const RenderPipelineDesc pipelineDesc{
        .shaderStages = std::move(stages),
        .targetDesc = {.colorAttachments = {{.textureFormat = TextureFormat::RGBA_UNorm8}}},
    };
    auto pipeline = iglDev_->createRenderPipeline(pipelineDesc, &res);
    EXPECT_TRUE(res.isOk()) << res.message;
    auto texture =
        iglDev_->createTexture(TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                  1,
                                                  1,
                                                  TextureDesc::TextureUsageBits::Sampled |
                                                      TextureDesc::TextureUsageBits::Attachment),
                               &res);
    EXPECT_TRUE(res.isOk()) << res.message;
    auto framebuffer =
        iglDev_->createFramebuffer({.colorAttachments = {{.texture = texture}}}, &res);
    EXPECT_TRUE(res.isOk()) << res.message;
    auto commandBuffer = cmdQueue_->createCommandBuffer({}, &res);
    EXPECT_TRUE(res.isOk()) << res.message;
    if (pipeline == nullptr || framebuffer == nullptr || commandBuffer == nullptr) {
      return pixel;
    }
    const RenderPassDesc renderPass{
        .colorAttachments = {{.loadAction = LoadAction::Clear, .storeAction = StoreAction::Store}}};
    auto encoder = commandBuffer->createRenderCommandEncoder(renderPass, framebuffer);
    encoder->bindRenderPipelineState(pipeline);
    encoder->draw(3);
    encoder->endEncoding();
    cmdQueue_->submit(*commandBuffer);
    commandBuffer->waitUntilCompleted();
    framebuffer->copyBytesColorAttachment(
        *cmdQueue_, 0, pixel.data(), TextureRangeDesc::new2D(0, 0, 1, 1), pixel.size());
    return pixel;
  }

  const metal::PlatformDevice* platformDevice_ = nullptr;
  NSString* directory_ = nil;
  NSString* installName_ = nil;
};

TEST_F(MetalDynamicLibraryTest, CreatesALibraryFromADynamicMetallib) {
  NSData* bytes = buildTint(1, 0, [directory_ stringByAppendingPathComponent:@"red.metallib"]);
  ASSERT_TRUE(bytes != nil);

  Result res;
  auto library = platformDevice_->createDynamicLibrary(bytes.bytes, bytes.length, "red", &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(library, nullptr);
  const char* installName = installName_.UTF8String;
  ASSERT_NE(installName, nullptr);
  EXPECT_EQ(library->getInstallName(), installName);
  if (@available(macOS 12.0, iOS 15.0, *)) {
    EXPECT_STREQ(library->get().label.UTF8String, "red");
  }
}

TEST_F(MetalDynamicLibraryTest, RefusesDataThatIsNotALibrary) {
  const std::array<uint8_t, 64> junk{1, 2, 3};
  Result res;
  EXPECT_EQ(platformDevice_->createDynamicLibrary(junk.data(), junk.size(), "junk", &res), nullptr);
  EXPECT_EQ(res.code, Result::Code::ArgumentInvalid);

  EXPECT_EQ(platformDevice_->createDynamicLibrary(nullptr, 0, "empty", &res), nullptr);
  EXPECT_EQ(res.code, Result::Code::ArgumentNull);
}

TEST_F(MetalDynamicLibraryTest, RelinksStagesToAnotherLibraryWithoutRecompiling) {
  // The red library sits at the install name, where every pipeline finds it by default.
  auto red = createTint(1, 0, installName_);
  auto green = createTint(0, 1, [directory_ stringByAppendingPathComponent:@"green.metallib"]);
  ASSERT_NE(red, nullptr);
  ASSERT_NE(green, nullptr);
  ASSERT_EQ(green->getInstallName(), red->getInstallName());
  auto fill = createModule(compileAgainst(*red, kFillSource), "fill", ShaderStage::Compute);
  ASSERT_NE(fill, nullptr);

  Result res;
  auto defaultStages =
      platformDevice_->createShaderStages(ShaderStagesDesc::fromComputeModule(fill), {}, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  EXPECT_EQ(runFill(std::move(defaultStages)), (std::array<float, 4>{1.0f, 0.0f, 0.0f, 1.0f}));

  auto greenStages =
      platformDevice_->createShaderStages(ShaderStagesDesc::fromComputeModule(fill), {green}, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  ASSERT_NE(greenStages, nullptr);
  ASSERT_EQ(static_cast<const metal::ShaderStages&>(*greenStages).getLinkedLibraries().size(), 1u);
  EXPECT_EQ(runFill(std::move(greenStages)), (std::array<float, 4>{0.0f, 1.0f, 0.0f, 1.0f}));
}

TEST_F(MetalDynamicLibraryTest, RelinksRenderStagesToAnotherLibrary) {
  auto red = createTint(1, 0, installName_);
  auto green = createTint(0, 1, [directory_ stringByAppendingPathComponent:@"green.metallib"]);
  ASSERT_NE(red, nullptr);
  ASSERT_NE(green, nullptr);
  id<MTLLibrary> cover = compileAgainst(*red, kCoverSource);
  auto vertex = createModule(cover, "coverVertex", ShaderStage::Vertex);
  auto fragment = createModule(cover, "tintFragment", ShaderStage::Fragment);
  ASSERT_NE(vertex, nullptr);
  ASSERT_NE(fragment, nullptr);

  Result res;
  auto defaultStages = platformDevice_->createShaderStages(
      ShaderStagesDesc::fromRenderModules(vertex, fragment), {}, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  EXPECT_EQ(runCover(std::move(defaultStages)), (std::array<uint8_t, 4>{255, 0, 0, 255}));

  auto greenStages = platformDevice_->createShaderStages(
      ShaderStagesDesc::fromRenderModules(vertex, fragment), {green}, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  EXPECT_EQ(runCover(std::move(greenStages)), (std::array<uint8_t, 4>{0, 255, 0, 255}));
}

TEST_F(MetalDynamicLibraryTest, RefusesANullLibrary) {
  Result res;
  EXPECT_EQ(platformDevice_->createShaderStages(
                ShaderStagesDesc::fromComputeModule(nullptr), {nullptr}, &res),
            nullptr);
  EXPECT_EQ(res.code, Result::Code::ArgumentNull);
}

TEST_F(MetalDynamicLibraryTest, MeshPipelinesRefuseLinkedLibraries) {
  if (!iglDev_->hasFeature(DeviceFeatures::MeshShaders)) {
    GTEST_SKIP() << "Mesh shaders not supported on this device";
  }
  auto red = createTint(1, 0, installName_);
  ASSERT_NE(red, nullptr);

  Result res;
  auto stages = platformDevice_->createShaderStages(
      ShaderStagesDesc::fromMeshRenderModules(nullptr, nullptr, nullptr), {red}, &res);
  ASSERT_TRUE(res.isOk()) << res.message;
  const RenderPipelineDesc desc{.shaderStages = std::move(stages)};
  EXPECT_EQ(iglDev_->createRenderPipeline(desc, &res), nullptr);
  EXPECT_EQ(res.code, Result::Code::Unsupported);
}

} // namespace igl::tests
