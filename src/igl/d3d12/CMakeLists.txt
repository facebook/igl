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

# Windows-specific definitions (removed - defined in D3D12Headers.h to avoid conflicts)

if(WIN32 AND MSVC)
  # Enable multithreaded compilation
  target_compile_options(IGLD3D12 PRIVATE "/MP")
  # Disable NOMINMAX warning (already defined in D3D12Headers.h)
  target_compile_options(IGLD3D12 PRIVATE "/wd4005")
  # Note: /Zc:preprocessor (conformant preprocessor) causes issues with
  # some d3dx12 headers and WRL, so we're using the traditional preprocessor

  # Copy dxil.dll to output directories for DXIL signing support
  # This DLL is required by DXC validator to sign DXIL bytecode
  find_file(DXIL_DLL
    NAMES dxil.dll
    PATHS
      "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
      "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22000.0/x64"
      "C:/Program Files (x86)/Windows Kits/10/bin/10.0.19041.0/x64"
      "$ENV{WindowsSdkBinPath}/x64"
  )

  if(DXIL_DLL)
    message(STATUS "Found dxil.dll: ${DXIL_DLL}")
    # Copy to test and shell output directories
    add_custom_target(copy_dxil_dll ALL
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DXIL_DLL}" "$<TARGET_FILE_DIR:IGLD3D12>/"
      COMMENT "Copying dxil.dll for DXIL signing support"
    )
    add_dependencies(IGLD3D12 copy_dxil_dll)
  else()
    message(WARNING "dxil.dll not found - DXIL shaders will be unsigned")
  endif()
endif()
