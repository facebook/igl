/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <igl/webgpu/WebGPUContext.h>

#include <memory>
#include <vector>
#include <igl/webgpu/HWDevice.h>

namespace igl::tests {

namespace {

using webgpu::WebGPUContext;
using webgpu::WebGPUContextDesc;

std::unique_ptr<WebGPUContext> createContextWithDevice(const WebGPUContextDesc& desc) {
  Result ret;
  auto ctx = webgpu::HWDevice::createContext(desc, &ret);
  EXPECT_TRUE(ret.isOk()) << ret.message;
  if (!ctx) {
    return nullptr;
  }
  ret = ctx->initDevice();
  EXPECT_TRUE(ret.isOk()) << ret.message;
  return ret.isOk() ? std::move(ctx) : nullptr;
}

WGPUBuffer createInvalidBuffer(const WebGPUContext& ctx) {
  WGPUBufferDescriptor desc = WGPU_BUFFER_DESCRIPTOR_INIT;
  // MapRead may only be combined with CopyDst.
  desc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_Uniform;
  desc.size = 16;
  return wgpuDeviceCreateBuffer(ctx.getDevice(), &desc);
}

} // namespace

class WebGPUContextTest : public ::testing::Test {
 public:
  void SetUp() override {
    setDebugBreakEnabled(false);
  }
};

TEST_F(WebGPUContextTest, ParseBackendType) {
  EXPECT_EQ(WebGPUContext::parseBackendType("metal"), WGPUBackendType_Metal);
  EXPECT_EQ(WebGPUContext::parseBackendType("vulkan"), WGPUBackendType_Vulkan);
  EXPECT_EQ(WebGPUContext::parseBackendType("null"), WGPUBackendType_Null);
  EXPECT_EQ(WebGPUContext::parseBackendType("d3d12"), WGPUBackendType_D3D12);
  EXPECT_EQ(WebGPUContext::parseBackendType("d3d11"), WGPUBackendType_D3D11);
  EXPECT_EQ(WebGPUContext::parseBackendType("opengl"), WGPUBackendType_OpenGL);
  EXPECT_EQ(WebGPUContext::parseBackendType("opengles"), WGPUBackendType_OpenGLES);
  EXPECT_FALSE(WebGPUContext::parseBackendType("Metal").has_value());
  EXPECT_FALSE(WebGPUContext::parseBackendType("").has_value());
}

TEST_F(WebGPUContextTest, ResolveBackendTypeKeepsExplicitChoice) {
  EXPECT_EQ(WebGPUContext::resolveBackendType(WGPUBackendType_Null), WGPUBackendType_Null);
  EXPECT_EQ(WebGPUContext::resolveBackendType(WGPUBackendType_Vulkan), WGPUBackendType_Vulkan);
  EXPECT_NE(WebGPUContext::resolveBackendType(WGPUBackendType_Undefined),
            WGPUBackendType_Undefined);
}

TEST_F(WebGPUContextTest, CreateDefaultDevice) {
  auto ctx = createContextWithDevice({});
  ASSERT_NE(ctx, nullptr);
  EXPECT_NE(ctx->getInstance(), nullptr);
  EXPECT_NE(ctx->getAdapter(), nullptr);
  EXPECT_NE(ctx->getDevice(), nullptr);
  EXPECT_NE(ctx->getQueue(), nullptr);
  EXPECT_TRUE(ctx->hasTimedWaitAny());
  EXPECT_EQ(ctx->getBackendType(), WebGPUContext::resolveBackendType(WGPUBackendType_Undefined));
  EXPECT_FALSE(ctx->isDeviceLost());
  EXPECT_EQ(ctx->getUncapturedErrorCount(), 0u);
}

TEST_F(WebGPUContextTest, CreateNullBackendDevice) {
  auto ctx = createContextWithDevice({.backendType = WGPUBackendType_Null});
  ASSERT_NE(ctx, nullptr);
  EXPECT_EQ(ctx->getBackendType(), WGPUBackendType_Null);
}

TEST_F(WebGPUContextTest, InitDeviceTwiceFails) {
  auto ctx = createContextWithDevice({});
  ASSERT_NE(ctx, nullptr);
  EXPECT_EQ(ctx->initDevice().code, Result::Code::InvalidOperation);
}

TEST_F(WebGPUContextTest, RequiredFeatureMissingFails) {
  Result ret;
  auto ctx =
      webgpu::HWDevice::createContext({.backendType = WGPUBackendType_Null,
                                       .requiredFeatures = {WGPUFeatureName_TextureCompressionETC2,
                                                            WGPUFeatureName_TextureCompressionASTC,
                                                            WGPUFeatureName_TextureCompressionBC}},
                                      &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(ctx, nullptr);
  bool allSupported = true;
  for (const WGPUFeatureName feature : ctx->getDesc().requiredFeatures) {
    allSupported = allSupported && wgpuAdapterHasFeature(ctx->getAdapter(), feature) != 0;
  }
  if (allSupported) {
    GTEST_SKIP() << "The adapter supports every compressed texture family";
  }
  EXPECT_FALSE(ctx->initDevice().isOk());
  EXPECT_EQ(ctx->getDevice(), nullptr);
}

TEST_F(WebGPUContextTest, ErrorScopeCapturesValidationError) {
  auto ctx = createContextWithDevice({});
  ASSERT_NE(ctx, nullptr);

  ctx->pushErrorScope(WGPUErrorFilter_Validation);
  const webgpu::Handle<WGPUBuffer> buffer(createInvalidBuffer(*ctx));
  const Result result = ctx->popErrorScope();

  EXPECT_EQ(result.code, Result::Code::ArgumentInvalid);
  EXPECT_FALSE(result.message.empty());
  EXPECT_EQ(ctx->getUncapturedErrorCount(), 0u);
}

TEST_F(WebGPUContextTest, ErrorScopeWithoutErrorIsOk) {
  auto ctx = createContextWithDevice({});
  ASSERT_NE(ctx, nullptr);

  ctx->pushErrorScope(WGPUErrorFilter_Validation);
  EXPECT_TRUE(ctx->popErrorScope().isOk());
}

TEST_F(WebGPUContextTest, PopErrorScopesReturnsTheInnermostError) {
  auto ctx = createContextWithDevice({});
  ASSERT_NE(ctx, nullptr);

  ctx->pushErrorScope(WGPUErrorFilter_OutOfMemory);
  ctx->pushErrorScope(WGPUErrorFilter_Validation);
  const webgpu::Handle<WGPUBuffer> buffer(createInvalidBuffer(*ctx));
  const Result result = ctx->popErrorScopes(2);

  EXPECT_EQ(result.code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(ctx->getUncapturedErrorCount(), 0u);
}

TEST_F(WebGPUContextTest, PopErrorScopesReturnsAnOuterError) {
  auto ctx = createContextWithDevice({});
  ASSERT_NE(ctx, nullptr);

  ctx->pushErrorScope(WGPUErrorFilter_Validation);
  ctx->pushErrorScope(WGPUErrorFilter_OutOfMemory);
  const webgpu::Handle<WGPUBuffer> buffer(createInvalidBuffer(*ctx));
  const Result result = ctx->popErrorScopes(2);

  EXPECT_EQ(result.code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(ctx->getUncapturedErrorCount(), 0u);
}

TEST_F(WebGPUContextTest, PopErrorScopesPopsEveryScope) {
  auto ctx = createContextWithDevice({});
  ASSERT_NE(ctx, nullptr);

  ctx->pushErrorScope(WGPUErrorFilter_OutOfMemory);
  ctx->pushErrorScope(WGPUErrorFilter_Validation);
  EXPECT_TRUE(ctx->popErrorScopes(2).isOk());

  // With no scope left, the next error is uncaptured.
  const webgpu::Handle<WGPUBuffer> buffer(createInvalidBuffer(*ctx));
  ctx->processEvents();
  EXPECT_EQ(ctx->getUncapturedErrorCount(), 1u);
}

TEST_F(WebGPUContextTest, UncapturedErrorIsCounted) {
  auto ctx = createContextWithDevice({});
  ASSERT_NE(ctx, nullptr);

  const webgpu::Handle<WGPUBuffer> buffer(createInvalidBuffer(*ctx));
  ctx->processEvents();

  EXPECT_EQ(ctx->getUncapturedErrorCount(), 1u);
  EXPECT_FALSE(ctx->isDeviceLost());
}

TEST_F(WebGPUContextTest, DeviceDestroyReportsLoss) {
  auto ctx = createContextWithDevice({});
  ASSERT_NE(ctx, nullptr);

  wgpuDeviceDestroy(ctx->getDevice());
  ctx->processEvents();

  EXPECT_TRUE(ctx->isDeviceLost());
}

TEST_F(WebGPUContextTest, QueryDevicesReturnsAdapter) {
  Result ret;
  auto ctx = webgpu::HWDevice::createContext({}, &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(ctx, nullptr);

  const std::vector<HWDeviceDesc> devices =
      webgpu::HWDevice::queryDevices(*ctx, HWDeviceQueryDesc(HWDeviceType::Unknown), &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_EQ(devices.size(), 1u);
  EXPECT_EQ(devices[0].guid, reinterpret_cast<uintptr_t>(ctx->getAdapter()));
  EXPECT_EQ(devices[0].name, ctx->getAdapterName());

  auto device = webgpu::HWDevice::create(std::move(ctx), devices[0], &ret);
  ASSERT_TRUE(ret.isOk()) << ret.message;
  ASSERT_NE(device, nullptr);
  EXPECT_EQ(device->getBackendType(), BackendType::WebGPU);
  EXPECT_NE(device->getNativeDevice(), nullptr);
  EXPECT_FALSE(device->isDeviceLost());
}

TEST_F(WebGPUContextTest, CreateRejectsForeignHWDeviceDesc) {
  Result ret;
  auto ctx = webgpu::HWDevice::createContext({}, &ret);
  ASSERT_NE(ctx, nullptr);

  const HWDeviceDesc foreign(0, HWDeviceType::Unknown);
  auto device = webgpu::HWDevice::create(std::move(ctx), foreign, &ret);
  EXPECT_EQ(ret.code, Result::Code::ArgumentInvalid);
  EXPECT_EQ(device, nullptr);
}

TEST_F(WebGPUContextTest, CreateRejectsNullContext) {
  Result ret;
  auto device = webgpu::HWDevice::create(nullptr, HWDeviceDesc(0, HWDeviceType::Unknown), &ret);
  EXPECT_EQ(ret.code, Result::Code::ArgumentNull);
  EXPECT_EQ(device, nullptr);
}

} // namespace igl::tests
