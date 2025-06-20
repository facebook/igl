# IGL Install Configuration
# This file contains all install logic to minimize changes to original CMake files

if(NOT IGL_ENABLE_INSTALL)
    return()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

message(STATUS "Configuring IGL install targets...")

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
                # Skip binary directory includes
                continue()
            endif()
            
            # Convert known paths to generator expressions
            if("${include_dir}" STREQUAL "${IGL_ROOT_DIR}/src")
                list(APPEND new_includes 
                    "$<BUILD_INTERFACE:${IGL_ROOT_DIR}/src>"
                    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
                )
            elseif("${include_dir}" STREQUAL "${IGL_ROOT_DIR}/third-party/deps/src/glm")
                list(APPEND new_includes 
                    "$<BUILD_INTERFACE:${IGL_ROOT_DIR}/third-party/deps/src/glm>"
                    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/glm>"
                )
            elseif("${include_dir}" STREQUAL "${CMAKE_SOURCE_DIR}/third-party/deps/src/stb" OR 
                   "${include_dir}" STREQUAL "third-party/deps/src/stb")
                list(APPEND new_includes 
                    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third-party/deps/src/stb>"
                    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/stb>"
                )
            elseif("${include_dir}" STREQUAL "${CMAKE_SOURCE_DIR}/third-party/deps/src" OR 
                   "${include_dir}" STREQUAL "third-party/deps/src")
                list(APPEND new_includes 
                    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/third-party/deps/src>"
                    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
                )
            elseif("${include_dir}" STREQUAL "${CMAKE_SOURCE_DIR}/IGLU/simdtypes" OR 
                   "${include_dir}" STREQUAL "simdtypes")
                list(APPEND new_includes 
                    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/IGLU/simdtypes>"
                    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/IGLU/simdtypes>"
                )
            elseif("${include_dir}" STREQUAL "${IGL_ROOT_DIR}")
                # Convert IGL_ROOT_DIR to appropriate interface
                list(APPEND new_includes 
                    "$<BUILD_INTERFACE:${IGL_ROOT_DIR}>"
                    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
                )
            else()
                # For other paths, try to convert them to generator expressions
                string(REPLACE "${CMAKE_SOURCE_DIR}/" "" relative_path "${include_dir}")
                string(REPLACE "${IGL_ROOT_DIR}/" "" relative_path2 "${include_dir}")
                
                if(NOT "${relative_path}" STREQUAL "${include_dir}")
                    # Path was under CMAKE_SOURCE_DIR, use relative path
                    list(APPEND new_includes 
                        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/${relative_path}>"
                    )
                elseif(NOT "${relative_path2}" STREQUAL "${include_dir}")
                    # Path was under IGL_ROOT_DIR, use relative path
                    list(APPEND new_includes 
                        "$<BUILD_INTERFACE:${IGL_ROOT_DIR}/${relative_path2}>"
                    )
                else()
                    # For other paths, keep as build interface only if not problematic
                    if(NOT "${include_dir}" MATCHES "^${CMAKE_SOURCE_DIR}" AND 
                       NOT "${include_dir}" MATCHES "^${IGL_ROOT_DIR}")
                        list(APPEND new_includes "$<BUILD_INTERFACE:${include_dir}>")
                    endif()
                endif()
            endif()
        endforeach()
        
        if(new_includes)
            set_target_properties(${target_name} PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${new_includes}"
            )
        endif()
    endif()
endfunction()

# Function to check if a target has third-party dependencies that would cause export issues
function(igl_has_third_party_deps target_name result_var)
    if(NOT TARGET ${target_name})
        set(${result_var} FALSE PARENT_SCOPE)
        return()
    endif()
    
    get_target_property(link_libs ${target_name} LINK_LIBRARIES)
    if(link_libs)
        foreach(lib ${link_libs})
            # Check for known problematic third-party targets
            if("${lib}" MATCHES "^(glslang|SPIRV|spirv-cross|glslang-default-resource-limits|IGLShellShared|ktx)$")
                set(${result_var} TRUE PARENT_SCOPE)
                return()
            endif()
        endforeach()
    endif()
    
    set(${result_var} FALSE PARENT_SCOPE)
endfunction()

# Function to setup install for any target
function(igl_setup_target_install target_name)
    if(NOT TARGET ${target_name})
        message(STATUS "  - Skipping ${target_name}: target not found")
        return()
    endif()
    
    # Check for third-party dependencies
    igl_has_third_party_deps(${target_name} has_third_party)
    if(has_third_party)
        message(STATUS "  - Skipping ${target_name}: has third-party dependencies")
        return()
    endif()
    
    get_target_property(target_type ${target_name} TYPE)
    
    # Only process libraries and executables
    if(NOT target_type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|INTERFACE_LIBRARY)$")
        message(STATUS "  - Skipping ${target_name}: not a library")
        return()
    endif()
    
    # Fix include directories before install
    igl_fix_target_includes(${target_name})
    
    # Install the target
    install(TARGETS ${target_name}
        EXPORT IGLTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    
    # Set export properties
    set_target_properties(${target_name} PROPERTIES
        EXPORT_NAME ${target_name}
    )
    
    # Create alias if it doesn't exist
    if(NOT TARGET IGL::${target_name})
        if(target_type STREQUAL "INTERFACE_LIBRARY")
            add_library(IGL::${target_name} ALIAS ${target_name})
        else()
            add_library(IGL::${target_name} ALIAS ${target_name})
        endif()
    endif()
    
    message(STATUS "  - Configured install for: ${target_name}")
endfunction()

# Function to install headers for a directory
function(igl_install_headers source_dir dest_dir)
    if(EXISTS "${source_dir}")
        install(DIRECTORY "${source_dir}/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dest_dir}"
            FILES_MATCHING 
            PATTERN "*.h"
            PATTERN "*.hpp"
            PATTERN "*.inl"
            PATTERN "tests" EXCLUDE
            PATTERN "test" EXCLUDE
            PATTERN ".git" EXCLUDE
        )
        message(STATUS "  - Configured headers install: ${source_dir} -> ${dest_dir}")
    endif()
endfunction()

# Core IGL targets (only safe ones without third-party deps)
set(IGL_CORE_TARGETS 
    IGLLibrary 
    IGLOpenGL 
    IGLMetal 
    IGLstb
)

# Add Vulkan if available
if(TARGET IGLVulkan)
    list(APPEND IGL_CORE_TARGETS IGLVulkan)
endif()

# IGLU targets (only safe ones without third-party deps)
set(IGLU_TARGETS
    IGLUmanagedUniformBuffer
    IGLUsentinel
    IGLUsimple_renderer
    IGLUstate_pool
    IGLUuniform
    IGLUsimdtypes
)

# Process all IGL and IGLU targets
foreach(target IN LISTS IGL_CORE_TARGETS IGLU_TARGETS)
    igl_setup_target_install(${target})
endforeach()

# Install headers
igl_install_headers("${CMAKE_SOURCE_DIR}/src/igl" "igl")
igl_install_headers("${CMAKE_SOURCE_DIR}/IGLU" "IGLU")

# Install essential third-party headers that are part of IGL's public API
igl_install_headers("${CMAKE_SOURCE_DIR}/third-party/deps/src/glm/glm" "glm")
igl_install_headers("${CMAKE_SOURCE_DIR}/third-party/deps/src/stb" "stb")

# Platform-specific headers
if(WIN32)
    install(FILES "${CMAKE_SOURCE_DIR}/src/igl/win/LogDefault.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/igl/win
    )
endif()

if(ANDROID)
    install(FILES 
        "${CMAKE_SOURCE_DIR}/src/igl/android/LogDefault.h" 
        "${CMAKE_SOURCE_DIR}/src/igl/android/NativeHWBuffer.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/igl/android
    )
endif()

# Generate CMake config files
configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/IGLConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/IGLConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/IGL"
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Install CMake config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/IGLConfig.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/IGL"
)

# Install export targets
install(EXPORT IGLTargets
    FILE IGLTargets.cmake
    NAMESPACE IGL::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/IGL"
)

message(STATUS "IGL install configuration completed.") 