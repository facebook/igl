# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# CMake 3.16 can load Vulkan SDK
cmake_minimum_required(VERSION 3.16)

project(IGLVulkan CXX C)

file(GLOB SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c)
file(GLOB HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

if(APPLE)
  file(GLOB APPLE_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} moltenvk/*.mm)
  file(GLOB APPLE_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} moltenvk/*.h)
  list(APPEND SRC_FILES ${APPLE_SRC_FILES})
  list(APPEND HEADER_FILES ${APPLE_HEADER_FILES})
endif()

add_library(IGLVulkan ${SRC_FILES} ${HEADER_FILES})

target_link_libraries(IGLVulkan PRIVATE IGLLibrary)

igl_set_cxxstd(IGLVulkan 17)
igl_set_folder(IGLVulkan "IGL")

# glslang
# cmake-format: off
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "")
set(ENABLE_HLSL             OFF CACHE BOOL "")
set(ENABLE_CTEST            OFF CACHE BOOL "")
set(ENABLE_OPT              OFF CACHE BOOL "")
set(ENABLE_SPVREMAPPER      OFF CACHE BOOL "")
set(SKIP_GLSLANG_INSTALL    ON  CACHE BOOL "")
add_subdirectory(${IGL_ROOT_DIR}/third-party/deps/src/glslang "glslang")
igl_set_folder(GenericCodeGen     "third-party/glslang")
igl_set_folder(glslang            "third-party/glslang")
igl_set_folder(MachineIndependent "third-party/glslang")
igl_set_folder(OGLCompiler        "third-party/glslang")
igl_set_folder(OSDependent        "third-party/glslang")
igl_set_folder(SPIRV              "third-party/glslang")
igl_set_folder(glslang-default-resource-limits "third-party/glslang")
# cmake-format: on

target_link_libraries(IGLVulkan PUBLIC glslang SPIRV glslang-default-resource-limits)

find_package(Vulkan REQUIRED)

target_link_libraries(IGLVulkan PUBLIC Vulkan::Vulkan)
target_link_libraries(IGLVulkan PUBLIC fmt)

target_include_directories(IGLVulkan PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/volk")
target_include_directories(IGLVulkan PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/vma/include")

if(WIN32)
  add_definitions("-DVK_USE_PLATFORM_WIN32_KHR=1")
  add_definitions("-DNOMINMAX")
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID)
  add_definitions("-DVK_USE_PLATFORM_XLIB_KHR=1")
endif()

if(ANDROID)
  add_definitions("-DVK_USE_PLATFORM_ANDROID_KHR=1")
endif()

if(APPLE)
  add_definitions("-DVK_USE_PLATFORM_METAL_EXT=1")
  target_link_libraries(IGLVulkan PRIVATE "-framework QuartzCore")
  target_link_libraries(IGLVulkan PRIVATE "-framework Metal")
  if(MACOSX)
    target_link_libraries(IGLVulkan PRIVATE "-framework Cocoa")
  endif()
  if(IOS)
    target_link_libraries(IGLVulkan PRIVATE "-framework UIKit")
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(IGLVulkan PRIVATE "-Wno-nullability-completeness")
endif()
