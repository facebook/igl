# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# CMake 3.16 can load Vulkan SDK
cmake_minimum_required(VERSION 3.16)

project(IGLVulkan CXX C)

file(GLOB SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c)
file(GLOB HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

add_library(IGLVulkan ${SRC_FILES} ${HEADER_FILES})

igl_set_cxxstd(IGLVulkan 20)
igl_set_folder(IGLVulkan "IGL")

target_link_libraries(IGLVulkan PRIVATE LVKLibrary)
target_link_libraries(IGLVulkan PRIVATE LVKVulkan)
target_link_libraries(IGLVulkan PUBLIC  glslang SPIRV glslang-default-resource-limits)
target_link_libraries(IGLVulkan PUBLIC  Vulkan::Vulkan)

target_include_directories(IGLVulkan PUBLIC "${LVK_ROOT_DIR}/third-party/deps/src/volk")
target_include_directories(IGLVulkan PUBLIC "${LVK_ROOT_DIR}/third-party/deps/src/vma/include")

if(WIN32)
  add_definitions("-DVK_USE_PLATFORM_WIN32_KHR=1")
  add_definitions("-DNOMINMAX")
elseif(UNIX)
  add_definitions("-DVK_USE_PLATFORM_XLIB_KHR=1")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(IGLVulkan PRIVATE "-Wno-nullability-completeness")
endif()
