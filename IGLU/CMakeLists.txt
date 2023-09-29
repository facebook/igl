# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "IGLU")

macro(ADD_IGLU_MODULE module)
  file(GLOB_RECURSE SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${module}/*.cpp)
  file(GLOB_RECURSE HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${module}/*.h)
  add_library(IGLU${module} ${SRC_FILES} ${HEADER_FILES})
  igl_set_cxxstd(IGLU${module} 17)
  igl_set_folder(IGLU${module} "IGL/${PROJECT_NAME}")
  target_link_libraries(IGLU${module} PRIVATE IGLLibrary)
  target_include_directories(IGLU${module} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include")
  target_include_directories(IGLU${module} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include_${module}")
  target_include_directories(IGLU${module} PUBLIC "${IGL_ROOT_DIR}")
endmacro()

add_iglu_module(imgui)
add_iglu_module(managedUniformBuffer)
add_iglu_module(sentinel)
add_iglu_module(simple_renderer)
add_iglu_module(state_pool)
add_iglu_module(texture_accessor)
add_iglu_module(texture_loader)
add_iglu_module(uniform)

# header-only
add_library(IGLUsimdtypes INTERFACE)
target_include_directories(IGLUsimdtypes INTERFACE "simdtypes")

target_link_libraries(IGLUtexture_loader PRIVATE IGLstb)
target_link_libraries(IGLUtexture_loader PRIVATE ktx)

if(IGL_WITH_SHELL)
  target_link_libraries(IGLUimgui PRIVATE IGLShellShared)
else()
  target_sources(IGLUimgui PRIVATE "${IGL_ROOT_DIR}/shell/shared/input/InputDispatcher.cpp")
endif()

# ImGui
target_sources(IGLUimgui PRIVATE "${IGL_ROOT_DIR}/third-party/deps/src/imgui/imgui.cpp")
target_sources(IGLUimgui PRIVATE "${IGL_ROOT_DIR}/third-party/deps/src/imgui/imgui_demo.cpp")
target_sources(IGLUimgui PRIVATE "${IGL_ROOT_DIR}/third-party/deps/src/imgui/imgui_draw.cpp")
target_sources(IGLUimgui PRIVATE "${IGL_ROOT_DIR}/third-party/deps/src/imgui/imgui_tables.cpp")
target_sources(IGLUimgui PRIVATE "${IGL_ROOT_DIR}/third-party/deps/src/imgui/imgui_widgets.cpp")
target_include_directories(IGLUimgui PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/imgui")
target_include_directories(IGLUtexture_accessor PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/glew/include")

if(WIN32)
  target_include_directories(IGLUtexture_accessor PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/glew/include")
endif()
