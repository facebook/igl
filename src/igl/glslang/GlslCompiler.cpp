/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/glslang/GlslCompiler.h>
#include <igl/glslang/GlslangHelpers.h>

#include <igl/Macros.h>

#include <cstdint>
#include <string>

namespace igl::glslang {
namespace {
[[nodiscard]] glslang_stage_t getGLSLangShaderStage(igl::ShaderStage stage) noexcept {
  switch (stage) {
  case igl::ShaderStage::Vertex:
    return GLSLANG_STAGE_VERTEX;
  case igl::ShaderStage::Fragment:
    return GLSLANG_STAGE_FRAGMENT;
  case igl::ShaderStage::Compute:
    return GLSLANG_STAGE_COMPUTE;
  default:
    IGL_DEBUG_ABORT("Not supported shader stage (%d)", static_cast<int>(stage));
  };
  return GLSLANG_STAGE_COUNT;
}

// Logs GLSL shaders with line numbers annotation
void logShaderSource(const char* text) {
#if IGL_DEBUG
  uint32_t line = 1;

  // IGLLog on Android also writes a new line,
  // so to make things easier to read separate out the Android logging
#if IGL_PLATFORM_ANDROID
  std::string outputLine = "";
  while (text && *text) {
    if (*text == '\n') {
      // Write out the line along with the line number, and reset the line
      IGL_LOG_INFO("(%3u) %s", line++, outputLine.c_str());
      outputLine = "";
    } else if (*text == '\r') {
      // skip it
    } else {
      outputLine += *text;
    }
    text++;
  }
  IGL_LOG_INFO("");
#else
  IGL_LOG_INFO("\n(%3u) ", line);
  while (text && *text) {
    if (*text == '\n') {
      IGL_LOG_INFO("\n(%3u) ", ++line);
    } else if (*text == '\r') {
      // skip it to support Windows/UNIX EOLs
    } else {
      IGL_LOG_INFO("%c", *text);
    }
    text++;
  }
  IGL_LOG_INFO("\n");
#endif
#endif
}
} // namespace

void initializeCompiler() noexcept {
  glslang_initialize_process();
}

igl::Result compileShader(igl::ShaderStage stage,
                          const char* code,
                          std::vector<uint32_t>& outSPIRV,
                          const glslang_resource_t* glslLangResource) noexcept {
  IGL_PROFILER_FUNCTION();

  glslang_input_t input{};
  glslangGetDefaultInput(code, getGLSLangShaderStage(stage), glslLangResource, &input);

  glslang_shader_t* shader = glslang_shader_create(&input);
  IGL_SCOPE_EXIT {
    glslang_shader_delete(shader);
  };

  if (!glslang_shader_preprocess(shader, &input)) {
    IGL_LOG_ERROR("Shader preprocessing failed:\n");
    IGL_LOG_ERROR("  %s\n", glslang_shader_get_info_log(shader));
    IGL_LOG_ERROR("  %s\n", glslang_shader_get_info_debug_log(shader));
    logShaderSource(code);
    IGL_DEBUG_ABORT("glslang_shader_preprocess() failed");
    return Result(Result::Code::InvalidOperation, "glslang_shader_preprocess() failed");
  }

  if (!glslang_shader_parse(shader, &input)) {
    IGL_LOG_ERROR("Shader parsing failed:\n");
    IGL_LOG_ERROR("  %s\n", glslang_shader_get_info_log(shader));
    IGL_LOG_ERROR("  %s\n", glslang_shader_get_info_debug_log(shader));
    logShaderSource(glslang_shader_get_preprocessed_code(shader));
    IGL_DEBUG_ABORT("glslang_shader_parse() failed");
    return Result(Result::Code::InvalidOperation, "glslang_shader_parse() failed");
  }

  glslang_program_t* program = glslang_program_create();
  glslang_program_add_shader(program, shader);

  IGL_SCOPE_EXIT {
    glslang_program_delete(program);
  };

  if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
    IGL_LOG_ERROR("Shader linking failed:\n");
    IGL_LOG_ERROR("  %s\n", glslang_program_get_info_log(program));
    IGL_LOG_ERROR("  %s\n", glslang_program_get_info_debug_log(program));
    IGL_DEBUG_ABORT("glslang_program_link() failed");
    return Result(Result::Code::InvalidOperation, "glslang_program_link() failed");
  }

  glslang_spv_options_t options = {
      /* .generate_debug_info = */ true,
      /* .strip_debug_info = */ false,
      /* .disable_optimizer = */ false,
      /* .optimize_size = */ true,
      /* .disassemble = */ false,
      /* .validate = */ true,
      /* .emit_nonsemantic_shader_debug_info = */ false,
      /* .emit_nonsemantic_shader_debug_source = */ false,
  };

  glslang_program_SPIRV_generate_with_options(program, input.stage, &options);

  if (glslang_program_SPIRV_get_messages(program)) {
    IGL_LOG_ERROR("%s\n", glslang_program_SPIRV_get_messages(program));
  }

  const unsigned int* codePtr = glslang_program_SPIRV_get_ptr(program);

  outSPIRV = std::vector(codePtr, codePtr + glslang_program_SPIRV_get_size(program));

  return Result();
}

void finalizeCompiler() noexcept {
  glslang_finalize_process();
}

} // namespace igl::glslang
