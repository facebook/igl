# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "iOS")

file(GLOB PLATFORM_SHARED_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../shared/fileLoader/ios/*.mm ../shared/imageLoader/ios/*.mm ../shared/imageWriter/ios/*.mm ../shared/platform/ios/*.mm)
file(GLOB PLATFORM_SHARED_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../shared/fileLoader/ios/*.h ../shared/imageLoader/ios/*.h ../shared/imageWriter/ios/*.h ../shared/platform/ios/*.h)

add_library(IGLShellPlatform ${PLATFORM_SHARED_SRC_FILES} ${PLATFORM_SHARED_HEADER_FILES})
target_link_libraries(IGLShellPlatform PUBLIC IGLLibrary)

set_target_properties(IGLShellPlatform PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES)
set_target_properties(IGLShellPlatform PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)

target_link_libraries(IGLShellPlatform PUBLIC IGLLibrary)

igl_set_folder(IGLShellPlatform "IGL")
igl_set_cxxstd(IGLShellPlatform 17)

set_target_properties(IGLShellShared PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES)
set_target_properties(IGLShellShared PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)

function(ADD_SHELL_SESSION_WITH_SRCS target srcs libs)
  file(GLOB PLATFORM_SHELL_SRC_FILES LIST_DIRECTORIES false ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.mm
                                                            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.m)
  file(GLOB PLATFORM_SHELL_HEADER_FILES LIST_DIRECTORIES false ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.h)

  add_executable(${target} MACOSX_BUNDLE ${srcs} "${PLATFORM_SHELL_SRC_FILES}" "${PLATFORM_SHELL_HEADER_FILES}")
  igl_set_folder(${target} "IGL Shell Sessions")
  igl_set_cxxstd(${target} 17)
  target_compile_definitions(${target} PRIVATE "IGL_SHELL_SESSION=${target}")
  target_link_libraries(${target} PUBLIC ${libs})
  target_link_libraries(${target} PUBLIC IGLShellShared)
  target_link_libraries(${target} PUBLIC IGLShellPlatform)
  set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES)
  set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
  target_link_libraries(
    ${target}
    PUBLIC "-framework AVFoundation"
           "-framework CoreGraphics"
           "-framework CoreMotion"
           "-framework Foundation"
           "-framework Metal"
           "-framework MetalKit"
           "-framework QuartzCore"
           "-framework UIKit")
  set_target_properties(
    ${target} PROPERTIES MACOSX_BUNDLE TRUE MACOSX_BUNDLE_GUI_IDENTIFIER "com.meta.iglshell" MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
                         MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0" RESOURCE "${RESOURCES}")
endfunction()
