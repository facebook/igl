# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19)

set(PROJECT_NAME "Desktop")

add_definitions("-DNOMINMAX")
add_definitions("-D_USE_MATH_DEFINES=1")

file(GLOB PLATFORM_SHARED_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../shared/fileLoader/win/*.cpp ../shared/imageWriter/win/*.cpp
     ../shared/imageWriter/stb/*.cpp ../shared/platform/win/*.cpp common/*.cpp)
file(GLOB PLATFORM_SHARED_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../shared/fileLoader/win/*.h ../shared/imageWriter/win/*.h ../shared/imageWriter/stb/*.h
     ../shared/platform/win/*.h common/*.h)

add_library(IGLShellPlatform ${PLATFORM_SHARED_SRC_FILES} ${PLATFORM_SHARED_HEADER_FILES})
target_link_libraries(IGLShellPlatform PUBLIC IGLLibrary)
target_link_libraries(IGLShellPlatform PUBLIC glfw)
target_link_libraries(IGLShellPlatform PUBLIC IGLShellShared)

igl_set_folder(IGLShellPlatform "IGL")
igl_set_cxxstd(IGLShellPlatform 20)

function(ADD_SHELL_APP backend)
  add_library(IGLShellApp_${backend} ${CMAKE_CURRENT_SOURCE_DIR}/../windows/${backend}/App.cpp)
  target_link_libraries(IGLShellApp_${backend} PUBLIC IGLShellPlatform)
  igl_set_folder(IGLShellApp_${backend} "IGL Shell App/${backend}")
  igl_set_cxxstd(IGLShellApp_${backend} 20)
endfunction()

if(IGL_WITH_VULKAN)
  add_shell_app(vulkan)
endif()
if(IGL_WITH_OPENGL)
  add_shell_app(opengl)
endif()
if(IGL_WITH_OPENGLES)
  add_shell_app(opengles)
endif()

function(ADD_SHELL_SESSION_BACKEND targetApp backend srcs libs)
  set(target ${targetApp}_${backend})
  add_executable(${target} ${srcs})
  igl_set_folder(${target} "IGL Shell Sessions/${backend}")
  igl_set_cxxstd(${target} 20)
  target_compile_definitions(${target} PRIVATE "IGL_SHELL_SESSION=${targetApp}")
  target_link_libraries(${target} PUBLIC ${libs})
  target_link_libraries(${target} PUBLIC IGLShellApp_${backend})
endfunction()

function(ADD_SHELL_SESSION_BACKEND_OPENXR_SIM targetApp backend srcs libs compileDefs)
  set(target ${targetApp}_${backend}_openxr_sim)
  add_executable(${target} ${srcs})
  igl_set_folder(${target} "IGL Shell Sessions/${backend}_openxr_sim")
  igl_set_cxxstd(${target} 20)
  target_compile_definitions(${target} PRIVATE "IGL_SHELL_SESSION=${targetApp}" ${compileDefs})
  target_include_directories(${target} PRIVATE "${IGL_ROOT_DIR}/third-party/deps/src/openxr-sdk/src/common")
  target_link_libraries(${target} PUBLIC IGLShellOpenXR_sim_${backend} IGLShellShared IGLShellPlatform Shcore.lib ${libs})
endfunction()

macro(ADD_SHELL_SESSION_WITH_SRCS target srcs libs)
  if(IGL_WITH_VULKAN)
    add_shell_session_backend(${target} vulkan "${srcs}" "${libs}")
  endif()
  if(IGL_WITH_OPENGL)
    add_shell_session_backend(${target} opengl "${srcs}" "${libs}")
  endif()
  if(IGL_WITH_OPENGLES)
    add_shell_session_backend(${target} opengles "${srcs}" "${libs}")
  endif()
endmacro()
