# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# CMake 3.16 can load Vulkan SDK
cmake_minimum_required(VERSION 3.19)

project(IGLVulkan CXX C)

file(GLOB SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c util/*.cpp)
file(GLOB HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h util/*.h)

if(APPLE)
  file(GLOB APPLE_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} moltenvk/*.mm)
  file(GLOB APPLE_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} moltenvk/*.h)
  list(APPEND SRC_FILES ${APPLE_SRC_FILES})
  list(APPEND HEADER_FILES ${APPLE_HEADER_FILES})
endif()

if(ANDROID)
  file(GLOB ANDROID_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} android/*.cpp)
  file(GLOB ANDROID_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} android/*.h)
  list(APPEND SRC_FILES ${ANDROID_SRC_FILES})
  list(APPEND HEADER_FILES ${ANDROID_HEADER_FILES})
endif()

add_library(IGLVulkan ${SRC_FILES} ${HEADER_FILES})

target_link_libraries(IGLVulkan PRIVATE IGLLibrary IGLGlslang)

igl_set_cxxstd(IGLVulkan 17)
igl_set_folder(IGLVulkan "IGL")

find_package(Vulkan REQUIRED)

target_link_libraries(IGLVulkan PUBLIC Vulkan::Vulkan)
target_link_libraries(IGLVulkan PUBLIC fmt)

target_include_directories(IGLVulkan PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/fmt/include")
target_include_directories(IGLVulkan PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/volk")
target_include_directories(IGLVulkan PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/vma/include")
target_include_directories(IGLVulkan PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/SPIRV-Headers/include")
target_include_directories(IGLVulkan PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/glslang")

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
