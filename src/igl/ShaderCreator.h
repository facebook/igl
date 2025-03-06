/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/Shader.h>

namespace igl {

class IDevice;

/**
 * @brief Helper class simplifying the creation of IShaderModules in common scenarios.
 */
class ShaderModuleCreator {
 public:
  ShaderModuleCreator() = delete;

  /**
   * @brief Constructs an IShaderModule for a shader from source code.
   * @param device The device to construct the shader module with.
   * @param source Null-terminated string containing shader source code.
   * @param info Shader module metadata.
   * @param debugName Debug name for the shader module.
   * @param outResult Optional param to receive success or error information.
   */
  static std::shared_ptr<IShaderModule> fromStringInput(const IDevice& device,
                                                        const char* IGL_NONNULL source,
                                                        ShaderModuleInfo info,
                                                        std::string debugName,
                                                        Result* IGL_NULLABLE outResult);

  /**
   * @brief Constructs an IShaderModule for a shader from binary data.
   * @param device The device to construct the shader module with.
   * @param data Pre-compiled shader binary data.
   * @param dataLength Length in bytes of the precompiled shader binary data.
   * @param info Shader module metadata.
   * @param debugName Debug name for the shader module.
   * @param outResult Optional param to receive success or error information.
   */
  static std::shared_ptr<IShaderModule> fromBinaryInput(const IDevice& device,
                                                        const void* IGL_NONNULL data,
                                                        size_t dataLength,
                                                        ShaderModuleInfo info,
                                                        std::string debugName,
                                                        Result* IGL_NULLABLE outResult);
};

/**
 * @brief Helper class simplifying the creation of IShaderLibrary instances in common scenarios.
 */
class ShaderLibraryCreator {
 public:
  ShaderLibraryCreator() = delete;
  /**
   * @brief Constructs an IShaderLibrary with a vertex and fragment shader from source code.
   * @param device The device to construct the shader library with.
   * @param librarySource Null-terminated string containing shader source code.
   * @param vertexEntryPoint Vertex shader entry point name.
   * @param fragmentEntryPoint Fragment shader entry point name.
   * @param libraryDebugName Optional debug name for the library.
   * @param outResult Optional param to receive success or error information.
   */
  static std::unique_ptr<IShaderLibrary> fromStringInput(const IDevice& device,
                                                         const char* IGL_NONNULL librarySource,
                                                         std::string vertexEntryPoint,
                                                         std::string fragmentEntryPoint,
                                                         std::string libraryDebugName,
                                                         Result* IGL_NULLABLE outResult);
  /**
   * @brief Constructs an IShaderLibrary with a vertex and fragment shader from binary data.
   * @param device The device to construct the shader library with.
   * @param libraryData Pre-compiled shader binary data.
   * @param libraryDataLength Length in bytes of the precompiled shader binary data.
   * @param vertexEntryPoint Vertex shader entry point name.
   * @param fragmentEntryPoint Fragment shader entry point name.
   * @param libraryDebugName Optional debug name for the library.
   * @param outResult Optional param to receive success or error information.
   */
  static std::unique_ptr<IShaderLibrary> fromBinaryInput(const IDevice& device,
                                                         const void* IGL_NONNULL libraryData,
                                                         size_t libraryDataLength,
                                                         std::string vertexEntryPoint,
                                                         std::string fragmentEntryPoint,
                                                         std::string libraryDebugName,
                                                         Result* IGL_NULLABLE outResult);

  /**
   * @brief Constructs an IShaderLibrary with shaders from source code.
   * @param device The device to construct the shader library with.
   * @param librarySource Null-terminated string containing shader source code.
   * @param moduleInfo Vector of metadata for shader modules in the library.
   * @param libraryDebugName Optional debug name for the library.
   * @param outResult Optional param to receive success or error information.
   */
  static std::unique_ptr<IShaderLibrary> fromStringInput(const IDevice& device,
                                                         const char* IGL_NONNULL librarySource,
                                                         std::vector<ShaderModuleInfo> moduleInfo,
                                                         std::string libraryDebugName,
                                                         Result* IGL_NULLABLE outResult);

  /**
   * @brief Constructs an IShaderLibrary with shaders from binary data.
   * @param device The device to construct the shader library with.
   * @param libraryData Pre-compiled shader binary data.
   * @param libraryDataLength Length in bytes of the precompiled shader binary data.
   * @param moduleInfo Vector of metadata for shader modules in the library.
   * @param libraryDebugName Optional debug name for the library.
   * @param outResult Optional param to receive success or error information.
   */
  static std::unique_ptr<IShaderLibrary> fromBinaryInput(const IDevice& device,
                                                         const void* IGL_NONNULL libraryData,
                                                         size_t libraryDataLength,
                                                         std::vector<ShaderModuleInfo> moduleInfo,
                                                         std::string libraryDebugName,
                                                         Result* IGL_NULLABLE outResult);
};

/**
 * @brief Helper class simplifying the creation of IShaderStages in common scenarios.
 */
class ShaderStagesCreator {
 public:
  ShaderStagesCreator() = delete;

  /**
   * @brief Constructs IShaderStages for a vertex and fragment shader from module source code.
   * @param device The device to construct the shader stages with.
   * @param vertexSource Null-terminated string containing vertex shader source code.
   * @param vertexEntryPoint Vertex shader entry point name.
   * @param vertexDebugName Debug name for vertex shader module.
   * @param fragmentSource Null-terminated string containing fragment shader source code.
   * @param fragmentEntryPoint Fragment shader entry point name.
   * @param fragmentDebugName Debug name for fragment shader module.
   * @param outResult Optional param to receive success or error information.
   */
  static std::unique_ptr<IShaderStages> fromModuleStringInput(const IDevice& device,
                                                              const char* IGL_NONNULL vertexSource,
                                                              std::string vertexEntryPoint,
                                                              std::string vertexDebugName,
                                                              const char* IGL_NONNULL
                                                                  fragmentSource,
                                                              std::string fragmentEntryPoint,
                                                              std::string fragmentDebugName,
                                                              Result* IGL_NULLABLE outResult);

  /**
   * @brief Constructs IShaderStages for a vertex and fragment shader from module binary code.
   * @param device The device to construct the shader stages with.
   * @param vertexData Pre-compiled vertex shader binary data.
   * @param vertexDataLength Length in bytes of the precompiled vertex shader binary data.
   * @param vertexEntryPoint Vertex shader entry point name.
   * @param vertexDebugName Debug name for vertex shader module.
   * @param fragmentData Pre-compiled fragment shader binary data.
   * @param fragmentDataLength Length in bytes of the precompiled fragment shader binary data.
   * @param fragmentEntryPoint Fragment shader entry point name.
   * @param fragmentDebugName Debug name for fragment shader module.
   * @param outResult Optional param to receive success or error information.
   */
  static std::unique_ptr<IShaderStages> fromModuleBinaryInput(const IDevice& device,
                                                              const void* IGL_NONNULL vertexData,
                                                              size_t vertexDataLength,
                                                              std::string vertexEntryPoint,
                                                              std::string vertexDebugName,
                                                              const void* IGL_NONNULL fragmentData,
                                                              size_t fragmentDataLength,
                                                              std::string fragmentEntryPoint,
                                                              std::string fragmentDebugName,
                                                              Result* IGL_NULLABLE outResult);

  /**
   * @brief Constructs IShaderStages with a vertex and fragment shader from library source code.
   * @param device The device to construct the shader stages with.
   * @param librarySource Null-terminated string containing shader source code.
   * @param vertexEntryPoint Vertex shader entry point name.
   * @param fragmentEntryPoint Fragment shader entry point name.
   * @param libraryDebugName Optional debug name for the library.
   * @param outResult Optional param to receive success or error information.
   */
  static std::unique_ptr<IShaderStages> fromLibraryStringInput(const IDevice& device,
                                                               const char* IGL_NONNULL
                                                                   librarySource,
                                                               std::string vertexEntryPoint,
                                                               std::string fragmentEntryPoint,
                                                               std::string libraryDebugName,
                                                               Result* IGL_NULLABLE outResult);

  /**
   * @brief Constructs IShaderStages with a vertex and fragment shader from library binary data.
   * @param device The device to construct the shader stages with.
   * @param libraryData Pre-compiled shader binary data.
   * @param libraryDataLength Length in bytes of the precompiled shader binary data.
   * @param vertexEntryPoint Vertex shader entry point name.
   * @param fragmentEntryPoint Fragment shader entry point name.
   * @param libraryDebugName Optional debug name for the library.
   * @param outResult Optional param to receive success or error information.
   */
  static std::unique_ptr<IShaderStages> fromLibraryBinaryInput(const IDevice& device,
                                                               const void* IGL_NONNULL libraryData,
                                                               size_t libraryDataLength,
                                                               std::string vertexEntryPoint,
                                                               std::string fragmentEntryPoint,
                                                               std::string libraryDebugName,
                                                               Result* IGL_NULLABLE outResult);

  /**
   * @brief Constructs IShaderStages for a compute shader from module source code.
   * @param device The device to construct the shader stages with.
   * @param computeSource Null-terminated string containing vcompute shader source code.
   * @param computeEntryPoint Compute shader entry point name.
   * @param computeDebugName Debug name for compute shader module.
   * @param outResult Optional param to receive success or error information.
   */
  static std::unique_ptr<IShaderStages> fromModuleStringInput(const IDevice& device,
                                                              const char* IGL_NONNULL computeSource,
                                                              std::string computeEntryPoint,
                                                              std::string computeDebugName,
                                                              Result* IGL_NULLABLE outResult);

  /**
   * @brief Constructs IShaderStages for a compute shader from module binary code.
   * @param device The device to construct the shader stages with.
   * @param computeData Pre-compiled compute shader binary data.
   * @param computeDataLength Length in bytes of the precompiled compute shader binary data.
   * @param computeEntryPoint Compute shader entry point name.
   * @param computeDebugName Debug name for compute shader module.
   * @param outResult Optional param to receive success or error information.
   */
  static std::unique_ptr<IShaderStages> fromModuleBinaryInput(const IDevice& device,
                                                              const void* IGL_NONNULL computeData,
                                                              size_t computeDataLength,
                                                              std::string computeEntryPoint,
                                                              std::string computeDebugName,
                                                              Result* IGL_NULLABLE outResult);

  /**
   * @brief Constructs an IShaderStages with a vertex and fragment shader.
   * @param device The device to construct the shader stages with.
   * @param vertexModule The vertex shader module.
   * @param fragmentModule The vertex shader module.
   * @param outResult Optional param to receive success or error information.
   */
  static std::unique_ptr<IShaderStages> fromRenderModules(
      const IDevice& device,
      std::shared_ptr<IShaderModule> vertexModule,
      std::shared_ptr<IShaderModule> fragmentModule,
      Result* IGL_NULLABLE outResult);

  /**
   * @brief Constructs IShaderStages with a compute shader.
   * @param device The device to construct the shader stages with.
   * @param computeModule The compute shader module.
   * @param outResult Optional param to receive success or error information.
   */
  static std::unique_ptr<IShaderStages> fromComputeModule(
      const IDevice& device,
      std::shared_ptr<IShaderModule> computeModule,
      Result* IGL_NULLABLE outResult);
};

} // namespace igl
