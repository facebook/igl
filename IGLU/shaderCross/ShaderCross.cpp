/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/shaderCross/ShaderCross.h>

#include <igl/glslang/GlslCompiler.h>
#include <igl/glslang/GlslangHelpers.h>

#include <spirv_glsl.hpp>
#include <spirv_msl.hpp>

#include <algorithm>
#include <vector>

namespace iglu {

ShaderCross::ShaderCross(igl::IDevice& device) noexcept : device_(device) {
  igl::glslang::initializeCompiler();
}

ShaderCross::~ShaderCross() noexcept {
  igl::glslang::finalizeCompiler();
}

std::string ShaderCross::entryPointName(igl::ShaderStage /*stage*/) const noexcept {
  if (device_.getBackendType() == igl::BackendType::Metal) {
    return "main0";
  }
  if (device_.getBackendType() == igl::BackendType::OpenGL) {
    return "main";
  }
  return {};
}

std::string ShaderCross::crossCompileFromVulkanSource(const char* source,
                                                      igl::ShaderStage stage,
                                                      igl::Result* IGL_NULLABLE
                                                          outResult) const noexcept {
  if (device_.getBackendType() == igl::BackendType::Vulkan) {
    return source;
  }

  // Compile to SPIR-V.
  std::vector<uint32_t> spirvCode;
  glslang_resource_t resource{};
  glslangGetDefaultResource(&resource);
  const auto result = igl::glslang::compileShader(stage, source, spirvCode, &resource);
  if (!result.isOk()) {
    if (outResult) {
      *outResult = result;
    }
    return {};
  }

  // Cross-compile to MSL.
  if (device_.getBackendType() == igl::BackendType::Metal) {
    spirv_cross::CompilerMSL mslCompiler(std::move(spirvCode));
    spirv_cross::CompilerMSL::Options options;
#if IGL_PLATFORM_MACOS
    options.platform = spirv_cross::CompilerMSL::Options::macOS;
#else
    options.platform = spirv_cross::CompilerMSL::Options::iOS;
#endif
    options.set_msl_version(2, 2);
    options.enable_decoration_binding = true;
    mslCompiler.set_msl_options(options);
    try {
      return mslCompiler.compile();
    } catch (const spirv_cross::CompilerError& e) {
      if (outResult) {
        *outResult = igl::Result(igl::Result::Code::RuntimeError, e.what());
      }
      return {};
    }
  }

  // Cross-compile to GLSL.
  if (device_.getBackendType() == igl::BackendType::OpenGL) {
    const auto shaderVersion = device_.getShaderVersion();

    spirv_cross::CompilerGLSL glslCompiler(std::move(spirvCode));
    spirv_cross::CompilerGLSL::Options options;
    options.version =
        static_cast<uint32_t>(shaderVersion.majorVersion * 100) + shaderVersion.minorVersion;
    options.es = (shaderVersion.family == igl::ShaderFamily::GlslEs);
    options.emit_push_constant_as_uniform_buffer = true;
    options.emit_uniform_buffer_as_plain_uniforms = true;
    options.enable_420pack_extension = device_.hasFeature(igl::DeviceFeatures::ExplicitBindingExt);

    // In multiview mode in IGL, 2 views are always used.
    const auto& exts = glslCompiler.get_declared_extensions();
    const auto& caps = glslCompiler.get_declared_capabilities();
    if (stage == igl::ShaderStage::Vertex &&
        (std::any_of(std::begin(exts),
                     std::end(exts),
                     [](const std::string& ext) { return ext == "GL_OVR_multiview2"; }) ||
         std::any_of(std::begin(caps), std::end(caps), [](const spv::Capability cap) {
           return cap == spv::Capability::CapabilityMultiView;
         }))) {
      options.ovr_multiview_view_count = 2;
    }

    glslCompiler.set_common_options(options);

    try {
      return glslCompiler.compile();
    } catch (const spirv_cross::CompilerError& e) {
      if (outResult) {
        *outResult = igl::Result(igl::Result::Code::RuntimeError, e.what());
      }
      return {};
    }
  }

  if (outResult) {
    *outResult =
        igl::Result(igl::Result::Code::Unimplemented, "Cross-compilation is not implemented.");
  }
  return {};
}

} // namespace iglu
