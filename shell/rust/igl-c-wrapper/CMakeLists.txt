# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19)

project(IGLCWrapper)

add_library(igl_c_wrapper STATIC
    src/igl_c_wrapper.mm
    src/igl_graphics.mm
    src/igl_platform_helper.mm
    include/igl_c_wrapper.h
    include/igl_graphics.h
    include/igl_platform_helper.h
    ${CMAKE_SOURCE_DIR}/shell/renderSessions/ThreeCubesRenderSession.cpp
    ${CMAKE_SOURCE_DIR}/shell/renderSessions/ThreeCubesRenderSession.h
)

target_include_directories(igl_c_wrapper
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(igl_c_wrapper
    PUBLIC
        IGLLibrary
        IGLShellShared
        IGLShellPlatform
)

# Enable ARC for Objective-C++
if(CMAKE_GENERATOR STREQUAL "Xcode")
    set_target_properties(igl_c_wrapper PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
else()
    set_source_files_properties(src/igl_c_wrapper.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()

igl_set_folder(igl_c_wrapper "IGL")
igl_set_cxxstd(igl_c_wrapper 20)

# Also create a shared library version for C# interop
add_library(igl_c_wrapper_shared SHARED
    src/igl_c_wrapper.mm
    src/igl_graphics.mm
    src/igl_platform_helper.mm
    include/igl_c_wrapper.h
    include/igl_graphics.h
    include/igl_platform_helper.h
    ${CMAKE_SOURCE_DIR}/shell/renderSessions/ThreeCubesRenderSession.cpp
    ${CMAKE_SOURCE_DIR}/shell/renderSessions/ThreeCubesRenderSession.h
)

target_include_directories(igl_c_wrapper_shared
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(igl_c_wrapper_shared
    PUBLIC
        IGLLibrary
        IGLShellShared
        IGLShellPlatform
)

if(CMAKE_GENERATOR STREQUAL "Xcode")
    set_target_properties(igl_c_wrapper_shared PROPERTIES XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
else()
    set_source_files_properties(src/igl_c_wrapper.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()

set_target_properties(igl_c_wrapper_shared PROPERTIES OUTPUT_NAME "igl_c_wrapper")
igl_set_folder(igl_c_wrapper_shared "IGL")
igl_set_cxxstd(igl_c_wrapper_shared 20)
