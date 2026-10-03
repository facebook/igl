/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/webgpu/ShaderModule.h>

#include <algorithm>
#include <cstring>
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

Result compileWgsl(const WebGPUContext& ctx,
                   const char* IGL_NULLABLE source,
                   const std::string& debugName,
                   WgslModule& outModule) {
  if (source == nullptr || *source == '\0') {
    return Result(Result::Code::ArgumentInvalid, "Empty WGSL source");
  }
  WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
  wgsl.code = toWGPUStringView(source);
  WGPUShaderModuleDescriptor moduleDesc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
  moduleDesc.nextInChain = &wgsl.chain;
  moduleDesc.label = toWGPUStringView(debugName);

  ctx.pushErrorScope(WGPUErrorFilter_Validation);
  Handle<WGPUShaderModule> module(wgpuDeviceCreateShaderModule(ctx.getDevice(), &moduleDesc));

  // Without waiting, compilation errors arrive through the latched error scope.
  const bool waits = ctx.waitsForErrors(ErrorScopeKind::Pipeline);
  const auto state = std::make_shared<CompilationInfoState>();
  WGPUFuture infoFuture = {};
  if (waits) {
    const WGPUCompilationInfoCallbackInfo callbackInfo = {
        .nextInChain = nullptr,
        .mode = WGPUCallbackMode_WaitAnyOnly,
        .callback = onCompilationInfo,
        .userdata1 = new std::shared_ptr<CompilationInfoState>(state),
        .userdata2 = nullptr,
    };
    infoFuture = wgpuShaderModuleGetCompilationInfo(module.get(), callbackInfo);
  }
  // Error scopes are one stack per device, and under JSPI a wait suspends to the event loop. The
  // scope is popped before waiting, so errors of other work on the device meanwhile cannot land in
  // it.
  Result validation = ctx.popErrorScope(ErrorScopeKind::Pipeline);
  const bool completed = !waits || ctx.waitFuture(infoFuture);

  if (state->hasErrors) {
    return Result(Result::Code::ArgumentInvalid,
                  "WGSL compilation failed (" + debugName + "):\n" + state->messages);
  }
  if (!validation.isOk()) {
    return validation;
  }
  if (waits && (!completed || !state->completed)) {
    return Result(Result::Code::RuntimeError, "Shader compilation info unavailable");
  }
  auto reflection = std::make_shared<WgslReflection>();
  Result result = parseWgslReflection(
      std::string_view(static_cast<const char* IGL_NONNULL>(source)), *reflection);
  if (!result.isOk()) {
    return result;
  }
  outModule = {.module = std::move(module), .reflection = std::move(reflection)};
  return Result();
}

ShaderModule::ShaderModule(ShaderModuleInfo info, WgslModule module) :
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
  WgslModule module;
  Result result = compileWgsl(ctx, desc.input.source, desc.debugName, module);
  if (!result.isOk()) {
    Result::setResult(outResult, std::move(result));
    return nullptr;
  }
  return create(module, desc.info, outResult);
}

std::shared_ptr<ShaderModule> ShaderModule::create(const WgslModule& module,
                                                   const ShaderModuleInfo& info,
                                                   Result* IGL_NULLABLE outResult) {
  const WgslEntryPoint* entryPoint = module.reflection->findEntryPoint(info.entryPoint);
  if (entryPoint == nullptr || entryPoint->stage != info.stage) {
    Result::setResult(outResult,
                      Result::Code::ArgumentInvalid,
                      "No " +
                          std::string(info.stage == ShaderStage::Vertex     ? "vertex"
                                      : info.stage == ShaderStage::Fragment ? "fragment"
                                                                            : "compute") +
                          " entry point '" + info.entryPoint + "' in the WGSL module");
    return nullptr;
  }
  Result::setOk(outResult);
  return std::make_shared<ShaderModule>(info, module);
}

PipelineConstants ShaderModule::getPipelineConstants() const {
  PipelineConstants constants;
  const FunctionConstantValues& values = info().functionConstantValues;
  const auto& entries = values.getConstantValues();
  std::vector<double> numbers;
  for (size_t id = 0; id < entries.size(); ++id) {
    const FunctionConstantValues::Entry& entry = entries[id];
    if (entry.type == ConstantValueType::Invalid) {
      continue;
    }
    const bool declared = std::any_of(getReflection().overrides.begin(),
                                      getReflection().overrides.end(),
                                      [id](const WgslOverride& o) { return o.id && *o.id == id; });
    if (!declared) {
      continue;
    }
    const uint8_t* data = values.getData().data() + entry.offset;
    double value = 0.0;
    switch (entry.type) {
    case ConstantValueType::Float1: {
      float f = 0.0f;
      std::memcpy(&f, data, sizeof(f));
      value = f;
      break;
    }
    case ConstantValueType::Int1: {
      int32_t i = 0;
      std::memcpy(&i, data, sizeof(i));
      value = i;
      break;
    }
    case ConstantValueType::Boolean1: {
      uint32_t b = 0;
      std::memcpy(&b, data, sizeof(b));
      value = b != 0 ? 1.0 : 0.0;
      break;
    }
    default:
      IGL_LOG_ERROR_ONCE("WGSL override constants are scalars; constant %zu is ignored\n", id);
      continue;
    }
    constants.keys.push_back(std::to_string(id));
    numbers.push_back(value);
  }
  constants.entries.reserve(numbers.size());
  for (size_t i = 0; i < numbers.size(); ++i) {
    constants.entries.push_back({
        .nextInChain = nullptr,
        .key = toWGPUStringView(constants.keys[i]),
        .value = numbers[i],
    });
  }
  return constants;
}

} // namespace igl::webgpu
