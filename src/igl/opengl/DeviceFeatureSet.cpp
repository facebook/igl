/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/DeviceFeatureSet.h>

#include <igl/Common.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/IContext.h>
#include <igl/opengl/Texture.h>

namespace igl::opengl {
namespace {
bool hasVersion(const DeviceFeatureSet& dfs, bool usesOpenGLES, GLVersion minSupportedVersion) {
  return DeviceFeatureSet::usesOpenGLES() == usesOpenGLES &&
         dfs.getGLVersion() >= minSupportedVersion;
}

bool hasVersionOrExtension(const DeviceFeatureSet& dfs,
                           bool usesOpenGLES,
                           GLVersion minSupportedVersion,
                           const char* extension) {
  return hasVersion(dfs, usesOpenGLES, minSupportedVersion) || dfs.isSupported(extension);
}

bool hasDesktopVersion(const DeviceFeatureSet& dfs, GLVersion minSupportedVersion) {
  return hasVersion(dfs, false, minSupportedVersion);
}

bool hasESVersion(const DeviceFeatureSet& dfs, GLVersion minSupportedVersion) {
  return hasVersion(dfs, true, minSupportedVersion);
}

bool hasESExtension(const DeviceFeatureSet& dfs, const char* extension) {
  return DeviceFeatureSet::usesOpenGLES() && dfs.isSupported(extension);
}

bool hasDesktopExtension(const DeviceFeatureSet& dfs, const char* extension) {
  return !DeviceFeatureSet::usesOpenGLES() && dfs.isSupported(extension);
}

bool hasDesktopOrESVersion(const DeviceFeatureSet& dfs,
                           GLVersion minDesktopSupportedVersion,
                           GLVersion minESSupportedVersion) {
  return hasDesktopVersion(dfs, minDesktopSupportedVersion) ||
         hasESVersion(dfs, minESSupportedVersion);
}

bool hasDesktopVersionOrExtension(const DeviceFeatureSet& dfs,
                                  GLVersion minSupportedVersion,
                                  const char* extension) {
  return hasVersionOrExtension(dfs, false, minSupportedVersion, extension);
}

bool hasESVersionOrExtension(const DeviceFeatureSet& dfs,
                             GLVersion minSupportedVersion,
                             const char* extension) {
  return hasVersionOrExtension(dfs, true, minSupportedVersion, extension);
}

bool hasDesktopOrESVersionOrExtension(const DeviceFeatureSet& dfs,
                                      GLVersion minDesktopSupportedVersion,
                                      GLVersion minESSupportedVersion,
                                      const char* desktopExtension,
                                      const char* esExtension) {
  return hasDesktopVersionOrExtension(dfs, minDesktopSupportedVersion, desktopExtension) ||
         hasESVersionOrExtension(dfs, minESSupportedVersion, esExtension);
}

bool hasDesktopOrESVersionOrExtension(const DeviceFeatureSet& dfs,
                                      GLVersion minDesktopSupportedVersion,
                                      GLVersion minESSupportedVersion,
                                      const char* extension) {
  return hasDesktopOrESVersionOrExtension(
      dfs, minDesktopSupportedVersion, minESSupportedVersion, extension, extension);
}

bool hasDesktopOrESExtension(const DeviceFeatureSet& dfs,
                             const char* desktopExtension,
                             const char* esExtension) {
  return (!DeviceFeatureSet::usesOpenGLES() && dfs.isSupported(desktopExtension)) ||
         (DeviceFeatureSet::usesOpenGLES() && dfs.isSupported(esExtension));
}

bool hasDesktopOrESExtension(const DeviceFeatureSet& dfs, const char* extension) {
  return hasDesktopOrESExtension(dfs, extension, extension);
}
} // namespace

bool DeviceFeatureSet::usesOpenGLES() noexcept {
#if IGL_OPENGL_ES
  return true;
#else
  return false;
#endif
}

DeviceFeatureSet::DeviceFeatureSet(IContext& glContext) : glContext_(glContext) {}

void DeviceFeatureSet::initializeVersion(GLVersion version) {
  version_ = version;
}

void DeviceFeatureSet::initializeExtensions(std::string extensions,
                                            std::unordered_set<std::string> supportedExtensions) {
  extensions_ = std::move(extensions);
  supportedExtensions_ = std::move(supportedExtensions);
}

GLVersion DeviceFeatureSet::getGLVersion() const noexcept {
  return version_;
}

ShaderVersion DeviceFeatureSet::getShaderVersion() const {
  return ::igl::opengl::getShaderVersion(version_);
}

bool DeviceFeatureSet::isSupported(const std::string& extensionName) const {
  if (!extensions_.empty()) {
    return extensions_.find(extensionName) != std::string::npos;
  } else {
    return supportedExtensions_.find(extensionName) != supportedExtensions_.end();
  }
}

bool DeviceFeatureSet::isExtensionSupported(Extensions extension) const {
  switch (extension) {
  case Extensions::AppleRgb422:
    return hasDesktopOrESExtension(*this, "GL_APPLE_rgb_422");
  case Extensions::BindlessTextureArb:
    return hasDesktopExtension(*this, "GL_ARB_bindless_texture");
  case Extensions::BindlessTextureNv:
    return hasDesktopOrESExtension(*this, "GL_NV_bindless_texture");
  case Extensions::Debug:
    return hasDesktopOrESExtension(*this, "GL_KHR_debug");
  case Extensions::DebugMarker:
    return hasDesktopOrESExtension(*this, "GL_EXT_debug_marker");
  case Extensions::Depth24:
    return hasESExtension(*this, "GL_OES_depth24");
  case Extensions::Depth32:
    return hasESExtension(*this, "GL_OES_depth32");
  case Extensions::DepthTexture:
    return hasESExtension(*this, "GL_OES_depth_texture");
  case Extensions::DiscardFramebuffer:
    return hasESExtension(*this, "GL_EXT_discard_framebuffer");
  case Extensions::DrawBuffers:
    return hasESExtension(*this, "GL_EXT_draw_buffers");
  case Extensions::Es2Compatibility:
    return hasDesktopExtension(*this, "GL_ARB_ES2_compatibility");
  case Extensions::FramebufferBlit:
    return hasDesktopExtension(*this, "GL_EXT_framebuffer_blit");
  case Extensions::FramebufferObject:
    return hasDesktopExtension(*this, "GL_ARB_framebuffer_object");
  case Extensions::InvalidateSubdata:
    return isSupported("GL_ARB_invalidate_subdata");
  case Extensions::MapBuffer:
    return hasESExtension(*this, "GL_OES_mapbuffer");
  case Extensions::MapBufferRange:
    return hasESExtension(*this, "GL_EXT_map_buffer_range");
  case Extensions::MultiSampleApple:
    return hasESExtension(*this, "GL_APPLE_framebuffer_multisample");
  case Extensions::MultiSampleExt:
    return hasESExtension(*this, "GL_EXT_multisampled_render_to_texture");
  case Extensions::MultiSampleImg:
    return hasESExtension(*this, "GL_IMG_multisampled_render_to_texture");
  case Extensions::RequiredInternalFormat:
    return hasESExtension(*this, "GL_OES_required_internalformat");
  case Extensions::ShaderImageLoadStore:
    return hasESExtension(*this, "GL_EXT_shader_image_load_store");
  case Extensions::Srgb:
    return hasESExtension(*this, "GL_EXT_sRGB");
  case Extensions::SrgbWriteControl:
    return hasESExtension(*this, "GL_EXT_sRGB_write_control");
  case Extensions::Sync:
    return hasESExtension(*this, "GL_APPLE_sync");
  case Extensions::TexStorage:
    return isSupported("GL_EXT_texture_storage");
  case Extensions::Texture3D:
    return hasESExtension(*this, "GL_OES_texture_3D");
  case Extensions::TextureFormatBgra8888Ext:
    return hasESExtension(*this, "GL_EXT_texture_format_BGRA8888");
  case Extensions::TextureFormatBgra8888Apple:
    return hasESExtension(*this, "GL_APPLE_texture_format_BGRA8888");
  case Extensions::TextureFloat:
    return hasDesktopExtension(*this, "GL_ARB_texture_float");
  case Extensions::TextureHalfFloat:
    // Necessary for GL_HALF_FLOAT_OES, which is different than GL_HALF_FLOAT
    return hasESExtension(*this, "GL_OES_texture_half_float");
  case Extensions::TextureRgArb:
    return hasDesktopExtension(*this, "GL_ARB_texture_rg");
  case Extensions::TextureRgExt:
    return hasESExtension(*this, "GL_EXT_texture_rg");
  case Extensions::TextureSrgb:
    return hasDesktopExtension(*this, "GL_EXT_texture_sRGB");
  case Extensions::TextureType2_10_10_10_Rev:
    return hasESExtension(*this, "GL_EXT_texture_type_2_10_10_10_REV");
  case Extensions::VertexArrayObject:
    return hasESExtension(*this, "GL_OES_vertex_array_object");
  }
  IGL_UNREACHABLE_RETURN(false);
}

bool DeviceFeatureSet::isFeatureSupported(DeviceFeatures feature) const {
  switch (feature) {
  case DeviceFeatures::MultiSample:
    return hasDesktopVersion(*this, GLVersion::v3_0) ||
           hasExtension(Extensions::FramebufferObject) || hasESVersion(*this, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::MultiSampleApple) || hasExtension(Extensions::MultiSampleExt) ||
           hasExtension(Extensions::MultiSampleImg);

  case DeviceFeatures::MultiSampleResolve:
    return false;

  case DeviceFeatures::TextureFilterAnisotropic:
    return hasDesktopVersion(*this, GLVersion::v4_6) ||
           hasDesktopOrESExtension(*this, "GL_EXT_texture_filter_anisotropic") ||
           hasDesktopExtension(*this, "GL_ARB_texture_filter_anisotropic");

  case DeviceFeatures::MapBufferRange:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) ||
           hasDesktopExtension(*this, "GL_ARB_map_buffer_range") ||
           hasExtension(Extensions::MapBufferRange);

  case DeviceFeatures::MultipleRenderTargets:
    return hasDesktopOrESVersionOrExtension(
        *this, GLVersion::v2_0, GLVersion::v3_0_ES, "GL_EXT_draw_buffers");

  case DeviceFeatures::StandardDerivative:
    return hasDesktopOrESVersionOrExtension(
        *this, GLVersion::v2_0, GLVersion::v3_0_ES, "GL_OES_standard_derivatives");

  case DeviceFeatures::StandardDerivativeExt:
    return hasESExtension(*this, "GL_OES_standard_derivatives");

  case DeviceFeatures::TextureFormatRG:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::TextureRgArb) || hasExtension(Extensions::TextureRgExt);

  case DeviceFeatures::TextureFormatRGB:
    return true;

  case DeviceFeatures::ReadWriteFramebuffer:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::FramebufferObject) ||
           hasESExtension(*this, "GL_APPLE_framebuffer_multisample");

  case DeviceFeatures::TextureNotPot:
    return hasDesktopOrESVersionOrExtension(
        *this, GLVersion::v2_0, GLVersion::v3_0_ES, "GL_OES_texture_npot");

  case DeviceFeatures::UniformBlocks:
    return hasDesktopOrESVersionOrExtension(
        *this, GLVersion::v3_1, GLVersion::v3_0_ES, "GL_ARB_uniform_buffer_object");

  case DeviceFeatures::TextureHalfFloat:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::TextureFloat) || hasExtension(Extensions::TextureHalfFloat);

  case DeviceFeatures::TextureFloat:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::TextureFloat) || hasESExtension(*this, "GL_OES_texture_float");

  case DeviceFeatures::Texture2DArray:
    return (hasDesktopOrESVersionOrExtension(
                *this, GLVersion::v3_0, GLVersion::v3_0_ES, "GL_EXT_texture_array") ||
            hasDesktopExtension(*this, "GL_EXT_gpu_shader4"));

  case DeviceFeatures::Texture3D:
    return hasDesktopOrESVersionOrExtension(
        *this, GLVersion::v2_0, GLVersion::v3_0_ES, "GL_OES_texture_3D");

  case DeviceFeatures::TextureArrayExt:
    return hasDesktopExtension(*this, "GL_EXT_texture_array") ||
           hasDesktopExtension(*this, "GL_EXT_gpu_shader4");

  case DeviceFeatures::ShaderTextureLod:
    return hasDesktopOrESVersionOrExtension(*this,
                                            GLVersion::v3_0,
                                            GLVersion::v3_0_ES,
                                            "GL_ARB_shader_texture_lod",
                                            "GL_EXT_shader_texture_lod");

  case DeviceFeatures::ShaderTextureLodExt:
    return hasDesktopOrESExtension(*this, "GL_ARB_shader_texture_lod", "GL_EXT_shader_texture_lod");

  case DeviceFeatures::DepthShaderRead:
    // Currently it is unclear if Depth Shader Read is the same as ARB_depth_texture extension so
    // we are using v2.1 because we know it works on the Mac.
    return hasDesktopOrESVersion(*this, GLVersion::v2_1, GLVersion::v3_0_ES);

  case DeviceFeatures::DepthCompare:
    return hasDesktopOrESVersion(*this, GLVersion::v2_0, GLVersion::v3_0_ES);

  case DeviceFeatures::MinMaxBlend:
    return hasDesktopOrESVersionOrExtension(
        *this, GLVersion::v2_0, GLVersion::v3_0_ES, "GL_EXT_blend_minmax");

  case DeviceFeatures::TextureExternalImage:
    return hasESVersionOrExtension(*this, GLVersion::v3_0_ES, "GL_OES_EGL_image_external_essl3") ||
           hasESExtension(*this, "GL_OES_EGL_image_external");

  case DeviceFeatures::Compute:
    return hasDesktopOrESVersion(*this, GLVersion::v4_3, GLVersion::v3_1_ES) ||
           (hasDesktopExtension(*this, "GL_ARB_compute_shader") &&
            hasInternalFeature(InternalFeatures::ProgramInterfaceQuery) &&
            hasInternalFeature(InternalFeatures::ShaderImageLoadStore));

  case DeviceFeatures::TextureBindless:
    return hasDesktopExtension(*this, "GL_ARB_bindless_texture");

  case DeviceFeatures::ExplicitBinding:
    return hasDesktopOrESVersionOrExtension(
        *this, GLVersion::v4_2, GLVersion::v3_1_ES, "GL_ARB_shading_language_420pack");

  case DeviceFeatures::ExplicitBindingExt:
    return hasDesktopExtension(*this, "GL_ARB_shading_language_420pack");

  case DeviceFeatures::PushConstants:
    return false;

  case DeviceFeatures::BufferDeviceAddress:
    return false;

  case DeviceFeatures::Multiview:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) &&
           isSupported("GL_OVR_multiview2");

  case DeviceFeatures::TexturePartialMipChain:
    return hasDesktopOrESVersion(*this, GLVersion::v2_0, GLVersion::v3_0_ES) ||
           hasESExtension(*this, "GL_APPLE_texture_max_level");

  case DeviceFeatures::BindUniform:
    return true;
  case DeviceFeatures::BufferRing:
    return false;
  case DeviceFeatures::BufferNoCopy:
    return false;
  case DeviceFeatures::ShaderLibrary:
    return false;
  case DeviceFeatures::BindBytes:
    return false;
  case DeviceFeatures::SRGB:
    return hasDesktopOrESVersionOrExtension(
        *this, GLVersion::v2_1, GLVersion::v3_0_ES, "GL_EXT_texture_sRGB", "GL_EXT_sRGB");
  case DeviceFeatures::SRGBWriteControl:
    return hasDesktopVersion(*this, GLVersion::v3_0) ||
           hasDesktopExtension(*this, "GL_ARB_framebuffer_sRGB") ||
           hasDesktopExtension(*this, "GL_EXT_framebuffer_sRGB") ||
           hasESExtension(*this, "GL_EXT_sRGB_write_control");

  case DeviceFeatures::SamplerMinMaxLod:
    return hasDesktopOrESVersion(*this, GLVersion::v2_0, GLVersion::v3_0_ES);

  case DeviceFeatures::DrawIndexedIndirect:
    return hasDesktopOrESVersionOrExtension(
        *this, GLVersion::v4_0, GLVersion::v3_1_ES, "GL_ARB_draw_indirect");

  case DeviceFeatures::ValidationLayersEnabled:
    return false;
  }

  return false;
}

bool DeviceFeatureSet::isInternalFeatureSupported(InternalFeatures feature) const {
  switch (feature) {
  case InternalFeatures::ClearDepthf:
    return hasDesktopOrESVersion(*this, GLVersion::v4_1, GLVersion::v2_0_ES);

  case InternalFeatures::Debug:
    return hasDesktopOrESVersion(*this, GLVersion::v4_3, GLVersion::v3_2_ES) ||
           hasExtension(Extensions::Debug) || hasExtension(Extensions::DebugMarker);

  case InternalFeatures::FramebufferBlit:
    // TODO: Add support for GL_ANGLE_framebuffer_blit
    return hasDesktopOrESVersionOrExtension(
               *this, GLVersion::v3_0, GLVersion::v3_0_ES, "GL_EXT_framebuffer_blit") ||
           hasExtension(Extensions::FramebufferObject);

  case InternalFeatures::FramebufferObject:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v2_0_ES) ||
           hasExtension(Extensions::FramebufferObject);

  case InternalFeatures::GetStringi:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES);

  case InternalFeatures::InvalidateFramebuffer:
    return hasDesktopOrESVersion(*this, GLVersion::v4_3, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::InvalidateSubdata) ||
           hasExtension(Extensions::DiscardFramebuffer);

  case InternalFeatures::MapBuffer:
    return hasDesktopVersion(*this, GLVersion::v2_0) || hasExtension(Extensions::MapBuffer);

  case InternalFeatures::PixelBufferObject:
    return hasDesktopOrESVersionOrExtension(*this,
                                            GLVersion::v2_1,
                                            GLVersion::v3_0_ES,
                                            "GL_ARB_pixel_buffer_object",
                                            "GL_NV_pixel_buffer_object");

  case InternalFeatures::PolygonFillMode:
    return hasDesktopVersion(*this, GLVersion::v2_0);

  case InternalFeatures::ProgramInterfaceQuery:
    return hasDesktopOrESVersion(*this, GLVersion::v4_3, GLVersion::v3_1_ES) ||
           hasDesktopExtension(*this, "GL_ARB_program_interface_query");

  case InternalFeatures::SeamlessCubeMap:
    return hasDesktopVersionOrExtension(*this, GLVersion::v3_2, "GL_ARB_seamless_cube_map");

  case InternalFeatures::Sync:
    return hasDesktopOrESVersion(*this, GLVersion::v3_2, GLVersion::v3_0_ES) ||
           hasDesktopExtension(*this, "GL_ARB_sync") || hasExtension(Extensions::Sync);

  case InternalFeatures::TexStorage:
    return hasDesktopOrESVersionOrExtension(
               *this, GLVersion::v4_2, GLVersion::v3_0_ES, "GL_ARB_texture_storage") ||
           hasExtension(Extensions::TexStorage);

  case InternalFeatures::ShaderImageLoadStore:
    return hasDesktopOrESVersion(*this, GLVersion::v4_2, GLVersion::v3_1_ES) ||
           hasDesktopExtension(*this, "GL_ARB_shader_image_load_store") ||
           hasExtension(Extensions::ShaderImageLoadStore);

  case InternalFeatures::TextureCompare:
    return hasDesktopOrESVersion(*this, GLVersion::v2_0, GLVersion::v3_0_ES) ||
           hasESExtension(*this, "GL_EXT_shadow_samplers");

  case InternalFeatures::UnmapBuffer:
    return hasDesktopVersion(*this, GLVersion::v2_0) || hasExtension(Extensions::MapBuffer) ||
           hasExtension(Extensions::MapBufferRange);

  case InternalFeatures::VertexArrayObject:
    // We've had issues with VertexArrayObject support on mobile so this is disabled for OpenGL ES.
    // Previously it was enabled specifically for Quest 2 on OpenGLES by checking if
    // GL_VENDOR == "Qualcomm" and GL_RENDERER == "Adreno (TM) 650".
    // However, Galaxy S20 also matched that and VAO support caused issues.
    // @fb-only
    // @fb-only
    return hasDesktopVersionOrExtension(*this, GLVersion::v3_0, "GL_ARB_vertex_array_object");
  }

  return false;
}

bool DeviceFeatureSet::isTextureFeatureSupported(TextureFeatures feature) const {
  switch (feature) {
  case TextureFeatures::ColorFilterable16f:
    return hasDesktopOrESVersion(*this, GLVersion::v2_0, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::TextureFloat) ||
           hasESExtension(*this, "GL_OES_texture_half_float_linear");

  case TextureFeatures::ColorFilterable32f:
    return hasDesktopVersion(*this, GLVersion::v3_0) || hasExtension(Extensions::TextureFloat) ||
           hasESExtension(*this, "GL_OES_texture_float_linear");

  case TextureFeatures::ColorFormatRgb10A2UI:
    return hasDesktopOrESVersionOrExtension(
        *this, GLVersion::v4_0, GLVersion::v3_0_ES, "GL_ARB_texture_rgb10_a2ui");

  case TextureFeatures::ColorFormatRgInt:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) ||
           hasDesktopExtension(*this, "GL_ARB_texture_rg");

  case TextureFeatures::ColorFormatRgUNorm16:
    return hasDesktopVersionOrExtension(*this, GLVersion::v3_0, "GL_ARB_texture_rg") ||
           hasESExtension(*this, "GL_EXT_texture_norm16");

  case TextureFeatures::ColorRenderbuffer16f:
    return hasDesktopOrESVersionOrExtension(
               *this, GLVersion::v3_0, GLVersion::v3_2_ES, "GL_EXT_color_buffer_half_float") ||
           (hasExtension(Extensions::FramebufferObject) && hasExtension(Extensions::TextureFloat));

  case TextureFeatures::ColorRenderbuffer32f:
    return hasDesktopOrESVersionOrExtension(
               *this, GLVersion::v3_0, GLVersion::v3_2_ES, "GL_EXT_color_buffer_float") ||
           (hasExtension(Extensions::FramebufferObject) && hasExtension(Extensions::TextureFloat));

  case TextureFeatures::ColorRenderbufferRg16f:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_2_ES) ||
           hasESExtension(*this, "GL_EXT_color_buffer_float") ||
           hasESExtension(*this, "GL_EXT_color_buffer_half_float");

  case TextureFeatures::ColorRenderbufferRg32f:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_2_ES) ||
           hasESExtension(*this, "GL_EXT_color_buffer_float");

  case TextureFeatures::ColorRenderbufferRg8:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) ||
           (hasExtension(Extensions::FramebufferObject) &&
            hasExtension(Extensions::TextureRgArb)) ||
           hasExtension(Extensions::TextureRgExt);

  case TextureFeatures::ColorRenderbufferRgb10A2:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::RequiredInternalFormat);

  case TextureFeatures::ColorRenderbufferRgb16f:
    return hasESExtension(*this, "GL_EXT_color_buffer_half_float");

  case TextureFeatures::ColorRenderbufferRgba8:
    return hasDesktopOrESVersionOrExtension(
               *this, GLVersion::v3_0, GLVersion::v3_0_ES, "GL_OES_rgb8_rgba8") ||
           hasExtension(Extensions::FramebufferObject) ||
           hasExtension(Extensions::RequiredInternalFormat);

  case TextureFeatures::ColorRenderbufferSrgba8:
    return hasDesktopOrESVersion(*this, GLVersion::v2_1, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::Srgb);

  case TextureFeatures::ColorTexImage16f:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::TextureFloat);

  case TextureFeatures::ColorTexImage32f:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::TextureFloat);

  case TextureFeatures::ColorTexImageA8:
    // Sized alpha texture were available on Desktop OpenGL prior to deprecation in
    // Version 3.0. For later versions of OpenGL, we create GL_R8 textures and use texture
    // swizzling. Sized alpha textures are only available on OpenGL ES through extensions.
    return (hasDesktopVersion(*this, GLVersion::v2_0) &&
            !hasDesktopVersion(*this, GLVersion::v3_0)) ||
           (hasDesktopVersion(*this, GLVersion::v3_0) &&
            hasTextureFeature(TextureFeatures::ColorTexImageRg8)) ||
           hasExtension(Extensions::RequiredInternalFormat);

  case TextureFeatures::ColorTexImageBgr10A2:
    return hasDesktopVersion(*this, GLVersion::v2_0);

  case TextureFeatures::ColorTexImageBgr5A1:
    // There's no OpenGL ES extension that specifically enables support for this, but Apple
    // platforms support it.
    return !usesOpenGLES() || hasExtension(Extensions::TextureFormatBgra8888Apple);

  case TextureFeatures::ColorTexImageBgra:
    return !usesOpenGLES() || hasExtension(Extensions::TextureFormatBgra8888Ext) ||
           hasExtension(Extensions::TextureFormatBgra8888Apple);

  case TextureFeatures::ColorTexImageBgraRgba8:
    return hasDesktopVersion(*this, GLVersion::v2_0);

  case TextureFeatures::ColorTexImageBgraSrgba:
    // There's no OpenGL ES extension that specifically enables support for this, but Apple
    // platforms support it.
    return !usesOpenGLES() || (hasESVersion(*this, GLVersion::v3_0_ES) &&
                               hasExtension(Extensions::TextureFormatBgra8888Apple));

  case TextureFeatures::ColorTexImageLa:
    // LUMINANCE and LUMINANCE_ALPHA were deprecated in Desktop OpenGL 3.0, and we don't use any
    // work arounds for support after that.
    return !hasDesktopVersion(*this, GLVersion::v3_0);

  case TextureFeatures::ColorTexImageLa8:
    // Sized luminance and luminance alpha texture were available on Desktop OpenGL prior to
    // deprecation in Version 3.0. Sized luminance alpha textures are only available on OpenGL ES
    // through extensions.
    return (hasDesktopVersion(*this, GLVersion::v2_0) &&
            !hasDesktopVersion(*this, GLVersion::v3_0)) ||
           hasExtension(Extensions::RequiredInternalFormat);

  case TextureFeatures::ColorTexImageRg8:
    return hasDesktopOrESVersion(*this, GLVersion::v2_0, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::TextureRgArb);

  case TextureFeatures::ColorTexImageRgb10A2:
    return hasTextureFeature(TextureFeatures::ColorRenderbufferRgb10A2) ||
           hasExtension(Extensions::TextureType2_10_10_10_Rev);

  case TextureFeatures::ColorTexImageRgba8:
    return hasDesktopOrESVersion(*this, GLVersion::v2_0, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::RequiredInternalFormat);

  case TextureFeatures::ColorTexImageSrgba8:
    return hasDesktopOrESVersion(*this, GLVersion::v2_1, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::TextureSrgb);

  case TextureFeatures::ColorTexStorage16f:
    return hasDesktopOrESVersion(*this, GLVersion::v4_2, GLVersion::v3_0_ES) ||
           ((hasFeature(DeviceFeatures::TextureHalfFloat) ||
             hasTextureFeature(TextureFeatures::ColorRenderbuffer16f)) &&
            hasInternalFeature(InternalFeatures::TexStorage));

  case TextureFeatures::ColorTexStorage32f:
    return hasDesktopOrESVersion(*this, GLVersion::v4_2, GLVersion::v3_0_ES) ||
           ((hasFeature(DeviceFeatures::TextureFloat) ||
             hasTextureFeature(TextureFeatures::ColorRenderbuffer32f)) &&
            hasInternalFeature(InternalFeatures::TexStorage));

  case TextureFeatures::ColorTexStorageA8:
    // Sized alpha texture were available on Desktop OpenGL prior to deprecation in
    // Version 3.0. For later versions of OpenGL, we create GL_R8 textures and use texture
    // swizzling. Sized alpha textures are only available on OpenGL ES through extensions.
    return (hasDesktopVersion(*this, GLVersion::v3_0) &&
            hasTextureFeature(TextureFeatures::ColorTexStorageRg8)) ||
           hasExtension(Extensions::TexStorage);

  case TextureFeatures::ColorTexStorageBgra8:
    // TexStorage is explicitly supported when available by GL_APPLE_texture_format_BGRA8888
    // TexStorage for GL_EXT_texture_format_BGRA8888 is added by GL_EXT_texture_storage
    return (hasExtension(Extensions::TextureFormatBgra8888Apple) &&
            (hasESVersion(*this, GLVersion::v3_0_ES) ||
             hasInternalFeature(InternalFeatures::TexStorage))) ||
           (hasExtension(Extensions::TextureFormatBgra8888Ext) &&
            hasExtension(Extensions::TexStorage));

  case TextureFeatures::ColorTexStorageLa8:
    // TexStorage with sized luminance alpha formats is only supported with GL_EXT_texture_storage
    return hasExtension(Extensions::TexStorage);

  case TextureFeatures::ColorTexStorageRg8:
    return hasDesktopOrESVersion(*this, GLVersion::v4_2, GLVersion::v3_0_ES) ||
           (hasExtension(Extensions::TexStorage) && hasExtension(Extensions::TextureRgExt));

  case TextureFeatures::ColorTexStorageRgb10A2:
    return hasDesktopOrESVersion(*this, GLVersion::v4_2, GLVersion::v3_0_ES) ||
           (hasExtension(Extensions::TexStorage) &&
            hasExtension(Extensions::TextureType2_10_10_10_Rev));

  case TextureFeatures::ColorTexStorageRgba8:
    return hasTextureFeature(TextureFeatures::ColorRenderbufferRgba8) &&
           hasInternalFeature(InternalFeatures::TexStorage);

  case TextureFeatures::ColorTexStorageSrgba8:
    // NOTE: GL_EXT_texture_storage does NOT support GL_SRGB8_ALPHA8.
    return hasFeature(DeviceFeatures::SRGB) && hasInternalFeature(InternalFeatures::TexStorage) &&
           !(hasInternalRequirement(InternalRequirement::TexStorageExtReq) &&
             hasExtension(Extensions::TexStorage));

  case TextureFeatures::DepthFilterable:
    return hasDesktopVersion(*this, GLVersion::v2_0);

  case TextureFeatures::DepthRenderbuffer16:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v2_0_ES) ||
           hasExtension(Extensions::FramebufferObject);

  case TextureFeatures::DepthRenderbuffer24:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::FramebufferObject) || hasExtension(Extensions::Depth24);

  case TextureFeatures::DepthRenderbuffer32:
    // 32-bit integer depth textures are only supported on ES through specific extensions.
    return hasDesktopVersion(*this, GLVersion::v3_0) ||
           hasExtension(Extensions::FramebufferObject) || hasExtension(Extensions::Depth32);

  case TextureFeatures::Depth24Stencil8:
    return hasDesktopOrESVersionOrExtension(*this,
                                            GLVersion::v3_0,
                                            GLVersion::v3_0_ES,
                                            "GL_EXT_packed_depth_stencil",
                                            "GL_OES_packed_depth_stencil") ||
           hasExtension(Extensions::FramebufferObject);

  case TextureFeatures::Depth32FStencil8:
    return hasDesktopOrESVersionOrExtension(
        *this, GLVersion::v3_0, GLVersion::v3_0_ES, "GL_ARB_depth_buffer_float");

  case TextureFeatures::DepthTexImage:
    return hasDesktopOrESVersion(*this, GLVersion::v2_0, GLVersion::v3_0_ES) ||
           hasExtension(Extensions::DepthTexture);

  case TextureFeatures::DepthTexImage16:
    return hasDesktopOrESVersion(*this, GLVersion::v2_0, GLVersion::v3_0_ES) ||
           (hasTextureFeature(TextureFeatures::DepthTexImage) &&
            hasExtension(Extensions::RequiredInternalFormat));

  case TextureFeatures::DepthTexImage24:
    return hasDesktopOrESVersion(*this, GLVersion::v2_0, GLVersion::v3_0_ES) ||
           (hasExtension(Extensions::Depth24) && hasExtension(Extensions::RequiredInternalFormat));

  case TextureFeatures::DepthTexImage32:
    // 32-bit integer depth textures are only supported on ES through specific extensions.
    return hasDesktopVersion(*this, GLVersion::v2_0) || hasExtension(Extensions::DepthTexture) ||
           (hasExtension(Extensions::Depth32) && hasExtension(Extensions::RequiredInternalFormat));

  case TextureFeatures::DepthTexStorage16:
    return hasDesktopOrESVersion(*this, GLVersion::v4_2, GLVersion::v3_0_ES) ||
           (hasTextureFeature(TextureFeatures::DepthRenderbuffer16) &&
            hasInternalFeature(InternalFeatures::TexStorage));

  case TextureFeatures::DepthTexStorage24:
    return hasDesktopOrESVersion(*this, GLVersion::v4_2, GLVersion::v3_0_ES) ||
           (hasTextureFeature(TextureFeatures::DepthRenderbuffer24) &&
            hasInternalFeature(InternalFeatures::TexStorage));

  case TextureFeatures::DepthTexStorage32:
    // 32-bit integer depth textures are only supported on ES through specific extensions.
    return hasDesktopVersion(*this, GLVersion::v4_2) ||
           (hasExtension(Extensions::DepthTexture) && hasExtension(Extensions::TexStorage));

  case TextureFeatures::StencilTexture8:
    return hasDesktopOrESVersionOrExtension(*this,
                                            GLVersion::v4_4,
                                            GLVersion::v3_2_ES,
                                            "GL_ARB_texture_stencil8",
                                            "GL_OES_texture_stencil8");

  case TextureFeatures::TextureCompressionAstc:
    return hasESVersion(*this, GLVersion::v3_2_ES) ||
           hasDesktopOrESExtension(*this, "GL_KHR_texture_compression_astc_hdr") ||
           hasDesktopOrESExtension(*this, "GL_KHR_texture_compression_astc_ldr") ||
           hasDesktopOrESExtension(*this, "GL_OES_texture_compression_astc");

  case TextureFeatures::TextureCompressionBptc:
    return hasDesktopExtension(*this, "GL_ARB_texture_compression_bptc") ||
           hasESExtension(*this, "GL_EXT_texture_compression_bptc") ||
           hasDesktopVersion(*this, GLVersion::v4_2);

  case TextureFeatures::TextureCompressionEtc1:
    return hasESExtension(*this, "GL_EXT_compressed_ETC1_RGB8_sub_texture") ||
           hasESExtension(*this, "GL_OES_compressed_ETC1_RGB8_texture");

  case TextureFeatures::TextureCompressionEtc2Eac:
    return hasDesktopOrESVersion(*this, GLVersion::v4_3, GLVersion::v3_0_ES) ||
           hasDesktopExtension(*this, "GL_ARB_ES3_compatibility");

  case TextureFeatures::TextureCompressionPvrtc:
    return hasESExtension(*this, "GL_IMG_texture_compression_pvrtc");

  case TextureFeatures::TextureCompressionTexImage:
    // On Desktop GL, TexImage can be used to initialize a compressed texture.
    // On OpenGL ES, TexImage CANNOT be used.
    return !usesOpenGLES();

  case TextureFeatures::TextureCompressionTexStorage:
    // On Desktop GL, TexStorage CANNOT be used to initialize a compressed texture.
    // On OpenGL ES, TexStorage can be used if it is available.
    return usesOpenGLES() && hasInternalFeature(InternalFeatures::TexStorage);

  case TextureFeatures::TextureInteger:
    return hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) ||
           hasDesktopExtension(*this, "GL_EXT_texture_integer");

  case TextureFeatures::TextureTypeUInt8888Rev:
    return hasDesktopVersion(*this, GLVersion::v2_0);
  }

  return false;
}

bool DeviceFeatureSet::hasExtension(Extensions extension) const {
  const uint64_t extensionIndex = static_cast<uint64_t>(extension);
  IGL_ASSERT(extensionIndex < 64);
  const uint64_t extensionBit = 1ull << extensionIndex;
  if ((extensionCacheInitialized_ & extensionBit) == 0) {
    if (isExtensionSupported(extension)) {
      extensionCache_ |= extensionBit;
    }
    extensionCacheInitialized_ |= extensionBit;
  }

  return (extensionCache_ & extensionBit) != 0;
}

bool DeviceFeatureSet::hasFeature(DeviceFeatures feature) const {
  const uint64_t featureIndex = static_cast<uint64_t>(feature);
  IGL_ASSERT(featureIndex < 64);
  const uint64_t featureBit = 1ull << featureIndex;
  if ((featureCacheInitialized_ & featureBit) == 0) {
    if (isFeatureSupported(feature)) {
      featureCache_ |= featureBit;
    }
    featureCacheInitialized_ |= featureBit;
  }

  return (featureCache_ & featureBit) != 0;
}

bool DeviceFeatureSet::hasInternalFeature(InternalFeatures feature) const {
  const uint32_t featureIndex = static_cast<uint32_t>(feature);
  IGL_ASSERT(featureIndex < 32);
  const uint32_t featureBit = 1u << featureIndex;
  if ((internalFeatureCacheInitialized_ & featureBit) == 0) {
    if (isInternalFeatureSupported(feature)) {
      internalFeatureCache_ |= featureBit;
    }
    internalFeatureCacheInitialized_ |= featureBit;
  }

  return (internalFeatureCache_ & featureBit) != 0;
}

bool DeviceFeatureSet::hasTextureFeature(TextureFeatures feature) const {
  const uint64_t featureIndex = static_cast<uint64_t>(feature);
  IGL_ASSERT(featureIndex < 64);
  const uint64_t featureBit = 1ull << featureIndex;
  if ((textureFeatureCacheInitialized_ & featureBit) == 0) {
    if (isTextureFeatureSupported(feature)) {
      textureFeatureCache_ |= featureBit;
    }
    textureFeatureCacheInitialized_ |= featureBit;
  }

  return (textureFeatureCache_ & featureBit) != 0;
}

bool DeviceFeatureSet::hasRequirement(DeviceRequirement requirement) const {
  switch (requirement) {
  case DeviceRequirement::ExplicitBindingExtReq:
    return !usesOpenGLES() && !hasDesktopVersion(*this, GLVersion::v4_2);

  case DeviceRequirement::StandardDerivativeExtReq:

    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/fwidth.xhtml
    // On desktop GL derivatives were supported from 2.0
    // no need for extension

    // GL_OES_standard_derivatives extension required only for versions prior to ES 3.0
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    return usesOpenGLES() && !hasESVersion(*this, GLVersion::v3_0_ES);

  case DeviceRequirement::TextureArrayExtReq:
    // Array textures were introduced in OpenGL 3.0. Before OpenGL 3.0, they can be supported via
    // `GL_EXT_texture_array`
    return !usesOpenGLES() && !hasDesktopVersion(*this, GLVersion::v3_0);

  case DeviceRequirement::TextureFormatRGExtReq:
    // If we are running in on a platform that supports OpenGL ES 3.0 (which has GL_RED/GL_RG)
    // we can check if our context is using ES 3.0, otherwise fall back to `GL_EXT_texture_rg`
    return usesOpenGLES() && !hasESVersion(*this, GLVersion::v3_0_ES);

  case DeviceRequirement::ShaderTextureLodExtReq:
    // Desktop GL
    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/textureLod.xhtml
    // textureLod() was introduced in GLSL 1.3 (which corresponds to OpenGL 3.0)
    // So if we are running on anything lower than OpenGL 3.0, we will need the extension

    // GL_EXT_shader_texture_lod extension required only for versions prior to ES 3.0
    // @fb-only
    // @fb-only
    // @fb-only
    // @fb-only
    return !hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES);
  }
  return false;
}

bool DeviceFeatureSet::hasInternalRequirement(InternalRequirement requirement) const {
  switch (requirement) {
  case InternalRequirement::ColorTexImageRgb5A1Unsized:
    return usesOpenGLES() && !hasESVersion(*this, GLVersion::v3_0_ES);

  case InternalRequirement::ColorTexImageRgb10A2Unsized:
    return !hasTextureFeature(TextureFeatures::ColorRenderbufferRgb10A2) &&
           hasExtension(Extensions::TextureType2_10_10_10_Rev);

  case InternalRequirement::ColorTexImageRgba4Unsized:
    return usesOpenGLES() && !hasESVersion(*this, GLVersion::v3_0_ES) &&
           !hasExtension(Extensions::RequiredInternalFormat);

  case InternalRequirement::ColorTexImageRgbApple422Unsized:
    return usesOpenGLES() && !hasESVersion(*this, GLVersion::v3_0_ES);

  case InternalRequirement::DebugExtReq:
    return !hasDesktopOrESVersion(*this, GLVersion::v4_3, GLVersion::v3_2_ES);

  case InternalRequirement::DrawBuffersExtReq:
    return usesOpenGLES() && !hasESVersion(*this, GLVersion::v3_0_ES);

  case InternalRequirement::Depth24Stencil8Unsized:
    return usesOpenGLES() && !hasESVersion(*this, GLVersion::v3_0_ES);

  case InternalRequirement::Depth32Unsized:
    return hasExtension(Extensions::DepthTexture);

  case InternalRequirement::FramebufferBlitExtReq:
    // GL_ARB_framebuffer_object also includes glBlitFramebuffer so no need to use
    // BlitFramebufferEXT if it is present.
    return !hasDesktopOrESVersion(*this, GLVersion::v3_0, GLVersion::v3_0_ES) &&
           !hasExtension(Extensions::FramebufferObject);

  case InternalRequirement::InvalidateFramebufferExtReq:
    return !hasDesktopOrESVersion(*this, GLVersion::v4_3, GLVersion::v3_0_ES) &&
           !hasExtension(Extensions::InvalidateSubdata);

  case InternalRequirement::MapBufferExtReq:
    // OpenGL ES does not include MapBuffer
    return usesOpenGLES();

  case InternalRequirement::MapBufferRangeExtReq:
    // OpenGL ES 2 does not include MapBufferRange
    return usesOpenGLES() && !hasESVersion(*this, GLVersion::v3_0_ES);

  case InternalRequirement::MultiSampleExtReq:
    // OpenGL ES has various extensions before 3.0 that are required, and
    // GL_IMG_multisampled_render_to_texture uses different enum values than later standard
    // versions.
    return !(hasDesktopVersion(*this, GLVersion::v3_0) ||
             hasExtension(Extensions::FramebufferObject) ||
             hasESVersion(*this, GLVersion::v3_0_ES));

  case InternalRequirement::ShaderImageLoadStoreExtReq:
    return !usesOpenGLES() && !hasDesktopVersion(*this, GLVersion::v4_2);

  case InternalRequirement::SyncExtReq:
    return usesOpenGLES() && !hasESVersion(*this, GLVersion::v3_0_ES);

  case InternalRequirement::SwizzleAlphaTexturesReq:

    return hasDesktopVersion(*this, GLVersion::v3_0);

  case InternalRequirement::TexStorageExtReq:
    return !hasDesktopOrESVersionOrExtension(
        *this, GLVersion::v4_2, GLVersion::v3_0_ES, "GL_ARB_texture_storage");

  case InternalRequirement::Texture3DExtReq:
    return !hasDesktopOrESVersion(*this, GLVersion::v2_0, GLVersion::v3_0_ES);

  case InternalRequirement::TextureHalfFloatExtReq:
    // GL_OES_texture_half_float extension uses different enum values for GL_HALF_FLOAT_OES than
    // GL_HALF_FLOAT.
    return usesOpenGLES() && !hasESVersion(*this, GLVersion::v3_0_ES);

  case InternalRequirement::UnmapBufferExtReq:
    // OpenGL ES 2 does not include UnmapBuffer
    return usesOpenGLES() && !hasESVersion(*this, GLVersion::v3_0_ES);

  case InternalRequirement::VertexArrayObjectExtReq:
    return usesOpenGLES() && !hasESVersion(*this, GLVersion::v3_0_ES);
  }
  return false;
}

bool DeviceFeatureSet::getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const {
  GLint tsize;
  switch (featureLimits) {
  case DeviceFeatureLimits::MaxTextureDimension1D2D:
    glContext_.getIntegerv(GL_MAX_TEXTURE_SIZE, &tsize);
    result = (size_t)tsize;
    return true;

  case DeviceFeatureLimits::MaxCubeMapDimension:
    glContext_.getIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &tsize);
    result = (size_t)tsize;
    return true;

  case DeviceFeatureLimits::MaxVertexUniformVectors:
    result = getMaxVertexUniforms();
    return true;

  case DeviceFeatureLimits::MaxFragmentUniformVectors:
    result = getMaxFragmentUniforms();
    return true;

  case DeviceFeatureLimits::MaxMultisampleCount:
    tsize = 0;
    if (hasFeature(DeviceFeatures::MultiSample)) {
      if (hasInternalRequirement(InternalRequirement::MultiSampleExtReq) &&
          hasExtension(Extensions::MultiSampleImg)) {
        glContext_.getIntegerv(GL_MAX_SAMPLES_IMG, &tsize);
      } else {
        // Official standards and all other extensions use the same value for GL_MAX_SAMPLES
        glContext_.getIntegerv(GL_MAX_SAMPLES, &tsize);
      }
    }
    result = (size_t)tsize;
    return true;
  case DeviceFeatureLimits::MaxPushConstantBytes:
    result = 0;
    return true;
  case DeviceFeatureLimits::MaxUniformBufferBytes:
    tsize = 0;
    if (hasFeature(DeviceFeatures::UniformBlocks)) {
      glContext_.getIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &tsize);
    }
    result = (size_t)tsize;
    return true;
  case DeviceFeatureLimits::PushConstantsAlignment:
    result = 0;
    return true;
  case DeviceFeatureLimits::ShaderStorageBufferOffsetAlignment:
    tsize = 256;
#ifdef GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT
    if (hasFeature(DeviceFeatures::UniformBlocks)) {
      glContext_.getIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &tsize);
    }
#endif
    result = (size_t)tsize;
    return true;
  case DeviceFeatureLimits::BufferAlignment:
    result = 16;
    return true;
  case DeviceFeatureLimits::BufferNoCopyAlignment:
    result = 0;
    return true;
  case DeviceFeatureLimits::MaxBindBytesBytes:
    result = 0;
    return true;
  default:
    IGL_ASSERT_MSG(0,
                   "invalid feature limit query: feature limit query is not implemented or does "
                   "not exist\n");
    return false;
  }
}

///
/// Returns the supported capabilities of the selected format.
/// @param format: The texture format to query
/// @return a combination of TextureFormatCapabilities flags
ICapabilities::TextureFormatCapabilities DeviceFeatureSet::getTextureFormatCapabilities(
    TextureFormat format) const {
  // TODO: Remove this fallback once devices can properly provide a supported format
  if (format == TextureFormat::S8_UInt_Z32_UNorm &&
      !hasTextureFeature(TextureFeatures::Depth32FStencil8)) {
    format = TextureFormat::S8_UInt_Z24_UNorm;
  }
  const auto it = textureCapabilityCache_.find(format);
  if (it != textureCapabilityCache_.end()) {
    return it->second;
  }

  const auto sampled = ICapabilities::TextureFormatCapabilityBits::Sampled;
  const auto attachment = ICapabilities::TextureFormatCapabilityBits::Attachment;
  const auto storage = hasInternalFeature(InternalFeatures::TexStorage)
                           ? ICapabilities::TextureFormatCapabilityBits::Storage
                           : 0;
  const auto sampledFiltered = ICapabilities::TextureFormatCapabilityBits::SampledFiltered;
  const auto sampledAttachment = ICapabilities::TextureFormatCapabilityBits::SampledAttachment;
  const auto unsupported = ICapabilities::TextureFormatCapabilityBits::Unsupported;
  const auto compressed = ICapabilities::TextureFormatCapabilityBits::Sampled |
                          (hasTextureFeature(TextureFeatures::TextureCompressionTexStorage)
                               ? ICapabilities::TextureFormatCapabilityBits::Storage
                               : 0);

  // Need to define here to properly include storage support.
  const auto all = sampled | sampledFiltered | storage | attachment | sampledAttachment;

  ICapabilities::TextureFormatCapabilities capabilities = unsupported;

  // First check common formats
  switch (format) {
  case TextureFormat::LA_UNorm8:
  case TextureFormat::L_UNorm8:
    if (hasTextureFeature(TextureFeatures::ColorTexImageLa)) {
      capabilities |= sampled | sampledFiltered;
      if (hasTextureFeature(TextureFeatures::ColorTexStorageLa8)) {
        capabilities |= storage;
      }
    }
    break;
  case TextureFormat::A_UNorm8:
    capabilities |= sampled | sampledFiltered;
    if (hasTextureFeature(TextureFeatures::ColorTexStorageA8)) {
      capabilities |= storage;
    }
    break;
  case TextureFormat::RGBA_UNorm8:
  case TextureFormat::RGBX_UNorm8:
    capabilities |= sampled | sampledFiltered;
    if (hasTextureFeature(TextureFeatures::ColorTexStorageRgba8)) {
      capabilities |= storage;
    }
    if (hasTextureFeature(TextureFeatures::ColorTexImageRgba8)) {
      capabilities |= sampledAttachment;
    }
    if (hasTextureFeature(TextureFeatures::ColorRenderbufferRgba8)) {
      capabilities |= attachment;
    }
    break;
  case TextureFormat::RG_UNorm8:
  case TextureFormat::R_UNorm8:
    if (hasFeature(DeviceFeatures::TextureFormatRG)) {
      capabilities |= sampled | sampledFiltered;
      if (hasTextureFeature(TextureFeatures::ColorTexStorageRg8)) {
        capabilities |= storage;
      }
      if (hasTextureFeature(TextureFeatures::ColorTexImageRg8)) {
        capabilities |= sampledAttachment;
      }
      if (hasTextureFeature(TextureFeatures::ColorRenderbufferRg8)) {
        capabilities |= attachment;
      }
    }
    break;
  case TextureFormat::BGRA_UNorm8:
    // EXT_texture_format_BGRA8888 adds support for GL_BGRA as a Renderbuffer format, but this was
    // in a later revision of the extension. It is not supported on our test devices.
    if (hasTextureFeature(TextureFeatures::ColorTexImageBgra)) {
      capabilities |= sampled | sampledFiltered;
    }
    if (hasTextureFeature(TextureFeatures::ColorTexImageBgraRgba8)) {
      capabilities |= sampledAttachment;
    }
    if (hasTextureFeature(TextureFeatures::ColorTexStorageBgra8)) {
      capabilities |= storage;
    }
    break;
  case TextureFormat::RGBA_SRGB:
    if (hasFeature(DeviceFeatures::SRGB)) {
      capabilities |= sampled | sampledFiltered;
      if (hasTextureFeature(TextureFeatures::ColorTexStorageSrgba8)) {
        capabilities |= storage;
      }
      if (hasTextureFeature(TextureFeatures::ColorTexImageSrgba8)) {
        capabilities |= sampledAttachment;
      }
      if (hasTextureFeature(TextureFeatures::ColorRenderbufferSrgba8)) {
        capabilities |= attachment;
      }
    }
    break;
  case TextureFormat::BGRA_SRGB:
    if (hasFeature(DeviceFeatures::SRGB) &&
        hasTextureFeature(TextureFeatures::ColorTexImageBgraSrgba)) {
      capabilities |= sampled | sampledFiltered;
    }
    break;
  case TextureFormat::RGBA_F16:
    if (hasFeature(DeviceFeatures::TextureHalfFloat)) {
      capabilities |= sampled;
    }
    if (hasTextureFeature(TextureFeatures::ColorTexImage16f)) {
      capabilities |= sampledAttachment;
    }
    if (hasTextureFeature(TextureFeatures::ColorTexStorage16f)) {
      capabilities |= storage;
    }
    if (hasTextureFeature(TextureFeatures::ColorRenderbuffer16f)) {
      capabilities |= attachment;
    }
    if (hasTextureFeature(TextureFeatures::ColorFilterable16f)) {
      capabilities |= sampledFiltered;
    }
    break;
  case TextureFormat::RGB_F16:
    // RGB floating point textures are NOT renderable
    if (hasFeature(DeviceFeatures::TextureHalfFloat)) {
      capabilities |= sampled;
    }
    if (hasTextureFeature(TextureFeatures::ColorRenderbufferRgb16f)) {
      capabilities |= attachment | sampledAttachment;
    }
    if (hasTextureFeature(TextureFeatures::ColorTexStorage16f)) {
      capabilities |= storage;
    }
    if (hasTextureFeature(TextureFeatures::ColorFilterable16f)) {
      capabilities |= sampledFiltered;
    }
    break;
  case TextureFormat::RG_F16:
  case TextureFormat::R_F16:
    if (hasFeature(DeviceFeatures::TextureFormatRG)) {
      if (hasFeature(DeviceFeatures::TextureHalfFloat)) {
        capabilities |= sampled;
      }
      if (hasTextureFeature(TextureFeatures::ColorRenderbufferRg16f)) {
        capabilities |= attachment | sampledAttachment;
      }
      if (hasTextureFeature(TextureFeatures::ColorTexStorage16f)) {
        capabilities |= storage;
      }
      if (hasTextureFeature(TextureFeatures::ColorFilterable16f)) {
        capabilities |= sampledFiltered;
      }
    }
    break;
  case TextureFormat::RGBA_F32:
    if (hasFeature(DeviceFeatures::TextureFloat)) {
      capabilities |= sampled;
    }
    if (hasTextureFeature(TextureFeatures::ColorTexStorage32f)) {
      capabilities |= storage;
    }
    if (hasTextureFeature(TextureFeatures::ColorRenderbuffer32f)) {
      capabilities |= attachment | sampledAttachment;
    }
    if (hasTextureFeature(TextureFeatures::ColorFilterable32f)) {
      capabilities |= sampledFiltered;
    }
    break;
  case TextureFormat::RGB_F32:
    // RGB floating point textures are NOT renderable
    if (hasFeature(DeviceFeatures::TextureFloat)) {
      capabilities |= sampled;
    }
    if (hasTextureFeature(TextureFeatures::ColorTexStorage32f)) {
      capabilities |= storage;
    }
    if (hasTextureFeature(TextureFeatures::ColorFilterable32f)) {
      capabilities |= sampledFiltered;
    }
    break;
  case TextureFormat::R_F32:
    if (hasFeature(DeviceFeatures::TextureFormatRG)) {
      if (hasFeature(DeviceFeatures::TextureFloat)) {
        capabilities |= sampled;
      }
      if (hasTextureFeature(TextureFeatures::ColorTexStorage32f)) {
        capabilities |= storage;
      }
      if (hasTextureFeature(TextureFeatures::ColorRenderbufferRg32f)) {
        capabilities |= sampledAttachment | attachment;
      }
      if (hasTextureFeature(TextureFeatures::ColorFilterable32f)) {
        capabilities |= sampledFiltered;
      }
    }
    break;
  case TextureFormat::R_UNorm16:
  case TextureFormat::RG_UNorm16:
    if (hasTextureFeature(TextureFeatures::ColorFormatRgUNorm16)) {
      capabilities |= all;
    }
    break;
  case TextureFormat::R_UInt16:
  case TextureFormat::RG_UInt16:
    if (hasTextureFeature(TextureFeatures::ColorFormatRgInt)) {
      capabilities |= sampled | storage | attachment;
    }
    break;
  case TextureFormat::RGBA_UInt32:
    if (hasTextureFeature(TextureFeatures::TextureInteger)) {
      capabilities |= sampled | storage | attachment | sampledAttachment;
    }
    break;
  case TextureFormat::B5G5R5A1_UNorm:
    if (hasTextureFeature(TextureFeatures::ColorTexImageBgr5A1)) {
      capabilities |= sampled | sampledFiltered;
    }
    break;
  case TextureFormat::ABGR_UNorm4:
    capabilities |= all;

    break;
  case TextureFormat::R4G2B2_UNorm_Apple:
    if (hasExtension(Extensions::AppleRgb422)) {
      // GL_APPLE_rgb_422 formats are not color-renderable formats
      capabilities |= sampled | sampledFiltered;
      if (hasInternalFeature(InternalFeatures::TexStorage)) {
        capabilities |= storage;
      }
    }
    break;
  case TextureFormat::R4G2B2_UNorm_Rev_Apple:
    if (hasExtension(Extensions::AppleRgb422)) {
      // GL_APPLE_rgb_422 formats are not color-renderable formats
      // TexStorage does not support UNSIGNED_SHORT_8_8_REV_APPLE
      capabilities |= sampled | sampledFiltered;
    }
    break;
  case TextureFormat::R5G5B5A1_UNorm:
    capabilities |= sampled | sampledFiltered | storage;
    if (hasInternalFeature(InternalFeatures::FramebufferObject)) {
      capabilities |= attachment | sampledAttachment;
    }
    break;
  case TextureFormat::BGR10_A2_Unorm:
    if (hasTextureFeature(TextureFeatures::ColorTexImageBgr10A2)) {
      capabilities |= sampled | sampledFiltered;
    }
    break;
  case TextureFormat::RGB10_A2_UNorm_Rev:
    if (hasTextureFeature(TextureFeatures::ColorTexImageRgb10A2)) {
      capabilities |= sampled | sampledFiltered;
      if (!hasInternalRequirement(InternalRequirement::ColorTexImageRgb10A2Unsized)) {
        capabilities |= sampledAttachment;
      }
    }
    if (hasTextureFeature(TextureFeatures::ColorTexStorageRgb10A2)) {
      capabilities |= storage;
    }
    if (hasTextureFeature(TextureFeatures::ColorRenderbufferRgb10A2)) {
      capabilities |= attachment;
    }
    break;
  case TextureFormat::RGB10_A2_Uint_Rev:
    if (hasTextureFeature(TextureFeatures::ColorFormatRgb10A2UI)) {
      capabilities |= sampled | storage | attachment | sampledAttachment;
    }
    break;
  case TextureFormat::BGRA_UNorm8_Rev:
    if (hasTextureFeature(TextureFeatures::TextureTypeUInt8888Rev)) {
      capabilities |= sampled | sampledFiltered;
    }
    break;
  case TextureFormat::B5G6R5_UNorm:
    // Unsupported
    break;
  case TextureFormat::Z_UNorm16:
    if (hasTextureFeature(TextureFeatures::DepthTexImage)) {
      capabilities |= sampled;
    }
    if (hasTextureFeature(TextureFeatures::DepthTexImage16)) {
      capabilities |= sampledAttachment;
    }
    if (hasTextureFeature(TextureFeatures::DepthTexStorage16)) {
      capabilities |= storage;
    }
    if (hasTextureFeature(TextureFeatures::DepthRenderbuffer16)) {
      capabilities |= attachment;
    }
    if (hasTextureFeature(TextureFeatures::DepthFilterable)) {
      capabilities |= sampledFiltered;
    }
    break;
  case TextureFormat::Z_UNorm24:
    if (hasTextureFeature(TextureFeatures::DepthTexImage24)) {
      capabilities |= sampled | sampledAttachment;
    }
    if (hasTextureFeature(TextureFeatures::DepthTexStorage24)) {
      capabilities |= storage;
    }
    if (hasTextureFeature(TextureFeatures::DepthRenderbuffer24)) {
      capabilities |= attachment;
    }
    if (hasTextureFeature(TextureFeatures::DepthFilterable)) {
      capabilities |= sampledFiltered;
    }

    // TODO: Remove these fallback once devices can properly provide a supported format
    if (hasTextureFeature(TextureFeatures::DepthTexImage32)) {
      capabilities |= sampled | sampledAttachment;
    }
    if (hasTextureFeature(TextureFeatures::DepthTexStorage32)) {
      capabilities |= storage;
    }
    if (hasTextureFeature(TextureFeatures::DepthRenderbuffer32)) {
      capabilities |= attachment;
    }
    if (hasTextureFeature(TextureFeatures::DepthFilterable)) {
      capabilities |= sampledFiltered;
    }
    break;
  case TextureFormat::Z_UNorm32:
    if (hasTextureFeature(TextureFeatures::DepthTexImage32)) {
      capabilities |= sampled | sampledAttachment;
    }
    if (hasTextureFeature(TextureFeatures::DepthTexStorage32)) {
      capabilities |= storage;
    }
    if (hasTextureFeature(TextureFeatures::DepthRenderbuffer32)) {
      capabilities |= attachment;
    }
    if (hasTextureFeature(TextureFeatures::DepthFilterable)) {
      capabilities |= sampledFiltered;
    }
    break;
  case TextureFormat::S8_UInt_Z24_UNorm:
    if (hasTextureFeature(TextureFeatures::Depth24Stencil8)) {
      capabilities |= sampled | attachment | sampledAttachment;
      if (hasInternalFeature(InternalFeatures::TexStorage)) {
        capabilities |= storage;
      }
    }
    break;
  case TextureFormat::S8_UInt_Z32_UNorm:
    if (hasTextureFeature(TextureFeatures::Depth32FStencil8)) {
      capabilities |= sampled | attachment | sampledAttachment;
      if (hasInternalFeature(InternalFeatures::TexStorage)) {
        capabilities |= storage;
      }
    }
    break;
  case TextureFormat::S_UInt8:
    if (hasTextureFeature(TextureFeatures::StencilTexture8)) {
      capabilities |= sampled | storage;
    }
    capabilities |= attachment;
    break;

  case TextureFormat::RGBA_ASTC_4x4:
  case TextureFormat::SRGB8_A8_ASTC_4x4:
  case TextureFormat::RGBA_ASTC_5x4:
  case TextureFormat::SRGB8_A8_ASTC_5x4:
  case TextureFormat::RGBA_ASTC_5x5:
  case TextureFormat::SRGB8_A8_ASTC_5x5:
  case TextureFormat::RGBA_ASTC_6x5:
  case TextureFormat::SRGB8_A8_ASTC_6x5:
  case TextureFormat::RGBA_ASTC_6x6:
  case TextureFormat::SRGB8_A8_ASTC_6x6:
  case TextureFormat::RGBA_ASTC_8x5:
  case TextureFormat::SRGB8_A8_ASTC_8x5:
  case TextureFormat::RGBA_ASTC_8x6:
  case TextureFormat::SRGB8_A8_ASTC_8x6:
  case TextureFormat::RGBA_ASTC_8x8:
  case TextureFormat::SRGB8_A8_ASTC_8x8:
  case TextureFormat::RGBA_ASTC_10x5:
  case TextureFormat::SRGB8_A8_ASTC_10x5:
  case TextureFormat::RGBA_ASTC_10x6:
  case TextureFormat::SRGB8_A8_ASTC_10x6:
  case TextureFormat::RGBA_ASTC_10x8:
  case TextureFormat::SRGB8_A8_ASTC_10x8:
  case TextureFormat::RGBA_ASTC_10x10:
  case TextureFormat::SRGB8_A8_ASTC_10x10:
  case TextureFormat::RGBA_ASTC_12x10:
  case TextureFormat::SRGB8_A8_ASTC_12x10:
  case TextureFormat::RGBA_ASTC_12x12:
  case TextureFormat::SRGB8_A8_ASTC_12x12:
    if (hasTextureFeature(TextureFeatures::TextureCompressionAstc)) {
      capabilities |= compressed;
    }
    break;
  case TextureFormat::RGBA_BC7_UNORM_4x4:
    if (hasTextureFeature(TextureFeatures::TextureCompressionBptc)) {
      capabilities |= compressed;
    }
    break;
  case TextureFormat::RGBA_PVRTC_2BPPV1:
  case TextureFormat::RGB_PVRTC_2BPPV1:
  case TextureFormat::RGBA_PVRTC_4BPPV1:
  case TextureFormat::RGB_PVRTC_4BPPV1:
    if (hasTextureFeature(TextureFeatures::TextureCompressionPvrtc)) {
      capabilities |= compressed;
    }
    break;
  case TextureFormat::RGB8_ETC1:
    if (hasTextureFeature(TextureFeatures::TextureCompressionEtc1)) {
      capabilities |= compressed;
    }
    break;
  case TextureFormat::RGB8_ETC2:
  case TextureFormat::SRGB8_ETC2:
  case TextureFormat::RGB8_Punchthrough_A1_ETC2:
  case TextureFormat::SRGB8_Punchthrough_A1_ETC2:
  case TextureFormat::RGBA8_EAC_ETC2:
  case TextureFormat::SRGB8_A8_EAC_ETC2:
  case TextureFormat::RG_EAC_UNorm:
  case TextureFormat::RG_EAC_SNorm:
  case TextureFormat::R_EAC_UNorm:
  case TextureFormat::R_EAC_SNorm:
    if (hasTextureFeature(TextureFeatures::TextureCompressionEtc2Eac)) {
      capabilities |= compressed;
    }
    break;
  default:
    // We are relying on the fact that TextureFormatCapabilities::Unsupported is 0
    return textureCapabilityCache_[format];
  };

  textureCapabilityCache_[format] = capabilities;
  return capabilities;
}

uint32_t DeviceFeatureSet::getMaxVertexUniforms() const {
  GLint tsize;
  // MaxVertexUniformVectors is the maximum number of 4-element vectors that can be passed as
  // uniform to a vertex shader. All uniforms are 4-element aligned, a single uniform counts at
  // least as one 4-element vector.
  // GL_MAX_VERTEX_UNIFORM_COMPONENTS available on Desktop OpenGL 2.0+ and on OpenGL ES 3.0+.
  // GL_MAX_VERTEX_UNIFORM_VECTORS is available on Desktop OpenGL 3.0+ and on OpenGL ES 2.0+.
  // GL_MAX_VERTEX_UNIFORM_VECTORS is equal to GL_MAX_VERTEX_UNIFORM_COMPONENTS / 4.
  if (hasDesktopOrESVersion(*this, GLVersion::v2_0, GLVersion::v3_0_ES)) {
    glContext_.getIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &tsize);
    tsize /= 4;
  } else {
    glContext_.getIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &tsize);
  }
  return static_cast<uint32_t>(tsize);
}

uint32_t DeviceFeatureSet::getMaxFragmentUniforms() const {
  GLint tsize;
  // PLease see comments above in getMaxVertexUniforms
  if (hasDesktopOrESVersion(*this, GLVersion::v2_0, GLVersion::v3_0_ES)) {
    glContext_.getIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &tsize);
    tsize /= 4;
  } else {
    glContext_.getIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &tsize);
  }
  return static_cast<uint32_t>(tsize);
}

uint32_t DeviceFeatureSet::getMaxComputeUniforms() const {
  if (hasFeature(DeviceFeatures::Compute)) {
    GLint tsize;
    glContext_.getIntegerv(GL_MAX_COMPUTE_UNIFORM_COMPONENTS, &tsize);
    return static_cast<uint32_t>(tsize);
  }
  return 0;
}

} // namespace igl::opengl
