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

  # For D3D12 builds on Windows, ensure dxil.dll is deployed next to sample
  # executables so that DXC/DXIL validation and signed DXIL shaders work in
  # both Debug and Release configurations. This mirrors the behavior used for
  # render sessions (shell/windows/CMakeLists.txt) and unit tests
  # (test_all_unittests.bat).
  if(IGL_WITH_D3D12 AND WIN32 AND MSVC)
    find_file(DXIL_DLL_FOR_${app}
      NAMES dxil.dll
      PATHS
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22000.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.19041.0/x64"
        "$ENV{WindowsSdkBinPath}/x64"
      NO_DEFAULT_PATH
    )
    if(DXIL_DLL_FOR_${app})
      add_custom_command(TARGET ${app} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${DXIL_DLL_FOR_${app}}"
          "$<TARGET_FILE_DIR:${app}>/"
        COMMENT "Copying dxil.dll for ${app}"
      )
    endif()
  endif()
endmacro()

if(IGL_WITH_OPENGL OR IGL_WITH_VULKAN)
  add_demo("Tiny")
endif()

if(IGL_WITH_VULKAN)
  # this demo app does not work without Vulkan (yet)
  add_demo("Tiny_Mesh")
endif()

# Tiny_MeshLarge can run on Vulkan/OpenGL; expose it for D3D12 configs too so the binary is available.
if(IGL_WITH_OPENGL OR IGL_WITH_VULKAN OR IGL_WITH_D3D12)
  add_demo("Tiny_MeshLarge")

  target_sources(
    Tiny_MeshLarge
    PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/3D-Graphics-Rendering-Cookbook/shared/UtilsCubemap.cpp")
  if(NOT IGL_WITH_VULKAN)
    target_sources(Tiny_MeshLarge PUBLIC "${IGL_ROOT_DIR}/src/igl/vulkan/util/TextureFormat.cpp")
  endif()
else()
  message(STATUS "Skipping Tiny_MeshLarge: no compatible backend enabled (needs OpenGL/Vulkan/D3D12)")
endif()
