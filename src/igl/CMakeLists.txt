# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19)

project(IGLLibrary CXX C)

add_definitions("-DIGL_CMAKE_BUILD=1")

file(GLOB SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)
file(GLOB HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

if(WIN32)
  list(APPEND SRC_FILES win/LogDefault.cpp)
  list(APPEND HEADER_FILES win/LogDefault.h)
endif()

if(ANDROID)
  list(APPEND SRC_FILES android/LogDefault.cpp)
  list(APPEND HEADER_FILES android/LogDefault.h)
  list(APPEND SRC_FILES android/NativeHWBuffer.cpp)
  list(APPEND HEADER_FILES android/NativeHWBuffer.h)
endif()

add_library(IGLLibrary ${SRC_FILES} ${HEADER_FILES})

target_include_directories(IGLLibrary PUBLIC "${IGL_ROOT_DIR}/src")
target_include_directories(IGLLibrary PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/glm")

if(NOT IGL_WITH_OPENGL)
  target_include_directories(IGLLibrary PUBLIC "${IGL_ROOT_DIR}/src/igl/opengl/util")
  target_sources(IGLLibrary PRIVATE opengl/util/TextureFormat.cpp opengl/util/TextureFormat.h)
endif()

if(NOT EMSCRIPTEN)
  # Only link fmt if it's available, use PRIVATE to avoid exporting the dependency
  if(TARGET fmt::fmt)
    target_link_libraries(IGLLibrary PRIVATE fmt::fmt)
  elseif(TARGET fmt)
    target_link_libraries(IGLLibrary PRIVATE fmt)
  endif()
endif()

igl_set_cxxstd(IGLLibrary 20)
igl_set_folder(IGLLibrary "IGL")

add_subdirectory(glslang)

if(IGL_WITH_OPENGL OR IGL_WITH_OPENGLES OR IGL_WITH_WEBGL)
  add_subdirectory(opengl)
  target_link_libraries(IGLLibrary PUBLIC IGLOpenGL)
endif()

if(IGL_WITH_VULKAN)
  add_subdirectory(vulkan)
  target_link_libraries(IGLLibrary PUBLIC IGLVulkan)
endif()

if(IGL_WITH_METAL)
  add_subdirectory(metal)
  target_link_libraries(IGLLibrary PUBLIC IGLMetal)
endif()

# OpenGL tests use GLES on Windows and we do not use Angle with CMake - so OGL
# tests are disabled for now on Windows
if(IGL_WITH_TESTS AND IGL_WITH_IGLU AND (IGL_WITH_VULKAN OR (NOT WIN32)))
  add_subdirectory(tests)
  if((IGL_WITH_OPENGL OR IGL_WITH_OPENGLES) AND NOT APPLE)
    target_sources(IGLTests PRIVATE opengl/egl/Context.cpp opengl/egl/Device.cpp opengl/egl/HWDevice.cpp
                                    opengl/egl/PlatformDevice.cpp)
  endif()
endif()
