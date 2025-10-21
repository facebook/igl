# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19)

project(IGLD3D12 CXX C)

file(GLOB SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)
file(GLOB HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

add_library(IGLD3D12 ${SRC_FILES} ${HEADER_FILES})

target_link_libraries(IGLD3D12 PRIVATE IGLLibrary IGLGlslang)

igl_set_cxxstd(IGLD3D12 20)
igl_set_folder(IGLD3D12 "IGL")

# Link DirectX 12 system libraries
target_link_libraries(IGLD3D12 PUBLIC
  d3d12.lib
  dxgi.lib
  dxguid.lib
  dxcompiler.lib
)

# Include d3dx12 helper headers
target_include_directories(IGLD3D12 PUBLIC "${IGL_ROOT_DIR}/third-party/d3d12/include")

# Include SPIRV-Cross for potential SPIR-V to HLSL conversion
target_include_directories(IGLD3D12 PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/SPIRV-Cross")
target_include_directories(IGLD3D12 PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/glslang")

# Windows-specific definitions
if(WIN32)
  add_definitions("-DNOMINMAX")
  add_definitions("-DWIN32_LEAN_AND_MEAN")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  # Enable conformant preprocessor and multithreaded compilation
  target_compile_options(IGLD3D12 PRIVATE "/Zc:preprocessor" "/MP")
endif()
