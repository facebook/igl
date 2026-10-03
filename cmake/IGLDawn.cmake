# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# Provides IGL_DAWN_TARGET, the Dawn library the WebGPU backend links (native builds only;
# Emscripten builds use the emdawnwebgpu port instead). Tried in order:
#   1. find_package(Dawn CONFIG), e.g. with CMAKE_PREFIX_PATH pointing at an installed Dawn;
#   2. IGL_WEBGPU_DAWN_DIR, the install prefix (or the directory with DawnConfig.cmake) of a Dawn
#      built with DAWN_ENABLE_INSTALL=ON;
#   3. FetchContent of Dawn at IGL_WEBGPU_DAWN_REVISION, the revision fbsource vendors, so that
#      webgpu.h matches the one WebGPUCompat.h is written against. Building it takes a while.

set(IGL_WEBGPU_DAWN_DIR "" CACHE PATH "Installed Dawn to use for IGL_WITH_WEBGPU")
set(IGL_WEBGPU_DAWN_REVISION "d57bbdb7c907b4d276c6d7bd6fc50115afd9dc85" CACHE STRING
                                                                           "Dawn revision to fetch")

if(IGL_WEBGPU_DAWN_DIR)
  find_package(Dawn CONFIG REQUIRED PATHS "${IGL_WEBGPU_DAWN_DIR}" NO_DEFAULT_PATH)
else()
  find_package(Dawn CONFIG QUIET)
endif()

if(NOT Dawn_FOUND)
  message(STATUS "IGL: fetching Dawn ${IGL_WEBGPU_DAWN_REVISION}")
  include(FetchContent)
  # cmake-format: off
  set(DAWN_FETCH_DEPENDENCIES ON  CACHE BOOL "")
  set(DAWN_BUILD_SAMPLES      OFF CACHE BOOL "")
  set(DAWN_BUILD_TESTS        OFF CACHE BOOL "")
  set(DAWN_USE_GLFW           OFF CACHE BOOL "")
  set(DAWN_ENABLE_DESKTOP_GL  OFF CACHE BOOL "")
  set(DAWN_ENABLE_OPENGLES    OFF CACHE BOOL "")
  set(DAWN_ENABLE_INSTALL     OFF CACHE BOOL "")
  set(TINT_BUILD_TESTS        OFF CACHE BOOL "")
  set(TINT_BUILD_CMD_TOOLS    OFF CACHE BOOL "")
  # cmake-format: on
  FetchContent_Declare(
    dawn
    GIT_REPOSITORY https://dawn.googlesource.com/dawn
    GIT_TAG ${IGL_WEBGPU_DAWN_REVISION})
  FetchContent_MakeAvailable(dawn)
endif()

if(TARGET dawn::webgpu_dawn)
  set(IGL_DAWN_TARGET dawn::webgpu_dawn)
elseif(TARGET webgpu_dawn)
  set(IGL_DAWN_TARGET webgpu_dawn)
else()
  message(FATAL_ERROR "IGL_WITH_WEBGPU: Dawn provides no webgpu_dawn library")
endif()
message(STATUS "IGL: WebGPU backend uses ${IGL_DAWN_TARGET}")
