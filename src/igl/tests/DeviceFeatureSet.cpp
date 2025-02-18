/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "util/Common.h"
#include "util/TestDevice.h"
#if IGL_BACKEND_OPENGL
#include <igl/opengl/IContext.h>
#include <igl/opengl/PlatformDevice.h>
#endif // IGL_BACKEND_OPENGL
#include <string>

namespace igl::tests {

// DeviceFeatureSetTest
// This test exercises the igl::ICapabilities API.

class DeviceFeatureSetTest : public ::testing::Test {
 public:
  DeviceFeatureSetTest() = default;
  ~DeviceFeatureSetTest() override = default;

  // Set up common resources. This will create a device and a command queue
  void SetUp() override {
    setDebugBreakEnabled(false);

    util::createDeviceAndQueue(iglDev_, cmdQueue_);
  }

  void TearDown() override {}

  // Member variables
 public:
  std::shared_ptr<IDevice> iglDev_;
  std::shared_ptr<ICommandQueue> cmdQueue_;
};

//
// hasFeature
//
// Check DeviceFeatureSet hasFeature list
//
TEST_F(DeviceFeatureSetTest, hasFeatureForMacOSOrWinOrAndroidTest) {
  EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::StandardDerivative));

  const bool backendOpenGL = iglDev_->getBackendType() == igl::BackendType::OpenGL;

  if (backendOpenGL) {
#if IGL_BACKEND_OPENGL
    auto& context = iglDev_->getPlatformDevice<igl::opengl::PlatformDevice>()->getContext();
    const bool usesOpenGLES = igl::opengl::DeviceFeatureSet::usesOpenGLES();
    const auto& deviceFeatures = context.deviceFeatures();
    auto glVersion = deviceFeatures.getGLVersion();

    const bool readWriteFramebuffer =
        glVersion >= igl::opengl::GLVersion::v3_0_ES ||
        deviceFeatures.isSupported("GL_ARB_framebuffer_object") ||
        deviceFeatures.isSupported("GL_APPLE_framebuffer_multisample");
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::ReadWriteFramebuffer), readWriteFramebuffer);

    const bool texture2DArray =
        (usesOpenGLES ? glVersion >= igl::opengl::GLVersion::v3_0_ES
                      : (glVersion >= igl::opengl::GLVersion::v3_0 ||
                         deviceFeatures.isSupported("GL_EXT_texture_array")));
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::Texture2DArray), texture2DArray);

    const bool texture3D = (usesOpenGLES ? glVersion >= igl::opengl::GLVersion::v3_0_ES
                                         : glVersion >= igl::opengl::GLVersion::v2_0) ||
                           deviceFeatures.isSupported("GL_OES_texture_3D");
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::Texture3D), texture3D);

    const bool textureArrayExt = !usesOpenGLES &&
                                 (deviceFeatures.isSupported("GL_EXT_texture_array") ||
                                  deviceFeatures.isSupported("GL_EXT_gpu_shader4"));
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::TextureArrayExt), textureArrayExt);

    const bool textureExternalImage =
        usesOpenGLES && (glVersion >= igl::opengl::GLVersion::v3_0_ES ||
                         deviceFeatures.isSupported("GL_OES_EGL_image_external_essl3") ||
                         deviceFeatures.isSupported("GL_OES_EGL_image_external"));
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::TextureExternalImage), textureExternalImage);

    const bool textureNotPot = (!usesOpenGLES || glVersion >= igl::opengl::GLVersion::v3_0_ES) ||
                               deviceFeatures.isSupported("GL_OES_texture_npot");
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::TextureNotPot), textureNotPot);

    const bool multiView = deviceFeatures.isSupported("GL_OVR_multiview2") &&
                           (usesOpenGLES ? glVersion >= igl::opengl::GLVersion::v3_0_ES
                                         : glVersion >= igl::opengl::GLVersion::v3_0);
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::Multiview), multiView);

    EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::BindUniform));

    const bool texturePartialMipChain = !usesOpenGLES ||
                                        glVersion >= igl::opengl::GLVersion::v3_0_ES ||
                                        deviceFeatures.isSupported("GL_APPLE_texture_max_level");
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::TexturePartialMipChain), texturePartialMipChain);

    EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BufferRing));
    EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BufferNoCopy));
    EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ShaderLibrary));
    EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BindBytes));
    EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BufferDeviceAddress));

    const bool shaderTextureLod = iglDev_->hasFeature(DeviceFeatures::ShaderTextureLodExt) ||
                                  (usesOpenGLES ? glVersion >= igl::opengl::GLVersion::v3_0_ES
                                                : glVersion >= igl::opengl::GLVersion::v3_0);
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::ShaderTextureLod), shaderTextureLod);

    const bool shaderTextureLodExt = usesOpenGLES
                                         ? deviceFeatures.isSupported("GL_EXT_shader_texture_lod")
                                         : deviceFeatures.isSupported("GL_ARB_shader_texture_lod");
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::ShaderTextureLodExt), shaderTextureLodExt);

    const bool standardDerivativeExt = usesOpenGLES &&
                                       deviceFeatures.isSupported("GL_OES_standard_derivatives");
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::StandardDerivativeExt), standardDerivativeExt);

    const bool supportsSRGB =
        (usesOpenGLES ? (glVersion >= igl::opengl::GLVersion::v3_0_ES ||
                         deviceFeatures.isSupported("GL_EXT_sRGB"))
                      : glVersion >= igl::opengl::GLVersion::v2_1 ||
                            deviceFeatures.isSupported("GL_EXT_texture_sRGB"));
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::SRGB), supportsSRGB);

    const bool supportsSRGBWriteControl =
        usesOpenGLES ? deviceFeatures.isSupported("GL_EXT_sRGB_write_control")
                     : glVersion >= igl::opengl::GLVersion::v3_0 ||
                           deviceFeatures.isSupported("GL_ARB_framebuffer_sRGB") ||
                           deviceFeatures.isSupported("GL_EXT_framebuffer_sRGB");
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::SRGBWriteControl), supportsSRGBWriteControl);

    const bool samplerMinMaxLod = !usesOpenGLES || glVersion >= igl::opengl::GLVersion::v3_0_ES;
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::SamplerMinMaxLod), samplerMinMaxLod);

    const bool drawIndexedIndirect =
        (usesOpenGLES && glVersion >= igl::opengl::GLVersion::v3_1_ES) ||
        (!usesOpenGLES && glVersion >= igl::opengl::GLVersion::v4_0) ||
        deviceFeatures.isSupported("GL_ARB_draw_indirect");
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::DrawIndexedIndirect), drawIndexedIndirect);

    const bool multipleRenderTargets = !usesOpenGLES ||
                                       glVersion >= igl::opengl::GLVersion::v3_0_ES ||
                                       deviceFeatures.isSupported("GL_EXT_draw_buffers");
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::MultipleRenderTargets), multipleRenderTargets);

    const bool explicitBinding =
        (usesOpenGLES && glVersion >= igl::opengl::GLVersion::v3_1_ES) ||
        (!usesOpenGLES && (glVersion >= igl::opengl::GLVersion::v4_2 ||
                           deviceFeatures.isSupported("GL_ARB_shading_language_420pack")));
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::ExplicitBinding), explicitBinding);

    const bool explicitBindingExt = deviceFeatures.isSupported("GL_ARB_shading_language_420pack");
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::ExplicitBindingExt), explicitBindingExt);

    const bool textureFormatRG =
        (usesOpenGLES && (glVersion >= igl::opengl::GLVersion::v3_0_ES ||
                          deviceFeatures.isSupported("GL_EXT_texture_rg"))) ||
        (!usesOpenGLES && (glVersion >= igl::opengl::GLVersion::v3_0 ||
                           deviceFeatures.isSupported("GL_ARB_texture_rg")));
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::TextureFormatRG), textureFormatRG);
    EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ValidationLayersEnabled));
    const bool externalMemoryObjects = deviceFeatures.isSupported("GL_EXT_memory_object") &&
                                       deviceFeatures.isSupported("GL_EXT_memory_object_fd");
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::ExternalMemoryObjects), externalMemoryObjects);

    EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::PushConstants));
#endif // IGL_BACKEND_OPENGL
  } else {
    // non OpenGL backends
    EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::ReadWriteFramebuffer));
    EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::TextureNotPot));
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::SRGB), true);
    EXPECT_EQ(iglDev_->hasFeature(DeviceFeatures::SRGBWriteControl), false);

    if (iglDev_->getBackendType() == igl::BackendType::Vulkan) {
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::Texture2DArray));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::Texture3D));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::TextureArrayExt));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::TextureExternalImage));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::Multiview));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BindUniform));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::TexturePartialMipChain));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BufferRing));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BufferNoCopy));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::ShaderLibrary));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BindBytes));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::BufferDeviceAddress));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::ShaderTextureLod));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ShaderTextureLodExt));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::StandardDerivativeExt));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::SamplerMinMaxLod));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::DrawIndexedIndirect));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::MultipleRenderTargets));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::ExplicitBinding));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ExplicitBindingExt));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::TextureFormatRG));
      // On Android Validation Layers are only enabled for debug builds by default
#if (IGL_PLATFORM_ANDROID && !IGL_DEBUG) || !IGL_DEBUG || defined(IGL_DISABLE_VALIDATION)
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ValidationLayersEnabled));
#else
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::ValidationLayersEnabled));
#endif // IGL_PLATFORM_ANDROID
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::ExternalMemoryObjects));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::PushConstants));
    } else if (iglDev_->getBackendType() == igl::BackendType::Metal) {
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::Texture2DArray));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::Texture3D));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::TextureArrayExt));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::TextureExternalImage));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::Multiview));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BindUniform));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::TexturePartialMipChain));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::BufferRing));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::BufferNoCopy));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::ShaderLibrary));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::BindBytes));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BufferDeviceAddress));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::ShaderTextureLod));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ShaderTextureLodExt));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::StandardDerivativeExt));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::SamplerMinMaxLod));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::DrawIndexedIndirect));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::MultipleRenderTargets));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::ExplicitBinding));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ExplicitBindingExt));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::TextureFormatRG));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ValidationLayersEnabled));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ExternalMemoryObjects));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::PushConstants));
    } else {
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::Texture2DArray));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::Texture3D));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::TextureArrayExt));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::TextureExternalImage));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::Multiview));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BindUniform));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::TexturePartialMipChain));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BufferRing));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BufferNoCopy));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ShaderLibrary));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::BindBytes));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::BufferDeviceAddress));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::ShaderTextureLod));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ShaderTextureLodExt));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::StandardDerivativeExt));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::SamplerMinMaxLod));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::DrawIndexedIndirect));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::MultipleRenderTargets));
      EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::ExplicitBinding));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ExplicitBindingExt));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::TextureFormatRG));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ValidationLayersEnabled));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::ExternalMemoryObjects));
      EXPECT_FALSE(iglDev_->hasFeature(DeviceFeatures::PushConstants));
    }
  }

  EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::TextureHalfFloat));
  EXPECT_TRUE(iglDev_->hasFeature(DeviceFeatures::TextureFloat));
}

//
// getTextureFormatCapabilities
//
// Since some of the formats may be platform-dependent, we will only spot
// check a few to make sure the API is working. We should continue to add formats
// here as we see fit.
//
TEST_F(DeviceFeatureSetTest, getTextureFormatCapabilities) {
  ICapabilities::TextureFormatCapabilities capability = 0;

  // RGBA_UNorm8 should be able to do everything except SampledAttachment on all the platforms
  capability = iglDev_->getTextureFormatCapabilities(TextureFormat::RGBA_UNorm8);
  EXPECT_TRUE(contains(capability, ICapabilities::TextureFormatCapabilityBits::Sampled));
  EXPECT_TRUE(contains(capability, ICapabilities::TextureFormatCapabilityBits::Attachment));
  EXPECT_TRUE(contains(capability, ICapabilities::TextureFormatCapabilityBits::SampledFiltered));
  if (iglDev_->hasFeature(DeviceFeatures::Compute)) {
    EXPECT_TRUE(contains(capability, ICapabilities::TextureFormatCapabilityBits::Storage));
  }

  // Z_UNorm16 should always be readable by a shader
  capability = iglDev_->getTextureFormatCapabilities(TextureFormat::Z_UNorm16);
  EXPECT_TRUE(contains(capability, ICapabilities::TextureFormatCapabilityBits::Sampled));
}

} // namespace igl::tests
