# LightweightVK
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

project(LVKLibrary CXX C)

file(GLOB SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)
file(GLOB HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

add_library(LVKLibrary ${SRC_FILES} ${HEADER_FILES})

lvk_setup_groups("${SRC_FILES}")
lvk_setup_groups("${HEADER_FILES}")

target_include_directories(LVKLibrary PUBLIC "${LVK_ROOT_DIR}/lvk")

lvk_setup_target(LVKLibrary)
lvk_set_folder(LVKLibrary "LVK")

add_subdirectory(vulkan)

target_link_libraries(LVKLibrary PUBLIC LVKVulkan)

# temporary
target_link_libraries(LVKLibrary PUBLIC IGLVulkan)
