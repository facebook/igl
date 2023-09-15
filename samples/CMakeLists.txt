# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "LVK Samples")

if(MSVC)
  add_definitions(-D_CONSOLE)
else()
  add_compile_options(-Wno-deprecated-volatile)
endif()

if(WIN32)
  add_definitions("-DVK_USE_PLATFORM_WIN32_KHR=1")
  add_definitions("-DNOMINMAX")
endif()

macro(ADD_DEMO app)
  add_executable(${app} "${app}.cpp")
  lvk_set_cxxstd(${app} 20)
  lvk_set_folder(${app} ${PROJECT_NAME})
  target_link_libraries(${app} PRIVATE LVKLibrary)
  target_link_libraries(${app} PRIVATE bc7enc)
  target_link_libraries(${app} PRIVATE meshoptimizer)
  target_link_libraries(${app} PRIVATE tinyobjloader)
  target_link_libraries(${app} PRIVATE glfw)
  if(UNIX AND NOT APPLE)
    target_link_libraries(${app} PRIVATE EGL)
  endif()
  target_link_libraries(${app} PRIVATE LVKstb)
endmacro()

ADD_DEMO("HelloTriangle")
ADD_DEMO("Tiny")
ADD_DEMO("Tiny_Mesh")
ADD_DEMO("Tiny_MeshLarge")

target_sources(Tiny_MeshLarge
               PUBLIC "${LVK_ROOT_DIR}/third-party/deps/src/3D-Graphics-Rendering-Cookbook/shared/UtilsCubemap.cpp")
