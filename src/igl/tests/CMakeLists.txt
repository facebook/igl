# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

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
  list(APPEND SRC_FILES util/device/metal/TestDevice.mm)
  list(APPEND HEADER_FILES util/device/metal/TestDevice.h)
endif()

if(IGL_WITH_IGLU)
  file(GLOB IGLU_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} iglu/*.cpp)
  list(APPEND SRC_FILES ${IGLU_SRC_FILES})
endif()

enable_testing()

add_executable(IGLTests ${SRC_FILES} ${HEADER_FILES})

if(WIN32)
  target_compile_definitions(IGLTests PRIVATE -DNOMINMAX)
  target_compile_definitions(IGLTests PRIVATE -DIGL_UNIT_TESTS_GLES_VERSION="3.0")
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

# gtest
# cmake-format: off
set(BUILD_GMOCK    OFF CACHE BOOL "")
set(INSTALL_GTEST  OFF CACHE BOOL "")
set(GTEST_HAS_ABSL OFF CACHE BOOL "")
# cmake-format: on
add_subdirectory(${IGL_ROOT_DIR}/third-party/deps/src/gtest "gtest")

igl_set_folder(gtest "third-party")
igl_set_folder(gtest_main "third-party")

target_link_libraries(IGLTests PUBLIC IGLLibrary)
target_link_libraries(IGLTests PUBLIC gtest)
target_link_libraries(IGLTests PUBLIC gtest_main)
target_link_libraries(IGLTests PUBLIC IGLUmanagedUniformBuffer)
target_link_libraries(IGLTests PUBLIC IGLUshaderCross)
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

if(IGL_WITH_VULKAN)
  target_compile_definitions(IGLTests PUBLIC -DIGL_BACKEND_TYPE="vulkan")
elseif(IGL_WITH_OPENGL OR IGL_WITH_OPENGLES)
  target_compile_definitions(IGLTests PUBLIC -DIGL_BACKEND_TYPE="ogl")
endif()

if(UNIX)
  if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(IGLTests PUBLIC "-Wno-volatile")
  endif()
endif()
