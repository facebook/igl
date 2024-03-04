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
     shared/fileLoader/*.cpp shared/imageLoader/*.cpp shared/extension/*.cpp shared/input/*.cpp shared/platform/*.cpp shared/renderSession/*.cpp shared/netservice/*.cpp)
file(GLOB SHELL_SHARED_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     shared/fileLoader/*.h shared/imageLoader/*.h shared/extension/*.h shared/input/*.h shared/platform/*.h shared/renderSession/*.h shared/netservice/*.h)

add_library(IGLShellShared ${SHELL_SHARED_SRC_FILES} ${SHELL_SHARED_HEADER_FILES})

target_include_directories(IGLShellShared PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include_renderSessions")
target_include_directories(IGLShellShared PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include_shared")

target_link_libraries(IGLShellShared PUBLIC IGLLibrary)
target_link_libraries(IGLShellShared PUBLIC IGLUimgui)
target_link_libraries(IGLShellShared PUBLIC IGLUmanagedUniformBuffer)
target_link_libraries(IGLShellShared PUBLIC IGLUsimdtypes)
target_link_libraries(IGLShellShared PUBLIC IGLUsimple_renderer)
target_link_libraries(IGLShellShared PUBLIC IGLUtexture_accessor)
target_link_libraries(IGLShellShared PUBLIC IGLUtexture_loader)
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

if(IGL_WITH_OPENXR)
  add_subdirectory(openxr)
  target_link_libraries(IGLShellShared PUBLIC IGLShellOpenXR)
endif()

macro(ADD_SHELL_SESSION target libs)
  set(shell_srcs apps/SessionApp.cpp renderSessions/${target}.cpp renderSessions/${target}.h)
  add_shell_session_with_srcs(${target} "${shell_srcs}" "${libs}")
endmacro()

macro(ADD_SHELL_SESSION_OPENXR target)
  set(shell_srcs apps/SessionApp.cpp renderSessions/${target}.cpp renderSessions/${target}.h)
  if(NOT ANDROID)
    list(APPEND shell_srcs openxr/desktop/main.cpp)
  endif()
  add_shell_session_backend_openxr(${target} vulkan "${shell_srcs}" IGLShellOpenXR)
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
  if(IGL_WITH_OPENXR AND IGL_WITH_VULKAN)
    add_shell_session_OpenXR(ColorSession "")
    add_shell_session_OpenXR(Textured3DCubeSession)
  endif()
endif()

if(IGL_WITH_OPENXR AND ANDROID)
  set(IGL_OPENXR_RENDER_SESSION CACHE STRING "Textured3DCubeSession")
  if(IGL_WITH_VULKAN)
    if(DEFINED ENV{ANDROID_NDK})
      cmake_path(SET NDK_PATH $ENV{ANDROID_NDK})
    elseif(DEFINED ENV{NDK_ROOT})
      cmake_path(SET NDK_PATH $ENV{NDK_ROOT})
    else()
      message(FATAL_ERROR "Android NDK not found, check environment variables ANDROID_NDK and NDK_ROOT")
    endif()
    add_library(openxr-vulkan-Jni SHARED
      openxr/mobile/XrApp.cpp
      openxr/mobile/AndroidMain.cpp
      "${NDK_PATH}/sources/android/native_app_glue/android_native_app_glue.c")
    target_include_directories(openxr-vulkan-Jni PRIVATE "${NDK_PATH}/sources/android/native_app_glue")
    target_link_libraries(openxr-vulkan-Jni PUBLIC IGLShellShared IGLShellOpenXR ${IGL_OPENXR_RENDER_SESSION})
    target_compile_definitions(openxr-vulkan-Jni PUBLIC "USE_VULKAN_BACKEND" "XR_USE_PLATFORM_ANDROID")
  endif()
endif()
