# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "IGL Samples")

macro(ADD_DEMO app shellHTML)
  add_executable(${app} "${app}.cpp")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  igl_set_cxxstd(${app} 17)
  igl_set_folder(${app} ${PROJECT_NAME})
  target_link_libraries(${app} PUBLIC IGLLibrary)
  set_target_properties(
    ${app}
    PROPERTIES
      LINK_FLAGS
      "-s USE_WEBGL2=1 -s GL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=1 -s GL_EMULATE_GLES_VERSION_STRING_FORMAT=1 -s ALLOW_MEMORY_GROWTH=1 -s SINGLE_FILE=1 -s LLD_REPORT_UNDEFINED --shell-file ${shellHTML}"
  )

endmacro()

add_demo("tiny" "../samples/wasm/igl.html")
