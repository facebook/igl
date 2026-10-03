/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/ShaderModule.h>

#include <memory>
#include <string>
#include <utility>
#include <igl/webgpu/WebGPUContext.h>

namespace igl::webgpu {

namespace {

struct CompilationInfoState {
  bool completed = false;
  bool hasErrors = false;
  std::string messages;
};

void onCompilationInfo(WGPUCompilationInfoRequestStatus status,
                       const WGPUCompilationInfo* IGL_NULLABLE info,
                       void* IGL_NULLABLE userdata1,
                       void* IGL_NULLABLE /*userdata2*/) {
  // Owned by the callback: it can still run after the wait timed out.
  const std::unique_ptr<std::shared_ptr<CompilationInfoState>> holder(
      static_cast<std::shared_ptr<CompilationInfoState>*>(userdata1));
  if (!holder) {
    return;
  }
  CompilationInfoState* state = holder->get();
  state->completed = status == WGPUCompilationInfoRequestStatus_Success;
  if (info == nullptr) {
    return;
  }
  for (size_t i = 0; i < info->messageCount; ++i) {
    const WGPUCompilationMessage& message = info->messages[i];
    if (message.type != WGPUCompilationMessageType_Error) {
      continue;
    }
    state->hasErrors = true;
    state->messages += std::to_string(message.lineNum) + ":" + std::to_string(message.linePos) +
                       ": " + toStdString(message.message) + "\n";
  }
}

} // namespace

ShaderModule::ShaderModule(ShaderModuleInfo info, Handle<WGPUShaderModule> module) :
  IShaderModule(std::move(info)), module_(std::move(module)) {}

std::shared_ptr<ShaderModule> ShaderModule::create(const WebGPUContext& ctx,
                                                   const ShaderModuleDesc& desc,
                                                   Result* IGL_NULLABLE outResult) {
  if (!desc.input.isValid()) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Invalid shader input");
    return nullptr;
  }
  if (desc.input.type != ShaderInputType::String) {
    Result::setResult(outResult, Result::Code::Unsupported, "WebGPU shaders must be WGSL source");
    return nullptr;
  }
  if (desc.info.entryPoint.empty()) {
    Result::setResult(outResult, Result::Code::ArgumentInvalid, "Missing shader entry point");
    return nullptr;
  }

  WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
  wgsl.code = toWGPUStringView(desc.input.source);
  WGPUShaderModuleDescriptor moduleDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
  moduleDesc.nextInChain = &wgsl.chain;
  moduleDesc.label = toWGPUStringView(desc.debugName);

  ctx.pushErrorScope(WGPUErrorFilter_Validation);
  Handle<WGPUShaderModule> module(wgpuDeviceCreateShaderModule(ctx.getDevice(), &moduleDesc));

  const auto state = std::make_shared<CompilationInfoState>();
  const WGPUCompilationInfoCallbackInfo callbackInfo = {
      .nextInChain = nullptr,
      .mode = WGPUCallbackMode_WaitAnyOnly,
      .callback = onCompilationInfo,
      .userdata1 = new std::shared_ptr<CompilationInfoState>(state),
      .userdata2 = nullptr,
  };
  const bool completed =
      ctx.waitFuture(wgpuShaderModuleGetCompilationInfo(module.get(), callbackInfo));
  Result validation = ctx.popErrorScope();

  if (state->hasErrors) {
    Result::setResult(outResult,
                      Result::Code::ArgumentInvalid,
                      "WGSL compilation failed (" + desc.debugName + "):\n" + state->messages);
    return nullptr;
  }
  if (!validation.isOk()) {
    Result::setResult(outResult, std::move(validation));
    return nullptr;
  }
  if (!completed || !state->completed) {
    Result::setResult(outResult, Result::Code::RuntimeError, "Shader compilation info unavailable");
    return nullptr;
  }
  Result::setOk(outResult);
  return std::make_shared<ShaderModule>(desc.info, std::move(module));
}

} // namespace igl::webgpu
