/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/DeviceFeatures.h>
#include <igl/opengl/IContext.h>

#include <cstring>
#include <igl/Assert.h>
#include <igl/opengl/Errors.h>
#include <igl/opengl/GLFunc.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/Macros.h>
#include <optional>
#include <sstream>
#include <string>

// Uncomment to enable GL API logging
// #define IGL_API_LOG

#if defined(IGL_API_LOG) && (IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS))
#define APILOG_DEC_DRAW_COUNT() \
  if (apiLogDrawsLeft_) {       \
    apiLogDrawsLeft_--;         \
  }
#define APILOG(format, ...)                 \
  if (apiLogDrawsLeft_ || apiLogEnabled_) { \
    IGL_DEBUG_LOG(format, ##__VA_ARGS__);   \
  }
#else
#define APILOG_DEC_DRAW_COUNT() static_cast<void>(0)
#define APILOG(format, ...) static_cast<void>(0)
#endif // defined(IGL_API_LOG) && (IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS))

#define GLCALL(funcName)                                         \
  IGL_REPORT_ERROR(isCurrentContext() || isCurrentSharegroup()); \
  callCounter_++;                                                \
  gl##funcName

#define IGLCALL(funcName)                                        \
  IGL_REPORT_ERROR(isCurrentContext() || isCurrentSharegroup()); \
  callCounter_++;                                                \
  igl##funcName

#define GLCALL_WITH_RETURN(ret, funcName)                        \
  IGL_REPORT_ERROR(isCurrentContext() || isCurrentSharegroup()); \
  callCounter_++;                                                \
  ret = gl##funcName

#define IGLCALL_WITH_RETURN(ret, funcName)                       \
  IGL_REPORT_ERROR(isCurrentContext() || isCurrentSharegroup()); \
  callCounter_++;                                                \
  ret = igl##funcName

#define GLCALL_PROC(funcPtr, ...)                                \
  IGL_REPORT_ERROR(isCurrentContext() || isCurrentSharegroup()); \
  if (funcPtr) {                                                 \
    callCounter_++;                                              \
    (*funcPtr)(__VA_ARGS__);                                     \
  }

#define GLCALL_PROC_WITH_RETURN(ret, funcPtr, returnOnError, ...) \
  IGL_REPORT_ERROR(isCurrentContext() || isCurrentSharegroup());  \
  if (funcPtr) {                                                  \
    callCounter_++;                                               \
    ret = (*funcPtr)(__VA_ARGS__);                                \
  } else {                                                        \
    ret = returnOnError;                                          \
  }

#if IGL_DEBUG
#define GLCHECK_ERRORS()                      \
  do {                                        \
    if (alwaysCheckError_)                    \
      checkForErrors(__FUNCTION__, __LINE__); \
  } while (false)
#else
#define GLCHECK_ERRORS()
#endif // IGL_DEBUG

#define RESULT_CASE(res) \
  case res:              \
    return #res;

#if defined(IGL_API_LOG) && (IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS))
// Enclose this function with the #ifdef to stop the compiler
// from complaining that this function is unused (in the non debug/log path)
namespace {
std::string GLboolToString(GLboolean val) {
  return val ? "true" : "false";
}

std::string GLenumToString(GLenum code) {
  switch (code) {
    RESULT_CASE(GL_ACTIVE_ATTRIBUTES)
    RESULT_CASE(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH)
    RESULT_CASE(GL_ACTIVE_RESOURCES)
    RESULT_CASE(GL_ACTIVE_TEXTURE)
    RESULT_CASE(GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH)
    RESULT_CASE(GL_ACTIVE_UNIFORM_BLOCKS)
    RESULT_CASE(GL_ACTIVE_UNIFORM_MAX_LENGTH)
    RESULT_CASE(GL_ACTIVE_UNIFORMS)
    RESULT_CASE(GL_ALIASED_LINE_WIDTH_RANGE)
    RESULT_CASE(GL_ALPHA_BITS)
    RESULT_CASE(GL_ALPHA8)
    RESULT_CASE(GL_ALWAYS)
    RESULT_CASE(GL_ARRAY_BUFFER)
    RESULT_CASE(GL_ARRAY_BUFFER_BINDING)
    RESULT_CASE(GL_ATTACHED_SHADERS)
    RESULT_CASE(GL_BACK)
    RESULT_CASE(GL_BGR)
    RESULT_CASE(GL_BGRA)
    RESULT_CASE(GL_BGRA8_EXT)
    RESULT_CASE(GL_BLEND)
    RESULT_CASE(GL_BLEND_COLOR)
    RESULT_CASE(GL_BLEND_DST_ALPHA)
    RESULT_CASE(GL_BLEND_EQUATION_RGB)
    RESULT_CASE(GL_BLEND_EQUATION_ALPHA)
    RESULT_CASE(GL_BLEND_SRC_ALPHA)
    RESULT_CASE(GL_BLEND_SRC_RGB)
    RESULT_CASE(GL_BLUE)
    RESULT_CASE(GL_BUFFER_SIZE)
    RESULT_CASE(GL_BUFFER_USAGE)
    RESULT_CASE(GL_BYTE)
    RESULT_CASE(GL_CLAMP_TO_EDGE)
    RESULT_CASE(GL_COLOR_ATTACHMENT0)
    RESULT_CASE(GL_COLOR_ATTACHMENT1)
    RESULT_CASE(GL_COMPARE_REF_TO_TEXTURE)
    RESULT_CASE(GL_COMPRESSED_R11_EAC)
    RESULT_CASE(GL_COMPRESSED_RG11_EAC)
    RESULT_CASE(GL_COMPRESSED_RGB8_ETC2)
    RESULT_CASE(GL_COMPRESSED_RGBA8_ETC2_EAC)
    RESULT_CASE(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2)
    RESULT_CASE(GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG)
    RESULT_CASE(GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG)
    RESULT_CASE(GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG)
    RESULT_CASE(GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_10x10_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_10x6_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_10x8_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_10x5_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_12x10_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_12x12_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_4x4_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_5x4_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_5x5_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_6x5_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_6x6_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_8x5_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_8x6_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_ASTC_8x8_KHR)
    RESULT_CASE(GL_COMPRESSED_RGBA_BPTC_UNORM)
    RESULT_CASE(GL_COMPRESSED_SIGNED_R11_EAC)
    RESULT_CASE(GL_COMPRESSED_SIGNED_RG11_EAC)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ETC2)
    RESULT_CASE(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC)
    RESULT_CASE(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2)
    RESULT_CASE(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM)
    RESULT_CASE(GL_COMPILE_STATUS)
    RESULT_CASE(GL_COMPUTE_SHADER)
    RESULT_CASE(GL_CONSTANT_ALPHA)
    RESULT_CASE(GL_CONSTANT_COLOR)
    RESULT_CASE(GL_COPY_READ_BUFFER)
    RESULT_CASE(GL_COPY_WRITE_BUFFER)
    RESULT_CASE(GL_CULL_FACE)
    RESULT_CASE(GL_CURRENT_PROGRAM)
    RESULT_CASE(GL_CW)
    RESULT_CASE(GL_CCW)
    RESULT_CASE(GL_CURRENT_VERTEX_ATTRIB)
    RESULT_CASE(GL_DEBUG_SEVERITY_LOW)
    RESULT_CASE(GL_DEBUG_SOURCE_APPLICATION)
    RESULT_CASE(GL_DEBUG_TYPE_MARKER)
    RESULT_CASE(GL_DECR)
    RESULT_CASE(GL_DECR_WRAP)
    RESULT_CASE(GL_DELETE_STATUS)
    RESULT_CASE(GL_DEPTH)
    RESULT_CASE(GL_DEPTH_ATTACHMENT)
    RESULT_CASE(GL_DEPTH_COMPONENT)
    RESULT_CASE(GL_DEPTH_COMPONENT16)
    RESULT_CASE(GL_DEPTH_COMPONENT24)
    RESULT_CASE(GL_DEPTH_COMPONENT32)
    RESULT_CASE(GL_DEPTH_STENCIL)
    RESULT_CASE(GL_DEPTH24_STENCIL8)
    RESULT_CASE(GL_DEPTH32F_STENCIL8)
    RESULT_CASE(GL_DEPTH_TEST)
    RESULT_CASE(GL_DITHER)
    RESULT_CASE(GL_DONT_CARE)
    RESULT_CASE(GL_DRAW_FRAMEBUFFER)
    RESULT_CASE(GL_DRAW_INDIRECT_BUFFER)
    RESULT_CASE(GL_DST_ALPHA)
    RESULT_CASE(GL_DST_COLOR)
    RESULT_CASE(GL_DYNAMIC_COPY)
    RESULT_CASE(GL_DYNAMIC_DRAW)
    RESULT_CASE(GL_DYNAMIC_READ)
    RESULT_CASE(GL_ELEMENT_ARRAY_BUFFER)
    RESULT_CASE(GL_EQUAL)
    RESULT_CASE(GL_ETC1_RGB8_OES)
    RESULT_CASE(GL_EXTENSIONS)
    RESULT_CASE(GL_FASTEST)
    RESULT_CASE(GL_FILL)
    RESULT_CASE(GL_FLOAT)
    RESULT_CASE(GL_FLOAT_32_UNSIGNED_INT_24_8_REV)
    RESULT_CASE(GL_FLOAT_MAT2)
    RESULT_CASE(GL_FLOAT_MAT3)
    RESULT_CASE(GL_FLOAT_MAT4)
    RESULT_CASE(GL_FLOAT_VEC2)
    RESULT_CASE(GL_FLOAT_VEC3)
    RESULT_CASE(GL_FLOAT_VEC4)
    RESULT_CASE(GL_FRAGMENT_SHADER)
    RESULT_CASE(GL_FRAGMENT_SHADER_DERIVATIVE_HINT)
    RESULT_CASE(GL_FRAMEBUFFER)
    RESULT_CASE(GL_FRAMEBUFFER_DEFAULT)
    RESULT_CASE(GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE)
    RESULT_CASE(GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE)
    RESULT_CASE(GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE)
    RESULT_CASE(GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING)
    RESULT_CASE(GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE)
    RESULT_CASE(GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE)
    RESULT_CASE(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME)
    RESULT_CASE(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
    RESULT_CASE(GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE)
    RESULT_CASE(GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE)
    RESULT_CASE(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE)
    RESULT_CASE(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL)
    RESULT_CASE(GL_FRAMEBUFFER_BINDING)
    RESULT_CASE(GL_FRAMEBUFFER_COMPLETE)
    RESULT_CASE(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
    RESULT_CASE(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
    RESULT_CASE(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS)
    RESULT_CASE(GL_FRAMEBUFFER_UNDEFINED)
    RESULT_CASE(GL_FRAMEBUFFER_UNSUPPORTED)
    RESULT_CASE(GL_FRAMEBUFFER_SRGB)
    RESULT_CASE(GL_FRONT)
    RESULT_CASE(GL_FRONT_AND_BACK)
    RESULT_CASE(GL_FUNC_ADD)
    RESULT_CASE(GL_FUNC_SUBTRACT)
    RESULT_CASE(GL_FUNC_REVERSE_SUBTRACT)
    RESULT_CASE(GL_KEEP)
    RESULT_CASE(GL_GENERATE_MIPMAP_HINT)
    RESULT_CASE(GL_GEQUAL)
    RESULT_CASE(GL_GREATER)
    RESULT_CASE(GL_GREEN)
    RESULT_CASE(GL_HALF_FLOAT)
    RESULT_CASE(GL_HALF_FLOAT_OES)
    RESULT_CASE(GL_HANDLE_TYPE_OPAQUE_FD_EXT)
    RESULT_CASE(GL_HIGH_FLOAT)
    RESULT_CASE(GL_HIGH_INT)
    RESULT_CASE(GL_IMAGE_1D)
    RESULT_CASE(GL_IMAGE_1D_ARRAY)
    RESULT_CASE(GL_IMAGE_2D)
    RESULT_CASE(GL_IMAGE_2D_MULTISAMPLE)
    RESULT_CASE(GL_IMAGE_2D_ARRAY)
    RESULT_CASE(GL_IMAGE_2D_MULTISAMPLE_ARRAY)
    RESULT_CASE(GL_IMAGE_3D)
    RESULT_CASE(GL_IMAGE_CUBE)
    RESULT_CASE(GL_INCR)
    RESULT_CASE(GL_INCR_WRAP)
    RESULT_CASE(GL_INFO_LOG_LENGTH)
    RESULT_CASE(GL_INT)
    RESULT_CASE(GL_INT_2_10_10_10_REV)
    RESULT_CASE(GL_INVERT)
    RESULT_CASE(GL_LESS)
    RESULT_CASE(GL_LEQUAL)
    RESULT_CASE(GL_LINE)
    RESULT_CASE(GL_LINE_STRIP)
    RESULT_CASE(GL_LINE_LOOP)
    RESULT_CASE(GL_LINEAR)
    RESULT_CASE(GL_LINEAR_MIPMAP_NEAREST)
    RESULT_CASE(GL_LINEAR_MIPMAP_LINEAR)
    RESULT_CASE(GL_LINES)
    RESULT_CASE(GL_LINK_STATUS)
    RESULT_CASE(GL_LOW_FLOAT)
    RESULT_CASE(GL_LOW_INT)
    RESULT_CASE(GL_LUMINANCE)
    RESULT_CASE(GL_LUMINANCE_ALPHA)
    RESULT_CASE(GL_LUMINANCE8)
    RESULT_CASE(GL_LUMINANCE8_ALPHA8)
    RESULT_CASE(GL_MAX)
    RESULT_CASE(GL_MAX_COMPUTE_UNIFORM_COMPONENTS)
    RESULT_CASE(GL_MAX_CUBE_MAP_TEXTURE_SIZE)
    RESULT_CASE(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS)
    RESULT_CASE(GL_MAX_FRAGMENT_UNIFORM_VECTORS)
    RESULT_CASE(GL_MAX_NAME_LENGTH)
    RESULT_CASE(GL_MAX_SAMPLES)
    RESULT_CASE(GL_MAX_SAMPLES_IMG)
    RESULT_CASE(GL_MAX_TEXTURE_SIZE)
    RESULT_CASE(GL_MAX_UNIFORM_BLOCK_SIZE)
    RESULT_CASE(GL_MAX_VERTEX_ATTRIBS)
    RESULT_CASE(GL_MAX_VERTEX_UNIFORM_COMPONENTS)
    RESULT_CASE(GL_MAX_VERTEX_UNIFORM_VECTORS)
    RESULT_CASE(GL_MEDIUM_FLOAT)
    RESULT_CASE(GL_MEDIUM_INT)
    RESULT_CASE(GL_MIN)
    RESULT_CASE(GL_MIRRORED_REPEAT)
    RESULT_CASE(GL_NEAREST)
    RESULT_CASE(GL_NEAREST_MIPMAP_NEAREST)
    RESULT_CASE(GL_NEAREST_MIPMAP_LINEAR)
    RESULT_CASE(GL_NEVER)
    RESULT_CASE(GL_NICEST)
    RESULT_CASE(GL_NOTEQUAL)
    RESULT_CASE(GL_NUM_EXTENSIONS)
    RESULT_CASE(GL_ONE_MINUS_CONSTANT_ALPHA)
    RESULT_CASE(GL_ONE_MINUS_CONSTANT_COLOR)
    RESULT_CASE(GL_ONE_MINUS_DST_ALPHA)
    RESULT_CASE(GL_ONE_MINUS_DST_COLOR)
    RESULT_CASE(GL_ONE_MINUS_SRC_ALPHA)
    RESULT_CASE(GL_ONE_MINUS_SRC_COLOR)
    RESULT_CASE(GL_PACK_ALIGNMENT)
    RESULT_CASE(GL_PIXEL_PACK_BUFFER)
    RESULT_CASE(GL_PIXEL_UNPACK_BUFFER)
    RESULT_CASE(GL_POINTS)
    RESULT_CASE(GL_POLYGON_OFFSET_FILL)
    RESULT_CASE(GL_R16)
    RESULT_CASE(GL_R16F)
    RESULT_CASE(GL_R16UI)
    RESULT_CASE(GL_R8)
    RESULT_CASE(GL_R32F)
    RESULT_CASE(GL_READ_FRAMEBUFFER)
    RESULT_CASE(GL_READ_FRAMEBUFFER_BINDING)
    RESULT_CASE(GL_READ_WRITE)
    RESULT_CASE(GL_RED)
    RESULT_CASE(GL_RED_INTEGER)
    RESULT_CASE(GL_RENDERBUFFER)
    RESULT_CASE(GL_RENDERBUFFER_ALPHA_SIZE)
    RESULT_CASE(GL_RENDERBUFFER_BINDING)
    RESULT_CASE(GL_RENDERBUFFER_BLUE_SIZE)
    RESULT_CASE(GL_RENDERBUFFER_DEPTH_SIZE)
    RESULT_CASE(GL_RENDERBUFFER_GREEN_SIZE)
    RESULT_CASE(GL_RENDERBUFFER_HEIGHT)
    RESULT_CASE(GL_RENDERBUFFER_INTERNAL_FORMAT)
    RESULT_CASE(GL_RENDERBUFFER_RED_SIZE)
    RESULT_CASE(GL_RENDERBUFFER_STENCIL_SIZE)
    RESULT_CASE(GL_RENDERBUFFER_WIDTH)
    RESULT_CASE(GL_RENDERER)
    RESULT_CASE(GL_REPEAT)
    RESULT_CASE(GL_RG)
    RESULT_CASE(GL_RG_INTEGER)
    RESULT_CASE(GL_RG16)
    RESULT_CASE(GL_RG16F)
    RESULT_CASE(GL_RG16UI)
    RESULT_CASE(GL_RG8)
    RESULT_CASE(GL_RGB)
    RESULT_CASE(GL_RGB_422_APPLE)
    RESULT_CASE(GL_RGB_INTEGER)
    RESULT_CASE(GL_RGB_RAW_422_APPLE)
    RESULT_CASE(GL_RGB10_A2)
    RESULT_CASE(GL_RGB10_A2UI)
    RESULT_CASE(GL_RGB16F)
    RESULT_CASE(GL_RGB32F)
    RESULT_CASE(GL_RGB5_A1)
    RESULT_CASE(GL_RGB8)
    RESULT_CASE(GL_RGBA)
    RESULT_CASE(GL_RGBA_INTEGER)
    RESULT_CASE(GL_RGBA16F)
    RESULT_CASE(GL_RGBA32F)
    RESULT_CASE(GL_RGBA32UI)
    RESULT_CASE(GL_RGBA4)
    RESULT_CASE(GL_RGBA8)
    RESULT_CASE(GL_REPLACE)
    RESULT_CASE(GL_SAMPLE_ALPHA_TO_COVERAGE)
    RESULT_CASE(GL_SAMPLE_COVERAGE)
    RESULT_CASE(GL_SAMPLER_1D)
    RESULT_CASE(GL_SAMPLER_1D_ARRAY)
    RESULT_CASE(GL_SAMPLER_2D_ARRAY)
    RESULT_CASE(GL_SAMPLER_3D)
    RESULT_CASE(GL_SAMPLER_EXTERNAL_OES)
    RESULT_CASE(GL_SCISSOR_TEST)
    RESULT_CASE(GL_SHADER_SOURCE_LENGTH)
    RESULT_CASE(GL_SHADER_STORAGE_BLOCK)
    RESULT_CASE(GL_SHADER_STORAGE_BUFFER)
    RESULT_CASE(GL_SHADER_TYPE)
    RESULT_CASE(GL_SHADING_LANGUAGE_VERSION)
    RESULT_CASE(GL_SHORT)
    RESULT_CASE(GL_SIGNALED)
    RESULT_CASE(GL_SIGNED_NORMALIZED)
    RESULT_CASE(GL_SRGB)
    RESULT_CASE(GL_SRGB_ALPHA)
    RESULT_CASE(GL_SRGB8)
    RESULT_CASE(GL_SRGB8_ALPHA8)
    RESULT_CASE(GL_SRC_ALPHA)
    RESULT_CASE(GL_SRC_ALPHA_SATURATE)
    RESULT_CASE(GL_SRC_COLOR)
    RESULT_CASE(GL_STATIC_COPY)
    RESULT_CASE(GL_STATIC_DRAW)
    RESULT_CASE(GL_STATIC_READ)
    RESULT_CASE(GL_STENCIL)
    RESULT_CASE(GL_STENCIL_INDEX)
    RESULT_CASE(GL_STENCIL_INDEX8)
    RESULT_CASE(GL_STENCIL_TEST)
    RESULT_CASE(GL_STENCIL_ATTACHMENT)
    RESULT_CASE(GL_STREAM_COPY)
    RESULT_CASE(GL_STREAM_DRAW)
    RESULT_CASE(GL_STREAM_READ)
    RESULT_CASE(GL_SYNC_GPU_COMMANDS_COMPLETE)
    RESULT_CASE(GL_SYNC_STATUS)
    RESULT_CASE(GL_TEXTURE_SWIZZLE_A)
    RESULT_CASE(GL_TEXTURE_SWIZZLE_B)
    RESULT_CASE(GL_TEXTURE_SWIZZLE_G)
    RESULT_CASE(GL_TEXTURE_SWIZZLE_R)
    RESULT_CASE(GL_TEXTURE)
    RESULT_CASE(GL_TEXTURE_1D)
    RESULT_CASE(GL_TEXTURE_1D_ARRAY)
    RESULT_CASE(GL_TEXTURE_2D)
    RESULT_CASE(GL_TEXTURE_2D_MULTISAMPLE)
    RESULT_CASE(GL_TEXTURE_2D_ARRAY)
    RESULT_CASE(GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
    RESULT_CASE(GL_TEXTURE_3D)
    RESULT_CASE(GL_TEXTURE_COMPARE_FUNC)
    RESULT_CASE(GL_TEXTURE_COMPARE_MODE)
    RESULT_CASE(GL_TEXTURE_CUBE_MAP)
    RESULT_CASE(GL_TEXTURE_CUBE_MAP_NEGATIVE_X)
    RESULT_CASE(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y)
    RESULT_CASE(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
    RESULT_CASE(GL_TEXTURE_CUBE_MAP_POSITIVE_X)
    RESULT_CASE(GL_TEXTURE_CUBE_MAP_POSITIVE_Y)
    RESULT_CASE(GL_TEXTURE_CUBE_MAP_POSITIVE_Z)
    RESULT_CASE(GL_TEXTURE_CUBE_MAP_SEAMLESS)
    RESULT_CASE(GL_TEXTURE_EXTERNAL_OES)
    RESULT_CASE(GL_TEXTURE_MAG_FILTER)
    RESULT_CASE(GL_TEXTURE_MAX_LEVEL)
    RESULT_CASE(GL_TEXTURE_MAX_LOD)
    RESULT_CASE(GL_TEXTURE_MIN_FILTER)
    RESULT_CASE(GL_TEXTURE_MIN_LOD)
    RESULT_CASE(GL_TEXTURE_RECTANGLE)
    RESULT_CASE(GL_TEXTURE_WRAP_R)
    RESULT_CASE(GL_TEXTURE_WRAP_S)
    RESULT_CASE(GL_TEXTURE_WRAP_T)
    RESULT_CASE(GL_TEXTURE0)
    RESULT_CASE(GL_TEXTURE1)
    RESULT_CASE(GL_TEXTURE2)
    RESULT_CASE(GL_TEXTURE3)
    RESULT_CASE(GL_TEXTURE4)
    RESULT_CASE(GL_TEXTURE5)
    RESULT_CASE(GL_TEXTURE6)
    RESULT_CASE(GL_TEXTURE7)
    RESULT_CASE(GL_TEXTURE8)
    RESULT_CASE(GL_TRIANGLES)
    RESULT_CASE(GL_TRIANGLE_FAN)
    RESULT_CASE(GL_TRIANGLE_STRIP)
    RESULT_CASE(GL_TRANSFORM_FEEDBACK_BUFFER)
    RESULT_CASE(GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES)
    RESULT_CASE(GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS)
    RESULT_CASE(GL_UNIFORM_BLOCK_BINDING)
    RESULT_CASE(GL_UNIFORM_BLOCK_DATA_SIZE)
    RESULT_CASE(GL_UNIFORM_BUFFER)
    RESULT_CASE(GL_UNIFORM_OFFSET)
    RESULT_CASE(GL_UNPACK_ALIGNMENT)
    RESULT_CASE(GL_UNSIGNED_BYTE)
    RESULT_CASE(GL_UNSIGNED_INT)
    RESULT_CASE(GL_UNSIGNED_INT_10F_11F_11F_REV)
    RESULT_CASE(GL_UNSIGNED_INT_2_10_10_10_REV)
    RESULT_CASE(GL_UNSIGNED_INT_24_8)
    RESULT_CASE(GL_UNSIGNED_INT_5_9_9_9_REV)
    RESULT_CASE(GL_UNSIGNED_INT_8_8_8_8_REV)
    RESULT_CASE(GL_UNSIGNED_SHORT)
    RESULT_CASE(GL_UNSIGNED_SHORT_1_5_5_5_REV)
    RESULT_CASE(GL_UNSIGNED_SHORT_4_4_4_4)
    RESULT_CASE(GL_UNSIGNED_SHORT_4_4_4_4_REV)
    RESULT_CASE(GL_UNSIGNED_SHORT_5_5_5_1)
    RESULT_CASE(GL_UNSIGNED_SHORT_5_6_5)
    RESULT_CASE(GL_UNSIGNED_SHORT_8_8_APPLE)
    RESULT_CASE(GL_UNSIGNED_SHORT_8_8_REV_APPLE)
    RESULT_CASE(GL_UNSIGNED_NORMALIZED)
    RESULT_CASE(GL_VALIDATE_STATUS)
    RESULT_CASE(GL_VENDOR)
    RESULT_CASE(GL_VERSION)
    RESULT_CASE(GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING)
    RESULT_CASE(GL_VERTEX_ATTRIB_ARRAY_ENABLED)
    RESULT_CASE(GL_VERTEX_ATTRIB_ARRAY_NORMALIZED)
    RESULT_CASE(GL_VERTEX_ATTRIB_ARRAY_SIZE)
    RESULT_CASE(GL_VERTEX_ATTRIB_ARRAY_STRIDE)
    RESULT_CASE(GL_VERTEX_ATTRIB_ARRAY_TYPE)
    RESULT_CASE(GL_VERTEX_ARRAY_BINDING)
    RESULT_CASE(GL_VERTEX_SHADER)
    RESULT_CASE(GL_VIEWPORT)
  default:
    std::stringstream stream;
    stream << "0x" << std::hex << code;
    return stream.str();
  }
}

#define GL_ENUM_TO_STRING(code) GLenumToString(code).c_str()
#define GL_BOOL_TO_STRING(code) GLboolToString(code).c_str()

} // namespace
#endif // defined(IGL_API_LOG) && (IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS))

namespace igl::opengl {
namespace {
const char* GLerrorToString(GLenum error) {
  switch (error) {
  case GL_NO_ERROR:
    return "";

    RESULT_CASE(GL_INVALID_ENUM)
    RESULT_CASE(GL_INVALID_VALUE)
    RESULT_CASE(GL_INVALID_OPERATION)
    RESULT_CASE(GL_INVALID_FRAMEBUFFER_OPERATION)
    RESULT_CASE(GL_OUT_OF_MEMORY)
  default:
    return "UNKNOWN GL ERROR";
    break;
  }
}

Result::Code GLerrorToCode(GLenum error) {
  switch (error) {
  case GL_NO_ERROR:
    return Result::Code::Ok;
  case GL_INVALID_ENUM:
  case GL_INVALID_VALUE:
    return Result::Code::ArgumentInvalid;
  case GL_INVALID_OPERATION:
  case GL_INVALID_FRAMEBUFFER_OPERATION:
    return Result::Code::InvalidOperation;
  default:
    return Result::Code::RuntimeError;
  }
}

#define GL_ERROR_TO_STRING(error) GLerrorToString(error)
#define GL_ERROR_TO_RESULT(error) Result(GLerrorToCode(error), GLerrorToString(error))
} // namespace

// NOLINTNEXTLINE(modernize-use-equals-default)
IContext::IContext() : deviceFeatureSet_(*this) {
#if IGL_DEBUG
  // In debug mode, we default to always checking errors after each OGL call
  alwaysCheckError_ = true;
#endif
}

IContext::~IContext() {
  IGL_REPORT_ERROR_MSG(refCount_ == 0,
                       "Dangling IContext reference left behind."
                       // @fb-only
  );
  // Clear the zombie guard explicitly so our "secret" stays secret.
  zombieGuard_ = 0;
}

// Creates a global map to ensure multiple IContexts are not created for a single glContext
std::unordered_map<void*, IContext*>& IContext::getExistingContexts() {
  static auto& map = *(new std::unordered_map<void*, IContext*>());
  return map;
}

void IContext::registerContext(void* glContext, IContext* context) {
#if IGL_DEBUG
  auto result = IContext::getExistingContexts().find(glContext);
  if (result != IContext::getExistingContexts().end()) {
    const char* errorMessage =
        "Your application creates multiple IContext wrappers for the same underlying context "
        "object, which can result in problems if those contexts are used simultaneously across "
        "different threads. It's recommended to preserve a one-to-one relationship between native "
        "and IGL contexts. Ignore this warning at your own risk.";
#if IGL_PLATFORM_ANDROID
    IGL_LOG_ERROR(errorMessage);
#else
    IGL_ASSERT_MSG(0, errorMessage);
#endif
  }
  IContext::getExistingContexts().insert({glContext, context});
#endif
}

void IContext::unregisterContext(void* glContext) {
#if IGL_DEBUG
  IContext::getExistingContexts().erase(glContext);
#endif
}

void IContext::flushDeletionQueue() {
  deletionQueues_.flushDeletionQueue(*this);
}

bool IContext::shouldQueueAPI() const {
  return !isCurrentContext() && !isCurrentSharegroup();
}

void IContext::activeTexture(GLenum texture) {
  GLCALL(ActiveTexture)(texture);
  APILOG("glActiveTexture(%s)\n", GL_ENUM_TO_STRING(texture));
  GLCHECK_ERRORS();
}

void IContext::attachShader(GLuint program, GLuint shader) {
  GLCALL(AttachShader)(program, shader);
  APILOG("glAttachShader(%u, %u)\n", program, shader);
  GLCHECK_ERRORS();
}

void IContext::bindAttribLocation(GLuint program, GLuint index, const GLchar* name) {
  GLCALL(BindAttribLocation)(program, index, name);
  APILOG("glBindAttribLocation(%u, %u, %s)\n", program, index, name);
  GLCHECK_ERRORS();
}

void IContext::bindBuffer(GLenum target, GLuint buffer) {
  GLCALL(BindBuffer)(target, buffer);
  APILOG("glBindBuffer(%s, %u)\n", GL_ENUM_TO_STRING(target), buffer);
  GLCHECK_ERRORS();
}

void IContext::bindBufferBase(GLenum target, GLuint index, GLuint buffer) {
  IGLCALL(BindBufferBase)(target, index, buffer);
  APILOG("glBindBufferBase(%s, %u, %u)\n", GL_ENUM_TO_STRING(target), index, buffer);
  GLCHECK_ERRORS();
}

void IContext::bindBufferRange(GLenum target,
                               GLuint index,
                               GLuint buffer,
                               GLintptr offset,
                               GLsizeiptr size) {
  IGLCALL(BindBufferRange)(target, index, buffer, offset, size);
  APILOG("glBindBufferRange(%s, %u, %u)\n", GL_ENUM_TO_STRING(target), index, buffer);
  GLCHECK_ERRORS();
}

void IContext::bindFramebuffer(GLenum target, GLuint framebuffer) {
  IGLCALL(BindFramebuffer)(target, framebuffer);
  APILOG("glBindFramebuffer(%s, %u)\n", GL_ENUM_TO_STRING(target), framebuffer);
  GLCHECK_ERRORS();
}

void IContext::bindRenderbuffer(GLenum target, GLuint renderbuffer) {
  IGLCALL(BindRenderbuffer)(target, renderbuffer);
  APILOG("glBindRenderbuffer(%s, %u)\n", GL_ENUM_TO_STRING(target), renderbuffer);
  GLCHECK_ERRORS();
}

void IContext::bindTexture(GLenum target, GLuint texture) {
  GLCALL(BindTexture)(target, texture);
  APILOG("glBindTexture(%s, %u)\n", GL_ENUM_TO_STRING(target), texture);
  GLCHECK_ERRORS();
}

void IContext::bindImageTexture(GLuint unit,
                                GLuint texture,
                                GLint level,
                                GLboolean layered,
                                GLint layer,
                                GLenum access,
                                GLenum format) {
  if (bindImageTexturerProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::ShaderImageLoadStoreExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::ShaderImageLoadStore)) {
        bindImageTexturerProc_ = iglBindImageTextureEXT;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::ShaderImageLoadStore)) {
      bindImageTexturerProc_ = iglBindImageTexture;
    }
  }
  GLCALL_PROC(bindImageTexturerProc_, unit, texture, level, layered, layer, access, format);
  APILOG("glBindImageTexture(%u, %u, %i, %s, %i %s %s)\n",
         unit,
         texture,
         level,
         GL_BOOL_TO_STRING(layered),
         layer,
         GL_ENUM_TO_STRING(access),
         GL_ENUM_TO_STRING(format));
  GLCHECK_ERRORS();
}

void IContext::bindVertexArray(GLuint vao) {
  if (bindVertexArrayProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::VertexArrayObjectExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::VertexArrayObject)) {
        bindVertexArrayProc_ = iglBindVertexArrayOES;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::VertexArrayObject)) {
      bindVertexArrayProc_ = iglBindVertexArray;
    }
  }
  GLCALL_PROC(bindVertexArrayProc_, vao);
  APILOG("glBindVertexArray(%u)\n", vao);
  GLCHECK_ERRORS();
}

void IContext::blendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
  GLCALL(BlendColor)(red, green, blue, alpha);
  APILOG("glBlendColor(%f, %f, %f, %f)\n", red, green, blue, alpha);
  GLCHECK_ERRORS();
}

void IContext::blendEquation(GLenum mode) {
  GLCALL(BlendEquation)(mode);
  APILOG("glBlendEquation(%s)\n", GL_ENUM_TO_STRING(mode));
  GLCHECK_ERRORS();
}

void IContext::blendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
  GLCALL(BlendEquationSeparate)(modeRGB, modeAlpha);
  APILOG("glBlendEquationSeparate(%s, %s)\n",
         GL_ENUM_TO_STRING(modeRGB),
         GL_ENUM_TO_STRING(modeAlpha));
  GLCHECK_ERRORS();
}

void IContext::blendFunc(GLenum sfactor, GLenum dfactor) {
  GLCALL(BlendFunc)(sfactor, dfactor);
  APILOG("glBlendFunc(%s, %s)\n", GL_ENUM_TO_STRING(sfactor), GL_ENUM_TO_STRING(dfactor));
  GLCHECK_ERRORS();
}

void IContext::blendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
  GLCALL(BlendFuncSeparate)(srcRGB, dstRGB, srcAlpha, dstAlpha);
  APILOG("glBlendFuncSeparate(%s, %s, %s, %s)\n",
         GL_ENUM_TO_STRING(srcRGB),
         GL_ENUM_TO_STRING(dstRGB),
         GL_ENUM_TO_STRING(srcAlpha),
         GL_ENUM_TO_STRING(dstAlpha));
  GLCHECK_ERRORS();
}

void IContext::blitFramebuffer(GLint srcX0,
                               GLint srcY0,
                               GLint srcX1,
                               GLint srcY1,
                               GLint dstX0,
                               GLint dstY0,
                               GLint dstX1,
                               GLint dstY1,
                               GLbitfield mask,
                               GLenum filter) {
  if (blitFramebufferProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::FramebufferBlitExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::FramebufferBlit)) {
        blitFramebufferProc_ = iglBlitFramebufferEXT;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::FramebufferBlit)) {
      blitFramebufferProc_ = iglBlitFramebuffer;
    }
  }
  GLCALL_PROC(
      blitFramebufferProc_, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
  APILOG("glBlitFramebuffer(%d, %d, %d, %d, %d, %d, %d, %d, 0x%x, %s)\n",
         srcX0,
         srcY0,
         srcX1,
         srcY1,
         dstX0,
         dstY0,
         dstX1,
         dstY1,
         mask,
         GL_ENUM_TO_STRING(filter));
  GLCHECK_ERRORS();
}

void IContext::bufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage) {
  GLCALL(BufferData)(target, size, data, usage);
  APILOG("glBufferData(%s, %zu, %p, %s)\n",
         GL_ENUM_TO_STRING(target),
         size,
         data,
         GL_ENUM_TO_STRING(usage));
  GLCHECK_ERRORS();
}

void IContext::bufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data) {
  GLCALL(BufferSubData)(target, offset, size, data);
  APILOG("glBufferSubData(%s, %zu, %zu, %p)\n", GL_ENUM_TO_STRING(target), offset, size, data);
  GLCHECK_ERRORS();
}

GLenum IContext::checkFramebufferStatus(GLenum target) {
  GLenum ret;

  IGLCALL_WITH_RETURN(ret, CheckFramebufferStatus)(target);
  APILOG("glCheckFramebufferStatus(%s) = %s\n", GL_ENUM_TO_STRING(target), GL_ENUM_TO_STRING(ret));
  GLCHECK_ERRORS();
  return ret;
}

void IContext::clear(GLbitfield mask) {
  GLCALL(Clear)(mask);
  APILOG("glClear(%s %s %s)\n",
         mask & GL_COLOR_BUFFER_BIT ? "GL_COLOR_BUFFER_BIT" : "",
         mask & GL_DEPTH_BUFFER_BIT ? "GL_DEPTH_BUFFER_BIT" : "",
         mask & GL_STENCIL_BUFFER_BIT ? "GL_STENCIL_BUFFER_BIT" : "");
  GLCHECK_ERRORS();
}

void IContext::clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
  GLCALL(ClearColor)(red, green, blue, alpha);
  APILOG("glClearColor(%f, %f, %f, %f)\n", red, green, blue, alpha);
  GLCHECK_ERRORS();
}

void IContext::clearDepthf(GLfloat depth) {
  if (clearDepthfProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::ClearDepthf)) {
      clearDepthfProc_ = iglClearDepthf;
    } else {
      clearDepthfProc_ = iglClearDepth;
    }
  }

  GLCALL_PROC(clearDepthfProc_, depth);
  APILOG("glClearDepthf(%f)\n", depth);
  GLCHECK_ERRORS();
}

void IContext::clearStencil(GLint s) {
  GLCALL(ClearStencil)(s);
  APILOG("glClearStencil(%d)\n", s);
  GLCHECK_ERRORS();
}

void IContext::colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
  GLCALL(ColorMask)(red, green, blue, alpha);
  APILOG("glColorMask(%s, %s, %s, %s)\n",
         GL_BOOL_TO_STRING(red),
         GL_BOOL_TO_STRING(green),
         GL_BOOL_TO_STRING(blue),
         GL_BOOL_TO_STRING(alpha));
  GLCHECK_ERRORS();
}

void IContext::compileShader(GLuint shader) {
  GLCALL(CompileShader)(shader);
  APILOG("glCompileShader(%u)\n", shader);
  GLCHECK_ERRORS();
}

void IContext::compressedTexImage1D(IGL_MAYBE_UNUSED GLenum target,
                                    IGL_MAYBE_UNUSED GLint level,
                                    IGL_MAYBE_UNUSED GLenum internalformat,
                                    IGL_MAYBE_UNUSED GLsizei width,
                                    IGL_MAYBE_UNUSED GLint border,
                                    IGL_MAYBE_UNUSED GLsizei imageSize,
                                    IGL_MAYBE_UNUSED const GLvoid* data) {
#if IGL_OPENGL
  GLCALL(CompressedTexImage1D)(target, level, internalformat, width, border, imageSize, data);
  APILOG("glCompressedTexImage1D(%s, %d, %s, %u, %d, %u, %p)\n",
         GL_ENUM_TO_STRING(target),
         level,
         GL_ENUM_TO_STRING(internalformat),
         width,
         border,
         imageSize,
         data);
  GLCHECK_ERRORS();
#else
  IGL_ASSERT_NOT_IMPLEMENTED();
#endif
}

void IContext::compressedTexImage2D(GLenum target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border,
                                    GLsizei imageSize,
                                    const GLvoid* data) {
  GLCALL(CompressedTexImage2D)
  (target, level, internalformat, width, height, border, imageSize, data);
  APILOG("glCompressedTexImage2D(%s, %d, %s, %u, %u, %d, %u, %p)\n",
         GL_ENUM_TO_STRING(target),
         level,
         GL_ENUM_TO_STRING(internalformat),
         width,
         height,
         border,
         imageSize,
         data);
  GLCHECK_ERRORS();
}

void IContext::compressedTexImage3D(GLenum target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLsizei depth,
                                    GLint border,
                                    GLsizei imageSize,
                                    const GLvoid* data) {
  if (compressedTexImage3DProc_ == nullptr) {
    if (deviceFeatureSet_.hasFeature(DeviceFeatures::Texture3D)) {
      if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::Texture3DExtReq)) {
        if (deviceFeatureSet_.hasExtension(Extensions::Texture3D)) {
          compressedTexImage3DProc_ = iglCompressedTexImage3DOES;
        }
      } else {
        compressedTexImage3DProc_ = iglCompressedTexImage3D;
      }
    }
  }
  GLCALL_PROC(compressedTexImage3DProc_,
              target,
              level,
              internalformat,
              width,
              height,
              depth,
              border,
              imageSize,
              data);
  APILOG("glCompressedTexImage3D(%s, %d, %s, %u, %u, %u, %d, %u, %p)\n",
         GL_ENUM_TO_STRING(target),
         level,
         GL_ENUM_TO_STRING(internalformat),
         width,
         height,
         depth,
         border,
         imageSize,
         data);
  GLCHECK_ERRORS();
}

void IContext::compressedTexSubImage1D(IGL_MAYBE_UNUSED GLenum target,
                                       IGL_MAYBE_UNUSED GLint level,
                                       IGL_MAYBE_UNUSED GLint xoffset,
                                       IGL_MAYBE_UNUSED GLsizei width,
                                       IGL_MAYBE_UNUSED GLenum format,
                                       IGL_MAYBE_UNUSED GLsizei imageSize,
                                       IGL_MAYBE_UNUSED const GLvoid* data) {
#if IGL_OPENGL
  GLCALL(CompressedTexSubImage1D)(target, level, xoffset, width, format, imageSize, data);
  APILOG("glCompressedTexSubImage1D(%s, %d, %d, %u, %s, %u, %p)\n",
         GL_ENUM_TO_STRING(target),
         level,
         xoffset,
         width,
         GL_ENUM_TO_STRING(format),
         imageSize,
         data);
  GLCHECK_ERRORS();
#else
  IGL_ASSERT_NOT_IMPLEMENTED();
#endif
}

void IContext::compressedTexSubImage2D(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLenum format,
                                       GLsizei imageSize,
                                       const GLvoid* data) {
  GLCALL(CompressedTexSubImage2D)
  (target, level, xoffset, yoffset, width, height, format, imageSize, data);
  APILOG("glCompressedTexSubImage2D(%s, %d, %d, %d, %u, %u, %s, %u, %p)\n",
         GL_ENUM_TO_STRING(target),
         level,
         xoffset,
         yoffset,
         width,
         height,
         GL_ENUM_TO_STRING(format),
         imageSize,
         data);
  GLCHECK_ERRORS();
}

void IContext::compressedTexSubImage3D(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       GLenum format,
                                       GLsizei imageSize,
                                       const GLvoid* data) {
  if (compressedTexSubImage3DProc_ == nullptr) {
    if (deviceFeatureSet_.hasFeature(DeviceFeatures::Texture3D)) {
      if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::Texture3DExtReq)) {
        if (deviceFeatureSet_.hasExtension(Extensions::Texture3D)) {
          compressedTexSubImage3DProc_ = iglCompressedTexSubImage3DOES;
        }
      } else {
        compressedTexSubImage3DProc_ = iglCompressedTexSubImage3D;
      }
    }
  }
  GLCALL_PROC(compressedTexSubImage3DProc_,
              target,
              level,
              xoffset,
              yoffset,
              zoffset,
              width,
              height,
              depth,
              format,
              imageSize,
              data);
  APILOG("glCompressedTexSubImage3D(%s, %d, %d, %d, %d, %u, %u, %u, %s, %u, %p)\n",
         GL_ENUM_TO_STRING(target),
         level,
         xoffset,
         yoffset,
         zoffset,
         width,
         height,
         depth,
         GL_ENUM_TO_STRING(format),
         imageSize,
         data);
  GLCHECK_ERRORS();
}

void IContext::copyTexImage2D(GLenum target,
                              GLint level,
                              GLenum internalFormat,
                              GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height,
                              GLint border) {
  GLCALL(CopyTexImage2D)(target, level, internalFormat, x, y, width, height, border);
  APILOG("glCopyTexImage2D(%s, %d, %s, %d, %d, %u, %u, %d)\n",
         GL_ENUM_TO_STRING(target),
         level,
         GL_ENUM_TO_STRING(internalFormat),
         x,
         y,
         width,
         height,
         border);
  GLCHECK_ERRORS();
}

void IContext::copyTexSubImage2D(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height) {
  GLCALL(CopyTexSubImage2D)(target, level, xoffset, yoffset, x, y, width, height);
  APILOG("glCopyTexSubImage2D(%s, %d, %d, %d, %d, %d, %u, %u)\n",
         GL_ENUM_TO_STRING(target),
         level,
         xoffset,
         yoffset,
         x,
         y,
         width,
         height);
  GLCHECK_ERRORS();
}

void IContext::createMemoryObjects(GLsizei n, GLuint* objects) {
  IGLCALL(CreateMemoryObjectsEXT)(n, objects);
  APILOG("glCreateMemoryObjectsEXT(%u, %p)\n", n, objects);
  GLCHECK_ERRORS();
}

GLuint IContext::createProgram() {
  GLuint ret;

  GLCALL_WITH_RETURN(ret, CreateProgram)();
  APILOG("glCreateProgram() = %u\n", ret);
  GLCHECK_ERRORS();

  return ret;
}

GLuint IContext::createShader(GLenum shaderType) {
  GLuint ret;

  GLCALL_WITH_RETURN(ret, CreateShader)(shaderType);
  APILOG("glCreateShader(%s) = %u\n", GL_ENUM_TO_STRING(shaderType), ret);
  GLCHECK_ERRORS();

  return ret;
}

void IContext::cullFace(GLint mode) {
  GLCALL(CullFace)(mode);
  APILOG("glCullFace(%s)\n", GL_ENUM_TO_STRING(mode));
  GLCHECK_ERRORS();
}

void IContext::debugMessageInsert(GLenum source,
                                  GLenum type,
                                  GLuint id,
                                  GLenum severity,
                                  GLsizei length,
                                  const GLchar* buf) {
  if (debugMessageInsertProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::Debug)) {
      if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::DebugExtReq)) {
        if (deviceFeatureSet_.hasExtension(Extensions::Debug)) {
          debugMessageInsertProc_ = iglDebugMessageInsertKHR;
        } else if (deviceFeatureSet_.hasExtension(Extensions::DebugMarker)) {
          debugMessageInsertProc_ = iglInsertEventMarkerEXT;
        }
      } else {
        debugMessageInsertProc_ = iglDebugMessageInsert;
      }
    }
  }

  GLCALL_PROC(debugMessageInsertProc_, source, type, id, severity, length, buf);
  APILOG("glDebugMessageInsert(%s, %s, %u, %s, %u, %s)\n",
         GL_ENUM_TO_STRING(source),
         GL_ENUM_TO_STRING(type),
         id,
         GL_ENUM_TO_STRING(severity),
         length,
         buf);
  GLCHECK_ERRORS();
}

void IContext::deleteBuffers(GLsizei n, const GLuint* buffers) {
  if (isDestructionAllowed() && IGL_VERIFY(buffers != nullptr)) {
    if (shouldQueueAPI()) {
      deletionQueues_.queueDeleteBuffers(n, buffers);
    } else {
      GLCALL(DeleteBuffers)(n, buffers);
      APILOG("glDeleteBuffers(%u, %p)\n", n, buffers);
      GLCHECK_ERRORS();
    }
  }
}

void IContext::deleteMemoryObjects(GLsizei n, const GLuint* objects) {
  IGLCALL(DeleteMemoryObjectsEXT)(n, objects);
  APILOG("glDeleteMemoryObjectsEXT(%u, %p)\n", n, objects);
  GLCHECK_ERRORS();
}

void IContext::unbindBuffer(GLenum target) {
  if (shouldQueueAPI()) {
    deletionQueues_.queueUnbindBuffer(target);
  } else {
    bindBuffer(target, 0);
  }
}

void IContext::deleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  if (isDestructionAllowed() && IGL_VERIFY(framebuffers != nullptr)) {
    if (shouldQueueAPI()) {
      deletionQueues_.queueDeleteFramebuffers(n, framebuffers);
    } else {
      IGLCALL(DeleteFramebuffers)(n, framebuffers);
      APILOG("glDeleteFramebuffers(%u, %p)\n", n, framebuffers);
      GLCHECK_ERRORS();
    }
  }
}

void IContext::deleteProgram(GLuint program) {
  if (isDestructionAllowed()) {
    if (shouldQueueAPI()) {
      deletionQueues_.queueDeleteProgram(program);
    } else {
      GLCALL(DeleteProgram)(program);
      APILOG("glDeleteProgram(%u)\n", program);
      GLCHECK_ERRORS();
    }
  }
}

void IContext::deleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  if (isDestructionAllowed() && IGL_VERIFY(renderbuffers != nullptr)) {
    if (shouldQueueAPI()) {
      deletionQueues_.queueDeleteRenderbuffers(n, renderbuffers);
    } else {
      IGLCALL(DeleteRenderbuffers)(n, renderbuffers);
      APILOG("glDeleteRenderbuffers(%u, %p)\n", n, renderbuffers);
      GLCHECK_ERRORS();
    }
  }
}

void IContext::deleteVertexArrays(GLsizei n, const GLuint* vertexArrays) {
  if (deleteVertexArraysProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::VertexArrayObjectExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::VertexArrayObject)) {
        deleteVertexArraysProc_ = iglDeleteVertexArraysOES;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::VertexArrayObject)) {
      deleteVertexArraysProc_ = iglDeleteVertexArrays;
    }
  }
  if (isDestructionAllowed() && IGL_VERIFY(vertexArrays != nullptr)) {
    if (shouldQueueAPI()) {
      deletionQueues_.queueDeleteVertexArrays(n, vertexArrays);
    } else {
      GLCALL_PROC(deleteVertexArraysProc_, n, vertexArrays);
      APILOG("glDeleteVertexArrays(%u, %p)\n", n, vertexArrays);
      GLCHECK_ERRORS();
    }
  }
}

void IContext::deleteShader(GLuint shaderId) {
  if (isDestructionAllowed()) {
    if (shouldQueueAPI()) {
      deletionQueues_.queueDeleteShader(shaderId);
    } else {
      GLCALL(DeleteShader)(shaderId);
      APILOG("glDeleteShader(%u)\n", shaderId);
      GLCHECK_ERRORS();
    }
  }
}

void IContext::deleteSync(GLsync sync) {
  if (deleteSyncProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::SyncExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::Sync)) {
        deleteSyncProc_ = iglDeleteSyncAPPLE;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::Sync)) {
      deleteSyncProc_ = iglDeleteSync;
    }
  }

  GLCALL_PROC(deleteSyncProc_, sync);
  APILOG("glDeleteSync(%p)\n", sync);
  GLCHECK_ERRORS();
}

void IContext::deleteTextures(const std::vector<GLuint>& textures) {
  if (isDestructionAllowed() && !textures.empty()) {
    if (shouldQueueAPI()) {
      deletionQueues_.queueDeleteTextures(textures);
    } else {
      GLCALL(DeleteTextures)(static_cast<GLsizei>(textures.size()), textures.data());
      APILOG("glDeleteTextures(%u, %p)\n", textures.size(), textures.data());
      GLCHECK_ERRORS();
    }
  }
}

void IContext::depthFunc(GLenum func) {
  GLCALL(DepthFunc)(func);
  APILOG("glDepthFunc(%s)\n", GL_ENUM_TO_STRING(func));
  GLCHECK_ERRORS();
}

void IContext::depthMask(GLboolean flag) {
  GLCALL(DepthMask)(flag);
  APILOG("glDepthMask(%s)\n", GL_BOOL_TO_STRING(flag));
  GLCHECK_ERRORS();
}

void IContext::depthRangef(GLfloat n, GLfloat f) {
  GLCALL(DepthRangef)(n, f);
  APILOG("glDepthRangef(%f, %f)\n", n, f);
  GLCHECK_ERRORS();
}

void IContext::detachShader(GLuint program, GLuint shader) {
  GLCALL(DetachShader)(program, shader);
  APILOG("glDetachShader(%u, %u)\n", program, shader);
  GLCHECK_ERRORS();
}

void IContext::disable(GLenum cap) {
  GLCALL(Disable)(cap);
  APILOG("glDisable(%s)\n", GL_ENUM_TO_STRING(cap));
  GLCHECK_ERRORS();
}

void IContext::disableVertexAttribArray(GLuint index) {
  GLCALL(DisableVertexAttribArray)(index);
  APILOG("glDisableVertexAttribArray(%u)\n", index);
  GLCHECK_ERRORS();
}

void IContext::drawArrays(GLenum mode, GLint first, GLsizei count) {
  drawCallCount_++;

  GLCALL(DrawArrays)(mode, first, count);
  APILOG("glDrawArrays(%s, %d, %u)\n", GL_ENUM_TO_STRING(mode), first, count);
  GLCHECK_ERRORS();
  APILOG_DEC_DRAW_COUNT();
}

void IContext::drawBuffers(GLsizei n, GLenum* buffers) {
  if (drawBuffersProc_ == nullptr) {
    if (deviceFeatureSet_.hasFeature(DeviceFeatures::MultipleRenderTargets)) {
      if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::DrawBuffersExtReq)) {
        if (deviceFeatureSet_.hasExtension(Extensions::DrawBuffers)) {
          drawBuffersProc_ = iglDrawBuffersEXT;
        }
      } else {
        drawBuffersProc_ = iglDrawBuffers;
      }
    }
  }

  GLCALL_PROC(drawBuffersProc_, n, buffers);
  APILOG("glDrawBuffers(%u, %u)\n", n, buffers);
  GLCHECK_ERRORS();
}

void IContext::drawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
  drawCallCount_++;
  GLCALL(DrawElements)(mode, count, type, indices);

  APILOG("glDrawElements(%s, %u, %s, %p)\n",
         GL_ENUM_TO_STRING(mode),
         count,
         GL_ENUM_TO_STRING(type),
         indices);
  GLCHECK_ERRORS();
  APILOG_DEC_DRAW_COUNT();
}

void IContext::drawElementsIndirect(GLenum mode, GLenum type, const GLvoid* indirect) {
  drawCallCount_++;
  IGLCALL(DrawElementsIndirect)(mode, type, indirect);
  APILOG("glDrawElementsIndirect(%s, %s, %p)\n",
         GL_ENUM_TO_STRING(mode),
         GL_ENUM_TO_STRING(type),
         indirect);
  GLCHECK_ERRORS();
  APILOG_DEC_DRAW_COUNT();
}

void IContext::enable(GLenum cap) {
  GLCALL(Enable)(cap);
  APILOG("glEnable(%s)\n", GL_ENUM_TO_STRING(cap));
  GLCHECK_ERRORS();
}

void IContext::enableVertexAttribArray(GLuint index) {
  GLCALL(EnableVertexAttribArray)(index);
  APILOG("glEnableVertexAttribArray(%u)\n", index);
  GLCHECK_ERRORS();
}

GLsync IContext::fenceSync(GLenum condition, GLbitfield flags) {
  if (fenceSyncProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::SyncExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::Sync)) {
        fenceSyncProc_ = iglFenceSyncAPPLE;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::Sync)) {
      fenceSyncProc_ = iglFenceSync;
    }
  }

  GLsync sync;

  GLCALL_PROC_WITH_RETURN(sync, fenceSyncProc_, GL_ZERO, condition, flags);
  APILOG("glFenceSync(%s, %u)\n", GL_ENUM_TO_STRING(condition), flags);
  GLCHECK_ERRORS();

  return sync;
}

void IContext::finish() {
  GLCALL(Finish)();
  APILOG("glFinish\n");
  GLCHECK_ERRORS();
}

void IContext::flush() {
  GLCALL(Flush)();
  APILOG("glFlush\n");
  GLCHECK_ERRORS();
}

void IContext::framebufferRenderbuffer(GLenum target,
                                       GLenum attachment,
                                       GLenum renderbuffertarget,
                                       GLuint renderbuffer) {
  IGLCALL(FramebufferRenderbuffer)(target, attachment, renderbuffertarget, renderbuffer);
  APILOG("glFramebufferRenderbuffer(%s, %s, %s, %u)\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(attachment),
         GL_ENUM_TO_STRING(renderbuffertarget),
         renderbuffer);
  GLCHECK_ERRORS();
}

void IContext::framebufferTexture2D(GLenum target,
                                    GLenum attachment,
                                    GLenum textarget,
                                    GLuint texture,
                                    GLint level) {
  IGLCALL(FramebufferTexture2D)(target, attachment, textarget, texture, level);
  APILOG("glFramebufferTexture2D(%s, %s, %s, %u, %d)\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(attachment),
         GL_ENUM_TO_STRING(textarget),
         texture,
         level);
  GLCHECK_ERRORS();
}

void IContext::framebufferTexture2DMultisample(GLenum target,
                                               GLenum attachment,
                                               GLenum textarget,
                                               GLuint texture,
                                               GLint level,
                                               GLsizei samples) {
  if (framebufferTexture2DMultisampleProc_ == nullptr) {
    // Use runtime checks to determine which of several potential methods are supported by the
    // context.
    if (deviceFeatureSet_.hasExtension(Extensions::MultiSampleExt)) {
      framebufferTexture2DMultisampleProc_ = iglFramebufferTexture2DMultisampleEXT;
    } else if (deviceFeatureSet_.hasExtension(Extensions::MultiSampleImg)) {
      framebufferTexture2DMultisampleProc_ = iglFramebufferTexture2DMultisampleIMG;
    }
  }

  if (maxSamples_ == -1 && framebufferTexture2DMultisampleProc_ != nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::MultiSampleExtReq) &&
        deviceFeatureSet_.hasExtension(Extensions::MultiSampleImg)) {
      getIntegerv(GL_MAX_SAMPLES_IMG, &maxSamples_);
    } else {
      getIntegerv(GL_MAX_SAMPLES, &maxSamples_);
    }
  }

  if (samples > maxSamples_) {
    samples = maxSamples_;
  }

  GLCALL_PROC(
      framebufferTexture2DMultisampleProc_, target, attachment, textarget, texture, level, samples);
  APILOG("glFramebufferTexture2DMultisample(%s, %s, %s, %u, %d, %u)\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(attachment),
         GL_ENUM_TO_STRING(textarget),
         texture,
         level,
         samples);
  GLCHECK_ERRORS();
}

void IContext::framebufferTextureLayer(GLenum target,
                                       GLenum attachment,
                                       GLuint texture,
                                       GLint level,
                                       GLint layer) {
  IGLCALL(FramebufferTextureLayer)(target, attachment, texture, level, layer);
  APILOG("glFramebufferTextureLayer(%s, %s, %u, %d, %d)\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(attachment),
         texture,
         level,
         layer);
  GLCHECK_ERRORS();
}

void IContext::frontFace(GLenum mode) {
  GLCALL(FrontFace)(mode);
  APILOG("glFrontFace(%s)\n", GL_ENUM_TO_STRING(mode));
  GLCHECK_ERRORS();
}

void IContext::polygonFillMode(IGL_MAYBE_UNUSED GLenum mode) {
#if IGL_OPENGL
  GLCALL(PolygonMode)(GL_FRONT_AND_BACK, mode);
  APILOG("glPolygonMode(%s)\n", GL_ENUM_TO_STRING(mode));
  GLCHECK_ERRORS();
#else
  IGL_ASSERT_NOT_IMPLEMENTED();
#endif
}

void IContext::generateMipmap(GLenum target) {
  IGLCALL(GenerateMipmap)(target);
  APILOG("glGenerateMipmap(%s)\n", GL_ENUM_TO_STRING(target));
  GLCHECK_ERRORS();
}

void IContext::genBuffers(GLsizei n, GLuint* buffers) {
  GLCALL(GenBuffers)(n, buffers);
  APILOG("glGenBuffers(%u, %p) = %u\n", n, buffers, buffers == nullptr ? 0 : *buffers);
  GLCHECK_ERRORS();
}

void IContext::genFramebuffers(GLsizei n, GLuint* framebuffers) {
  IGLCALL(GenFramebuffers)(n, framebuffers);
  APILOG("glGenFramebuffers(%u, %p) = %u\n",
         n,
         framebuffers,
         framebuffers == nullptr ? 0 : *framebuffers);
  GLCHECK_ERRORS();
}

void IContext::genRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  IGLCALL(GenRenderbuffers)(n, renderbuffers);
  APILOG("glGenRenderbuffers(%u, %p) = %u\n",
         n,
         renderbuffers,
         renderbuffers == nullptr ? 0 : *renderbuffers);
  GLCHECK_ERRORS();
}

void IContext::genTextures(GLsizei n, GLuint* textures) {
  GLCALL(GenTextures)(n, textures);
  APILOG("glGenTextures(%u, %p) = %u\n", n, textures, textures == nullptr ? 0 : *textures);
  GLCHECK_ERRORS();
}

void IContext::genVertexArrays(GLsizei n, GLuint* vertexArrays) {
  if (genVertexArraysProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::VertexArrayObjectExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::VertexArrayObject)) {
        genVertexArraysProc_ = iglGenVertexArraysOES;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::VertexArrayObject)) {
      genVertexArraysProc_ = iglGenVertexArrays;
    }
  }
  GLCALL_PROC(genVertexArraysProc_, n, vertexArrays);
  APILOG("glGenVertexArrays(%u, %p) = %u\n",
         n,
         vertexArrays,
         vertexArrays == nullptr ? 0 : *vertexArrays);
  GLCHECK_ERRORS();
}

void IContext::getActiveAttrib(GLuint program,
                               GLuint index,
                               GLsizei bufsize,
                               GLsizei* length,
                               GLint* size,
                               GLenum* type,
                               GLchar* name) const {
  GLCALL(GetActiveAttrib)(program, index, bufsize, length, size, type, name);
  APILOG("glGetActiveAttrib(%u, %u, %u, %p, %p, %p, %p) = (%d, %s, %.*s)\n",
         program,
         index,
         bufsize,
         length,
         size,
         type,
         name,
         size == nullptr ? 0 : *size,
         type == nullptr ? GL_ENUM_TO_STRING(0) : GL_ENUM_TO_STRING(*type),
         static_cast<int>(length == nullptr ? 0 : *length),
         name == nullptr ? "" : name);
  GLCHECK_ERRORS();
}

void IContext::getActiveUniform(GLuint program,
                                GLuint index,
                                GLsizei bufsize,
                                GLsizei* length,
                                GLint* size,
                                GLenum* type,
                                GLchar* name) const {
  GLCALL(GetActiveUniform)(program, index, bufsize, length, size, type, name);
  APILOG("glGetActiveUniform(%u, %u, %u, %p, %p, %p, %p) = (%d, %s, %.*s)\n",
         program,
         index,
         bufsize,
         length,
         size,
         type,
         name,
         size == nullptr ? 0 : *size,
         type == nullptr ? GL_ENUM_TO_STRING(0) : GL_ENUM_TO_STRING(*type),
         static_cast<int>(length == nullptr ? 0 : *length),
         name == nullptr ? "" : name);
  GLCHECK_ERRORS();
}

void IContext::getActiveUniformsiv(GLuint program,
                                   GLsizei uniformCount,
                                   const GLuint* uniformIndices,
                                   GLenum pname,
                                   GLint* params) const {
  IGLCALL(GetActiveUniformsiv)(program, uniformCount, uniformIndices, pname, params);
  APILOG("glGetActiveUniformsiv(%u, %u, %p, %s, %p)\n",
         program,
         uniformCount,
         uniformIndices,
         GL_ENUM_TO_STRING(pname),
         params);
  GLCHECK_ERRORS();
}

void IContext::getActiveUniformBlockiv(GLuint program,
                                       GLuint index,
                                       GLenum pname,
                                       GLint* params) const {
  IGLCALL(GetActiveUniformBlockiv)(program, index, pname, params);
  APILOG("glGetActiveUniformBlockiv(%u, %u, %s, %p) = %d\n",
         program,
         index,
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0 : *params);
  GLCHECK_ERRORS();
}

void IContext::getActiveUniformBlockName(GLuint program,
                                         GLuint index,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLchar* uniformBlockName) const {
  IGLCALL(GetActiveUniformBlockName)(program, index, bufSize, length, uniformBlockName);
  APILOG("glGetActiveUniformBlockName(%u, %u, %u, %p, %p) = %.*s\n",
         program,
         index,
         bufSize,
         length,
         uniformBlockName,
         static_cast<int>(length == nullptr ? 0 : *length),
         uniformBlockName == nullptr ? "" : uniformBlockName);
  GLCHECK_ERRORS();
}

void IContext::getAttachedShaders(GLuint program,
                                  GLsizei maxcount,
                                  GLsizei* count,
                                  GLuint* shaders) const {
  GLCALL(GetAttachedShaders)(program, maxcount, count, shaders);
  APILOG("glGetAttachedShaders(%u, %u, %p, %p)\n", program, maxcount, count, shaders);
  GLCHECK_ERRORS();
}

GLint IContext::getAttribLocation(GLuint program, const GLchar* name) const {
  GLint ret;

  GLCALL_WITH_RETURN(ret, GetAttribLocation)(program, name);
  APILOG("glGetAttribLocation(%u, %s) = %d\n", program, name, ret);
  GLCHECK_ERRORS();

  return ret;
}

void IContext::getBooleanv(GLenum pname, GLboolean* params) const {
  GLCALL(GetBooleanv)(pname, params);
  APILOG("glGetBooleanv(%s, %p) = %s\n",
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? "" : GL_BOOL_TO_STRING(*params));
  GLCHECK_ERRORS();
}

void IContext::getBufferParameteriv(GLenum target, GLenum pname, GLint* params) const {
  GLCALL(GetBufferParameteriv)(target, pname, params);
  APILOG("glGetBufferParameteriv(%s, %s, %p) = %d\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0 : *params);
  GLCHECK_ERRORS();
}

GLenum IContext::getError() const {
  // Using direct GL call here instead of wrapped one,
  // since we will add error call counting at some point.
  return glGetError();
}

void IContext::getFloatv(GLenum pname, GLfloat* params) const {
  GLCALL(GetFloatv)(pname, params);
  APILOG("glGetFloatv(%s, %p) = %f\n",
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0.f : *params);
  GLCHECK_ERRORS();
}

void IContext::getFramebufferAttachmentParameteriv(GLenum target,
                                                   GLenum attachment,
                                                   GLenum pname,
                                                   GLint* params) const {
  IGLCALL(GetFramebufferAttachmentParameteriv)(target, attachment, pname, params);
  APILOG("glGetFramebufferAttachmentParameteriv(%s, %s, %s, %p) = %d\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(attachment),
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0 : *params);
  GLCHECK_ERRORS();
}

void IContext::getIntegerv(GLenum pname, GLint* params) const {
  GLCALL(GetIntegerv)(pname, params);
  APILOG("glGetIntegerv(%s, %p) = %d\n",
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0 : *params);
  GLCHECK_ERRORS();
}

void IContext::getProgramiv(GLuint program, GLenum pname, GLint* params) const {
  GLCALL(GetProgramiv)(program, pname, params);
  APILOG("glGetProgramiv(%u, %s, %p) = %d\n",
         program,
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0 : *params);
  GLCHECK_ERRORS();
}

void IContext::getProgramInterfaceiv(GLuint program,
                                     GLenum programInterface,
                                     GLenum pname,
                                     GLint* params) const {
  IGLCALL(GetProgramInterfaceiv)(program, programInterface, pname, params);
  APILOG("glGetProgramInterfaceiv(%u, %s, %s, %p) = %d\n",
         program,
         GL_ENUM_TO_STRING(programInterface),
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0 : *params);
  GLCHECK_ERRORS();
}

void IContext::getProgramInfoLog(GLuint program,
                                 GLsizei bufsize,
                                 GLsizei* length,
                                 GLchar* infolog) const {
  GLCALL(GetProgramInfoLog)(program, bufsize, length, infolog);
  APILOG("glGetProgramInfoLog(%u, %u, %p, %p) = %.*s\n",
         program,
         bufsize,
         length,
         infolog,
         static_cast<int>(length == nullptr ? 0 : *length),
         infolog == nullptr ? "" : infolog);
  GLCHECK_ERRORS();
}

void IContext::getProgramResourceiv(GLuint program,
                                    GLenum programInterface,
                                    GLuint index,
                                    GLsizei propCount,
                                    const GLenum* props,
                                    GLsizei count,
                                    GLsizei* length,
                                    GLint* params) const {
  IGLCALL(GetProgramResourceiv)
  (program, programInterface, index, propCount, props, count, length, params);
  APILOG("glGetProgramResourceiv(%u, %s, %u, %u, %p, %u, %p, %p) = (%u, %d)\n",
         program,
         GL_ENUM_TO_STRING(programInterface),
         propCount,
         props,
         count,
         length,
         params,
         length == nullptr ? 0 : *length,
         params == nullptr ? 0 : *params);
  GLCHECK_ERRORS();
}

GLuint IContext::getProgramResourceIndex(GLuint program,
                                         GLenum programInterface,
                                         const GLchar* name) const {
  GLuint ret;

  IGLCALL_WITH_RETURN(ret, GetProgramResourceIndex)(program, programInterface, name);
  APILOG("glGetProgramResourceIndex(%u, %s, %s) = %u\n",
         program,
         GL_ENUM_TO_STRING(programInterface),
         name,
         ret);
  GLCHECK_ERRORS();

  return ret;
}

void IContext::getProgramResourceName(GLuint program,
                                      GLenum programInterface,
                                      GLuint index,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      char* name) const {
  IGLCALL(GetProgramResourceName)(program, programInterface, index, bufSize, length, name);
  APILOG("glGetProgramResourceName(%u, %s, %u, %u, %p, %p) = %.*s\n",
         program,
         GL_ENUM_TO_STRING(programInterface),
         index,
         bufSize,
         length,
         name,
         static_cast<int>(length == nullptr ? 0 : *length),
         name == nullptr ? "" : name);
  GLCHECK_ERRORS();
}

void IContext::getRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) const {
  IGLCALL(GetRenderbufferParameteriv)(target, pname, params);
  APILOG("glGetRenderbufferParameteriv(%s, %s, %p) = %d\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0 : *params);
  GLCHECK_ERRORS();
}

void IContext::getShaderiv(GLuint shader, GLenum pname, GLint* params) const {
  GLCALL(GetShaderiv)(shader, pname, params);
  APILOG("glGetShaderiv(%u, %s, %p) = %d\n",
         shader,
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0 : *params);
  GLCHECK_ERRORS();
}

void IContext::getShaderInfoLog(GLuint shader,
                                GLsizei maxLength,
                                GLsizei* length,
                                GLchar* infoLog) const {
  GLCALL(GetShaderInfoLog)(shader, maxLength, length, infoLog);
  APILOG("glGetShaderInfoLog(%u, %u, %p, %p) = %.*s\n",
         shader,
         maxLength,
         length,
         infoLog,
         static_cast<int>(length == nullptr ? 0 : *length),
         infoLog == nullptr ? "" : infoLog);
  GLCHECK_ERRORS();
}

void IContext::getShaderPrecisionFormat(GLenum shadertype,
                                        GLenum precisiontype,
                                        GLint* range,
                                        GLint* precision) const {
  GLCALL(GetShaderPrecisionFormat)(shadertype, precisiontype, range, precision);
  APILOG("glGetShaderPrecisionFormat(%s, %s, %p, %p) = (%d, %d)\n",
         GL_ENUM_TO_STRING(shadertype),
         GL_ENUM_TO_STRING(precisiontype),
         range,
         precision,
         range == nullptr ? 0 : *range,
         precision == nullptr ? 0 : *precision);
  GLCHECK_ERRORS();
}

void IContext::getShaderSource(GLuint shader,
                               GLsizei bufsize,
                               GLsizei* length,
                               GLchar* source) const {
  GLCALL(GetShaderSource)(shader, bufsize, length, source);
  APILOG("glGetShaderSource(%u, %u, %p, %p) = %.*s\n",
         shader,
         bufsize,
         length,
         source,
         static_cast<int>(length == nullptr ? 0 : *length),
         source == nullptr ? "" : source);
  GLCHECK_ERRORS();
}

const GLubyte* IContext::getString(GLenum name) const {
  const GLubyte* ret;

  GLCALL_WITH_RETURN(ret, GetString)(name);
  APILOG("glGetString(%s) = %s\n", GL_ENUM_TO_STRING(name), reinterpret_cast<const char*>(ret));
  GLCHECK_ERRORS();

  return ret;
}

const GLubyte* IContext::getStringi(GLenum name, GLint index) const {
  const GLubyte* ret;

  IGLCALL_WITH_RETURN(ret, GetStringi)(name, index);
  APILOG("glGetStringi(%s, %d) = %s\n",
         GL_ENUM_TO_STRING(name),
         index,
         reinterpret_cast<const char*>(ret));
  GLCHECK_ERRORS();

  return ret;
}

void IContext::getSynciv(GLsync sync,
                         GLenum pname,
                         GLsizei bufSize,
                         GLsizei* length,
                         GLint* values) const {
  if (getSyncivProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::SyncExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::Sync)) {
        getSyncivProc_ = iglGetSyncivAPPLE;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::Sync)) {
      getSyncivProc_ = iglGetSynciv;
    }
  }
  GLCALL_PROC(getSyncivProc_, sync, pname, bufSize, length, values);
  APILOG("glGetSynciv(%p, %s, %u, %p, %p)\n", sync, pname, bufSize, length, values);
  GLCHECK_ERRORS();
}

void IContext::getTexParameterfv(GLenum target, GLenum pname, GLfloat* params) const {
  GLCALL(GetTexParameterfv)(target, pname, params);
  APILOG("glGetTexParameterfv(%s, %s, %p) = %f\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0.f : *params);
  GLCHECK_ERRORS();
}

void IContext::getTexParameteriv(GLenum target, GLenum pname, GLint* params) const {
  GLCALL(GetTexParameteriv)(target, pname, params);
  APILOG("glGetTexParameteriv(%s, %s, %p) = %d\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0 : *params);
  GLCHECK_ERRORS();
}

void IContext::getUniformfv(GLuint program, GLint location, GLfloat* params) const {
  GLCALL(GetUniformfv)(program, location, params);
  APILOG("glGetUniformfv(%u, %d, %p) = %f\n",
         program,
         location,
         params,
         params == nullptr ? 0.f : *params);
  GLCHECK_ERRORS();
}

void IContext::getUniformiv(GLuint program, GLint location, GLint* params) const {
  GLCALL(GetUniformiv)(program, location, params);
  APILOG("glGetUniformiv(%u, %d, %p) = %d\n",
         program,
         location,
         params,
         params == nullptr ? 0 : *params);
  GLCHECK_ERRORS();
}

GLuint IContext::getUniformBlockIndex(GLuint program, const GLchar* name) const {
  GLuint ret = GL_INVALID_INDEX;
  IGLCALL_WITH_RETURN(ret, GetUniformBlockIndex)(program, name);
  APILOG("glGetUniformBlockIndex(%u, %s) = %u\n", program, name, ret);
  GLCHECK_ERRORS();
  return ret;
}

GLint IContext::getUniformLocation(GLuint program, const GLchar* name) const {
  GLint ret;

  GLCALL_WITH_RETURN(ret, GetUniformLocation)(program, name);
  APILOG("glGetUniformLocation(%u, %s) = %d\n", program, name, ret);
  GLCHECK_ERRORS();

  return ret;
}

void IContext::getVertexAttribfv(GLuint index, GLenum pname, GLfloat* params) const {
  GLCALL(GetVertexAttribfv)(index, pname, params);
  APILOG("glGetVertexAttribfv(%u, %s, %p) = %f\n",
         index,
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0.f : *params);
  GLCHECK_ERRORS();
}

void IContext::getVertexAttribiv(GLuint index, GLenum pname, GLint* params) const {
  GLCALL(GetVertexAttribiv)(index, pname, params);
  APILOG("glGetVertexAttribiv(%u, %s, %p) = %d\n",
         index,
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0 : *params);
  GLCHECK_ERRORS();
}

void IContext::hint(GLenum target, GLenum mode) {
  GLCALL(Hint)(target, mode);
  APILOG("glHint(%s, %s)\n", GL_ENUM_TO_STRING(target), GL_ENUM_TO_STRING(mode));
  GLCHECK_ERRORS();
}

void IContext::importMemoryFd(GLuint memory, GLuint64 size, GLenum handleType, GLint fd) {
  IGLCALL(ImportMemoryFdEXT)(memory, size, handleType, fd);
  APILOG(
      "glImportMemoryFdEXT(%u, %llu, %s, %d)\n", memory, size, GL_ENUM_TO_STRING(handleType), fd);
  GLCHECK_ERRORS();
}

void IContext::invalidateFramebuffer(IGL_MAYBE_UNUSED GLenum target,
                                     IGL_MAYBE_UNUSED GLsizei numAttachments,
                                     IGL_MAYBE_UNUSED const GLenum* attachments) {
  if (invalidateFramebufferProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(
            InternalRequirement::InvalidateFramebufferExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::DiscardFramebuffer)) {
        invalidateFramebufferProc_ = iglDiscardFramebufferEXT;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::InvalidateFramebuffer)) {
      invalidateFramebufferProc_ = iglInvalidateFramebuffer;
    }
  }
  GLCALL_PROC(invalidateFramebufferProc_, target, numAttachments, attachments);
  APILOG("glInvalidateFramebuffer(%s, %u, %p)\n",
         GL_ENUM_TO_STRING(target),
         numAttachments,
         attachments);
  GLCHECK_ERRORS();
}

GLboolean IContext::isBuffer(GLuint buffer) {
  GLboolean ret;

  GLCALL_WITH_RETURN(ret, IsBuffer)(buffer);
  APILOG("glIsBuffer(%u) = %s\n", buffer, GL_BOOL_TO_STRING(ret));
  GLCHECK_ERRORS();

  return ret;
}

GLboolean IContext::isEnabled(GLenum cap) {
  GLboolean ret;

  GLCALL_WITH_RETURN(ret, IsEnabled)(cap);
  APILOG("glIsEnabled(%s) = %s\n", GL_ENUM_TO_STRING(cap), GL_BOOL_TO_STRING(ret));
  GLCHECK_ERRORS();

  return ret;
}

GLboolean IContext::isFramebuffer(GLuint framebuffer) {
  GLboolean ret;

  IGLCALL_WITH_RETURN(ret, IsFramebuffer)(framebuffer);
  APILOG("glIsFramebuffer(%u) = %s\n", framebuffer, GL_BOOL_TO_STRING(ret));
  GLCHECK_ERRORS();

  return ret;
}

GLboolean IContext::isProgram(GLuint program) {
  GLboolean ret;

  GLCALL_WITH_RETURN(ret, IsProgram)(program);
  APILOG("glIsProgram(%u) = %s\n", program, GL_BOOL_TO_STRING(ret));
  GLCHECK_ERRORS();

  return ret;
}

GLboolean IContext::isRenderbuffer(GLuint renderbuffer) {
  GLboolean ret;

  GLCALL_WITH_RETURN(ret, IsRenderbuffer)(renderbuffer);
  APILOG("glIsRenderbuffer(%u) = %s\n", renderbuffer, GL_BOOL_TO_STRING(ret));
  GLCHECK_ERRORS();

  return ret;
}

GLboolean IContext::isShader(GLuint shader) {
  GLboolean ret;

  GLCALL_WITH_RETURN(ret, IsShader)(shader);
  APILOG("glIsShader(%u) = %s\n", shader, GL_BOOL_TO_STRING(ret));
  GLCHECK_ERRORS();

  return ret;
}

GLboolean IContext::isTexture(GLuint texture) {
  GLboolean ret;

  GLCALL_WITH_RETURN(ret, IsTexture)(texture);
  APILOG("glIsTexture(%u) = %s\n", texture, GL_BOOL_TO_STRING(ret));
  GLCHECK_ERRORS();

  return ret;
}

void IContext::lineWidth(GLfloat width) {
  GLCALL(LineWidth)(width);
  APILOG("glLineWidth(%f)\n", width);
  GLCHECK_ERRORS();
}

void IContext::linkProgram(GLuint program) {
  GLCALL(LinkProgram)(program);
  APILOG("glLinkProgram(%u)\n", program);

  // NOTE: Explicitly *not* checking for errors
  // If there is an error, we want the client code to get error message and report
  // to the user/logs, rather than assert here.
  // @fb-only
  // @fb-only
  // @fb-only
}

void* IContext::mapBuffer(GLenum target, GLbitfield access) {
  if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::MapBufferExtReq)) {
    if (deviceFeatureSet_.hasExtension(Extensions::MapBuffer)) {
      mapBufferProc_ = iglMapBufferOES;
    }
  } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::MapBuffer)) {
    mapBufferProc_ = iglMapBuffer;
  }
  void* ret = nullptr;
  GLCALL_PROC_WITH_RETURN(ret, mapBufferProc_, nullptr, target, access);
  APILOG("glMapBuffer(%s, %zu, %zu, 0x%x) = %p\n", GL_ENUM_TO_STRING(target), access, ret);
  GLCHECK_ERRORS();
  return ret;
}

void* IContext::mapBufferRange(GLenum target,
                               GLintptr offset,
                               GLsizeiptr length,
                               GLbitfield access) {
  if (mapBufferRangeProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::MapBufferRangeExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::MapBufferRange)) {
        mapBufferRangeProc_ = iglMapBufferRangeEXT;
      }
    } else if (deviceFeatureSet_.hasFeature(DeviceFeatures::MapBufferRange)) {
      mapBufferRangeProc_ = iglMapBufferRange;
    }
  }
  void* ret = nullptr;
  GLCALL_PROC_WITH_RETURN(ret, mapBufferRangeProc_, nullptr, target, offset, length, access);
  APILOG("glMapBufferRange(%s, %zu, %zu, 0x%x) = %p\n",
         GL_ENUM_TO_STRING(target),
         offset,
         length,
         access,
         ret);
  GLCHECK_ERRORS();
  return ret;
}

void IContext::pixelStorei(GLenum pname, GLint param) {
  GLCALL(PixelStorei)(pname, param);
  APILOG("glPixelStorei(%s, %d)\n", GL_ENUM_TO_STRING(pname), param);
  GLCHECK_ERRORS();
}

void IContext::polygonOffset(GLfloat factor, GLfloat units) {
  GLCALL(PolygonOffset)(factor, units);
  APILOG("glPolygonOffset(%f, %f)\n", factor, units);
  GLCHECK_ERRORS();
}

void IContext::pushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar* message) {
  if (pushDebugGroupProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::Debug)) {
      if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::DebugExtReq)) {
        if (deviceFeatureSet_.hasExtension(Extensions::Debug)) {
          pushDebugGroupProc_ = iglPushDebugGroupKHR;
        } else if (deviceFeatureSet_.hasExtension(Extensions::DebugMarker)) {
          pushDebugGroupProc_ = iglPushGroupMarkerEXT;
        }
      } else {
        pushDebugGroupProc_ = iglPushDebugGroup;
      }
    }
  }

  GLCALL_PROC(pushDebugGroupProc_, source, id, length, message);
  APILOG("glPushDebugGroup(%s, %u, %u, %s)\n", GL_ENUM_TO_STRING(source), id, length, message);
  GLCHECK_ERRORS();
}

void IContext::popDebugGroup() {
  if (popDebugGroupProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::Debug)) {
      if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::DebugExtReq)) {
        if (deviceFeatureSet_.hasExtension(Extensions::Debug)) {
          popDebugGroupProc_ = iglPopDebugGroupKHR;
        } else if (deviceFeatureSet_.hasExtension(Extensions::DebugMarker)) {
          popDebugGroupProc_ = iglPopGroupMarkerEXT;
        }
      } else {
        popDebugGroupProc_ = iglPopDebugGroup;
      }
    }
  }

  GLCALL_PROC(popDebugGroupProc_);
  APILOG("glPopDebugGroup()\n");
  GLCHECK_ERRORS();
}

void IContext::readPixels(GLint x,
                          GLint y,
                          GLsizei width,
                          GLsizei height,
                          GLenum format,
                          GLenum type,
                          GLvoid* pixels) {
  GLCALL(ReadPixels)(x, y, width, height, format, type, pixels);
  APILOG("glReadPixels(%d, %u, %u, %d, %s, %s, %p)\n",
         x,
         y,
         width,
         height,
         GL_ENUM_TO_STRING(format),
         GL_ENUM_TO_STRING(type),
         pixels);
  GLCHECK_ERRORS();
}

void IContext::releaseShaderCompiler() {
  GLCALL(ReleaseShaderCompiler)();
  APILOG("glReleaseShaderCompiler()\n");
  GLCHECK_ERRORS();
}

void IContext::renderbufferStorage(GLenum target,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height) {
  IGLCALL(RenderbufferStorage)(target, internalformat, width, height);
  APILOG("glRenderbufferStorage(%s, %s, %u, %u)\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(internalformat),
         width,
         height);
  GLCHECK_ERRORS();
}

void IContext::renderbufferStorageMultisample(GLenum target,
                                              GLsizei samples,
                                              GLenum internalformat,
                                              GLsizei width,
                                              GLsizei height) {
  if (renderbufferStorageMultisampleProc_ == nullptr) {
    // Use runtime checks to determine which of several potential methods are supported by the
    // context.
    if (deviceFeatureSet_.hasFeature(DeviceFeatures::MultiSample)) {
      if (!deviceFeatureSet_.hasInternalRequirement(InternalRequirement::MultiSampleExtReq)) {
        renderbufferStorageMultisampleProc_ = iglRenderbufferStorageMultisample;
      } else if (deviceFeatureSet_.hasExtension(Extensions::MultiSampleExt)) {
        renderbufferStorageMultisampleProc_ = iglRenderbufferStorageMultisampleEXT;
      } else if (deviceFeatureSet_.hasExtension(Extensions::MultiSampleImg)) {
        renderbufferStorageMultisampleProc_ = iglRenderbufferStorageMultisampleIMG;
      } else if (deviceFeatureSet_.hasExtension(Extensions::MultiSampleApple)) {
        renderbufferStorageMultisampleProc_ = iglRenderbufferStorageMultisampleAPPLE;
      }
    }
  }

  GLCALL_PROC(renderbufferStorageMultisampleProc_, target, samples, internalformat, width, height);
  APILOG("glRenderbufferStorageMultisampleProc(%s, %u, %s, %u, %u)\n",
         GL_ENUM_TO_STRING(target),
         samples,
         GL_ENUM_TO_STRING(internalformat),
         width,
         height);
  GLCHECK_ERRORS();
}

void IContext::framebufferTextureMultiview(GLenum target,
                                           GLenum attachment,
                                           GLuint texture,
                                           GLint level,
                                           GLint baseViewIndex,
                                           GLsizei numViews) {
  IGLCALL(FramebufferTextureMultiviewOVR)
  (target, attachment, texture, level, baseViewIndex, numViews);
  APILOG("glFramebufferTextureMultiviewOVR(%s, %s, %u, %d, %d, %u)\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(attachment),
         texture,
         level,
         baseViewIndex,
         numViews);
  GLCHECK_ERRORS();
}

void IContext::framebufferTextureMultisampleMultiview(GLenum target,
                                                      GLenum attachment,
                                                      GLuint texture,
                                                      GLint level,
                                                      GLsizei samples,
                                                      GLint baseViewIndex,
                                                      GLsizei numViews) {
  IGLCALL(FramebufferTextureMultisampleMultiviewOVR)
  (target, attachment, texture, level, samples, baseViewIndex, numViews);
  APILOG("glFramebufferTextureMultisampleMultiview(%s, %s, %u, %d, %u, %d, %u)\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(attachment),
         texture,
         level,
         samples,
         baseViewIndex,
         numViews);
  GLCHECK_ERRORS();
}

void IContext::sampleCoverage(GLfloat value, GLboolean invert) {
  GLCALL(SampleCoverage)(value, invert);
  APILOG("glSampleCoverage(%f, %s)\n", value, GL_BOOL_TO_STRING(invert));
  GLCHECK_ERRORS();
}

void IContext::scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
  GLCALL(Scissor)(x, y, width, height);
  APILOG("glScissor(%d, %d, %u, %u)\n", x, y, width, height);
  GLCHECK_ERRORS();
}

void IContext::setEnabled(bool shouldEnable, GLenum cap) {
  if (shouldEnable) {
    GLCALL(Enable)(cap);
    APILOG("glEnable(%s)\n", GL_ENUM_TO_STRING(cap));
  } else {
    GLCALL(Disable)(cap);
    APILOG("glDisable(%s)\n", GL_ENUM_TO_STRING(cap));
  }
  GLCHECK_ERRORS();
}

void IContext::shaderBinary(GLsizei n,
                            const GLuint* shaders,
                            GLenum binaryformat,
                            const GLvoid* binary,
                            GLsizei length) {
  GLCALL(ShaderBinary)(n, shaders, binaryformat, binary, length);
  APILOG("glShaderBinary(%u, %p, %s, %p, %u)\n",
         n,
         shaders,
         GL_ENUM_TO_STRING(binaryformat),
         binary,
         length);
  GLCHECK_ERRORS();
}

void IContext::shaderSource(GLuint shader,
                            GLsizei count,
                            const GLchar** string,
                            const GLint* length) {
  GLCALL(ShaderSource)(shader, count, string, length);
  APILOG("glShaderSource(%d, %u, %p, %p):\n%.*s\n",
         shader,
         count,
         string,
         length,
         length == nullptr ? (string == nullptr ? 0 : std::strlen(string[0])) : *length,
         string == nullptr ? "" : string[0]);
  GLCHECK_ERRORS();
}

void IContext::stencilFunc(GLenum func, GLint ref, GLuint mask) {
  GLCALL(StencilFunc)(func, ref, mask);
  APILOG("glStencilFunc(%s, %d, 0x%x)\n", GL_ENUM_TO_STRING(func), ref, mask);
  GLCHECK_ERRORS();
}

void IContext::stencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
  GLCALL(StencilFuncSeparate)(face, func, ref, mask);
  APILOG("glStencilFuncSeparate(%s, %s, %d, 0x%x)\n",
         GL_ENUM_TO_STRING(face),
         GL_ENUM_TO_STRING(func),
         ref,
         mask);
  GLCHECK_ERRORS();
}

void IContext::stencilMask(GLuint mask) {
  GLCALL(StencilMask)(mask);
  APILOG("glStencilMask(0x%x)\n", mask);
  GLCHECK_ERRORS();
}

void IContext::stencilMaskSeparate(GLenum face, GLuint mask) {
  GLCALL(StencilMaskSeparate)(face, mask);
  APILOG("glStencilMaskSeparate(%s, 0x%x)\n", GL_ENUM_TO_STRING(face), mask);
  GLCHECK_ERRORS();
}

void IContext::stencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
  GLCALL(StencilOp)(fail, zfail, zpass);
  APILOG("glStencilOp(%s, %s, %s)\n",
         GL_ENUM_TO_STRING(fail),
         GL_ENUM_TO_STRING(zfail),
         GL_ENUM_TO_STRING(zpass));
  GLCHECK_ERRORS();
}

void IContext::stencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass) {
  GLCALL(StencilOpSeparate)(face, fail, zfail, zpass);
  APILOG("glStencilOpSeparate(%s, %s, %s, %s)\n",
         GL_ENUM_TO_STRING(face),
         GL_ENUM_TO_STRING(fail),
         GL_ENUM_TO_STRING(zfail),
         GL_ENUM_TO_STRING(zpass));
  GLCHECK_ERRORS();
}
void IContext::texImage1D(IGL_MAYBE_UNUSED GLenum target,
                          IGL_MAYBE_UNUSED GLint level,
                          IGL_MAYBE_UNUSED GLint internalformat,
                          IGL_MAYBE_UNUSED GLsizei width,
                          IGL_MAYBE_UNUSED GLint border,
                          IGL_MAYBE_UNUSED GLenum format,
                          IGL_MAYBE_UNUSED GLenum type,
                          IGL_MAYBE_UNUSED const GLvoid* data) {
#if IGL_OPENGL
  GLCALL(TexImage1D)(target, level, internalformat, width, border, format, type, data);
  APILOG("glTexImage1D(%s, %d, %s, %u, %d, %s, %s, 0x%x)\n",
         GL_ENUM_TO_STRING(target),
         level,
         GL_ENUM_TO_STRING(internalformat),
         width,
         border,
         GL_ENUM_TO_STRING(format),
         GL_ENUM_TO_STRING(type),
         data);
  GLCHECK_ERRORS();
#else
  IGL_ASSERT_NOT_IMPLEMENTED();
#endif
}

void IContext::texSubImage1D(IGL_MAYBE_UNUSED GLenum target,
                             IGL_MAYBE_UNUSED GLint level,
                             IGL_MAYBE_UNUSED GLint xoffset,
                             IGL_MAYBE_UNUSED GLsizei width,
                             IGL_MAYBE_UNUSED GLenum format,
                             IGL_MAYBE_UNUSED GLenum type,
                             IGL_MAYBE_UNUSED const GLvoid* pixels) {
#if IGL_OPENGL
  GLCALL(TexSubImage1D)(target, level, xoffset, width, format, type, pixels);
  APILOG("glTexSubImage1D(%s, %d, %d, %u, %s, %s, 0x%x)\n",
         GL_ENUM_TO_STRING(target),
         level,
         xoffset,
         width,
         GL_ENUM_TO_STRING(format),
         GL_ENUM_TO_STRING(type),
         pixels);
  GLCHECK_ERRORS();
#else
  IGL_ASSERT_NOT_IMPLEMENTED();
#endif
}

void IContext::texImage2D(GLenum target,
                          GLint level,
                          GLint internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLint border,
                          GLenum format,
                          GLenum type,
                          const GLvoid* data) {
  GLCALL(TexImage2D)(target, level, internalformat, width, height, border, format, type, data);
  APILOG("glTexImage2D(%s, %d, %s, %u, %u, %d, %s, %s, 0x%x)\n",
         GL_ENUM_TO_STRING(target),
         level,
         GL_ENUM_TO_STRING(internalformat),
         width,
         height,
         border,
         GL_ENUM_TO_STRING(format),
         GL_ENUM_TO_STRING(type),
         data);
  GLCHECK_ERRORS();
}

void IContext::texStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
  if (texStorage1DProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::TexStorageExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::TexStorage)) {
        texStorage1DProc_ = iglTexStorage1DEXT;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::TexStorage)) {
      texStorage1DProc_ = iglTexStorage1D;
    }
  }

  GLCALL_PROC(texStorage1DProc_, target, levels, internalformat, width);
  APILOG("TexStorage1D(%s, %u, %s, %u)\n",
         GL_ENUM_TO_STRING(target),
         levels,
         GL_ENUM_TO_STRING(internalformat),
         width);
  GLCHECK_ERRORS();
}

void IContext::texStorage2D(GLenum target,
                            GLsizei levels,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height) {
  if (texStorage2DProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::TexStorageExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::TexStorage)) {
        texStorage2DProc_ = iglTexStorage2DEXT;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::TexStorage)) {
      texStorage2DProc_ = iglTexStorage2D;
    }
  }

  GLCALL_PROC(texStorage2DProc_, target, levels, internalformat, width, height);
  APILOG("glTexStorage2D(%s, %u, %s, %u, %u)\n",
         GL_ENUM_TO_STRING(target),
         levels,
         GL_ENUM_TO_STRING(internalformat),
         width,
         height);
  GLCHECK_ERRORS();
}

void IContext::texStorage3D(GLenum target,
                            GLsizei levels,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height,
                            GLsizei depth) {
  if (texStorage3DProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::TexStorageExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::TexStorage)) {
        texStorage3DProc_ = iglTexStorage3DEXT;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::TexStorage)) {
      texStorage3DProc_ = iglTexStorage3D;
    }
  }
  GLCALL_PROC(texStorage3DProc_, target, levels, internalformat, width, height, depth);
  APILOG("glTexStorage3D(%s, %u, %s, %u, %u, %u)\n",
         GL_ENUM_TO_STRING(target),
         levels,
         GL_ENUM_TO_STRING(internalformat),
         width,
         height,
         depth);
  GLCHECK_ERRORS();
}

void IContext::texStorageMem2D(GLenum target,
                               GLsizei levels,
                               GLenum internalFormat,
                               GLsizei width,
                               GLsizei height,
                               GLuint memory,
                               GLuint64 offset) {
  IGLCALL(TexStorageMem2DEXT)(target, levels, internalFormat, width, height, memory, offset);
  APILOG("glTexStorageMem2DEXT(%s, %u, %s, %u, %u, %u, %lu)\n",
         GL_ENUM_TO_STRING(target),
         levels,
         GL_ENUM_TO_STRING(internalFormat),
         width,
         height,
         memory,
         offset);
  GLCHECK_ERRORS();
}

void IContext::texStorageMem3D(GLenum target,
                               GLsizei levels,
                               GLenum internalFormat,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLuint memory,
                               GLuint64 offset) {
  IGLCALL(TexStorageMem3DEXT)(target, levels, internalFormat, width, height, depth, memory, offset);
  APILOG("glTexStorageMem3DEXT(%s, %u, %s, %u, %u, %u, %u, %lu)\n",
         GL_ENUM_TO_STRING(target),
         levels,
         GL_ENUM_TO_STRING(internalFormat),
         width,
         height,
         depth,
         memory,
         offset);
  GLCHECK_ERRORS();
}

void IContext::texImage3D(GLenum target,
                          GLint level,
                          GLint internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLsizei depth,
                          GLint border,
                          GLenum format,
                          GLenum type,
                          const GLvoid* data) {
  if (texImage3DProc_ == nullptr) {
    if (deviceFeatureSet_.hasFeature(DeviceFeatures::Texture3D)) {
      if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::Texture3DExtReq)) {
        if (deviceFeatureSet_.hasExtension(Extensions::Texture3D)) {
          texImage3DProc_ = iglTexImage3DOES;
        }
      } else {
        texImage3DProc_ = iglTexImage3D;
      }
    }
  }

  GLCALL_PROC(texImage3DProc_,
              target,
              level,
              internalformat,
              width,
              height,
              depth,
              border,
              format,
              type,
              data);
  APILOG("glTexImage3D(%s, %d, %s, %u, %u, %u, %d, %s, %s, 0x%x)\n",
         GL_ENUM_TO_STRING(target),
         level,
         GL_ENUM_TO_STRING(internalformat),
         width,
         height,
         depth,
         border,
         GL_ENUM_TO_STRING(format),
         GL_ENUM_TO_STRING(type),
         data);
  GLCHECK_ERRORS();
}

void IContext::texParameterf(GLenum target, GLenum pname, GLfloat param) {
  GLCALL(TexParameterf)(target, pname, param);
  APILOG(
      "glTexParameterf(%s, %s, %f)\n", GL_ENUM_TO_STRING(target), GL_ENUM_TO_STRING(pname), param);
  GLCHECK_ERRORS();
}

void IContext::texParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
  GLCALL(TexParameterfv)(target, pname, params);
  APILOG("glTexParameterfv(%s, %s, %p [%f])\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0.f : *params);
  GLCHECK_ERRORS();
}

void IContext::texParameteri(GLenum target, GLenum pname, GLint param) {
  // GLCALL(TexParameteri, target, pname, param);
  GLCALL(TexParameteri)(target, pname, param);
  APILOG("glTexParameteri(%s, %s, %s)\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(pname),
         GL_ENUM_TO_STRING(param));
  GLCHECK_ERRORS();
}

void IContext::texParameteriv(GLenum target, GLenum pname, const GLint* params) {
  GLCALL(TexParameteriv)(target, pname, params);
  APILOG("glTexParameteriv(%s, %s, %p [%d])\n",
         GL_ENUM_TO_STRING(target),
         GL_ENUM_TO_STRING(pname),
         params,
         params == nullptr ? 0 : *params);
  GLCHECK_ERRORS();
}

void IContext::texSubImage2D(GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLsizei width,
                             GLsizei height,
                             GLenum format,
                             GLenum type,
                             const GLvoid* pixels) {
  GLCALL(TexSubImage2D)(target, level, xoffset, yoffset, width, height, format, type, pixels);
  APILOG("glTexSubImage2D(%s, %d, %d, %d, %u, %u, %s, %s, %p)\n",
         GL_ENUM_TO_STRING(target),
         level,
         xoffset,
         yoffset,
         width,
         height,
         GL_ENUM_TO_STRING(format),
         GL_ENUM_TO_STRING(type),
         pixels);
  GLCHECK_ERRORS();
}

void IContext::texSubImage3D(GLenum target,
                             GLint level,
                             GLint xoffset,
                             GLint yoffset,
                             GLint zoffset,
                             GLsizei width,
                             GLsizei height,
                             GLsizei depth,
                             GLenum format,
                             GLenum type,
                             const GLvoid* pixels) {
  if (texSubImage3DProc_ == nullptr) {
    if (deviceFeatureSet_.hasFeature(DeviceFeatures::Texture3D)) {
      if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::Texture3DExtReq)) {
        if (deviceFeatureSet_.hasExtension(Extensions::Texture3D)) {
          texSubImage3DProc_ = iglTexSubImage3DOES;
        }
      } else {
        texSubImage3DProc_ = iglTexSubImage3D;
      }
    }
  }

  GLCALL_PROC(texSubImage3DProc_,
              target,
              level,
              xoffset,
              yoffset,
              zoffset,
              width,
              height,
              depth,
              format,
              type,
              pixels);
  APILOG("glTexSubImage3D(%s, %d, %d, %d, %d, %u, %u, %u, %s, %s, %p)\n",
         GL_ENUM_TO_STRING(target),
         level,
         xoffset,
         yoffset,
         zoffset,
         width,
         height,
         depth,
         GL_ENUM_TO_STRING(format),
         GL_ENUM_TO_STRING(type),
         pixels);
  GLCHECK_ERRORS();
}

void IContext::uniform1f(GLint location, GLfloat x) {
  GLCALL(Uniform1f)(location, x);
  APILOG("glUniform1f(%d, %f)\n", location, x);
  GLCHECK_ERRORS();
}

void IContext::uniform1fv(GLint location, GLsizei count, const GLfloat* v) {
  GLCALL(Uniform1fv)(location, count, v);
  APILOG("glUniform1fv(%d, %u, %p [%f])\n", location, count, v, v == nullptr ? 0.f : *v);
  GLCHECK_ERRORS();
}

void IContext::uniform1i(GLint location, GLint x) {
  GLCALL(Uniform1i)(location, x);
  APILOG("glUniform1i(%d, %d)\n", location, x);
  GLCHECK_ERRORS();
}

void IContext::uniform1iv(GLint location, GLsizei count, const GLint* v) {
  GLCALL(Uniform1iv)(location, count, v);
  APILOG("glUniform1iv(%d, %u, %p [%d])\n", location, count, v, v == nullptr ? 0 : *v);
  GLCHECK_ERRORS();
}

void IContext::uniform2f(GLint location, GLfloat x, GLfloat y) {
  GLCALL(Uniform2f)(location, x, y);
  APILOG("glUniform2f(%d, %f, %f)\n", location, x, y);
  GLCHECK_ERRORS();
}

void IContext::uniform2fv(GLint location, GLsizei count, const GLfloat* v) {
  GLCALL(Uniform2fv)(location, count, v);
  APILOG("glUniform2fv(%d, %u, %p [%f %f])\n",
         location,
         count,
         v,
         v == nullptr ? 0.f : v[0],
         v == nullptr ? 0.f : v[1]);
  GLCHECK_ERRORS();
}

void IContext::uniform2i(GLint location, GLint x, GLint y) {
  GLCALL(Uniform2i)(location, x, y);
  APILOG("glUniform2i(%d, %d, %d)\n", location, x, y);
  GLCHECK_ERRORS();
}

void IContext::uniform2iv(GLint location, GLsizei count, const GLint* v) {
  GLCALL(Uniform2iv)(location, count, v);
  APILOG("glUniform2iv(%d, %u, %p [%d %d])\n",
         location,
         count,
         v,
         v == nullptr ? 0 : v[0],
         v == nullptr ? 0 : v[1]);
  GLCHECK_ERRORS();
}

void IContext::uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  GLCALL(Uniform3f)(location, x, y, z);
  APILOG("glUniform3f(%d, %f, %f, %f)\n", location, x, y, z);
  GLCHECK_ERRORS();
}

void IContext::uniform3fv(GLint location, GLsizei count, const GLfloat* v) {
  GLCALL(Uniform3fv)(location, count, v);
  APILOG("glUniform3fv(%d, %u, %p [%f %f %f])\n",
         location,
         count,
         v,
         v == nullptr ? 0.f : v[0],
         v == nullptr ? 0.f : v[1],
         v == nullptr ? 0.f : v[2]);
  GLCHECK_ERRORS();
}

void IContext::uniform3i(GLint location, GLint x, GLint y, GLint z) {
  GLCALL(Uniform3i)(location, x, y, z);
  APILOG("glUniform3i(%d, %d, %d, %d)\n", location, x, y, z);
  GLCHECK_ERRORS();
}

void IContext::uniform3iv(GLint location, GLsizei count, const GLint* v) {
  GLCALL(Uniform3iv)(location, count, v);
  APILOG("glUniform3fv(%d, %u, %p [%d %d %d])\n",
         location,
         count,
         v,
         v == nullptr ? 0 : v[0],
         v == nullptr ? 0 : v[1],
         v == nullptr ? 0 : v[2]);
  GLCHECK_ERRORS();
}

void IContext::uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  GLCALL(Uniform4f)(location, x, y, z, w);
  APILOG("glUniform4f(%d, %f, %f, %f, %f)\n", location, x, y, z, w);
  GLCHECK_ERRORS();
}

void IContext::uniform4fv(GLint location, GLsizei count, const GLfloat* v) {
  GLCALL(Uniform4fv)(location, count, v);
  APILOG("glUniform4fv(%d, %u, %p [%f %f %f %f])\n",
         location,
         count,
         v,
         v == nullptr ? 0.f : v[0],
         v == nullptr ? 0.f : v[1],
         v == nullptr ? 0.f : v[2],
         v == nullptr ? 0.f : v[3]);
  GLCHECK_ERRORS();
}

void IContext::uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
  GLCALL(Uniform4i)(location, x, y, z, w);
  APILOG("glUniform4i(%d, %d, %d, %d, %d)\n", location, x, y, z, w);
  GLCHECK_ERRORS();
}

void IContext::uniform4iv(GLint location, GLsizei count, const GLint* v) {
  GLCALL(Uniform4iv)(location, count, v);
  APILOG("glUniform4iv(%d, %u, %p [%d %d %d %d])\n",
         location,
         count,
         v,
         v == nullptr ? 0 : v[0],
         v == nullptr ? 0 : v[1],
         v == nullptr ? 0 : v[2],
         v == nullptr ? 0 : v[3]);
  GLCHECK_ERRORS();
}

void IContext::uniformBlockBinding(GLuint pid,
                                   GLuint uniformBlockIndex,
                                   GLuint uniformBlockBinding) {
  IGLCALL(UniformBlockBinding)(pid, uniformBlockIndex, uniformBlockBinding);
  APILOG("glUniformBlockBinding(%u, %u, %u)\n", pid, uniformBlockIndex, uniformBlockBinding);
  GLCHECK_ERRORS();
}

void IContext::uniformMatrix2fv(GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat* value) {
  GLCALL(UniformMatrix2fv)(location, count, transpose, value);
  APILOG("glUniformMatrix2fv(%d, %u, %s, %p [[%f %f] [%f %f]])\n",
         location,
         count,
         GL_BOOL_TO_STRING(transpose),
         value,
         value == nullptr ? 0.f : (transpose ? value[0] : value[0]),
         value == nullptr ? 0.f : (transpose ? value[1] : value[2]),
         value == nullptr ? 0.f : (transpose ? value[2] : value[1]),
         value == nullptr ? 0.f : (transpose ? value[3] : value[3]));
  GLCHECK_ERRORS();
}

void IContext::uniformMatrix3fv(GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat* value) {
  GLCALL(UniformMatrix3fv)(location, count, transpose, value);
  APILOG("glUniformMatrix3fv(%d, %u, %s, %p [[%f %f %f] [%f %f %f] [%f %f %f]])\n",
         location,
         count,
         GL_BOOL_TO_STRING(transpose),
         value,
         value == nullptr ? 0.f : (transpose ? value[0] : value[0]),
         value == nullptr ? 0.f : (transpose ? value[1] : value[3]),
         value == nullptr ? 0.f : (transpose ? value[2] : value[6]),
         value == nullptr ? 0.f : (transpose ? value[3] : value[1]),
         value == nullptr ? 0.f : (transpose ? value[4] : value[4]),
         value == nullptr ? 0.f : (transpose ? value[5] : value[7]),
         value == nullptr ? 0.f : (transpose ? value[6] : value[2]),
         value == nullptr ? 0.f : (transpose ? value[7] : value[5]),
         value == nullptr ? 0.f : (transpose ? value[8] : value[8]));
  GLCHECK_ERRORS();
}

void IContext::uniformMatrix4fv(GLint location,
                                GLsizei count,
                                GLboolean transpose,
                                const GLfloat* value) {
  GLCALL(UniformMatrix4fv)(location, count, transpose, value);
  APILOG(
      "glUniformMatrix4fv(%d, %u, %s, %p [[%f %f %f %f] [%f %f %f %f] [%f %f %f %f] [%f %f %f "
      "%f]])\n",
      location,
      count,
      GL_BOOL_TO_STRING(transpose),
      value,
      value == nullptr ? 0.f : (transpose ? value[0] : value[0]),
      value == nullptr ? 0.f : (transpose ? value[1] : value[4]),
      value == nullptr ? 0.f : (transpose ? value[2] : value[8]),
      value == nullptr ? 0.f : (transpose ? value[3] : value[12]),
      value == nullptr ? 0.f : (transpose ? value[4] : value[1]),
      value == nullptr ? 0.f : (transpose ? value[5] : value[5]),
      value == nullptr ? 0.f : (transpose ? value[6] : value[9]),
      value == nullptr ? 0.f : (transpose ? value[7] : value[13]),
      value == nullptr ? 0.f : (transpose ? value[8] : value[2]),
      value == nullptr ? 0.f : (transpose ? value[9] : value[6]),
      value == nullptr ? 0.f : (transpose ? value[10] : value[10]),
      value == nullptr ? 0.f : (transpose ? value[11] : value[14]),
      value == nullptr ? 0.f : (transpose ? value[12] : value[3]),
      value == nullptr ? 0.f : (transpose ? value[13] : value[7]),
      value == nullptr ? 0.f : (transpose ? value[14] : value[11]),
      value == nullptr ? 0.f : (transpose ? value[15] : value[15]));
  GLCHECK_ERRORS();
}

void IContext::unmapBuffer(GLenum target) {
  if (unmapBufferProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::UnmapBufferExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::MapBuffer) ||
          deviceFeatureSet_.hasExtension(Extensions::MapBufferRange)) {
        unmapBufferProc_ = iglUnmapBufferOES;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::UnmapBuffer)) {
      unmapBufferProc_ = iglUnmapBuffer;
    }
  }
  GLCALL_PROC(unmapBufferProc_, target);
  APILOG("glUnmapBuffer(%s)\n", GL_ENUM_TO_STRING(target));
  GLCHECK_ERRORS();
}

void IContext::useProgram(GLuint program) {
  GLCALL(UseProgram)(program);
  APILOG("glUseProgram(%u)\n", program);
  GLCHECK_ERRORS();
}

void IContext::validateProgram(GLuint program) {
  GLCALL(ValidateProgram)(program);
  APILOG("glValidateProgram(%u)\n", program);
  GLCHECK_ERRORS();
}

void IContext::vertexAttrib1f(GLuint indx, GLfloat x) {
  GLCALL(VertexAttrib1f)(indx, x);
  APILOG("glVertexAttrib1f(%d, %f)\n", indx, x);
  GLCHECK_ERRORS();
}

void IContext::vertexAttrib1fv(GLuint indx, const GLfloat* values) {
  GLCALL(VertexAttrib1fv)(indx, values);
  APILOG("glVertexAttrib1fv(%d, %p [%f])\n", indx, values, values == nullptr ? 0.f : *values);
  GLCHECK_ERRORS();
}

void IContext::vertexAttrib2f(GLuint indx, GLfloat x, GLfloat y) {
  GLCALL(VertexAttrib2f)(indx, x, y);
  APILOG("glVertexAttrib2f(%u, %f, %f)\n", indx, x, y);
  GLCHECK_ERRORS();
}

void IContext::vertexAttrib2fv(GLuint indx, const GLfloat* values) {
  GLCALL(VertexAttrib2fv)(indx, values);
  APILOG("glVertexAttrib2fv(%u, %p [%f %f])\n",
         indx,
         values,
         values == nullptr ? 0.f : values[0],
         values == nullptr ? 0.f : values[1]);
  GLCHECK_ERRORS();
}

void IContext::vertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z) {
  GLCALL(VertexAttrib3f)(indx, x, y, z);
  APILOG("glVertexAttrib3f(%u, %f, %f, %f)\n", indx, x, y, z);
  GLCHECK_ERRORS();
}

void IContext::vertexAttrib3fv(GLuint indx, const GLfloat* values) {
  GLCALL(VertexAttrib3fv)(indx, values);
  APILOG("glVertexAttrib3fv(%u, %p [%f %f %f])\n",
         indx,
         values,
         values == nullptr ? 0.f : values[0],
         values == nullptr ? 0.f : values[1],
         values == nullptr ? 0.f : values[2]);
  GLCHECK_ERRORS();
}

void IContext::vertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  GLCALL(VertexAttrib4f)(indx, x, y, z, w);
  APILOG("glVertexAttrib3f(%u, %f, %f, %f, %f)\n", indx, x, y, z, w);
  GLCHECK_ERRORS();
}

void IContext::vertexAttrib4fv(GLuint indx, const GLfloat* values) {
  GLCALL(VertexAttrib4fv)(indx, values);
  APILOG("glVertexAttrib4fv(%u, %p [%f %f %f %f])\n",
         indx,
         values,
         values == nullptr ? 0.f : values[0],
         values == nullptr ? 0.f : values[1],
         values == nullptr ? 0.f : values[2],
         values == nullptr ? 0.f : values[3]);
  GLCHECK_ERRORS();
}

void IContext::viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
  GLCALL(Viewport)(x, y, width, height);
  APILOG("glViewport(%d, %d, %u, %u)\n", x, y, width, height);
  GLCHECK_ERRORS();
}

GLuint64 IContext::getTextureHandle(GLuint texture) {
  if (getTextureHandleProc_ == nullptr) {
    if (deviceFeatureSet_.hasExtension(Extensions::BindlessTextureArb)) {
      getTextureHandleProc_ = iglGetTextureHandleARB;
    } else if (deviceFeatureSet_.hasExtension(Extensions::BindlessTextureNv)) {
      getTextureHandleProc_ = iglGetTextureHandleNV;
    }
  }

  GLuint64 ret;
  GLCALL_PROC_WITH_RETURN(ret, getTextureHandleProc_, GL_ZERO, texture);
  APILOG("glGetTextureHandle(%u) = %llu\n", texture, ret);
  GLCHECK_ERRORS();
  return ret;
}

void IContext::makeTextureHandleResident(GLuint64 handle) {
  if (makeTextureHandleResidentProc_ == nullptr) {
    if (deviceFeatureSet_.hasExtension(Extensions::BindlessTextureArb)) {
      makeTextureHandleResidentProc_ = iglMakeTextureHandleResidentARB;
    } else if (deviceFeatureSet_.hasExtension(Extensions::BindlessTextureNv)) {
      makeTextureHandleResidentProc_ = iglMakeTextureHandleResidentNV;
    }
  }

  GLCALL_PROC(makeTextureHandleResidentProc_, handle);
  APILOG("glMakeTextureHandleResidentARB(%p)\n", handle);
  GLCHECK_ERRORS();
}

void IContext::makeTextureHandleNonResident(GLuint64 handle) {
  if (makeTextureHandleNonResidentProc_ == nullptr) {
    if (deviceFeatureSet_.hasExtension(Extensions::BindlessTextureArb)) {
      makeTextureHandleNonResidentProc_ = iglMakeTextureHandleNonResidentARB;
    } else if (deviceFeatureSet_.hasExtension(Extensions::BindlessTextureNv)) {
      makeTextureHandleNonResidentProc_ = iglMakeTextureHandleNonResidentNV;
    }
  }

  GLCALL_PROC(makeTextureHandleNonResidentProc_, handle);
  APILOG("glMakeTextureHandleNonResidentARB(%p)\n", handle);
  GLCHECK_ERRORS();
}

void IContext::dispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z) {
  IGLCALL(DispatchCompute)(num_groups_x, num_groups_y, num_groups_z);
  APILOG("glDispatchCompute(%u, %u, %u)\n", num_groups_x, num_groups_y, num_groups_z);
  GLCHECK_ERRORS();
}

void IContext::memoryBarrier(GLbitfield barriers) {
  if (memoryBarrierProc_ == nullptr) {
    if (deviceFeatureSet_.hasInternalRequirement(InternalRequirement::ShaderImageLoadStoreExtReq)) {
      if (deviceFeatureSet_.hasExtension(Extensions::ShaderImageLoadStore)) {
        memoryBarrierProc_ = iglMemoryBarrierEXT;
      }
    } else if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::ShaderImageLoadStore)) {
      memoryBarrierProc_ = iglMemoryBarrier;
    }
  }
  GLCALL_PROC(memoryBarrierProc_, barriers);
  APILOG("glMemoryBarrier(0x%x)\n", barriers);
  GLCHECK_ERRORS();
}

void IContext::vertexAttribPointer(GLuint indx,
                                   GLint size,
                                   GLenum type,
                                   GLboolean normalized,
                                   GLsizei stride,
                                   const GLvoid* ptr) {
  GLCALL(VertexAttribPointer)(indx, size, type, normalized, stride, ptr);
  APILOG("glVertexAttribPointer(%u, %d, %s, %s, %u, %p)\n",
         indx,
         size,
         GL_ENUM_TO_STRING(type),
         GL_BOOL_TO_STRING(normalized),
         stride,
         ptr);
  GLCHECK_ERRORS();
}

Result IContext::getLastError() const {
  return GL_ERROR_TO_RESULT(lastError_);
}

GLenum IContext::checkForErrors(IGL_MAYBE_UNUSED const char* callerName,
                                IGL_MAYBE_UNUSED size_t lineNum) const {
  lastError_ = getError();

  IGL_ASSERT_MSG(lastError_ == GL_NO_ERROR,
                 "[IGL] OpenGL error [%s:%zu] 0x%04X: %s\n",
                 callerName,
                 lineNum,
                 lastError_,
                 GL_ERROR_TO_STRING(lastError_));

  return lastError_;
}

// This function has no effect in release mode because the current thinking
// is there will be no need to call glGetError() after each GL call
void IContext::enableAutomaticErrorCheck(bool enable) {
#if IGL_DEBUG
  alwaysCheckError_ = enable;
#endif
}

/** Returns current `callCounter_` value. Exposed for testing only. */
unsigned int IContext::getCallCount() const {
  return callCounter_;
}

unsigned int IContext::getCurrentDrawCount() const {
  return drawCallCount_;
}

void IContext::resetCounters() {
  callCounter_ = 0;
}

bool IContext::addRef() {
  bool ret = isLikelyValidObject();
  if (ret) {
    ++refCount_;
  }
  return ret;
}

bool IContext::releaseRef() {
  bool ret = isLikelyValidObject();
  if (ret) {
    --refCount_;
  }
  return ret;
}

UnbindPolicy IContext::getUnbindPolicy() const {
  return unbindPolicy_;
}

void IContext::setUnbindPolicy(UnbindPolicy newValue) {
  unbindPolicy_ = newValue;
}

void IContext::initialize(Result* result) {
  setCurrent();
  if (!isCurrentContext()) {
    Result::setResult(result, Result::Code::ArgumentInvalid, "Invalid context, setCurrent failed.");
    return;
  }

  GLVersion glVersion = GLVersion::NotAvailable;

  const char* version = (char*)getString(GL_VERSION);
  if (version == nullptr) {
    IGL_LOG_ERROR("Unable to get GL version string\n");
    Result::setResult(result, Result::Code::RuntimeError, "Unable to get GL version string\n");
    glVersion = DeviceFeatureSet::usesOpenGLES() ? GLVersion::v2_0_ES : GLVersion::v2_0;
  } else {
    glVersion = ::igl::opengl::getGLVersion(version);
    if (glVersion == GLVersion::NotAvailable) {
      IGL_ASSERT_NOT_IMPLEMENTED();
      Result::setResult(result, Result::Code::RuntimeError, "Unable to get GL version\n");
    }
  }
  deviceFeatureSet_.initializeVersion(glVersion);

  std::string extensions;
  std::unordered_set<std::string> supportedExtensions;
  if (!deviceFeatureSet_.hasInternalFeature(InternalFeatures::GetStringi)) {
    const GLubyte* extensionStr = getString(GL_EXTENSIONS);

    // If setCurrent() fails, then extensions may be nullptr.
    if (extensionStr) {
      extensions = std::string((char*)extensionStr);
    }
  } else {
    GLint n = 0;
    getIntegerv(GL_NUM_EXTENSIONS, &n);
    for (GLint i = 0; i < n; i++) {
      auto ext = reinterpret_cast<const char*>(getStringi(GL_EXTENSIONS, i));
      if (ext) {
        supportedExtensions.insert(ext);
      }
    }
  }

#if IGL_DEBUG || defined(IGL_FORCE_ENABLE_LOGS)
  IGL_LOG_INFO("GL Context Initialized: %p", this);
  IGL_LOG_INFO("GL Version: %s\n", version);
  const char* vendor = (char*)getString(GL_VENDOR);
  IGL_LOG_INFO("GL Vendor: %s\n", (vendor != nullptr) ? vendor : "(null)");
  const char* renderer = (char*)getString(GL_RENDERER);
  IGL_LOG_INFO("GL Renderer: %s\n", (renderer != nullptr) ? renderer : "(null)");
  if (!extensions.empty() || supportedExtensions.empty()) {
    IGL_LOG_INFO("GL Extensions: %s\n", extensions.c_str());
  } else {
    std::vector<std::string> sortedExtensions(supportedExtensions.begin(),
                                              supportedExtensions.end());
    std::sort(sortedExtensions.begin(), sortedExtensions.end());
    std::stringstream extensionsLog;
    for (auto it = sortedExtensions.begin(); it != sortedExtensions.end(); ++it) {
      extensionsLog << *it;
      if (it != std::prev(sortedExtensions.end())) {
        extensionsLog << ", ";
      }
    }
    IGL_LOG_INFO("GL Extensions: %s\n", extensionsLog.str().c_str());
  }
#endif

  deviceFeatureSet_.initializeExtensions(std::move(extensions), std::move(supportedExtensions));

  if (deviceFeatureSet_.hasInternalFeature(InternalFeatures::SeamlessCubeMap)) {
    enable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  }
}

const DeviceFeatureSet& IContext::deviceFeatures() const {
  return deviceFeatureSet_;
}

void IContext::apiLogNextNDraws(const unsigned int n) {
  apiLogDrawsLeft_ = n;
}

void IContext::apiLogStart() {
  apiLogEnabled_ = true;
}

void IContext::apiLogEnd() {
  apiLogEnabled_ = false;
}

void IContext::SynchronizedDeletionQueues::flushDeletionQueue(IContext& context) {
  if (IGL_VERIFY(context.isCurrentContext() || context.isCurrentSharegroup())) {
    swapScratchDeletionQueues();

    if (!scratchBuffersQueue_.empty()) {
      context.deleteBuffers(static_cast<GLsizei>(scratchBuffersQueue_.size()),
                            scratchBuffersQueue_.data());
      scratchBuffersQueue_.clear();
    }

    for (auto i : scratchUnbindBuffersQueue_) {
      context.bindBuffer(i, 0);
    }
    scratchUnbindBuffersQueue_.clear();

    if (!scratchFramebuffersQueue_.empty()) {
      context.deleteFramebuffers(static_cast<GLsizei>(scratchFramebuffersQueue_.size()),
                                 scratchFramebuffersQueue_.data());
      scratchFramebuffersQueue_.clear();
    }

    if (!scratchRenderbuffersQueue_.empty()) {
      context.deleteRenderbuffers(static_cast<GLsizei>(scratchRenderbuffersQueue_.size()),
                                  scratchRenderbuffersQueue_.data());
      scratchRenderbuffersQueue_.clear();
    }

    if (!scratchVertexArraysQueue_.empty()) {
      context.deleteVertexArrays(static_cast<GLsizei>(scratchVertexArraysQueue_.size()),
                                 scratchVertexArraysQueue_.data());
      scratchVertexArraysQueue_.clear();
    }

    for (auto i : scratchProgramQueue_) {
      context.deleteProgram(i);
    }
    scratchProgramQueue_.clear();

    for (auto i : scratchShaderQueue_) {
      context.deleteShader(i);
    }
    scratchShaderQueue_.clear();

    if (!scratchTexturesQueue_.empty()) {
      context.deleteTextures(scratchTexturesQueue_);
      scratchTexturesQueue_.clear();
    }
  }
}

void IContext::SynchronizedDeletionQueues::swapScratchDeletionQueues() {
  std::lock_guard<std::mutex> guard(deletionQueueMutex_);

  std::swap(scratchBuffersQueue_, buffersQueue_);
  std::swap(scratchUnbindBuffersQueue_, unbindBuffersQueue_);
  std::swap(scratchFramebuffersQueue_, framebuffersQueue_);
  std::swap(scratchRenderbuffersQueue_, renderbuffersQueue_);
  std::swap(scratchVertexArraysQueue_, vertexArraysQueue_);
  std::swap(scratchProgramQueue_, programQueue_);
  std::swap(scratchShaderQueue_, shaderQueue_);
  std::swap(scratchTexturesQueue_, texturesQueue_);
}

void IContext::SynchronizedDeletionQueues::queueDeleteBuffers(GLsizei n, const GLuint* buffers) {
  std::lock_guard<std::mutex> guard(deletionQueueMutex_);
  for (GLsizei i = 0; i < n; ++i) {
    buffersQueue_.push_back(buffers[i]);
  }
}

void IContext::SynchronizedDeletionQueues::queueUnbindBuffer(GLenum target) {
  std::lock_guard<std::mutex> guard(deletionQueueMutex_);
  unbindBuffersQueue_.insert(target);
}

void IContext::SynchronizedDeletionQueues::queueDeleteFramebuffers(GLsizei n,
                                                                   const GLuint* framebuffers) {
  std::lock_guard<std::mutex> guard(deletionQueueMutex_);
  for (GLsizei i = 0; i < n; ++i) {
    framebuffersQueue_.push_back(framebuffers[i]);
  }
}

void IContext::SynchronizedDeletionQueues::queueDeleteRenderbuffers(GLsizei n,
                                                                    const GLuint* renderbuffers) {
  std::lock_guard<std::mutex> guard(deletionQueueMutex_);
  for (GLsizei i = 0; i < n; ++i) {
    renderbuffersQueue_.push_back(renderbuffers[i]);
  }
}

void IContext::SynchronizedDeletionQueues::queueDeleteVertexArrays(GLsizei n,
                                                                   const GLuint* vertexArrays) {
  std::lock_guard<std::mutex> guard(deletionQueueMutex_);
  for (GLsizei i = 0; i < n; ++i) {
    vertexArraysQueue_.push_back(vertexArrays[i]);
  }
}

void IContext::SynchronizedDeletionQueues::queueDeleteProgram(GLuint program) {
  std::lock_guard<std::mutex> guard(deletionQueueMutex_);
  programQueue_.push_back(program);
}

void IContext::SynchronizedDeletionQueues::queueDeleteShader(GLuint shaderId) {
  std::lock_guard<std::mutex> guard(deletionQueueMutex_);
  shaderQueue_.push_back(shaderId);
}

void IContext::SynchronizedDeletionQueues::queueDeleteTextures(
    const std::vector<GLuint>& textures) {
  std::lock_guard<std::mutex> guard(deletionQueueMutex_);
  texturesQueue_.insert(std::end(texturesQueue_), std::begin(textures), std::end(textures));
}
} // namespace igl::opengl
