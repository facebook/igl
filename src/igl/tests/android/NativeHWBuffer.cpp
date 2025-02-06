/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#if defined(IGL_ANDROID_HWBUFFER_SUPPORTED)

#include <android/hardware_buffer.h>
#include <gtest/gtest.h>

#include <igl/android/NativeHWBuffer.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/egl/PlatformDevice.h>
#include <igl/vulkan/Device.h>
#include <igl/vulkan/PlatformDevice.h>

#include "../data/ShaderData.h"
#include "../data/TextureData.h"
#include "../data/VertexIndexData.h"
#include "../util/Common.h"
#include "../util/TextureValidationHelpers.h"
#include "../util/device/TestDevice.h"

namespace igl::tests {

using namespace igl::android;

constexpr uint32_t kOffscreenTexWidth = 10u;
constexpr uint32_t kOffscreenTexHeight = 10u;

class NativeHWBufferTest : public ::testing::Test {
 public:
  NativeHWBufferTest() = default;
  ~NativeHWBufferTest() override = default;
};

class NativeHWTextureBufferTest : public igl::android::INativeHWTextureBuffer {
 protected:
  Result createTextureInternal(const TextureDesc& desc, AHardwareBuffer* buffer) override {
    return Result();
  }
};

TEST_F(NativeHWBufferTest, Basic_getNativeHWFormat) {
  EXPECT_EQ(getNativeHWFormat(TextureFormat::RGBX_UNorm8), AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::RGBA_UNorm8), AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::B5G6R5_UNorm), AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::RGBA_F16), AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::RGB10_A2_UNorm_Rev),
            AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::Z_UNorm16), AHARDWAREBUFFER_FORMAT_D16_UNORM);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::Z_UNorm24), AHARDWAREBUFFER_FORMAT_D24_UNORM);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::Z_UNorm32), AHARDWAREBUFFER_FORMAT_D32_FLOAT);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::S8_UInt_Z24_UNorm),
            AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT);
  EXPECT_EQ(getNativeHWFormat(TextureFormat::S_UInt8), AHARDWAREBUFFER_FORMAT_S8_UINT);
  EXPECT_EQ(getNativeHWFormat(igl::TextureFormat::YUV_NV12),
            AHARDWAREBUFFER_FORMAT_YCbCr_420_SP_VENUS);
  EXPECT_EQ(getNativeHWFormat(igl::TextureFormat::Invalid), 0);
#if __ANDROID_MIN_SDK_VERSION__ >= 30
  EXPECT_EQ(getNativeHWFormat(igl::TextureFormat::YUV_420p), AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420);
#endif
}

TEST_F(NativeHWBufferTest, Basic_getIglFormat) {
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM),
            TextureFormat::RGBX_UNorm8);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM),
            TextureFormat::RGBA_UNorm8);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM),
            TextureFormat::B5G6R5_UNorm);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT),
            TextureFormat::RGBA_F16);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM),
            TextureFormat::RGB10_A2_UNorm_Rev);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_D16_UNORM), TextureFormat::Z_UNorm16);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_D24_UNORM), TextureFormat::Z_UNorm24);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_D32_FLOAT), TextureFormat::Z_UNorm32);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT),
            TextureFormat::S8_UInt_Z24_UNorm);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_S8_UINT), TextureFormat::S_UInt8);
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_YCbCr_420_SP_VENUS),
            TextureFormat::YUV_NV12);
#if __ANDROID_MIN_SDK_VERSION__ >= 30
  EXPECT_EQ(igl::android::getIglFormat(AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420),
            TextureFormat::YUV_420p);
#endif
}

TEST_F(NativeHWBufferTest, getNativeHWBufferUsage) {
  EXPECT_TRUE(getNativeHWBufferUsage(TextureDesc::TextureUsageBits::Sampled) |
              AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE);
  EXPECT_TRUE(getNativeHWBufferUsage(TextureDesc::TextureUsageBits::Storage) |
              AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN);
  EXPECT_TRUE(getNativeHWBufferUsage(TextureDesc::TextureUsageBits::Attachment) |
              AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT);
}

TEST_F(NativeHWBufferTest, getIglBufferUsage) {
  EXPECT_TRUE(getIglBufferUsage(AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) |
              TextureDesc::TextureUsageBits::Sampled);
  EXPECT_TRUE(getIglBufferUsage(AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN |
                                AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN) |
              TextureDesc::TextureUsageBits::Storage);
  EXPECT_TRUE(getIglBufferUsage(AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT) |
              TextureDesc::TextureUsageBits::Attachment);
}

TEST_F(NativeHWBufferTest, allocateNativeHWBuffer) {
  AHardwareBuffer* hwBuffer;
  auto allocationResult = allocateNativeHWBuffer(
      TextureDesc::newNativeHWBufferImage(
          TextureFormat::RGBA_UNorm8, TextureDesc::TextureUsageBits::Sampled, 100, 100),
      false,
      &hwBuffer);

  EXPECT_TRUE(allocationResult.isOk());
  EXPECT_NE(hwBuffer, nullptr);

  AHardwareBuffer_Desc hwbDesc;
  AHardwareBuffer_describe(hwBuffer, &hwbDesc);

  AHardwareBuffer_release(hwBuffer);

  EXPECT_EQ(hwbDesc.format, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM);
  EXPECT_EQ(hwbDesc.width, 100);
  EXPECT_EQ(hwbDesc.height, 100);
  EXPECT_EQ(hwbDesc.layers, 1);
  EXPECT_EQ(hwbDesc.usage, AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE);
}

TEST_F(NativeHWBufferTest, LockBuffer) {
  AHardwareBuffer* hwBuffer;

  auto allocationResult = allocateNativeHWBuffer(
      TextureDesc::newNativeHWBufferImage(
          TextureFormat::RGBA_UNorm8, TextureDesc::TextureUsageBits::Sampled, 100, 100),
      false,
      &hwBuffer);

  EXPECT_TRUE(allocationResult.isOk());
  EXPECT_NE(hwBuffer, nullptr);

  {
    NativeHWTextureBufferTest testTxBuffer;

    EXPECT_TRUE(testTxBuffer.createWithHWBuffer(hwBuffer).isOk());

    std::byte* bytes = nullptr;
    INativeHWTextureBuffer::RangeDesc outRange;
    Result outResult;
    auto lockGuard = testTxBuffer.lockHWBuffer(&bytes, outRange, &outResult);

    EXPECT_TRUE(outResult.isOk());
    EXPECT_NE(bytes, nullptr);
    EXPECT_EQ(outRange.width, 100);
    EXPECT_EQ(outRange.height, 100);
    EXPECT_EQ(outRange.layer, 1);
    EXPECT_EQ(outRange.mipLevel, 1);

    lockGuard.~LockGuard();
  }

  AHardwareBuffer_release(hwBuffer);
}

template<igl::BackendType TBackendType>
class NativeHWBufferTextureTest : public ::testing::Test {
 public:
  NativeHWBufferTextureTest() = default;
  ~NativeHWBufferTextureTest() override = default;

  //
  // This function sets up a render pass and a graphics pipeline descriptor
  // so it is ready to render a simple quad with an input texture to an
  // offscreen texture.
  //
  // The actual creation of the graphics pipeline state object is left
  // to each test so that tests can replace the default settings with
  // something more appropriate.
  //
  void SetUp() override {
    setDebugBreakEnabled(false);

    Result ret;

    // Create Device
    if (TBackendType == igl::BackendType::OpenGL) {
      iglDev_ = util::device::createTestDevice(igl::BackendType::OpenGL, "3.0");
    } else if (TBackendType == igl::BackendType::Vulkan) {
      iglDev_ = util::device::createTestDevice(igl::BackendType::Vulkan);
    } else {
      static_assert("Unsupported backend");
    }
    ASSERT_TRUE(iglDev_ != nullptr);

    // Create Command Queue
    const CommandQueueDesc cqDesc = {};
    cmdQueue_ = iglDev_->createCommandQueue(cqDesc, &ret);
    ASSERT_TRUE(cmdQueue_ != nullptr);

    // Create an offscreen texture to render to
    const TextureDesc texDesc = TextureDesc::new2D(TextureFormat::RGBA_UNorm8,
                                                   kOffscreenTexWidth,
                                                   kOffscreenTexHeight,
                                                   TextureDesc::TextureUsageBits::Sampled |
                                                       TextureDesc::TextureUsageBits::Attachment,
                                                   "test");

    offscreenTexture_ = iglDev_->createTexture(texDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok) << ret.message;
    ASSERT_TRUE(offscreenTexture_ != nullptr);

    // Create framebuffer using the offscreen texture
    FramebufferDesc framebufferDesc;

    framebufferDesc.colorAttachments[0].texture = offscreenTexture_;
    framebuffer_ = iglDev_->createFramebuffer(framebufferDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(framebuffer_ != nullptr);

    // Initialize render pass descriptor
    renderPass_.colorAttachments.resize(1);
    renderPass_.colorAttachments[0].loadAction = LoadAction::Clear;
    renderPass_.colorAttachments[0].storeAction = StoreAction::Store;
    renderPass_.colorAttachments[0].clearColor = {0.0, 0.0, 0.0, 1.0};

    // Initialize shader stages
    std::unique_ptr<IShaderStages> stages;
    igl::tests::util::createSimpleShaderStages(iglDev_, stages);
    shaderStages_ = std::move(stages);

    // Initialize input to vertex shader
    VertexInputStateDesc inputDesc;

    inputDesc.attributes[0].format = VertexAttributeFormat::Float4;
    inputDesc.attributes[0].offset = 0;
    inputDesc.attributes[0].bufferIndex = data::shader::simplePosIndex;
    inputDesc.attributes[0].name = data::shader::simplePos;
    inputDesc.attributes[0].location = 0;
    inputDesc.inputBindings[0].stride = sizeof(float) * 4;

    inputDesc.attributes[1].format = VertexAttributeFormat::Float2;
    inputDesc.attributes[1].offset = 0;
    inputDesc.attributes[1].bufferIndex = data::shader::simpleUvIndex;
    inputDesc.attributes[1].name = data::shader::simpleUv;
    inputDesc.attributes[1].location = 1;
    inputDesc.inputBindings[1].stride = sizeof(float) * 2;

    // numAttributes has to equal to bindings when using more than 1 buffer
    inputDesc.numAttributes = inputDesc.numInputBindings = 2;

    vertexInputState_ = iglDev_->createVertexInputState(inputDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(vertexInputState_ != nullptr);

    // Initialize index buffer
    BufferDesc bufDesc;

    bufDesc.type = BufferDesc::BufferTypeBits::Index;
    bufDesc.data = data::vertex_index::QUAD_IND;
    bufDesc.length = sizeof(data::vertex_index::QUAD_IND);

    ib_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(ib_ != nullptr);

    // Initialize vertex and sampler buffers
    bufDesc.type = BufferDesc::BufferTypeBits::Vertex;
    bufDesc.data = data::vertex_index::QUAD_VERT;
    bufDesc.length = sizeof(data::vertex_index::QUAD_VERT);

    vb_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(vb_ != nullptr);

    bufDesc.type = BufferDesc::BufferTypeBits::Vertex;
    bufDesc.data = data::vertex_index::QUAD_UV;
    bufDesc.length = sizeof(data::vertex_index::QUAD_UV);

    uv_ = iglDev_->createBuffer(bufDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(uv_ != nullptr);

    // Initialize sampler state
    const SamplerStateDesc samplerDesc;
    samp_ = iglDev_->createSamplerState(samplerDesc, &ret);
    ASSERT_EQ(ret.code, Result::Code::Ok);
    ASSERT_TRUE(samp_ != nullptr);

    // Initialize Graphics Pipeline Descriptor, but leave the creation
    // to the individual tests in case further customization is required
    renderPipelineDesc_.vertexInputState = vertexInputState_;
    renderPipelineDesc_.shaderStages = shaderStages_;
    renderPipelineDesc_.targetDesc.colorAttachments.resize(1);
    renderPipelineDesc_.targetDesc.colorAttachments[0].textureFormat =
        offscreenTexture_->getFormat();
    renderPipelineDesc_.fragmentUnitSamplerMap[textureUnit_] =
        IGL_NAMEHANDLE(data::shader::simpleSampler);
    renderPipelineDesc_.cullMode = igl::CullMode::Disabled;

    auto allocationResult = allocateNativeHWBuffer(
        TextureDesc::newNativeHWBufferImage(TextureFormat::RGBA_UNorm8,
                                            TextureDesc::TextureUsageBits::Sampled,
                                            kOffscreenTexWidth,
                                            kOffscreenTexHeight),
        false,
        &hwBuffer_);

    ASSERT_TRUE(allocationResult.isOk());
    ASSERT_NE(hwBuffer_, nullptr);

    for (int i = 0; i < kOffscreenTexWidth * kOffscreenTexHeight; ++i) {
      pixels_[i] = rand() % 0xFFFFFFFF;
    }

    AHardwareBuffer_Desc hwbDesc;
    AHardwareBuffer_describe(hwBuffer_, &hwbDesc);

    std::byte* bytes;

    auto lock_result = AHardwareBuffer_lock(hwBuffer_,
                                            AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN,
                                            -1,
                                            nullptr,
                                            reinterpret_cast<void**>(&bytes));
    ASSERT_EQ(lock_result, 0);

    for (int i = 0; i < hwbDesc.stride; ++i) {
      for (int j = 0; j < hwbDesc.height; ++j) {
        uint32_t color = 0x00000000;
        if (i < hwbDesc.width && j < hwbDesc.height) {
          color = pixels_[i + j * hwbDesc.width];
        }
        reinterpret_cast<uint32_t*>(bytes)[i + j * hwbDesc.stride] = color;
      }
    }

    ASSERT_EQ(AHardwareBuffer_unlock(hwBuffer_, nullptr), 0);
  }

  void TearDown() override {
    AHardwareBuffer_release(hwBuffer_);
  }

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
  std::shared_ptr<ICommandBuffer> cmdBuf_;
  CommandBufferDesc cbDesc_ = {};

  RenderPassDesc renderPass_;
  std::shared_ptr<ITexture> offscreenTexture_;
  std::shared_ptr<IFramebuffer> framebuffer_;

  std::shared_ptr<IShaderStages> shaderStages_;

  std::shared_ptr<IVertexInputState> vertexInputState_;
  std::shared_ptr<IBuffer> vb_, uv_, ib_;

  std::shared_ptr<ISamplerState> samp_;

  RenderPipelineDesc renderPipelineDesc_;
  size_t textureUnit_ = 0;

  std::array<uint32_t, kOffscreenTexWidth * kOffscreenTexHeight> pixels_;
  AHardwareBuffer* hwBuffer_;
};

using NativeHWBufferTextureTestOpenGL3 = NativeHWBufferTextureTest<igl::BackendType::OpenGL>;

TEST_F(NativeHWBufferTextureTestOpenGL3, SharedMemoryTexture) {
  Result outResult;

  auto platformDevice = iglDev_->getPlatformDevice<igl::opengl::egl::PlatformDevice>();
  auto texture = platformDevice->createTextureWithSharedMemory(hwBuffer_, &outResult);

  EXPECT_EQ(outResult.isOk(), true);
  EXPECT_NE(texture.get(), nullptr);

  util::validateUploadedTexture(
      *iglDev_, *cmdQueue_, texture, pixels_.data(), "HWBufferTextureOpenGL3");
}

// @fb-only
using NativeHWBufferTextureTestVulkan = NativeHWBufferTextureTest<{igl::BackendType::Vulkan}>;

TEST_F(NativeHWBufferTextureTestVulkan, SharedMemoryTexture) {
  Result outResult;

  auto platformDevice = iglDev_->getPlatformDevice<igl::vulkan::PlatformDevice>();
  auto texture = platformDevice->createTextureWithSharedMemory(hwBuffer_, &outResult);

  EXPECT_EQ(outResult.isOk(), true);
  EXPECT_NE(texture.get(), nullptr);

  util::validateUploadedTexture(
      *iglDev_, *cmdQueue_, texture, pixels_.data(), "HWBufferTextureVulkan");
}
// @fb-only

} // namespace igl::tests

#endif // defined(IGL_ANDROID_HWBUFFER_SUPPORTED)
