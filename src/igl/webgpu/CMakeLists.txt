# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19)

project(IGLWebGPU CXX C)

file(GLOB SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)
file(GLOB HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

add_library(IGLWebGPU ${SRC_FILES} ${HEADER_FILES})

target_link_libraries(IGLWebGPU PRIVATE IGLLibrary)

igl_set_cxxstd(IGLWebGPU 20)
igl_set_folder(IGLWebGPU "IGL")

if(EMSCRIPTEN)
  # The browser's WebGPU through emdawnwebgpu (Emscripten >= 4.0.10); wasm exceptions are required.
  target_compile_options(IGLWebGPU PUBLIC "--use-port=emdawnwebgpu" "-fwasm-exceptions")
  target_link_options(IGLWebGPU PUBLIC "--use-port=emdawnwebgpu" "-fwasm-exceptions" "--js-library"
                      "${CMAKE_CURRENT_SOURCE_DIR}/emscripten/library_iglwebgpu.js")
else()
  target_link_libraries(IGLWebGPU PUBLIC ${IGL_DAWN_TARGET})
endif()
