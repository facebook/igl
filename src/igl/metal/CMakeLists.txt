# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19)

# CMake 3.19 is the first version to support Apple Silicon.

project(IGLMetal OBJC OBJCXX CXX C)

file(GLOB SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.mm)
file(GLOB HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

if(IOS)
  list(APPEND SRC_FILES ios/Device.mm ios/Framebuffer.mm)
  list(APPEND HEADER_FILES ios/Device.h ios/Framebuffer.h)
else()
  list(APPEND SRC_FILES macos/Device.mm macos/Framebuffer.mm)
  list(APPEND HEADER_FILES macos/Device.h macos/Framebuffer.h)
endif()

add_library(IGLMetal ${SRC_FILES} ${HEADER_FILES})

target_link_libraries(IGLMetal PRIVATE IGLLibrary "-framework Metal" "-framework QuartzCore")

igl_set_cxxstd(IGLMetal 20)
igl_set_folder(IGLMetal "IGL")

set(OBJC_EXTRA_FLAGS -fobjc-arc -fobjc-weak)
set(OBJC_DISABLE_WARNINGS -Wno-objc-designated-initializers -Wno-objc-missing-super-calls -Wno-objc-property-no-attribute)

if(CMAKE_GENERATOR MATCHES "Xcode")
  set_target_properties(IGLMetal PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES)
  set_target_properties(IGLMetal PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
else()
  target_compile_options(IGLMetal PRIVATE $<$<COMPILE_LANGUAGE:OBJC>:${OBJC_EXTRA_FLAGS}>)
  target_compile_options(IGLMetal PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:${OBJC_EXTRA_FLAGS}>)
endif()

target_compile_options(IGLMetal PRIVATE $<$<COMPILE_LANGUAGE:OBJC>:${OBJC_DISABLE_WARNINGS}>)
target_compile_options(IGLMetal PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:${OBJC_DISABLE_WARNINGS}>)
target_compile_options(IGLMetal PRIVATE "-Wno-deprecated-declarations")
