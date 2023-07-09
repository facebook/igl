# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "IGL Samples")

macro(ADD_DEMO app)
  add_executable(${app} "Triangle/${app}.cpp")
  igl_set_cxxstd(${app} 20)
  igl_set_folder(${app} ${PROJECT_NAME})
  target_link_libraries(${app} PUBLIC IGLLibrary)
  target_link_libraries(${app} PRIVATE IGLstb)

  set_target_properties(${app} PROPERTIES LINK_FLAGS "-s WASM=1 -s USE_WEBGL2=1 -s USE_GLFW=3" SUFFIX .html)
endmacro()

add_demo("Triangle")
