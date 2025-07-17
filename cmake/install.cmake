# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

# IGL Install Configuration
# This file contains all install logic to minimize changes to original CMake files

if(NOT IGL_ENABLE_INSTALL)
  return()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

message(STATUS "Configuring IGL install targets...")
message(STATUS "Note: fmt target gets special handling to fix PUBLIC_HEADER path conflicts")
message(STATUS "  We clear fmt's PUBLIC_HEADER and install headers manually from correct location")

# Function to fix include directories for install
function(igl_fix_target_includes target_name)
  if(NOT TARGET ${target_name})
    return()
  endif()

  get_target_property(current_includes ${target_name} INTERFACE_INCLUDE_DIRECTORIES)
  if(current_includes)
    set(new_includes "")
    foreach(include_dir ${current_includes})
      # Filter out problematic include directories
      if("${include_dir}" MATCHES "${CMAKE_BINARY_DIR}")
        # For build directory paths, convert them to build interface only
        # This prevents them from being included in the install interface
        list(APPEND new_includes "$<BUILD_INTERFACE:${include_dir}>")
        continue()
      endif()

      # Convert known paths to generator expressions
      if("${include_dir}" STREQUAL "${IGL_ROOT_DIR}/src")
        list(APPEND new_includes "$<BUILD_INTERFACE:${IGL_ROOT_DIR}/src>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
      elseif("${include_dir}" STREQUAL "${IGL_ROOT_DIR}/third-party/deps/src/glm")
        list(APPEND new_includes "$<BUILD_INTERFACE:${IGL_ROOT_DIR}/third-party/deps/src/glm>"
             "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/glm>")
      elseif("${include_dir}" STREQUAL "${CMAKE_SOURCE_DIR}/third-party/deps/src/stb" OR "${include_dir}" STREQUAL
                                                                                         "third-party/deps/src/stb")
        list(APPEND new_includes "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third-party/deps/src/stb>"
             "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/stb>")
      elseif("${include_dir}" STREQUAL "${CMAKE_SOURCE_DIR}/third-party/deps/src" OR "${include_dir}" STREQUAL
                                                                                     "third-party/deps/src")
        list(APPEND new_includes "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third-party/deps/src>"
             "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
      elseif("${include_dir}" STREQUAL "${CMAKE_SOURCE_DIR}/IGLU/simdtypes" OR "${include_dir}" STREQUAL "simdtypes")
        list(APPEND new_includes "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/IGLU/simdtypes>"
             "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/IGLU/simdtypes>")
      elseif("${include_dir}" STREQUAL "${IGL_ROOT_DIR}")
        # Convert IGL_ROOT_DIR to appropriate interface
        list(APPEND new_includes "$<BUILD_INTERFACE:${IGL_ROOT_DIR}>" "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
      else()
        # For other paths, try to convert them to generator expressions
        string(REPLACE "${CMAKE_SOURCE_DIR}/" "" relative_path "${include_dir}")
        string(REPLACE "${IGL_ROOT_DIR}/" "" relative_path2 "${include_dir}")

        if(NOT "${relative_path}" STREQUAL "${include_dir}")
          # Path was under CMAKE_SOURCE_DIR, use relative path
          list(APPEND new_includes "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/${relative_path}>")
        elseif(NOT "${relative_path2}" STREQUAL "${include_dir}")
          # Path was under IGL_ROOT_DIR, use relative path
          list(APPEND new_includes "$<BUILD_INTERFACE:${IGL_ROOT_DIR}/${relative_path2}>")
        else()
          # For other paths, keep as build interface only if not problematic
          if(NOT "${include_dir}" MATCHES "^${CMAKE_SOURCE_DIR}" AND NOT "${include_dir}" MATCHES "^${IGL_ROOT_DIR}")
            list(APPEND new_includes "$<BUILD_INTERFACE:${include_dir}>")
          endif()
        endif()
      endif()
    endforeach()

    if(new_includes)
      set_target_properties(${target_name} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${new_includes}")
    endif()
  endif()
endfunction()

# This function is no longer needed since we preserve all dependencies

# Function to setup install for any target
function(igl_setup_target_install target_name)
  if(NOT TARGET ${target_name})
    message(STATUS "  - Skipping ${target_name}: target not found")
    return()
  endif()

  get_target_property(target_type ${target_name} TYPE)

  # Only process libraries and executables
  if(NOT target_type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|INTERFACE_LIBRARY)$")
    message(STATUS "  - Skipping ${target_name}: not a library")
    return()
  endif()

  message(STATUS "  - Configuring install for: ${target_name}")

  # Special handling for fmt target - fix PUBLIC_HEADER issues
  if("${target_name}" STREQUAL "fmt")
    message(STATUS "    - Applying special handling for fmt target")

    # Get current PUBLIC_HEADER value for debugging
    get_target_property(fmt_public_headers ${target_name} PUBLIC_HEADER)
    message(STATUS "    - Original PUBLIC_HEADER: ${fmt_public_headers}")

    # Clear the problematic PUBLIC_HEADER property
    set_target_properties(${target_name} PROPERTIES PUBLIC_HEADER "")
    message(STATUS "    - Cleared PUBLIC_HEADER to avoid path conflicts")

    # We'll install fmt headers manually via igl_install_headers
  endif()

  # Fix include directories before install
  igl_fix_target_includes(${target_name})

  # Install the target
  install(
    TARGETS ${target_name}
    EXPORT IGLTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    INCLUDES
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  # Set export properties
  set_target_properties(${target_name} PROPERTIES EXPORT_NAME ${target_name})

  # Create alias if it doesn't exist
  if(NOT TARGET IGL::${target_name})
    if(target_type STREQUAL "INTERFACE_LIBRARY")
      add_library(IGL::${target_name} ALIAS ${target_name})
    else()
      add_library(IGL::${target_name} ALIAS ${target_name})
    endif()
  endif()

  message(STATUS "  - Successfully configured install for: ${target_name}")
endfunction()

# Function to install headers for a directory
function(igl_install_headers source_dir dest_dir)
  if(EXISTS "${source_dir}")
    install(
      DIRECTORY "${source_dir}/"
      DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dest_dir}"
      FILES_MATCHING
      PATTERN "*.h"
      PATTERN "*.hpp"
      PATTERN "*.inl"
      PATTERN "tests" EXCLUDE
      PATTERN "test" EXCLUDE
      PATTERN ".git" EXCLUDE)
    message(STATUS "  - Configured headers install: ${source_dir} -> ${dest_dir}")
  endif()
endfunction()

# Get all targets dynamically
get_property(ALL_DISCOVERED_TARGETS GLOBAL PROPERTY TARGETS)

# Backup method: If global property doesn't work, try to discover known targets manually
if(NOT ALL_DISCOVERED_TARGETS)
  message(STATUS "Global TARGETS property is empty, trying backup discovery method...")

  # List of known possible IGL targets that might exist
  set(POTENTIAL_IGL_TARGETS
      # Core IGL targets
      IGLLibrary
      IGLOpenGL
      IGLMetal
      IGLVulkan
      IGLGlslang
      IGLstb
      # IGLU targets
      IGLUimgui
      IGLUmanagedUniformBuffer
      IGLUsentinel
      IGLUshaderCross
      IGLUsimple_renderer
      IGLUstate_pool
      IGLUtexture_accessor
      IGLUtexture_loader
      IGLUuniform
      IGLUsimdtypes
      IGLUbitmap
      # Shell targets
      IGLShellShared
      # Third-party core libraries
      fmt
      glslang
      SPIRV
      spirv-cross-core
      spirv-cross-glsl
      spirv-cross-msl
      spirv-cross-util
      spirv-cross-c
      glslang-default-resource-limits
      # Graphics and media libraries
      ktx
      ktx_read
      ktx_version
      obj_basisu_cbind
      objUtil
      TracyClient
      bc7enc
      meshoptimizer
      tinyobjloader
      # ASTC encoder variants (for different SIMD instructions)
      astcenc-native-static
      astcenc-neon-static
      astcenc-avx2-static
      astcenc-sse4.1-static
      astcenc-sse2-static
      astcenc-none-static
      # Additional third-party libraries that might be built
      glfw
      glew
      glew_s
      zlibstatic)

  # Check which of these targets actually exist
  foreach(potential_target ${POTENTIAL_IGL_TARGETS})
    if(TARGET ${potential_target})
      list(APPEND ALL_DISCOVERED_TARGETS ${potential_target})
      message(STATUS "  - Found target: ${potential_target}")
    endif()
  endforeach()

  if(ALL_DISCOVERED_TARGETS)
    list(LENGTH ALL_DISCOVERED_TARGETS found_count)
    message(STATUS "Backup discovery found ${found_count} targets")
  else()
    message(STATUS "No targets found even with backup method")
  endif()
endif()

# Function to check if a target should be installed
function(should_install_target target_name result_var)
  if(NOT TARGET ${target_name})
    set(${result_var} FALSE PARENT_SCOPE)
    return()
  endif()

  get_target_property(target_type ${target_name} TYPE)

  # Only consider libraries (skip executables, utilities, tests)
  if(NOT target_type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|INTERFACE_LIBRARY)$")
    set(${result_var} FALSE PARENT_SCOPE)
    return()
  endif()

  # Skip test-related targets
  if("${target_name}" MATCHES "[Tt]est|TEST|gtest|benchmark")
    set(${result_var} FALSE PARENT_SCOPE)
    return()
  endif()

  # Skip CMake internal targets
  if("${target_name}" MATCHES "^(cmake_|CMake)")
    set(${result_var} FALSE PARENT_SCOPE)
    return()
  endif()

  # Skip utility targets that shouldn't be exported
  if("${target_name}" MATCHES "^(uninstall|doc|docs|example|sample)")
    set(${result_var} FALSE PARENT_SCOPE)
    return()
  endif()

  # Always include IGL/IGLU targets (highest priority)
  if("${target_name}" MATCHES "^(IGL|IGLU)")
    set(${result_var} TRUE PARENT_SCOPE)
    return()
  endif()

  # Include known important third-party libraries
  if("${target_name}" MATCHES
     "^(fmt|glslang|SPIRV|spirv-cross|ktx|glfw|glew|TracyClient|astcenc|meshoptimizer|tinyobjloader|bc7enc|stb)")
    set(${result_var} TRUE PARENT_SCOPE)
    return()
  endif()

  # Include other common third-party targets that might be useful
  if("${target_name}" MATCHES "^(zlibstatic|zlib|png|jpeg|freetype)")
    set(${result_var} TRUE PARENT_SCOPE)
    return()
  endif()

  # For unknown targets, be more permissive - include them unless they're clearly utilities
  if(NOT "${target_name}" MATCHES "^(build|install|package|doc|example|sample|util|tool)")
    set(${result_var} TRUE PARENT_SCOPE)
    return()
  endif()

  # By default, don't install
  set(${result_var} FALSE PARENT_SCOPE)
endfunction()

# Filter targets that should be installed
set(ALL_TARGETS "")
set(SKIPPED_TARGETS "")

if(NOT ALL_DISCOVERED_TARGETS)
  message(WARNING "No targets discovered at all! This might indicate a CMake configuration issue.")
  message(STATUS "Current CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
  message(STATUS "Current CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
  return()
endif()

foreach(target IN LISTS ALL_DISCOVERED_TARGETS)
  should_install_target(${target} should_install)
  if(should_install)
    list(APPEND ALL_TARGETS ${target})
    message(STATUS "  ✓ Will install target: ${target}")
  else()
    list(APPEND SKIPPED_TARGETS ${target})
    # Only show first few skipped targets to avoid spam
    list(LENGTH SKIPPED_TARGETS skipped_count)
    if(skipped_count LESS 10)
      message(STATUS "  ✗ Skipping target: ${target}")
    endif()
  endif()
endforeach()

list(LENGTH ALL_DISCOVERED_TARGETS total_targets)
list(LENGTH ALL_TARGETS installable_targets)
list(LENGTH SKIPPED_TARGETS skipped_count)

message(STATUS "Target discovery summary:")
message(STATUS "  - Total targets found: ${total_targets}")
message(STATUS "  - Targets to install: ${installable_targets}")
message(STATUS "  - Targets skipped: ${skipped_count}")

if(installable_targets EQUAL 0)
  message(STATUS "Discovered targets were:")
  foreach(target IN LISTS ALL_DISCOVERED_TARGETS)
    get_target_property(target_type ${target} TYPE)
    message(STATUS "  - ${target} (${target_type})")
  endforeach()
endif()

# Track if any targets were actually configured for install
set(IGL_TARGETS_CONFIGURED FALSE)

foreach(target IN LISTS ALL_TARGETS)
  if(TARGET ${target})
    igl_setup_target_install(${target})
    set(IGL_TARGETS_CONFIGURED TRUE)
  endif()
endforeach()

# Check if we have any targets to export
if(NOT IGL_TARGETS_CONFIGURED)
  message(WARNING "No IGL targets were found for installation. This may be due to:")
  message(WARNING "  - All backends disabled (OpenGL, Vulkan, Metal)")
  message(WARNING "  - Missing core library targets")
  message(WARNING "  - Configuration issues")

  # Still install headers even if no targets are available
  message(STATUS "Installing headers only (no library targets available)")
  igl_install_headers("${CMAKE_SOURCE_DIR}/src/igl" "igl")
  igl_install_headers("${CMAKE_SOURCE_DIR}/IGLU" "IGLU")

  # Create a minimal config file without targets
  configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/IGLConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/cmake/IGLConfigHeadersOnly.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/IGL" NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO)

  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cmake/IGLConfigHeadersOnly.cmake" DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/IGL"
          RENAME "IGLConfig.cmake")

  message(STATUS "IGL header-only install configuration completed.")
  return()
endif()

# Install headers
igl_install_headers("${CMAKE_SOURCE_DIR}/src/igl" "igl")
igl_install_headers("${CMAKE_SOURCE_DIR}/IGLU" "IGLU")
igl_install_headers("${CMAKE_SOURCE_DIR}/third-party/deps/src/glm/glm" "glm")
igl_install_headers("${CMAKE_SOURCE_DIR}/third-party/deps/src/stb" "stb")

# Special handling for fmt headers (since we cleared PUBLIC_HEADER for fmt target)
if(TARGET fmt)
  message(STATUS "Installing fmt headers manually to avoid PUBLIC_HEADER conflicts")

  # Verify the fmt headers source directory exists
  set(FMT_HEADERS_SOURCE "${CMAKE_SOURCE_DIR}/third-party/deps/src/fmt/include/fmt")
  if(EXISTS "${FMT_HEADERS_SOURCE}")
    message(STATUS "  ✓ fmt headers source found: ${FMT_HEADERS_SOURCE}")

    # Check if args.h specifically exists
    if(EXISTS "${FMT_HEADERS_SOURCE}/args.h")
      message(STATUS "  ✓ args.h confirmed at: ${FMT_HEADERS_SOURCE}/args.h")
    else()
      message(STATUS "  ✗ args.h missing at: ${FMT_HEADERS_SOURCE}/args.h")
    endif()

    igl_install_headers("${FMT_HEADERS_SOURCE}" "fmt")
    message(STATUS "  → fmt headers will be installed to: ${CMAKE_INSTALL_INCLUDEDIR}/fmt")
  else()
    message(WARNING "fmt headers source directory not found: ${FMT_HEADERS_SOURCE}")
  endif()
else()
  message(STATUS "fmt target not found - skipping manual header installation")
endif()

# Platform-specific headers
if(WIN32)
  install(FILES "${CMAKE_SOURCE_DIR}/src/igl/win/LogDefault.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/igl/win)
endif()

if(ANDROID)
  install(FILES "${CMAKE_SOURCE_DIR}/src/igl/android/LogDefault.h" "${CMAKE_SOURCE_DIR}/src/igl/android/NativeHWBuffer.h"
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/igl/android)
endif()

# Generate CMake config files
configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/IGLConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/cmake/IGLConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/IGL" NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO)

# Install CMake config files
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cmake/IGLConfig.cmake" DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/IGL")

# Only install export targets if we have any
get_property(IGL_EXPORT_TARGETS GLOBAL PROPERTY EXPORT_IGLTargets_TARGETS)
if(IGL_EXPORT_TARGETS OR IGL_TARGETS_CONFIGURED)
  install(EXPORT IGLTargets FILE IGLTargets.cmake NAMESPACE IGL:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/IGL")
  message(STATUS "Configured install for export with targets: ${IGL_EXPORT_TARGETS}")
else()
  message(WARNING "No targets found in IGLTargets export - skipping export installation")
endif()

message(STATUS "IGL install configuration completed.")
