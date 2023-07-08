/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/vulkan/VulkanShaderModule.h>

#include <glslang/Include/glslang_c_interface.h>
#include <igl/vulkan/Common.h>

namespace {

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
      LLOGL("(%3u) %s", line++, outputLine.c_str());
      outputLine = "";
    } else if (*text == '\r') {
      // skip it
    } else {
      outputLine += *text;
    }
    text++;
  }
  LLOGL("");
#else
  LLOGL("\n(%3u) ", line);
  while (text && *text) {
    if (*text == '\n') {
      LLOGL("\n(%3u) ", ++line);
    } else if (*text == '\r') {
      // skip it to support Windows/UNIX EOLs
    } else {
      LLOGL("%c", *text);
    }
    text++;
  }
  LLOGL("\n");
#endif
#endif
}

} // namespace

namespace igl {
namespace vulkan {

static glslang_stage_t getGLSLangShaderStage(VkShaderStageFlagBits stage) {
  switch (stage) {
  case VK_SHADER_STAGE_VERTEX_BIT:
    return GLSLANG_STAGE_VERTEX;
  case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
    return GLSLANG_STAGE_TESSCONTROL;
  case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
    return GLSLANG_STAGE_TESSEVALUATION;
  case VK_SHADER_STAGE_GEOMETRY_BIT:
    return GLSLANG_STAGE_GEOMETRY;
  case VK_SHADER_STAGE_FRAGMENT_BIT:
    return GLSLANG_STAGE_FRAGMENT;
  case VK_SHADER_STAGE_COMPUTE_BIT:
    return GLSLANG_STAGE_COMPUTE;
  default:
    assert(false);
  };
  assert(false);
  return GLSLANG_STAGE_COUNT;
}

Result compileShader(VkDevice device,
                     VkShaderStageFlagBits stage,
                     const char* code,
                     VkShaderModule* outShaderModule,
                     const glslang_resource_t* glslLangResource) {
  IGL_PROFILER_FUNCTION();

  if (!outShaderModule) {
    return Result(Result::Code::ArgumentNull, "outShaderModule is NULL");
  }

  const glslang_input_t input = {
      .language = GLSLANG_SOURCE_GLSL,
      .stage = getGLSLangShaderStage(stage),
      .client = GLSLANG_CLIENT_VULKAN,
      .client_version = GLSLANG_TARGET_VULKAN_1_3,
      .target_language = GLSLANG_TARGET_SPV,
      .target_language_version = GLSLANG_TARGET_SPV_1_5,
      .code = code,
      .default_version = 100,
      .default_profile = GLSLANG_NO_PROFILE,
      .force_default_version_and_profile = false,
      .forward_compatible = false,
      .messages = GLSLANG_MSG_DEFAULT_BIT,
      .resource = glslLangResource,
  };

  glslang_shader_t* shader = glslang_shader_create(&input);
  SCOPE_EXIT {
    glslang_shader_delete(shader);
  };

  if (!glslang_shader_preprocess(shader, &input)) {
    LLOGW("Shader preprocessing failed:\n");
    LLOGW("  %s\n", glslang_shader_get_info_log(shader));
    LLOGW("  %s\n", glslang_shader_get_info_debug_log(shader));
    logShaderSource(code);
    assert(false);
    return Result(Result::Code::InvalidOperation, "glslang_shader_preprocess() failed");
  }

  if (!glslang_shader_parse(shader, &input)) {
    LLOGW("Shader parsing failed:\n");
    LLOGW("  %s\n", glslang_shader_get_info_log(shader));
    LLOGW("  %s\n", glslang_shader_get_info_debug_log(shader));
    logShaderSource(glslang_shader_get_preprocessed_code(shader));
    assert(false);
    return Result(Result::Code::InvalidOperation, "glslang_shader_parse() failed");
  }

  glslang_program_t* program = glslang_program_create();
  glslang_program_add_shader(program, shader);

  SCOPE_EXIT {
    glslang_program_delete(program);
  };

  if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
    LLOGW("Shader linking failed:\n");
    LLOGW("  %s\n", glslang_program_get_info_log(program));
    LLOGW("  %s\n", glslang_program_get_info_debug_log(program));
    assert(false);
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
    LLOGW("%s\n", glslang_program_SPIRV_get_messages(program));
  }

  VK_ASSERT_RETURN(ivkCreateShaderModule(device, program, outShaderModule));

  return Result();
}

VulkanShaderModule::VulkanShaderModule(VkDevice device, VkShaderModule shaderModule) :
  device_(device), vkShaderModule_(shaderModule) {}

VulkanShaderModule::~VulkanShaderModule() {
  vkDestroyShaderModule(device_, vkShaderModule_, nullptr);
}

} // namespace vulkan
} // namespace igl
