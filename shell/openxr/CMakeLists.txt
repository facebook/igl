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
     ../openxr/desktop/*.cpp
     ../openxr/desktop/vulkan/*.cpp
     ../openxr/impl/*.cpp
     ../openxr/src/*.cpp)
file(GLOB SHELL_OPENXR_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../openxr/desktop/*.h
     ../openxr/desktop/vulkan/*.h
     ../openxr/impl/*.h
     ../openxr/src/*.h)

add_library(IGLShellOpenXR ${SHELL_OPENXR_SRC_FILES} ${SHELL_OPENXR_HEADER_FILES})

target_link_libraries(IGLShellOpenXR PUBLIC IGLLibrary)
target_link_libraries(IGLShellOpenXR PUBLIC IGLLibrary)

target_include_directories(IGLShellOpenXR PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/openxr-sdk/src/common")
target_include_directories(IGLShellOpenXR PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include_shell_openxr")

igl_set_folder(IGLShellOpenXR "IGL")
igl_set_cxxstd(IGLShellOpenXR 20)
