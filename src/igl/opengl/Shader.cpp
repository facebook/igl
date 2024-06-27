/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/Shader.h>

#include <cstdio>
#include <cstdlib>
#include <igl/opengl/CommandBuffer.h>
#include <igl/opengl/Device.h>
#include <igl/opengl/Errors.h>
#include <string>

#if IGL_SHADER_DUMP
#include <filesystem>
#include <fstream>
#endif // IGL_SHADER_DUMP

namespace igl::opengl {

ShaderStages::ShaderStages(const ShaderStagesDesc& desc, IContext& context) :
  IShaderStages(desc), WithContext(context) {}

ShaderStages::~ShaderStages() {
  if (programID_ != 0) {
    getContext().deleteProgram(programID_);
    programID_ = 0;
  }
}

void ShaderStages::createRenderProgram(Result* result) {
  if (!IGL_VERIFY(getVertexModule())) {
    // we need a vertex shader and a fragment shader in order to link the program
    Result::setResult(
        result, Result::Code::ArgumentInvalid, "Missing required vertex shader stage");
    return;
  }

  if (!IGL_VERIFY(getFragmentModule())) {
    // we need a vertex shader and a fragment shader in order to link the program
    Result::setResult(
        result, Result::Code::ArgumentInvalid, "Missing required fragment shader stage");
    return;
  }

  const auto& vertexShader = static_cast<ShaderModule&>(*getVertexModule());
  const auto& fragmentShader = static_cast<ShaderModule&>(*getFragmentModule());
  const GLuint vertexShaderID = vertexShader.getShaderID();
  const GLuint fragmentShaderID = fragmentShader.getShaderID();

  if (vertexShaderID == 0 || fragmentShaderID == 0) {
    // we need valid shaders in order to link the program
    Result::setResult(result, Result::Code::ArgumentInvalid, "Missing required shader stages");
    return;
  }

  // always create a new temp program ID
  // we'll set or update this object's program ID after the linking succeeds
  // otherwise we won't modify this program, so we can still use it
  const GLuint programID = getContext().createProgram();
  if (programID == 0) {
    Result::setResult(result, Result::Code::RuntimeError, "Failed to create GL program");
    return;
  }

  // attach the shaders and link them
  getContext().attachShader(programID, vertexShaderID);
  getContext().attachShader(programID, fragmentShaderID);
  getContext().linkProgram(programID);

  // detach the shaders now that they've been linked
  getContext().detachShader(programID, vertexShaderID);
  getContext().detachShader(programID, fragmentShaderID);

  // check to see if the linking succeeded
  GLint status;
  getContext().getProgramiv(programID, GL_LINK_STATUS, &status);
  if (status == GL_FALSE) {
    // Get the size of log
    GLsizei logSize = 0;
    getContext().getProgramiv(programID, GL_INFO_LOG_LENGTH, &logSize);

    // Pre-allocate vector for storage
    std::vector<GLchar> log(logSize);
    getContext().getProgramInfoLog(programID, logSize, nullptr, log.data());

    // Create actual string from it
    const std::string errorLog(log.begin(), log.end());
    IGL_LOG_ERROR("failed to link shaders:\n%s\n", errorLog.c_str());

    getContext().deleteProgram(programID);
    Result::setResult(result, Result::Code::RuntimeError, errorLog);
    return;
  }

  // now that the program successfully linked, set the program
  if (programID_ != 0) {
    getContext().deleteProgram(programID_);
  }
  programID_ = programID;

  Result::setResult(result, Result::Code::Ok);
}

void ShaderStages::createComputeProgram(Result* result) {
  if (!IGL_VERIFY(getComputeModule())) {
    // we need a vertex shader and a fragment shader in order to link the program
    Result::setResult(result, Result::Code::ArgumentInvalid, "Missing required compute shader");
    return;
  }

  const auto& shader = static_cast<ShaderModule&>(*getComputeModule());

  const GLuint shaderID = shader.getShaderID();

  if (shaderID == 0) {
    // we need valid shaders in order to link the program
    Result::setResult(result, Result::Code::ArgumentInvalid, "Missing required compute stage");
    return;
  }

  // always create a new temp program ID
  // we'll set or update this object's program ID after the linking succeeds
  // otherwise we won't modify this program, so we can still use it
  const GLuint programID = getContext().createProgram();
  if (programID == 0) {
    Result::setResult(result, Result::Code::RuntimeError, "Failed to create compute GL program");
    return;
  }

  // attach the shaders and link them
  getContext().attachShader(programID, shaderID);
  getContext().linkProgram(programID);

  // detach the shaders now that they've been linked
  getContext().detachShader(programID, shaderID);

  // check to see if the linking succeeded
  GLint status;
  getContext().getProgramiv(programID, GL_LINK_STATUS, &status);
  if (status == GL_FALSE) {
    const std::string errorLog = getProgramInfoLog(programID);
    IGL_LOG_ERROR("failed to link compute shaders:\n%s\n", errorLog.c_str());

    getContext().deleteProgram(programID);
    Result::setResult(result, Result::Code::RuntimeError, errorLog);
    return;
  }

  // now that the program successfully linked, set the program
  if (programID_ != 0) {
    getContext().deleteProgram(programID_);
  }
  programID_ = programID;

  Result::setResult(result, Result::Code::Ok);
}

// link the given shaders into this shader program
Result ShaderStages::create(const ShaderStagesDesc& /*desc*/) {
  Result result;
  if (getType() == ShaderStagesType::Render) {
    createRenderProgram(&result);
  } else if (getType() == ShaderStagesType::Compute) {
    createComputeProgram(&result);
  } else {
    IGL_ASSERT_NOT_REACHED();
  }

  return result;
}

Result ShaderStages::validate() {
  getContext().validateProgram(programID_);
  GLint status;
  getContext().getProgramiv(programID_, GL_VALIDATE_STATUS, &status);
  if (status == GL_FALSE) {
    std::string errorLog = getProgramInfoLog(programID_);
    IGL_LOG_ERROR("Failed to validate program:\n%s\n", errorLog.c_str());
    return Result(Result::Code::RuntimeError, std::move(errorLog));
  }

  return Result{};
}

void ShaderStages::bind() {
  if (getContext().shouldValidateShaders()) {
    const auto result = validate();
    IGL_ASSERT_MSG(result.isOk(), result.message.c_str());
  }
  getContext().useProgram(programID_);
}

void ShaderStages::unbind() {
  getContext().useProgram(0);
}

ShaderModule::ShaderModule(IContext& context, ShaderModuleInfo info) :
  WithContext(context), IShaderModule(std::move(info)) {}

ShaderModule::~ShaderModule() {
  if (getContext().isDestructionAllowed() && shaderID_ != 0) {
    getContext().deleteShader(shaderID_);
    shaderID_ = 0;
  }
}

// compile the shader from the given src shader code
Result ShaderModule::create(const ShaderModuleDesc& desc) {
  if (desc.input.type == ShaderInputType::Binary) {
    IGL_ASSERT_NOT_IMPLEMENTED();
    return Result(Result::Code::Unimplemented);
  }

  if (!desc.input.source || (*desc.input.source == '\0')) {
    Result result(Result::Code::ArgumentNull, "Null shader source");
    return result;
  }

  switch (desc.info.stage) {
  case ShaderStage::Vertex:
    shaderType_ = GL_VERTEX_SHADER;
    break;
  case ShaderStage::Fragment:
    shaderType_ = GL_FRAGMENT_SHADER;
    break;
  case ShaderStage::Compute:
    if (getContext().deviceFeatures().hasFeature(DeviceFeatures::Compute)) {
      shaderType_ = GL_COMPUTE_SHADER;
      break;
    } else {
      return Result(Result::Code::Unimplemented, "Compute shader for GL is not implemented");
    }
  default:
    IGL_LOG_ERROR("Shader stage type %u for GL is not supported",
                  static_cast<unsigned>(desc.info.stage));
    return Result(Result::Code::ArgumentInvalid, "Unknown shader type");
  }

  // always create a new temp shader ID
  // we'll set or update this object's shader ID after the compilation succeeds
  // otherwise we won't modify this shader
  const GLuint shaderID = getContext().createShader(shaderType_);
  if (shaderID == 0) {
    return Result(Result::Code::RuntimeError, "Failed to create shader ID");
  }

  if (!desc.debugName.empty() &&
      getContext().deviceFeatures().hasInternalFeature(InternalFeatures::DebugLabel)) {
    const GLenum identifier = getContext().deviceFeatures().hasInternalRequirement(
                                  InternalRequirement::DebugLabelExtEnumsReq)
                                  ? GL_SHADER_OBJECT_EXT
                                  : GL_SHADER;
    getContext().objectLabel(identifier, shaderID, desc.debugName.size(), desc.debugName.c_str());
  }

  // compile the shader
  const GLchar* src = (GLchar*)desc.input.source;

#if IGL_SHADER_DUMP
  auto hash = std::hash<const GLchar*>()(src);
  std::string shaderStageExt;
  switch (desc.info.stage) {
  case ShaderStage::Vertex:
    shaderStageExt = ".vert";
    break;
  case ShaderStage::Fragment:
    shaderStageExt = ".frag";
    break;
  case ShaderStage::Compute:
  default:
    shaderStageExt = ".compute";
  }

  // Replace filename with your own path according to the platform and recompile.
  // Ex. for Android your filepath should be specific to the package name:
  // /sdcard/Android/data/<packageName>/files/
  std::string filename = "/" + std::to_string(hash) + shaderStageExt + ".glsl";
  if (!std::filesystem::exists(filename)) {
    std::ofstream glslFile;
    glslFile.open(filename, std::ios::out);
    glslFile.write(src, (strlen(src)));
    glslFile.close();
    IGL_LOG_INFO("Shader dumped to file %s", filename.c_str());
  }
#endif // IGL_SHADER_DUMP

  getContext().shaderSource(shaderID, 1, &src, nullptr);
  getContext().compileShader(shaderID);

  // see if the compilation succeeded
  GLint status;
  getContext().getShaderiv(shaderID, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE) {
    // Get the size of log
    GLsizei logSize = 0;
    getContext().getShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logSize);

    // Pre-allocate vector for storage
    std::vector<GLchar> log(logSize);
    getContext().getShaderInfoLog(shaderID, logSize, nullptr, log.data());

    // Create actual string from it
    const std::string errorLog(log.begin(), log.end());
    IGL_LOG_ERROR("failed to compile %s shader:\n%s\nSource\n%s",
                  (shaderType_ == GL_VERTEX_SHADER ? "vertex" : "fragment"),
                  errorLog.c_str(),
                  src);

    // Delete shader to make sure that we don't have dangling resources
    getContext().deleteShader(shaderID);

    // Report back.
    return Result(Result::Code::ArgumentInvalid, errorLog);
  }

  // now that the shader successfully compiled, set the shader
  if (shaderID_ != 0) {
    getContext().deleteShader(shaderID_);
  }
  shaderID_ = shaderID;

  hash_ =
      std::hash<std::string_view>()(std::string_view(desc.input.source, strlen(desc.input.source)));

  return Result();
}

std::string ShaderStages::getProgramInfoLog(GLuint programID) {
  // Get the size of log
  GLsizei logSize = 0;
  getContext().getProgramiv(programID, GL_INFO_LOG_LENGTH, &logSize);

  // Pre-allocate vector for storage
  std::vector<GLchar> log(logSize);
  getContext().getProgramInfoLog(programID, logSize, nullptr, log.data());

  // Create actual string from it
  return {log.begin(), log.end()};
}

} // namespace igl::opengl
