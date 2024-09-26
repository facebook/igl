/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/ITrackedResource.h>
#include <utility>
#include <vector>

namespace igl {

/**
 * @brief Type of shader stage in the rendering pipeline.
 */
enum class ShaderStage : uint8_t {
  /** @brief Vertex shader. */
  Vertex,
  /** @brief Fragment shader. */
  Fragment,
  /** @brief Compute shader. */
  Compute
};

/**
 * @brief Configuration used when compiling a shader to toggle features such as fast math.
 */
struct ShaderCompilerOptions {
  /** @brief Enable optimizations for floating-point arithmetic that may violate the IEEE 754
   * standard. */
  bool fastMathEnabled = true;

  bool operator==(const ShaderCompilerOptions& other) const;
  bool operator!=(const ShaderCompilerOptions& other) const;
};

/**
 * @brief Metadata about a shader module.
 */
struct ShaderModuleInfo {
  /** @brief The module's shader stage. */
  ShaderStage stage = ShaderStage::Fragment;
  /** @brief The module's entry point. */
  std::string entryPoint;
    
  std::string debugName;

  bool operator==(const ShaderModuleInfo& other) const;
  bool operator!=(const ShaderModuleInfo& other) const;
};

/**
 * @brief A enumeration of shader input types.
 */
enum class ShaderInputType : uint8_t {
  /** @brief String with shader source code. */
  String,
  /** @brief Binary data with pre-compiled shader code. */
  Binary
};

/**
 * @brief A union of the available shader input structs.
 */
struct ShaderInput {
  /**
   * @brief Null-terminated string containing shader source code.
   * @remark Only used when type is ShaderInputType::String
   */
  const char* IGL_NULLABLE source = nullptr;
  /**
   * @brief Shader compiler configuration. Only used by Metal backends.
   * @remark Only used when type is ShaderInputType::String
   */
  ShaderCompilerOptions options;
  /**
   * @brief Pointer to pre-compiled shader binary data.
   * @remark Only used when type is ShaderInputType::Binary
   */
  const void* IGL_NULLABLE data = nullptr;
  /**
   * @brief Length in bytes of pre-compiled shader binary data.
   * @remark Only used when type is ShaderInputType::Binary
   */
  size_t length = 0;
  /**
   * @brief The type of shader input.
   */
  ShaderInputType type = ShaderInputType::String;

  /**
   * @brief Returns true if the input is valud.
   * The descriptor is valid for string input if:
   *   * source is NOT nullptr,
   *   * data is nullptr, and
   *   * length is 0.
   * The descriptor is valid for binary input if:
   *   * data is NOT nullptr,
   *   * length is greater than 0, and
   *   * source is nullptr.
   */
  [[nodiscard]] bool isValid() const noexcept;

  bool operator==(const ShaderInput& other) const;
  bool operator!=(const ShaderInput& other) const;
};

/**
 * @brief Descriptor used to construct a shader module.
 * @see igl::Device::createShaderModule
 */
struct ShaderModuleDesc {
  /** @brief Metadata about the shader module. */
  ShaderModuleInfo info;
  /** @brief The input to create the shader module. */
  ShaderInput input;
  /** @brief The module's debug name. */
  std::string debugName;

  /**
   * @brief Constructs a ShaderModuleDesc for a shader from source code.
   * @param source Null-terminated string containing compute shader source code.
   * @param info Shader module metadata.
   * @param debugName Debug name for the shader module.
   */
  static ShaderModuleDesc fromStringInput(const char* IGL_NONNULL source,
                                          igl::ShaderModuleInfo info,
                                          std::string debugName);

  /**
   * @brief Constructs a ShaderModuleDesc for a shader from binary data.
   * @param data Pre-compiled shader binary data.
   * @param dataLength Length in bytes of the precompiled shader binary data.
   * @param info Shader module metadata.
   * @param debugName Debug name for the shader module.
   */
  static ShaderModuleDesc fromBinaryInput(const void* IGL_NONNULL data,
                                          size_t dataLength,
                                          igl::ShaderModuleInfo info,
                                          std::string debugName);

  bool operator==(const ShaderModuleDesc& other) const;
  bool operator!=(const ShaderModuleDesc& other) const;
};

/**
 * @brief Descriptor used to construct a shader module.
 * @see igl::Device::createShaderModule
 */
struct ShaderLibraryDesc {
  /** @brief Metadata about each shader module the library provides. */
  std::vector<ShaderModuleInfo> moduleInfo;
  /** @brief The input to create the shader library. */
  ShaderInput input;
  /** @brief The library's debug name. */
  std::string debugName;

  /**
   * @brief Constructs a ShaderLibraryDesc for a shaders from source code.
   * @param librarySource Null-terminated string containing shader source code.
   * @param moduleInfo Vector of metadata for shader modules in the library.
   * @param libraryDebugName Debug name for the library.
   */
  static ShaderLibraryDesc fromStringInput(const char* IGL_NONNULL librarySource,
                                           std::vector<igl::ShaderModuleInfo> moduleInfo,
                                           std::string libraryDebugName);
  /**
   * @brief Constructs a ShaderLibraryDesc for a shaders from binary data.
   * @param libraryData Pre-compiled shader binary data.
   * @param libraryDataLength Length in bytes of the precompiled shader binary data.
   * @param moduleInfo Vector of metadata for shader modules in the library.
   * @param libraryDebugName Debug name for the library.
   */
  static ShaderLibraryDesc fromBinaryInput(const void* IGL_NONNULL libraryData,
                                           size_t libraryDataLength,
                                           std::vector<igl::ShaderModuleInfo> moduleInfo,
                                           std::string libraryDebugName);

  bool operator==(const ShaderLibraryDesc& other) const;
  bool operator!=(const ShaderLibraryDesc& other) const;
};

/**
 * @brief Represents an individual shader, such as a vertex shader or fragment shader.
 */
class IShaderModule : public ITrackedResource<IShaderModule> {
 protected:
  explicit IShaderModule(ShaderModuleInfo info);

 public:
  /** @brief Returns metadata about the shader module */
  [[nodiscard]] const ShaderModuleInfo& info() const noexcept;

 private:
  const ShaderModuleInfo info_;
};

/**
 * @brief A collection of compiled shaders. An individual shader module can be retrieved from a
 * library given an entry point name.
 */
class IShaderLibrary : public ITrackedResource<IShaderLibrary> {
 protected:
  explicit IShaderLibrary(std::vector<std::shared_ptr<IShaderModule>> modules);

 public:
  /**
   * @brief Retrieves a shader module from this library given an entry point name.
   * @param entryPoint The name of the desired entry point.
   * @return Shader module associated with the entry point. nullptr if one does not exist.
   */
  std::shared_ptr<IShaderModule> getShaderModule(const std::string& entryPoint);

  /**
   * @brief Retrieves a shader module from this library given an entry point name and a shader
   * stage.
   * @param stage The shader stage.
   * @param entryPoint The name of the desired entry point.
   * @return Shader module associated with the entry point and stage. nullptr if one does not exist.
   */
  std::shared_ptr<IShaderModule> getShaderModule(ShaderStage stage, const std::string& entryPoint);

 private:
  std::vector<std::shared_ptr<IShaderModule>> modules_;
};

/**
 * @brief Type of shader stage in the rendering pipeline.
 */
enum class ShaderStagesType : uint8_t {
  /** @brief Render shader stages. */
  Render,
  /** @brief Compute shader stages. */
  Compute,
};

/**
 * @brief The set of shader modules used to create a IShaderStages object.
 * @see igl::Device::createShaderStages
 */
struct ShaderStagesDesc {
  /**
   * @brief Constructs a ShaderStagesDesc for render shader stages.
   * @param vertexModule The vertex shader module.
   * @param fragmentModule The vertex shader module.
   */
  static ShaderStagesDesc fromRenderModules(std::shared_ptr<IShaderModule> vertexModule,
                                            std::shared_ptr<IShaderModule> fragmentModule);

  /**
   * @brief Constructs a ShaderStagesDesc for compute shader stages.
   * @param computeModule The compute shader module.
   */
  static ShaderStagesDesc fromComputeModule(std::shared_ptr<IShaderModule> computeModule);

  /** @brief The vertex shader module to be used in a render pipeline state. */
  std::shared_ptr<IShaderModule> vertexModule;
  /** @brief The fragment shader module to be used in a render pipeline state. */
  std::shared_ptr<IShaderModule> fragmentModule;
  /** @brief The fragment shader module to be used in a compute pipeline state. */
  std::shared_ptr<IShaderModule> computeModule;
  /** @brief The type of shader stages: render or compute. */
  ShaderStagesType type = ShaderStagesType::Render;
    
  /** @brief Identifier used for debugging */
  std::string debugName;
};

/**
 * @brief A set of shader modules used to configure a render pipeline state.
 */
class IShaderStages : public ITrackedResource<IShaderStages> {
 protected:
  explicit IShaderStages(ShaderStagesDesc desc);

 public:
  /**
   * @brief The type of shader stages: render or compute.
   */
  [[nodiscard]] ShaderStagesType getType() const noexcept;

  /**
   * @brief The vertex shader in this set of shader stages.
   * @return Vertex shader module.
   */
  [[nodiscard]] const std::shared_ptr<IShaderModule>& getVertexModule() const noexcept;

  /**
   * @brief The fragment shader in this set of shader stages.
   * @return Fragment shader module.
   */
  [[nodiscard]] const std::shared_ptr<IShaderModule>& getFragmentModule() const noexcept;

  /**
   * @brief The compute shader in this set of shader stages.
   * @return Compute shader module.
   */
  [[nodiscard]] const std::shared_ptr<IShaderModule>& getComputeModule() const noexcept;

  /**
   * @brief Checks if the IShaderStages object is valid.
   * The object is valid if it has both a vertex and fragment modules for render stages, and if it
   * has a compute module for compute stages.
   * @return True if valid; false otherwise.
   */
  [[nodiscard]] bool isValid() const noexcept;

 private:
  ShaderStagesDesc desc_;
};
} // namespace igl

/// Hashing function declarations
///
namespace std {

template<>
struct hash<igl::ShaderCompilerOptions> {
  size_t operator()(const igl::ShaderCompilerOptions& /*key*/) const;
};

template<>
struct hash<igl::ShaderModuleInfo> {
  size_t operator()(const igl::ShaderModuleInfo& /*key*/) const;
};

template<>
struct hash<igl::ShaderInput> {
  size_t operator()(const igl::ShaderInput& /*key*/) const;
};

template<>
struct hash<igl::ShaderModuleDesc> {
  size_t operator()(const igl::ShaderModuleDesc& /*key*/) const;
};

template<>
struct hash<igl::ShaderLibraryDesc> {
  size_t operator()(const igl::ShaderLibraryDesc& /*key*/) const;
};

} // namespace std
