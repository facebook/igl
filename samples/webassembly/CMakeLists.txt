# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "IGL Samples")

if(MSVC)
  add_definitions(-D_CONSOLE)
endif()

macro(ADD_DEMO app)
  add_executable(${app} "Triangle/${app}.cpp")
  igl_set_cxxstd(${app} 20)
  igl_set_folder(${app} ${PROJECT_NAME})
  target_link_libraries(${app} PUBLIC IGLLibrary)

  #   if(UNIX)
  #     target_link_libraries(${app} PRIVATE EGL)
  #   endif()
  target_link_libraries(${app} PRIVATE IGLstb)

  set(USE_FLAGS
      "
    -s USE_FREETYPE=1
    -s DISABLE_EXCEPTION_CATCHING=0
    -s DEMANGLE_SUPPORT=1
    -s SAFE_HEAP=1
    --bind
    -s WASM=1
    -O2
    -s LEGACY_GL_EMULATION=0
    -s GL_UNSAFE_OPTS=0
    -s USE_WEBGL2=1
    -s FULL_ES3=1
    -s USE_GLFW=3
    -s OFFSCREENCANVAS_SUPPORT=1")
  set_target_properties(${app} PROPERTIES LINK_FLAGS ${USE_FLAGS} SUFFIX .html)
endmacro()

add_demo("Triangle")
