# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

project(IGLLibrary CXX C)

file(GLOB SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)
file(GLOB HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

add_library(IGLLibrary ${SRC_FILES} ${HEADER_FILES})

target_include_directories(IGLLibrary PUBLIC "${IGL_ROOT_DIR}/src")

igl_set_cxxstd(IGLLibrary 20)
igl_set_folder(IGLLibrary "IGL")

add_subdirectory(vulkan)

target_link_libraries(IGLLibrary PUBLIC IGLVulkan)
target_link_libraries(IGLLibrary PRIVATE LVKLibrary)
