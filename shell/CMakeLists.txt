# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "Shell")

if(WIN32)
  add_definitions("-DNOMINMAX")
  add_definitions("-D_USE_MATH_DEFINES=1")
endif()

file(GLOB SHELL_SHARED_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     shared/fileLoader/*.cpp shared/extension/*.cpp shared/input/*.cpp shared/platform/*.cpp shared/renderSession/*.cpp shared/netservice/*.cpp)
file(GLOB SHELL_SHARED_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     shared/fileLoader/*.h shared/extension/*.h shared/input/*.h shared/platform/*.h shared/renderSession/*.h shared/netservice/*.h)

add_library(IGLShellShared ${SHELL_SHARED_SRC_FILES} ${SHELL_SHARED_HEADER_FILES})

target_include_directories(IGLShellShared PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include_renderSessions")
target_include_directories(IGLShellShared PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include_shared")

target_link_libraries(IGLShellShared PUBLIC IGLLibrary)
target_link_libraries(IGLShellShared PUBLIC IGLUimgui)
target_link_libraries(IGLShellShared PUBLIC IGLUmanagedUniformBuffer)
target_link_libraries(IGLShellShared PUBLIC IGLUsimdtypes)
target_link_libraries(IGLShellShared PUBLIC IGLUsimple_renderer)
target_link_libraries(IGLShellShared PUBLIC IGLUtexture_accessor)
target_link_libraries(IGLShellShared PUBLIC IGLUuniform)
target_link_libraries(IGLShellShared PUBLIC IGLstb)

igl_set_folder(IGLShellShared "IGL")
igl_set_cxxstd(IGLShellShared 17)

if(WIN32 OR UNIX AND NOT APPLE AND NOT ANDROID)
  add_subdirectory(windows)
endif()

if(APPLE)
  if(IOS)
    add_subdirectory(ios)
  else()
    add_subdirectory(mac)
  endif()
endif()

if(ANDROID)
  set(android_jni)
  add_subdirectory(android)
endif()

macro(ADD_SHELL_SESSION target libs)
  set(shell_srcs apps/SessionApp.cpp renderSessions/${target}.cpp renderSessions/${target}.h)
  add_shell_session_with_srcs(${target} "${shell_srcs}" "${libs}")
endmacro()

if(IGL_WITH_SAMPLES)
  add_shell_session(BasicFramebufferSession "")
  add_shell_session(ColorSession "")
  add_shell_session(EmptySession "")
  add_shell_session(HelloWorldSession "")
  add_shell_session(ImguiSession "")
  add_shell_session(MRTSession "")
  add_shell_session(Textured3DCubeSession "")
  add_shell_session(TQMultiRenderPassSession "")
  add_shell_session(TQSession "")
endif()
