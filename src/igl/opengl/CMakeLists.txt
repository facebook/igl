# Copyright (c) Meta Platforms, Inc. and affiliates.
#
# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.16)

project(IGLOpenGL CXX C)

if(NOT IOS AND NOT ANDROID)
  find_package(OpenGL REQUIRED)
endif()

file(GLOB SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)
file(GLOB HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

if(WIN32)
  file(GLOB WGL_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} wgl/*.cpp)
  file(GLOB WGL_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} wgl/*.h)
  list(APPEND SRC_FILES ${WGL_SRC_FILES})
  list(APPEND HEADER_FILES ${WGL_HEADER_FILES})
elseif(APPLE AND NOT IOS)
  file(GLOB MACOS_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} macos/*.mm)
  file(GLOB MACOS_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} macos/*.h)
  list(APPEND SRC_FILES ${MACOS_SRC_FILES})
  list(APPEND HEADER_FILES ${MACOS_HEADER_FILES})
elseif(APPLE AND IOS)
  file(GLOB IOS_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ios/*.mm)
  file(GLOB IOS_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ios/*.h)
  list(APPEND SRC_FILES ${IOS_SRC_FILES})
  list(APPEND HEADER_FILES ${IOS_HEADER_FILES})
elseif(UNIX AND NOT APPLE AND NOT ANDROID AND NOT IOS AND NOT EMSCRIPTEN)
  file(GLOB GLX_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} glx/*.cpp)
  file(GLOB GLX_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} glx/*.h)
  list(APPEND SRC_FILES ${GLX_SRC_FILES})
  list(APPEND HEADER_FILES ${GLX_HEADER_FILES})
endif()

if((IGL_WITH_OPENGLES OR (UNIX AND NOT APPLE AND NOT ANDROID AND NOT IOS  AND NOT EMSCRIPTEN)) AND NOT IOS)
  file(GLOB EGL_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} egl/*.cpp)
  file(GLOB EGL_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} egl/*.h)
  list(APPEND SRC_FILES ${EGL_SRC_FILES})
  list(APPEND HEADER_FILES ${EGL_HEADER_FILES})
endif()

if(IGL_WITH_WEBGL)
  file(GLOB WEBGL_SRC_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} webgl/*.cpp)
  file(GLOB WEBGL_HEADER_FILES LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} webgl/*.h)
  list(APPEND SRC_FILES ${WEBGL_SRC_FILES})
  list(APPEND HEADER_FILES ${WEBGL_HEADER_FILES})
endif()

add_library(IGLOpenGL ${SRC_FILES} ${HEADER_FILES})

target_link_libraries(IGLOpenGL PRIVATE IGLLibrary)

if(ANDROID)
  add_definitions("-DSUPPORT_ES31_IGL_OPENGL_ANDROID")
  add_definitions("-DGL_GLEXT_PROTOTYPES")
  target_link_libraries(IGLOpenGL PUBLIC log GLESv3 EGL)
endif()

if(APPLE)
  if(IOS)
    set(PLATFORM_FRAMEWORKS "-framework OpenGLES" "-framework CoreVideo")
    set(OPENGL_DEPRECATION_FLAG GLES_SILENCE_DEPRECATION)
  else()
    set(PLATFORM_FRAMEWORKS "-framework OpenGL" "-framework Cocoa")
    set(OPENGL_DEPRECATION_FLAG GL_SILENCE_DEPRECATION)
  endif()
  target_link_libraries(IGLOpenGL PRIVATE "-framework IOKit" "-framework CoreFoundation" ${PLATFORM_FRAMEWORKS})
  target_compile_definitions(IGLOpenGL PUBLIC -D${OPENGL_DEPRECATION_FLAG})
endif()

if (IGL_WITH_WEBGL)
  #"-s DEMANGLE_SUPPORT=1")
  set_target_properties(IGLOpenGL PROPERTIES LINK_FLAGS "-s USE_WEBGL2=1 -s GL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=1 -s GL_EMULATE_GLES_VERSION_STRING_FORMAT=1")
endif()

target_link_libraries(IGLOpenGL PRIVATE IGLLibrary)
if(NOT APPLE AND NOT ANDROID AND NOT EMSCRIPTEN)
  target_link_libraries(IGLOpenGL PUBLIC OpenGL::GL)
endif()

igl_set_cxxstd(IGLOpenGL 17)
igl_set_folder(IGLOpenGL "IGL")

if(WIN32)
  add_definitions("-DNOMINMAX")
  # IGLOpenGL uses GL/glew.h on Windows
  set(BUILD_UTILS OFF CACHE BOOL "")
  add_subdirectory("${IGL_ROOT_DIR}/third-party/deps/src/glew/build/cmake" glew)
  igl_set_folder(glew "third-party")
  igl_set_folder(glew_s "third-party")
  target_link_libraries(IGLOpenGL PUBLIC glew_s)
  target_include_directories(IGLOpenGL PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/glew/include")
  target_include_directories(IGLOpenGL PUBLIC "${IGL_ROOT_DIR}/third-party/deps/src/EGL/EGL/include")
elseif(UNIX AND NOT APPLE AND NOT ANDROID AND NOT EMSCRIPTEN)
  add_definitions("-DGL_GLEXT_PROTOTYPES")
  target_link_libraries(IGLOpenGL PUBLIC EGL OpenGL)
endif()

if(IGL_WITH_OPENGLES AND NOT APPLE)
  target_link_libraries(IGLOpenGL PUBLIC EGL)
endif()
