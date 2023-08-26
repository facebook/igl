# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

project("LVK" CXX C)

# cmake-format: off
option(LVK_WITH_SAMPLES "Enable sample demo apps"  ON)
option(LVK_WITH_TRACY   "Enable Tracy profiler"    ON)
option(LVK_DEPLOY_DEPS  "Deploy dependencies via CMake" ON)
# cmake-format: on

include(cmake/CommonMacros.txt)

if(DEFINED ENV{VULKAN_SDK})
  message(STATUS "VULKAN_SDK=$ENV{VULKAN_SDK}")
  if(NOT EXISTS $ENV{VULKAN_SDK})
    message(FATAL_ERROR "$ENV{VULKAN_SDK} does not exist.")
  endif()
endif()

function(lvk_set_folder target folder_name)
  set_property(TARGET ${target} PROPERTY FOLDER ${folder_name})
endfunction()

function(lvk_set_cxxstd target cpp_version)
  set_property(TARGET ${target} PROPERTY CXX_STANDARD ${cpp_version})
  set_property(TARGET ${target} PROPERTY CXX_STANDARD_REQUIRED ON)
endfunction()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# cmake-format: off
message(STATUS "LVK_WITH_SAMPLES = ${LVK_WITH_SAMPLES}")
message(STATUS "LVK_WITH_TRACY   = ${LVK_WITH_TRACY}")
message(STATUS "LVK_DEPLOY_DEPS  = ${LVK_DEPLOY_DEPS}")
# cmake-format: on

if(NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

message(STATUS "Generator : " ${CMAKE_GENERATOR})
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})

set(LVK_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LVK_DEPS_CACHE_FILE "${LVK_ROOT_DIR}/third-party/.bootstrap-deps.json")
find_package(Python3 COMPONENTS Interpreter)

message(STATUS "LVK_ROOT_DIR = " ${LVK_ROOT_DIR})

if(LVK_DEPLOY_DEPS)
  # deploy dependencies during configuration
  execute_process(COMMAND ${Python3_EXECUTABLE} deploy_deps.py WORKING_DIRECTORY ${LVK_ROOT_DIR} COMMAND_ERROR_IS_FATAL ANY)

  if(NOT EXISTS ${LVK_DEPS_CACHE_FILE})
    message(FATAL_ERROR "Cannot deploy dependencies.")
  endif()

  # regenerate dependencies when .bootstrap-deps.json is older than bootstrap-deps.json
  add_custom_command(COMMAND ${Python3_EXECUTABLE} deploy_deps.py OUTPUT ${LVK_DEPS_CACHE_FILE}
                     DEPENDS "${LVK_ROOT_DIR}/third-party/bootstrap-deps.json" WORKING_DIRECTORY ${LVK_ROOT_DIR})
  add_custom_target(LVKDependencies DEPENDS ${LVK_DEPS_CACHE_FILE})

  lvk_set_folder(LVKDependencies "LVK")
endif()

if(LVK_WITH_TRACY)
  add_definitions("-DTRACY_ENABLE=1")
  add_subdirectory(third-party/deps/src/tracy)
  lvk_set_folder(TracyClient "third-party")
endif()

# temporary
find_package(Vulkan REQUIRED)

include_directories(.)
include_directories(src)

add_subdirectory(src/igl/vulkan)
add_subdirectory(lvk)

target_include_directories(LVKLibrary PUBLIC ${LVK_ROOT_DIR})
target_include_directories(LVKLibrary PUBLIC ${LVK_ROOT_DIR}/src)

if(LVK_WITH_TRACY)
  target_compile_definitions(LVKLibrary PUBLIC "LVK_WITH_TRACY=1")
endif()

if(LVK_DEPLOY_DEPS)
  add_dependencies(LVKLibrary LVKDependencies)
endif()

add_library(LVKstb third-party/deps/patches/stb_impl/stb_image.c third-party/deps/patches/stb_impl/stb_image_resize.c
                   third-party/deps/patches/stb_impl/stb_image_write.c)
target_include_directories(LVKstb PUBLIC "third-party/deps/src/stb")
lvk_set_folder(LVKstb "LVK")

# cmake-format: off
set(MINILOG_BUILD_EXAMPLE OFF CACHE BOOL "")
set(MINILOG_RAW_OUTPUT     ON CACHE BOOL "")
# cmake-format: on
add_subdirectory(third-party/deps/src/minilog)
lvk_set_folder(minilog "third-party")

target_link_libraries(LVKLibrary PUBLIC minilog)
target_include_directories(LVKLibrary PUBLIC "third-party/deps/src")

if(LVK_WITH_SAMPLES)
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
    lvk_set_folder(update_mappings "third-party/GLFW3")
  endif()
  if(UNIX)
    find_package(OpenGL REQUIRED)
  endif()
  add_subdirectory(third-party/deps/src/bc7enc)
  lvk_set_cxxstd(bc7enc 17)
  add_subdirectory(third-party/deps/src/meshoptimizer)
  add_subdirectory(third-party/deps/src/tinyobjloader)
  add_subdirectory(samples)
  # cmake-format: off
  lvk_set_folder(bc7enc        "third-party")
  lvk_set_folder(meshoptimizer "third-party")
  lvk_set_folder(tinyobjloader "third-party/tinyobjloader")
  lvk_set_folder(uninstall     "third-party/tinyobjloader")
  lvk_set_folder(glfw          "third-party/GLFW3")
  # cmake-format: on
endif()

if(LVK_WITH_TRACY)
  target_link_libraries(LVKLibrary PUBLIC TracyClient)
endif()

# ImGui
target_sources(LVKLibrary PRIVATE "${LVK_ROOT_DIR}/third-party/deps/src/imgui/imgui.cpp")
target_sources(LVKLibrary PRIVATE "${LVK_ROOT_DIR}/third-party/deps/src/imgui/imgui_demo.cpp")
target_sources(LVKLibrary PRIVATE "${LVK_ROOT_DIR}/third-party/deps/src/imgui/imgui_draw.cpp")
target_sources(LVKLibrary PRIVATE "${LVK_ROOT_DIR}/third-party/deps/src/imgui/imgui_tables.cpp")
target_sources(LVKLibrary PRIVATE "${LVK_ROOT_DIR}/third-party/deps/src/imgui/imgui_widgets.cpp")
target_include_directories(LVKLibrary PUBLIC "${LVK_ROOT_DIR}/third-party/deps/src/imgui")
