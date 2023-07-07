# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "Mac")

file(GLOB PLATFORM_SHARED_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../shared/fileLoader/mac/*.mm ../shared/imageLoader/mac/*.mm ../shared/imageWriter/mac/*.mm ../shared/imageWriter/stb/*.cpp
     ../shared/platform/mac/*.cpp)
file(GLOB PLATFORM_SHARED_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../shared/fileLoader/mac/*.h ../shared/imageLoader/mac/*.h ../shared/imageWriter/mac/*.h ../shared/imageWriter/stb/*.h
     ../shared/platform/mac/*.h)

add_library(IGLShellPlatform ${PLATFORM_SHARED_SRC_FILES} ${PLATFORM_SHARED_HEADER_FILES})
target_link_libraries(IGLShellPlatform PUBLIC IGLLibrary)

igl_set_folder(IGLShellPlatform "IGL")
igl_set_cxxstd(IGLShellPlatform 17)

if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
  set_source_files_properties(${PLATFORM_SHARED_SRC_FILES} PROPERTIES COMPILE_FLAGS "-fobjc-arc")
  set_source_files_properties(${PLATFORM_SHARED_SRC_FILES} PROPERTIES COMPILE_FLAGS "-fobjc-weak")
else()
  set_target_properties(IGLShellShared PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES)
  set_target_properties(IGLShellShared PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
  set_target_properties(IGLShellPlatform PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES)
  set_target_properties(IGLShellPlatform PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
endif()

function(ADD_SHELL_SESSION_WITH_SRCS target srcs libs)
  set(RESOURCES ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../resources/mac/Base.lproj/MainMenu.xib)
  file(GLOB SHELL_SESSION_SRC_FILES LIST_DIRECTORIES false ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.mm
                                                           ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.m)
  file(GLOB SHELL_SESSION_HEADER_FILES LIST_DIRECTORIES false ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.h)

  add_executable(${target} MACOSX_BUNDLE ${srcs} "${SHELL_SESSION_SRC_FILES}" "${SHELL_SESSION_SHELL_HEADER_FILES}"
                                         "${RESOURCES}")
  igl_set_folder(${target} "IGL Shell Sessions")
  igl_set_cxxstd(${target} 17)
  target_compile_definitions(${target} PRIVATE "IGL_SHELL_SESSION=${target}")
  target_link_libraries(${target} PUBLIC ${libs})
  target_link_libraries(${target} PUBLIC IGLShellShared)
  target_link_libraries(${target} PUBLIC IGLShellPlatform)
  if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
    set_source_files_properties(${SHELL_SESSION_SRC_FILES} PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    set_source_files_properties(${SHELL_SESSION_SRC_FILES} PROPERTIES COMPILE_FLAGS "-fobjc-weak")
  else()
    set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES)
    set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
  endif()
  target_link_libraries(
    ${target} PUBLIC "-framework AVFoundation" "-framework CoreGraphics" "-framework CoreMotion" "-framework AppKit"
                     "-framework Metal" "-framework MetalKit" "-framework OpenGL" "-framework QuartzCore")
  set_target_properties(
    ${target}
    PROPERTIES MACOSX_BUNDLE TRUE MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../resources/mac/Info.plist
               RESOURCE "${RESOURCES}")
endfunction()
