# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

project("IGL" CXX C)

# cmake-format: off
option(IGL_WITH_SAMPLES "Enable sample demo apps"  ON)
option(IGL_WITH_TRACY   "Enable Tracy profiler"    OFF)
option(IGL_DEPLOY_DEPS  "Deploy dependencies via CMake" ON)
# cmake-format: on

if(DEFINED ENV{VULKAN_SDK})
  message(STATUS "VULKAN_SDK=$ENV{VULKAN_SDK}")
  if(NOT EXISTS $ENV{VULKAN_SDK})
    message(FATAL_ERROR "$ENV{VULKAN_SDK} does not exist.")
  endif()
endif()

function(igl_set_folder target folder_name)
  set_property(TARGET ${target} PROPERTY FOLDER ${folder_name})
endfunction()

function(igl_set_cxxstd target cpp_version)
  set_property(TARGET ${target} PROPERTY CXX_STANDARD ${cpp_version})
  set_property(TARGET ${target} PROPERTY CXX_STANDARD_REQUIRED ON)
endfunction()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# cmake-format: off
message(STATUS "IGL_WITH_SAMPLES = ${IGL_WITH_SAMPLES}")
message(STATUS "IGL_WITH_TRACY   = ${IGL_WITH_TRACY}")
message(STATUS "IGL_DEPLOY_DEPS  = ${IGL_DEPLOY_DEPS}")
# cmake-format: on

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

include_directories(.)

add_subdirectory(src/igl)
add_subdirectory(lvk)

if(IGL_DEPLOY_DEPS)
  add_dependencies(IGLLibrary IGLDependencies)
endif()

include_directories("third-party/deps/src/imgui")

add_library(IGLstb third-party/deps/patches/stb_impl/stb_image.c third-party/deps/patches/stb_impl/stb_image_resize.c
                   third-party/deps/patches/stb_impl/stb_image_write.c)
target_include_directories(IGLstb PUBLIC "third-party/deps/src/stb")
igl_set_folder(IGLstb "IGL")

set(MINILOG_BUILD_EXAMPLE OFF CACHE BOOL "")
set(MINILOG_RAW_OUTPUT    ON  CACHE BOOL "")
add_subdirectory(third-party/deps/src/minilog)
igl_set_folder(minilog "third-party")

target_link_libraries(IGLLibrary PUBLIC minilog)
target_include_directories(IGLLibrary PUBLIC "third-party/deps/src")

if(IGL_WITH_SAMPLES)
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
  if(UNIX)
    find_package(OpenGL REQUIRED)
  endif()
  add_subdirectory(third-party/deps/src/bc7enc)
  igl_set_cxxstd(bc7enc 17)
  add_subdirectory(third-party/deps/src/meshoptimizer)
  add_subdirectory(third-party/deps/src/tinyobjloader)
  add_subdirectory(samples)
  igl_set_folder(bc7enc        "third-party")
  igl_set_folder(meshoptimizer "third-party")
  igl_set_folder(tinyobjloader "third-party/tinyobjloader")
  igl_set_folder(uninstall     "third-party/tinyobjloader")
  igl_set_folder(glfw          "third-party/GLFW3")
endif()

if(IGL_WITH_TRACY)
  target_link_libraries(IGLLibrary PUBLIC TracyClient)
endif()
