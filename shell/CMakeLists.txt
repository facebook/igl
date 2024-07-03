# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19)

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

target_link_libraries(IGLShellShared PUBLIC fmt)
target_link_libraries(IGLShellShared PUBLIC IGLLibrary)
target_link_libraries(IGLShellShared PUBLIC IGLUimgui)
target_link_libraries(IGLShellShared PUBLIC IGLUmanagedUniformBuffer)
target_link_libraries(IGLShellShared PUBLIC IGLUsimdtypes)
target_link_libraries(IGLShellShared PUBLIC IGLUsimple_renderer)
target_link_libraries(IGLShellShared PUBLIC IGLUshaderCross)
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
endif()

macro(ADD_SHELL_SESSION target libs)
  set(shell_srcs apps/SessionApp.cpp renderSessions/${target}.cpp renderSessions/${target}.h)
  add_shell_session_with_srcs(${target} "${shell_srcs}" "${libs}")
endmacro()

macro(ADD_SHELL_SESSION_OPENXR_SIM target libs)
  set(shell_srcs apps/SessionApp.cpp renderSessions/${target}.cpp renderSessions/${target}.h)
  if(WIN32)
    if(IGL_WITH_VULKAN)
      set(compile_defs "USE_VULKAN_BACKEND" "XR_USE_PLATFORM_WIN32" "XR_USE_GRAPHICS_API_VULKAN")
      add_shell_session_backend_openxr_sim(${target} vulkan "${shell_srcs}" "${libs}" "${compile_defs}")
    endif()
    if(IGL_WITH_OPENGL)
      set(compile_defs "USE_OPENGL_BACKEND" "XR_USE_PLATFORM_WIN32" "XR_USE_GRAPHICS_API_OPENGL")
      add_shell_session_backend_openxr_sim(${target} opengl "${shell_srcs}" "${libs}" "${compile_defs}")
    endif()
    if(IGL_WITH_OPENGLES)
      message(FATAL_ERROR "OpenGL ES for Windows OpenXR is not supported")
    endif()
  endif()
endmacro()

if(IGL_WITH_SAMPLES)
  add_shell_session(BasicFramebufferSession "")
  add_shell_session(ColorSession "")
  add_shell_session(EmptySession "")
  add_shell_session(GPUStressSession "")
  add_shell_session(HelloWorldSession "")
  add_shell_session(DrawInstancedSession "")
  add_shell_session(ImguiSession "")
  add_shell_session(MRTSession "")
  add_shell_session(Textured3DCubeSession "")
  add_shell_session(TQMultiRenderPassSession "")
  add_shell_session(TQSession "")
  add_shell_session(YUVColorSession "")
  if(IGL_WITH_OPENXR)
    if(ANDROID)
      add_shell_session(HelloOpenXRSession "")
      add_shell_session(HandsOpenXRSession "")
    endif()
    if(WIN32)
      add_shell_session_openxr_sim(HelloOpenXRSession "")
    endif()
  endif()
endif()

if(IGL_WITH_OPENXR AND ANDROID)
  set(IGL_OPENXR_RENDER_SESSION CACHE STRING "Textured3DCubeSession")
  if(DEFINED ENV{ANDROID_NDK})
    cmake_path(SET NDK_PATH $ENV{ANDROID_NDK})
  elseif(DEFINED ENV{NDK_ROOT})
    cmake_path(SET NDK_PATH $ENV{NDK_ROOT})
  else()
    message(FATAL_ERROR "Android NDK not found, check environment variables ANDROID_NDK and NDK_ROOT")
  endif()
  if(IGL_WITH_VULKAN)
    add_library(openxr-vulkan-Jni SHARED
      openxr/mobile/XrApp.cpp
      openxr/mobile/AndroidMain.cpp
      "${NDK_PATH}/sources/android/native_app_glue/android_native_app_glue.c")
    target_include_directories(openxr-vulkan-Jni PRIVATE "${NDK_PATH}/sources/android/native_app_glue")
    target_link_libraries(openxr-vulkan-Jni PRIVATE IGLShellShared IGLShellOpenXR_AndroidVulkan ${IGL_OPENXR_RENDER_SESSION})
    target_compile_definitions(openxr-vulkan-Jni PRIVATE "USE_VULKAN_BACKEND" "XR_USE_PLATFORM_ANDROID" "XR_USE_GRAPHICS_API_VULKAN")
  endif()
  if(IGL_WITH_OPENGLES)
    add_library(openxr-gles-Jni SHARED
      openxr/mobile/XrApp.cpp
      openxr/mobile/AndroidMain.cpp
      "${NDK_PATH}/sources/android/native_app_glue/android_native_app_glue.c")
    target_include_directories(openxr-gles-Jni PRIVATE "${NDK_PATH}/sources/android/native_app_glue")
    target_link_libraries(openxr-gles-Jni PRIVATE IGLShellShared IGLShellOpenXR_AndroidOpenGLES ${IGL_OPENXR_RENDER_SESSION})
    target_compile_definitions(openxr-gles-Jni PRIVATE "USE_OPENGL_BACKEND" "XR_USE_PLATFORM_ANDROID" "XR_USE_GRAPHICS_API_OPENGL_ES")
  endif()
endif()
