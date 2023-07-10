# LightweightVK
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

project(LVKVulkan CXX C)

file(GLOB SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.c)
file(GLOB HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

add_library(LVKVulkan ${SRC_FILES} ${HEADER_FILES})

igl_set_cxxstd(LVKVulkan 20)
igl_set_folder(LVKVulkan "LVK")

find_package(Vulkan REQUIRED)

target_link_libraries(LVKVulkan PRIVATE IGLLibrary)
target_link_libraries(LVKVulkan PUBLIC  Vulkan::Vulkan)

target_include_directories(LVKVulkan PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/volk")
target_include_directories(LVKVulkan PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/vma/include")

if(WIN32)
  add_definitions("-DVK_USE_PLATFORM_WIN32_KHR=1")
  add_definitions("-DNOMINMAX")
elseif(UNIX)
  add_definitions("-DVK_USE_PLATFORM_XLIB_KHR=1")
endif()
