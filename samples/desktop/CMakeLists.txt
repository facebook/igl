# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.19)

set(PROJECT_NAME "IGL Samples")

if(MSVC)
  add_definitions(-D_CONSOLE)
endif()

if(WIN32)
  add_definitions("-DVK_USE_PLATFORM_WIN32_KHR=1")
  add_definitions("-DNOMINMAX")
endif()

macro(ADD_DEMO app)
  add_executable(${app} "Tiny/${app}.cpp")
  igl_set_cxxstd(${app} 20)
  igl_set_folder(${app} ${PROJECT_NAME})
  target_link_libraries(${app} PUBLIC IGLLibrary)
  target_link_libraries(${app} PRIVATE bc7enc)
  target_link_libraries(${app} PRIVATE meshoptimizer)
  target_link_libraries(${app} PRIVATE tinyobjloader)
  target_link_libraries(${app} PRIVATE glfw)
  target_link_libraries(${app} PRIVATE ktx)
  if(IGL_WITH_IGLU)
    target_link_libraries(${app} PRIVATE IGLUimgui)
    target_link_libraries(${app} PRIVATE IGLUsimple_renderer)
    target_link_libraries(${app} PRIVATE IGLUtexture_loader)
  endif()
  if(UNIX)
    target_link_libraries(${app} PRIVATE EGL)
  endif()
  target_link_libraries(${app} PRIVATE IGLstb)
endmacro()

add_demo("Tiny")

if(IGL_WITH_VULKAN)
  # this demo app does not work without Vulkan (yet)
  add_demo("Tiny_Mesh")
endif()

add_demo("Tiny_MeshLarge")

target_sources(Tiny_MeshLarge
               PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/3D-Graphics-Rendering-Cookbook/shared/UtilsCubemap.cpp")
if(NOT IGL_WITH_VULKAN)
  target_sources(Tiny_MeshLarge PUBLIC "${IGL_ROOT_DIR}/src/igl/vulkan/util/TextureFormat.cpp")
endif()
