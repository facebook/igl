# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "IGLShellOpenXR")

# emulate BUCK include paths
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include_shell_openxr/shell)
file(CREATE_LINK
     "${IGL_ROOT_DIR}/shell/openxr/src"
     "${CMAKE_CURRENT_BINARY_DIR}/include_shell_openxr/shell/openxr" COPY_ON_ERROR SYMBOLIC)
#

file(GLOB SHELL_OPENXR_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../openxr/impl/*.cpp
     ../openxr/src/*.cpp)

file(GLOB SHELL_OPENXR_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../openxr/impl/*.h
     ../openxr/src/*.h)

macro(ADD_SHELL_LIBRARY name srcs)
  set (extra_srcs ${ARGN})
  add_library(${name} ${srcs} ${extra_srcs})

  target_link_libraries(${name} PUBLIC IGLLibrary)

  target_include_directories(${name} PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/openxr-sdk/src/common")
  target_include_directories(${name} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include_shell_openxr")

  igl_set_folder(${name} "IGL")
  igl_set_cxxstd(${name} 20)
endmacro()

file(GLOB SHELL_OPENXR_PLATFORM_DEKTOP_VULKAN_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../openxr/desktop/*.cpp
     ../openxr/desktop/vulkan/*.cpp)

file(GLOB SHELL_OPENXR_PLATFORM_DEKTOP_VULKAN_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../openxr/desktop/*.h
     ../openxr/desktop/vulkan/*.h)

add_shell_library(IGLShellOpenXR_DesktopVulkan
  ${SHELL_OPENXR_SRC_FILES}
  ${SHELL_OPENXR_HEADER_FILES}
  ${SHELL_OPENXR_PLATFORM_DEKTOP_VULKAN_SRC_FILES}
  ${SHELL_OPENXR_PLATFORM_DEKTOP_VULKAN_HEADER_FILES})

if(ANDROID)
  if(IGL_WITH_VULKAN)
    file(GLOB SHELL_OPENXR_PLATFORM_ANDROID_VULKAN_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
      ../openxr/mobile/vulkan/*.cpp)

    file(GLOB SHELL_OPENXR_PLATFORM_ANDROID_VULKAN_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
      ../openxr/mobile/vulkan/*.h)

    add_shell_library(IGLShellOpenXR_AndroidVulkan
      ${SHELL_OPENXR_SRC_FILES}
      ${SHELL_OPENXR_HEADER_FILES}
      ${SHELL_OPENXR_PLATFORM_ANDROID_VULKAN_SRC_FILES}
      ${SHELL_OPENXR_PLATFORM_ANDROID_VULKAN_HEADER_FILES})
    target_compile_definitions(IGLShellOpenXR_AndroidVulkan PRIVATE "USE_VULKAN_BACKEND" "XR_USE_PLATFORM_ANDROID" "XR_USE_GRAPHICS_API_VULKAN")
  endif()
  if(IGL_WITH_OPENGLES)
    file(GLOB SHELL_OPENXR_PLATFORM_ANDROID_GL_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
      ../openxr/mobile/opengl/*.cpp)

    file(GLOB SHELL_OPENXR_PLATFORM_ANDROID_GL_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
      ../openxr/mobile/opengl/*.h)

    add_shell_library(IGLShellOpenXR_AndroidOpenGLES
      ${SHELL_OPENXR_SRC_FILES}
      ${SHELL_OPENXR_HEADER_FILES}
      ${SHELL_OPENXR_PLATFORM_ANDROID_GL_SRC_FILES}
      ${SHELL_OPENXR_PLATFORM_ANDROID_GL_HEADER_FILES})
    target_compile_definitions(IGLShellOpenXR_AndroidOpenGLES PRIVATE "USE_OPENGL_BACKEND" "XR_USE_PLATFORM_ANDROID" "XR_USE_GRAPHICS_API_OPENGL_ES")
  endif()
endif()
