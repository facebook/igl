# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19)

set(PROJECT_NAME "Android")

file(GLOB PLATFORM_SHARED_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../shared/imageLoader/android/*.cpp ../shared/imageWriter/android/*.cpp ../shared/imageWriter/stb/*.cpp ../shared/fileLoader/android/*.cpp
     ../shared/platform/android/*.cpp)
file(GLOB PLATFORM_SHARED_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ../shared/imageLoader/android/*.h ../shared/imageWriter/android/*.h ../shared/fileLoader/android/*.h
     ../shared/platform/android/*.h)

add_library(IGLShellPlatform ${PLATFORM_SHARED_SRC_FILES} ${PLATFORM_SHARED_HEADER_FILES})

target_link_libraries(IGLShellPlatform PUBLIC IGLLibrary)

target_include_directories(IGLShellPlatform PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src")

igl_set_folder(IGLShellPlatform "IGL")
igl_set_cxxstd(IGLShellPlatform 20)

find_library(log-lib log)
find_library(android-lib android)

function(ADD_SHELL_SESSION_WITH_SRCS target srcs libs)
  file(GLOB PLATFORM_SHELL_SRC_FILES LIST_DIRECTORIES false ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/jni/*.cpp)

  add_library(${target} SHARED ${srcs} "${PLATFORM_SHELL_SRC_FILES}")
  target_compile_definitions(${target} PRIVATE "IGL_SHELL_SESSION=${target}")

  target_link_libraries(${target} PUBLIC ${libs})
  target_link_libraries(${target} PUBLIC IGLShellShared)
  target_link_libraries(${target} PUBLIC IGLShellPlatform)
  target_link_libraries(${target} PUBLIC ${log-lib})
  target_link_libraries(${target} PUBLIC ${android-lib})
endfunction()
