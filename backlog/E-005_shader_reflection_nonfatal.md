# E-005: Shader Reflection Creation Failures Silently Ignored

## Severity
**HIGH** - Missing reflection data silently continues with degraded functionality

## Issue Description
Shader reflection creation failures in `Device::createShaderModule()` are logged at INFO level and execution continues without reflection data:

```
[INFO] Failed to create reflection: <error details>
// Code continues - shader is created without reflection
```

Shader reflection is critical infrastructure that provides:
- Texture/buffer binding information
- Shader input/output layouts
- Resource usage tracking
- Validation of bind groups

Without reflection:
- Bind group validation cannot work
- Resource usage cannot be verified
- Unknown shader layout causes bind errors at runtime
- Silent failures to create bind groups

## Location
- **File**: `src/igl/d3d12/Device.cpp`
- **Lines**: 2271-2301
- **Function**: `Device::createShaderModule()` reflection creation

## Root Cause
Reflection creation is treated as optional rather than mandatory. The assumption that shaders can work without reflection is incorrect - it's fundamental for proper resource binding validation.

## Impact
- Shaders created without reflection metadata
- Bind group creation can fail silently at runtime
- Resource binding mismatches not caught
- Rendering issues not properly diagnosed
- Debug/validation capabilities severely reduced
- Hard to detect shader configuration problems

## Fix Approach
Make reflection mandatory in debug builds and report failures as errors:

1. Treat reflection creation failure as fatal in debug/profile builds
2. Log reflection failures as ERROR instead of INFO
3. Return failure from `createShaderModule()` on reflection failure
4. Propagate error to caller
5. In release, log warning but allow (for robustness)

### Implementation Steps
1. Locate reflection creation in Device.cpp around line 2271-2301

2. Change failure handling:
   ```cpp
   // Create reflection
   if (FAILED(reflectionCreateResult)) {
       IGL_LOG_ERROR("Shader reflection creation failed: %s", errorDetails);

       #if IGL_DEBUG || IGL_PROFILE
       // Hard error in debug builds - reflection is mandatory
       return Result(Result::Code::ValidationError,
                     "Shader reflection creation failed - unable to determine resource bindings");
       #else
       // In release, allow but warn
       IGL_LOG_WARN("Shader created without reflection - bind group validation disabled");
       // Continue with null reflection for robustness
       #endif
   }
   ```

3. Update ShaderModule to handle null/missing reflection
4. Add validation to fail bind group creation if reflection is missing (in debug)
5. Update error messages for clarity

## Testing
- Create shader that fails reflection (modify reflection code temporarily)
- Verify compilation fails in debug builds
- Verify error propagates properly
- Verify valid shaders with valid reflection still work
- Test bind group validation with/without reflection
- Run full shader test suite
