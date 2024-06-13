# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

project(IGLGlslang CXX C)

file(GLOB SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c)
file(GLOB HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

add_library(IGLGlslang ${SRC_FILES} ${HEADER_FILES})

target_link_libraries(IGLGlslang PRIVATE IGLLibrary)

igl_set_cxxstd(IGLGlslang 17)
igl_set_folder(IGLGlslang "IGL")

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

target_link_libraries(IGLGlslang PUBLIC glslang SPIRV glslang-default-resource-limits)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(IGLGlslang PRIVATE "-Wno-nullability-completeness")
endif()
