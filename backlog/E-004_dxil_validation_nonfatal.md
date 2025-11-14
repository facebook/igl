# E-004: DXIL Validation Failures Treated as Non-Fatal

## Severity
**HIGH** - Silently accepts invalid DXIL and continues execution

## Issue Description
The DXC compiler's DXIL validation failures are logged at INFO level and the code continues with unsigned/invalid DXIL bytecode:

```
[INFO] DXIL validation failed: <error details>
// Code continues - unsigned shader is used anyway
```

This is a critical issue because:
- Invalid DXIL causes undefined GPU behavior
- Shader execution becomes unpredictable
- GPU hangs or crashes may occur later
- Debug information about shader issues is lost

The validation failure indicates the shader compiler generated invalid bytecode, which should never be used in any build.

## Location
- **File**: `src/igl/d3d12/DXCCompiler.cpp`
- **Lines**: 182-222
- **Function**: Shader compilation with DXIL validation

## Root Cause
Validation failures are treated as warnings rather than fatal errors. The assumption that validation failures are non-critical is incorrect - invalid DXIL should always be rejected.

## Impact
- Invalid shaders silently accepted and used in rendering
- GPU hangs or incorrect rendering at runtime
- Difficult to diagnose because shader validation errors are buried in logs
- Production builds ship with invalid shaders
- Security risk if invalid DXIL can be exploited

## Fix Approach
Make DXIL validation failures hard errors in debug and development builds:

1. Classify validation failures as FATAL in debug/profile builds
2. Log validation errors as ERROR level instead of INFO
3. Return failure from shader compilation
4. Propagate error to caller for proper error handling
5. Optional: Create debug shader warnings in release builds

### Implementation Steps
1. Locate DXIL validation check in DXCCompiler.cpp around line 182-222

2. Change validation failure handling:
   ```cpp
   if (FAILED(validationResult)) {
       // Log as ERROR not INFO
       IGL_LOG_ERROR("DXIL validation failed: %s", errorDetails);

       #if IGL_DEBUG || IGL_PROFILE
       // Hard error in debug builds
       return Result(Result::Code::ValidationError,
                     "Shader DXIL validation failed");
       #else
       // Warning in release, but allow for now
       IGL_LOG_WARN("Proceeding with unvalidated shader");
       #endif
   }
   ```

3. Update all callers to handle validation failure Result code
4. Add Result::Code::ValidationError if not already present
5. Update error messages to be descriptive

## Testing
- Create invalid shader that fails DXIL validation
- Verify compilation returns error in debug builds
- Verify error is properly propagated to caller
- Verify error message is helpful for debugging
- Run existing shader tests to verify valid shaders still work
- Test in both debug and release builds
