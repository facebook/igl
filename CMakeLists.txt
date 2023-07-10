# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

project("IGL" CXX C)

# cmake-format: off
option(IGL_WITH_SAMPLES  "Enable sample demo apps"        ON)

option(IGL_WITH_OPENGL   "Enable IGL/OpenGL"              ON)
option(IGL_WITH_OPENGLES "Enable IGL/OpenGL ES"          OFF)
option(IGL_WITH_VULKAN   "Enable IGL/Vulkan"              ON)
option(IGL_WITH_METAL    "Enable IGL/Metal"               ON)
option(IGL_WITH_WEBGL    "Enable IGL/WebGL"               OFF)

option(IGL_WITH_IGLU     "Enable IGLU utils"              ON)
option(IGL_WITH_SHELL    "Enable Shell utils"             ON)
option(IGL_WITH_TESTS    "Enable IGL tests (gtest)"      OFF)
option(IGL_WITH_TRACY    "Enable Tracy profiler"         OFF)
option(IGL_ENFORCE_LOGS  "Enable logs in Release builds"  ON)

option(IGL_DEPLOY_DEPS   "Deploy dependencies via CMake"  ON)
# cmake-format: on

if(DEFINED ENV{VULKAN_SDK})
  message(STATUS "VULKAN_SDK=$ENV{VULKAN_SDK}")
  if(NOT EXISTS $ENV{VULKAN_SDK})
    message(FATAL_ERROR "$ENV{VULKAN_SDK} does not exist.")
  endif()
endif()

if(IOS)
  if(IGL_WITH_OPENGL)
    set(IGL_WITH_OPENGLES ON)
  endif()
  set(IGL_WITH_OPENGL OFF)
  set(IGL_WITH_VULKAN OFF)
  set(IGL_WITH_WEBGL OFF)
endif()

if(NOT APPLE)
  set(IGL_WITH_METAL OFF)
endif()

if(ANDROID)
  if(IGL_WITH_OPENGL)
    set(IGL_WITH_OPENGLES ON)
  endif()
  set(IGL_WITH_OPENGL OFF)
  set(IGL_WITH_VULKAN ON)
  set(IGL_WITH_WEBGL OFF)
endif()

if(EMSCRIPTEN)
  set(IGL_WITH_OPENGL OFF)
  set(IGL_WITH_OPENGLES OFF)
  set(IGL_WITH_VULKAN OFF)
  set(IGL_WITH_WEBGL ON)
  set(IGL_WITH_SHELL OFF) # shell doesn't supported yet
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID AND NOT EMSCRIPTEN)
  if(IGL_WITH_SAMPLES OR IGL_WITH_SHELL)
    add_definitions("-DIGL_PLATFORM_LINUX_USE_EGL=0")
  else()
    add_definitions("-DIGL_PLATFORM_LINUX_USE_EGL=1")
  endif()
endif()

include(cmake/helpers.cmake)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# cmake-format: off
message(STATUS "IGL_WITH_SAMPLES  = ${IGL_WITH_SAMPLES}")

message(STATUS "IGL_WITH_OPENGL   = ${IGL_WITH_OPENGL}")
message(STATUS "IGL_WITH_OPENGLES = ${IGL_WITH_OPENGLES}")
message(STATUS "IGL_WITH_VULKAN   = ${IGL_WITH_VULKAN}")
message(STATUS "IGL_WITH_METAL    = ${IGL_WITH_METAL}")
message(STATUS "IGL_WITH_WEBGL    = ${IGL_WITH_WEBGL}")

message(STATUS "IGL_WITH_IGLU     = ${IGL_WITH_IGLU}")
message(STATUS "IGL_WITH_SHELL    = ${IGL_WITH_SHELL}")
message(STATUS "IGL_WITH_TESTS    = ${IGL_WITH_TESTS}")
message(STATUS "IGL_WITH_TRACY    = ${IGL_WITH_TRACY}")
message(STATUS "IGL_ENFORCE_LOGS  = ${IGL_ENFORCE_LOGS}")

message(STATUS "IGL_DEPLOY_DEPS   = ${IGL_DEPLOY_DEPS}")
# cmake-format: on

if(IGL_ENFORCE_LOGS)
  add_definitions("-DIGL_FORCE_ENABLE_LOGS=1")
endif()

if(APPLE)
  if(NOT (IGL_WITH_OPENGL OR IGL_WITH_VULKAN OR IGL_WITH_OPENGLES OR IGL_WITH_METAL OR IGL_WITH_WEBGL))
    message(FATAL_ERROR "At least one rendering backend should be defined (OpenGL, Vulkan or Metal).")
  endif()
else()
  if(NOT (IGL_WITH_OPENGL OR IGL_WITH_VULKAN OR IGL_WITH_OPENGLES OR IGL_WITH_WEBGL))
    message(FATAL_ERROR "At least one rendering backend should be defined (OpenGL or Vulkan).")
  endif()
endif()

if(IGL_WITH_SHELL AND (NOT IGL_WITH_IGLU))
  message(FATAL_ERROR "IGL_WITH_SHELL requires enabling IGL_WITH_IGLU.")
endif()

if(APPLE)
  enable_language(OBJC)
  enable_language(OBJCXX)
endif()

if(IGL_WITH_OPENGL)
  add_definitions("-DIGL_BACKEND_ENABLE_OPENGL=1")
endif()
if(IGL_WITH_OPENGLES OR IGL_WITH_WEBGL)
  add_definitions("-DIGL_BACKEND_ENABLE_OPENGL=1")
endif()
if(IGL_WITH_VULKAN)
  add_definitions("-DIGL_BACKEND_ENABLE_VULKAN=1")
endif()
if(IGL_WITH_IGLU)
  add_definitions("-DIGL_WITH_IGLU=1")
endif()
if(APPLE AND IGL_WITH_METAL)
  add_definitions("-DIGL_BACKEND_ENABLE_METAL=1")
endif()

add_definitions("-DIGL_CMAKE_BUILD=1")

if(NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

message(STATUS "IGL generator : " ${CMAKE_GENERATOR})
message(STATUS "IGL build type: " ${CMAKE_BUILD_TYPE})

set(IGL_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(IGL_DEPS_CACHE_FILE "${IGL_ROOT_DIR}/third-party/.bootstrap-deps.json")
find_package(Python3 COMPONENTS Interpreter)

message(STATUS "IGL_ROOT_DIR = " ${IGL_ROOT_DIR})

if(IGL_DEPLOY_DEPS)
  # deploy dependencies during configuration
  execute_process(COMMAND ${Python3_EXECUTABLE} deploy_deps.py WORKING_DIRECTORY ${IGL_ROOT_DIR} COMMAND_ERROR_IS_FATAL ANY)

  if(NOT EXISTS ${IGL_DEPS_CACHE_FILE})
    message(FATAL_ERROR "Cannot deploy dependencies.")
  endif()

  # regenerate dependencies when .bootstrap-deps.json is older than bootstrap-deps.json
  add_custom_command(COMMAND ${Python3_EXECUTABLE} deploy_deps.py OUTPUT ${IGL_DEPS_CACHE_FILE}
                     DEPENDS "${IGL_ROOT_DIR}/third-party/bootstrap-deps.json" WORKING_DIRECTORY ${IGL_ROOT_DIR})
  add_custom_target(IGLDependencies DEPENDS ${IGL_DEPS_CACHE_FILE})

  igl_set_folder(IGLDependencies "IGL")
endif()

if(IGL_WITH_TRACY)
  add_definitions("-DIGL_WITH_TRACY=1")
  add_definitions("-DTRACY_ENABLE=1")
  add_subdirectory(third-party/deps/src/tracy)
  igl_set_folder(TracyClient "third-party")
endif()

add_subdirectory(src/igl)

if(IGL_DEPLOY_DEPS)
  add_dependencies(IGLLibrary IGLDependencies)
endif()

include_directories(.)

if(IGL_WITH_IGLU)
  add_library(IGLstb third-party/deps/patches/stb_impl/stb_image.c third-party/deps/patches/stb_impl/stb_image_resize.c
                     third-party/deps/patches/stb_impl/stb_image_write.c)
  target_include_directories(IGLstb PUBLIC "third-party/deps/src/stb")
  igl_set_folder(IGLstb "IGL")
  include_directories("third-party/deps/src/imgui")
  add_subdirectory(IGLU)
  if(IGL_WITH_SHELL)
    include_directories("third-party/deps/src/stb")
    add_subdirectory(shell)
  endif()
endif()

if(IGL_WITH_SAMPLES)
  if (EMSCRIPTEN)
    add_subdirectory(samples/wasm)
  else()
    include_directories("third-party/deps/src")
    include_directories("third-party/deps/src/bc7enc")
    include_directories("third-party/deps/src/gli")
    include_directories("third-party/deps/src/glm")
    include_directories("third-party/deps/src/stb")
    include_directories("third-party/deps/src/taskflow")
    include_directories("third-party/deps/src/3D-Graphics-Rendering-Cookbook")
    if(WIN32 OR (UNIX AND NOT APPLE AND NOT ANDROID))
      # cmake-format: off
      set(GLFW_BUILD_DOCS     OFF CACHE BOOL "")
      set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
      set(GLFW_BUILD_TESTS    OFF CACHE BOOL "")
      set(GLFW_INSTALL        OFF CACHE BOOL "")
      set(GLFW_VULKAN_STATIC  OFF CACHE BOOL "")
      # cmake-format: on
      add_subdirectory(third-party/deps/src/glfw)
      igl_set_folder(update_mappings "third-party/GLFW3")
    endif()
    if(UNIX AND NOT APPLE AND NOT ANDROID)
      find_package(OpenGL REQUIRED)
    endif()
    add_subdirectory(third-party/deps/src/bc7enc)
    igl_set_cxxstd(bc7enc 17)
    add_subdirectory(third-party/deps/src/meshoptimizer)
    add_subdirectory(third-party/deps/src/tinyobjloader)
    igl_set_folder(bc7enc "third-party")
    igl_set_folder(meshoptimizer "third-party")
    igl_set_folder(tinyobjloader "third-party/tinyobjloader")
    igl_set_folder(uninstall "third-party/tinyobjloader")
    if(NOT APPLE AND NOT ANDROID)
      add_subdirectory(samples/desktop)
      igl_set_folder(glfw "third-party/GLFW3")
    endif()
    endif()
endif()

if(IGL_WITH_TRACY)
  target_link_libraries(IGLLibrary PUBLIC TracyClient)
endif()
