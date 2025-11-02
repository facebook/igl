# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19)

project(IGLTests CXX C)

file(GLOB SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp util/*.cpp util/device/*.cpp)
file(GLOB HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h util/*.h util/device/*.h)

if(IGL_WITH_VULKAN)
  file(GLOB VULKAN_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} vulkan/*.cpp)
  list(APPEND SRC_FILES ${VULKAN_SRC_FILES})
  list(APPEND SRC_FILES util/device/vulkan/TestDevice.cpp)
  list(APPEND HEADER_FILES util/device/vulkan/TestDevice.h)
  if(MACOSX)
    list(APPEND SRC_FILES util/device/vulkan/TestDeviceXCTestHelper.mm)
    list(APPEND HEADER_FILES util/device/vulkan/TestDeviceXCTestHelper.h)
  endif()
endif()

if(IGL_WITH_OPENGL OR IGL_WITH_OPENGLES)
  file(GLOB OPENGL_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ogl/*.cpp)
  list(APPEND SRC_FILES ${OPENGL_SRC_FILES})
  list(APPEND SRC_FILES util/device/opengl/TestDevice.cpp)
  list(APPEND HEADER_FILES util/device/opengl/TestDevice.h)
endif()

if(IGL_WITH_METAL)
  file(GLOB METAL_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} metal/*.cpp metal/*.mm)
  list(APPEND SRC_FILES ${METAL_SRC_FILES})
  list(APPEND SRC_FILES util/device/MetalTestDevice.mm)
  list(APPEND SRC_FILES util/device/metal/TestDevice.mm)
  list(APPEND HEADER_FILES util/device/MetalTestDevice.h)
  list(APPEND HEADER_FILES util/device/metal/TestDevice.h)
endif()

if(IGL_WITH_D3D12)
  file(GLOB D3D12_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} d3d12/*.cpp)
  list(APPEND SRC_FILES ${D3D12_SRC_FILES})
  list(APPEND SRC_FILES util/device/d3d12/TestDevice.cpp)
  list(APPEND HEADER_FILES util/device/d3d12/TestDevice.h)
endif()

if(IGL_WITH_IGLU)
  file(GLOB IGLU_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} iglu/*.cpp)
  file(GLOB IGLU_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} iglu/texture_loader/*.cpp)
  if((NOT IGL_WITH_OPENGL) AND (NOT IGL_WITH_OPENGLES))
    list(REMOVE_ITEM IGLU_SRC_FILES iglu/texture_loader/Ktx1TextureLoaderTest.cpp)
  endif()
  if(NOT IGL_WITH_VULKAN)
    list(REMOVE_ITEM IGLU_SRC_FILES iglu/texture_loader/Ktx2TextureLoaderTest.cpp)
  endif()
  list(APPEND SRC_FILES ${IGLU_SRC_FILES})
endif()

enable_testing()

# Add custom main to initialize COM and install signal handlers before gtest
list(APPEND SRC_FILES main.cpp)
add_executable(IGLTests ${SRC_FILES} ${HEADER_FILES})

if(WIN32)
  if(MSVC)
    target_compile_options(IGLTests PRIVATE /bigobj)
  endif()

  target_compile_definitions(IGLTests PRIVATE -DNOMINMAX)
  target_compile_definitions(IGLTests PRIVATE -DIGL_UNIT_TESTS_GLES_VERSION=3)
  # Ensure Unicode matches across GoogleTest and our test binary on Windows
  # Ensure tests are built without UNICODE to match GoogleTest defaults and avoid CRT mismatches
  if(MSVC)
    target_compile_options(IGLTests PRIVATE /UUNICODE /U_UNICODE)
    if(TARGET gtest)
      target_compile_options(gtest PRIVATE /UUNICODE /U_UNICODE)
    endif()
    if(TARGET gtest_main)
      target_compile_options(gtest_main PRIVATE /UUNICODE /U_UNICODE)
    endif()
    if(TARGET gmock)
      target_compile_options(gmock PRIVATE /UUNICODE /U_UNICODE)
    endif()
    if(TARGET gmock_main)
      target_compile_options(gmock_main PRIVATE /UUNICODE /U_UNICODE)
    endif()
  endif()
  target_include_directories(IGLTests PRIVATE "${IGL_ROOT_DIR}/third-party/deps/src/glew/include")
  # IGL tests use EGL, here's a stub for Windows
  add_library(
    EGL ${IGL_ROOT_DIR}/third-party/deps/src/EGL/EGL/src/egl.c ${IGL_ROOT_DIR}/third-party/deps/src/EGL/EGL/src/egl_common.cpp
        ${IGL_ROOT_DIR}/third-party/deps/src/EGL/EGL/src/egl_windows.cpp)
  target_include_directories(EGL PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/EGL/EGL/include")
  target_compile_definitions(EGL PUBLIC "-DEGL_NO_CONFIG_KHR=((EGLConfig)0)")
  target_compile_definitions(EGL PUBLIC -DEGL_NO_GLEW)
  target_compile_definitions(EGL PUBLIC __SCITECH_SNAP__)
  igl_set_cxxstd(EGL 17)
  igl_set_folder(EGL "third-party")
  target_link_libraries(IGLTests PUBLIC EGL)
elseif(UNIX AND NOT APPLE AND NOT ANDROID)
  target_link_libraries(IGLTests PUBLIC EGL)
endif()

igl_set_cxxstd(IGLTests 20)
igl_set_folder(IGLTests "IGL")

# gtest - FORCE static linkage to avoid DLL initialization issues
# NOTE: gmock is NOT needed - no tests use it
# cmake-format: off
set(BUILD_SHARED_LIBS      OFF CACHE BOOL "Build shared libraries" FORCE)
set(BUILD_GMOCK            OFF CACHE BOOL "Build gmock" FORCE)
set(INSTALL_GTEST          OFF CACHE BOOL "Install gtest" FORCE)
set(GTEST_HAS_ABSL         OFF CACHE BOOL "Use Abseil" FORCE)
set(gtest_force_shared_crt OFF CACHE BOOL "Force shared CRT" FORCE)
# Explicitly disable shared library build
set(BUILD_SHARED_LIBS OFF)
# CRITICAL: Set these compile definitions globally BEFORE including gtest
add_compile_definitions(GTEST_LINKED_AS_SHARED_LIBRARY=0 GTEST_CREATE_SHARED_LIBRARY=0)
# cmake-format: on
add_subdirectory(${IGL_ROOT_DIR}/third-party/deps/src/gtest "gtest" EXCLUDE_FROM_ALL)

# CRITICAL: Force static linking by removing any DLL export definitions
if(TARGET gtest)
  target_compile_definitions(gtest PRIVATE GTEST_CREATE_SHARED_LIBRARY=0)
  target_compile_definitions(gtest PUBLIC GTEST_LINKED_AS_SHARED_LIBRARY=0)
endif()

igl_set_folder(gtest "third-party")
igl_set_folder(gtest_main "third-party")
igl_set_cxxstd(gtest 17)
igl_set_cxxstd(gtest_main 17)

target_link_libraries(IGLTests PUBLIC IGLLibrary)
target_link_libraries(IGLTests PUBLIC gtest)
# Use our custom main.cpp instead of gtest_main
# NOTE: gmock is NOT linked - no tests use gmock, only gtest
target_link_libraries(IGLTests PUBLIC IGLUmanagedUniformBuffer)
target_link_libraries(IGLTests PUBLIC IGLUshaderCross)

# Enable experimental features ONLY for unit tests (NOT for render sessions)
# This allows D3D12 headless tests to use unsigned DXIL shaders
if(IGL_WITH_D3D12)
  target_compile_definitions(IGLTests PRIVATE IGL_ENABLE_EXPERIMENTAL_FEATURES_FOR_TESTS=1)
endif()
if(TARGET glfw)
  target_link_libraries(IGLTests PRIVATE glfw)
endif()

if(IGL_WITH_IGLU)
  target_link_libraries(IGLTests PUBLIC IGLUimgui)
  target_link_libraries(IGLTests PUBLIC IGLUsimple_renderer)
  target_link_libraries(IGLTests PUBLIC IGLUstate_pool)
  target_link_libraries(IGLTests PUBLIC IGLUtexture_accessor)
  target_link_libraries(IGLTests PUBLIC IGLUtexture_loader)
  target_link_libraries(IGLTests PUBLIC IGLUuniform)
endif()

if(IGL_WITH_D3D12)
  target_compile_definitions(IGLTests PUBLIC -DIGL_BACKEND_TYPE="d3d12")
elseif(IGL_WITH_VULKAN)
  target_compile_definitions(IGLTests PUBLIC -DIGL_BACKEND_TYPE="vulkan")
elseif(IGL_WITH_OPENGL OR IGL_WITH_OPENGLES)
  target_compile_definitions(IGLTests PUBLIC -DIGL_BACKEND_TYPE="ogl")
endif()

# Ensure gtest/gmock are linked statically
target_compile_definitions(IGLTests PRIVATE GTEST_LINKED_AS_SHARED_LIBRARY=0 GTEST_CREATE_SHARED_LIBRARY=0)

if(UNIX)
  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(IGLTests PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wno-volatile>)
  endif()
endif()

# Register tests with CTest so they can be discovered
include(GoogleTest)
gtest_discover_tests(IGLTests
  DISCOVERY_TIMEOUT 60
  DISCOVERY_MODE PRE_TEST
)

# Fallback: also register the test executable directly for manual runs
if (WIN32)
  add_test(NAME IGLTests COMMAND ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/IGLTests.exe)
else()
  add_test(NAME IGLTests COMMAND ${CMAKE_CURRENT_BINARY_DIR}/IGLTests)
endif()
