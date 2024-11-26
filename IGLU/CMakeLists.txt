# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19)

set(PROJECT_NAME "IGLU")

macro(ADD_IGLU_MODULE module)
  file(GLOB_RECURSE SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${module}/*.cpp ${module}/*.mm)
  file(GLOB_RECURSE HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${module}/*.h)
  if((NOT IGL_WITH_OPENGL) AND (NOT IGL_WITH_OPENGLES))
    list(REMOVE_ITEM SRC_FILES ${module}/OpenGLTextureAccessor.cpp)
    list(REMOVE_ITEM HEADER_FILES ${module}/OpenGLTextureAccessor.h)
  endif()
  if(NOT IGL_WITH_METAL)
    list(REMOVE_ITEM SRC_FILES ${module}/MetalTextureAccessor.mm)
    list(REMOVE_ITEM HEADER_FILES ${module}/MetalTextureAccessor.h)
  endif()
  add_library(IGLU${module} ${SRC_FILES} ${HEADER_FILES})
  igl_set_cxxstd(IGLU${module} 20)
  igl_set_folder(IGLU${module} "IGL/${PROJECT_NAME}")
  target_link_libraries(IGLU${module} PRIVATE IGLLibrary)
  target_include_directories(IGLU${module} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include")
  target_include_directories(IGLU${module} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include_${module}")
  target_include_directories(IGLU${module} PUBLIC "${IGL_ROOT_DIR}")
endmacro()

add_iglu_module(imgui)
add_iglu_module(managedUniformBuffer)
add_iglu_module(sentinel)
add_iglu_module(simple_renderer)
add_iglu_module(state_pool)
add_iglu_module(shaderCross)
add_iglu_module(texture_accessor)
add_iglu_module(texture_loader)
add_iglu_module(uniform)

# header-only
add_library(IGLUsimdtypes INTERFACE)
target_include_directories(IGLUsimdtypes INTERFACE "simdtypes")

target_link_libraries(IGLUtexture_loader PRIVATE IGLstb)
target_link_libraries(IGLUtexture_loader PRIVATE ktx)

if(IGL_WITH_SHELL)
  target_link_libraries(IGLUimgui PRIVATE IGLShellShared)
else()
  target_sources(IGLUimgui PRIVATE "${IGL_ROOT_DIR}/shell/shared/input/InputDispatcher.cpp")
endif()

# ImGui
target_sources(IGLUimgui PRIVATE "${IGL_ROOT_DIR}/third-party/deps/src/imgui/imgui.cpp")
target_sources(IGLUimgui PRIVATE "${IGL_ROOT_DIR}/third-party/deps/src/imgui/imgui_demo.cpp")
target_sources(IGLUimgui PRIVATE "${IGL_ROOT_DIR}/third-party/deps/src/imgui/imgui_draw.cpp")
target_sources(IGLUimgui PRIVATE "${IGL_ROOT_DIR}/third-party/deps/src/imgui/imgui_tables.cpp")
target_sources(IGLUimgui PRIVATE "${IGL_ROOT_DIR}/third-party/deps/src/imgui/imgui_widgets.cpp")
target_include_directories(IGLUimgui PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/imgui")
target_include_directories(IGLUtexture_accessor PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/glew/include")

if(UNIX)
  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(IGLUimgui PUBLIC "-Wno-volatile")
  endif()
endif()

# SPIRV-Cross
# cmake-format: off
set(SPIRV_CROSS_SHARED         OFF CACHE BOOL "")
set(SPIRV_CROSS_STATIC         ON  CACHE BOOL "")
set(SPIRV_CROSS_CLI            OFF CACHE BOOL "")
set(SPIRV_CROSS_ENABLE_TESTS   OFF CACHE BOOL "")
set(SPIRV_CROSS_ENABLE_GLSL    ON  CACHE BOOL "")
set(SPIRV_CROSS_ENABLE_HLSL    OFF CACHE BOOL "")
set(SPIRV_CROSS_ENABLE_MSL     ON  CACHE BOOL "")
set(SPIRV_CROSS_ENABLE_CPP     OFF CACHE BOOL "")
set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "")
set(SPIRV_CROSS_SKIP_INSTALL   ON  CACHE BOOL "")

add_subdirectory(${IGL_ROOT_DIR}/third-party/deps/src/SPIRV-Cross "SPIRV-Cross")

igl_set_folder(spirv-cross-core "third-party/spirv-cross/spirv-cross-core")
igl_set_folder(spirv-cross-glsl "third-party/spirv-cross/spirv-cross-glsl")
igl_set_folder(spirv-cross-msl  "third-party/spirv-cross/spirv-cross-msl")
igl_set_folder(spirv-cross-util "third-party/spirv-cross/spirv-cross-util")
igl_set_folder(spirv-cross-c    "third-party/spirv-cross/spirv-cross-c")
# cmake-format: on

target_include_directories(IGLUshaderCross PRIVATE "${IGL_ROOT_DIR}/third-party/deps/src/SPIRV-Cross" "${IGL_ROOT_DIR}/third-party/deps/src/glslang")
target_link_libraries(IGLUshaderCross PRIVATE IGLGlslang IGLUmanagedUniformBuffer spirv-cross-core spirv-cross-glsl spirv-cross-msl spirv-cross-util spirv-cross-c)
