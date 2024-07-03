# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19)

set(PROJECT_NAME "Mac")

file(GLOB PLATFORM_SHARED_SRC_CPP_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../shared/imageWriter/stb/*.cpp ../shared/platform/mac/*.cpp)
file(GLOB PLATFORM_SHARED_SRC_OBJC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../shared/fileLoader/apple/*.mm ../shared/imageWriter/mac/*.mm)
file(GLOB PLATFORM_SHARED_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../shared/fileLoader/apple/*.h ../shared/imageWriter/mac/*.h ../shared/imageWriter/stb/*.h
     ../shared/platform/mac/*.h)

add_library(IGLShellPlatform ${PLATFORM_SHARED_SRC_CPP_FILES} ${PLATFORM_SHARED_SRC_OBJC_FILES} ${PLATFORM_SHARED_HEADER_FILES})
target_link_libraries(IGLShellPlatform PUBLIC IGLLibrary)

igl_set_folder(IGLShellPlatform "IGL")
igl_set_cxxstd(IGLShellPlatform 17)

if(CMAKE_GENERATOR STREQUAL "Xcode")
  set_target_properties(IGLShellShared PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES)
  set_target_properties(IGLShellShared PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
  set_target_properties(IGLShellPlatform PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES)
  set_target_properties(IGLShellPlatform PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
else()
  set_source_files_properties(${PLATFORM_SHARED_SRC_OBJC_FILES} PROPERTIES COMPILE_OPTIONS "-fobjc-arc;-fobjc-weak")
endif()

function(ADD_SHELL_SESSION_WITH_SRCS target srcs libs)
  set(XIBFILE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../resources/mac/Base.lproj/MainMenu.xib)
  file(GLOB SHELL_SESSION_SRC_FILES LIST_DIRECTORIES false ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.mm
                                                           ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.m)
  file(GLOB SHELL_SESSION_HEADER_FILES LIST_DIRECTORIES false ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/*.h)
  file(GLOB RESOURCES LIST_DIRECTORIES false ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../resources/images/*.png)

  add_executable(${target} MACOSX_BUNDLE ${srcs} "${SHELL_SESSION_SRC_FILES}" "${SHELL_SESSION_SHELL_HEADER_FILES}"
                                         "${XIBFILE}" "${RESOURCES}")
  igl_set_folder(${target} "IGL Shell Sessions")
  igl_set_cxxstd(${target} 17)
  target_compile_definitions(${target} PRIVATE "IGL_SHELL_SESSION=${target}")
  target_link_libraries(${target} PUBLIC ${libs})
  target_link_libraries(${target} PUBLIC IGLShellShared)
  target_link_libraries(${target} PUBLIC IGLShellPlatform)
  if(CMAKE_GENERATOR STREQUAL "Xcode")
    set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_WEAK YES)
    set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
  else()
    set_source_files_properties(${SHELL_SESSION_SRC_FILES} PROPERTIES COMPILE_OPTIONS "-fobjc-arc;-fobjc-weak")

    # See https://catfox.life/2022/08/14/compiling-xibs-with-cmake-without-xcode/
    find_program(IBTOOL ibtool REQUIRED)
    get_filename_component(XIBFILENAME ${XIBFILE} NAME_WE)
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${IBTOOL} --compile ${CMAKE_CURRENT_BINARY_DIR}/${target}.app/Contents/Resources/${XIBFILENAME}.nib ${XIBFILE}
            COMMENT "Compiling NIB file ${XIBFILE}.nib")
  endif()
  target_link_libraries(
    ${target} PUBLIC "-framework AVFoundation" "-framework CoreGraphics" "-framework CoreMotion" "-framework AppKit"
                     "-framework Metal" "-framework MetalKit" "-framework OpenGL" "-framework QuartzCore")
  set_target_properties(${target} PROPERTIES MACOSX_BUNDLE TRUE RESOURCE "${XIBFILE}")
  set_source_files_properties(${RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
endfunction()
