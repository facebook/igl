# E-003: Weak Bytecode Validation in ShaderModule

## Severity
**HIGH** - Accepts corrupted or malformed DXIL as valid

## Issue Description
The `validateBytecode()` function only performs a superficial validation by checking for a 4-byte signature at the start of the bytecode buffer. This is insufficient for DXIL validation:

- Only checks for magic number (0x4C495844 for DXIL)
- Does not validate DXIL structure or contents
- Does not verify bytecode integrity
- Does not check for required DXIL sections
- Corrupted bytecode with valid signature passes validation

A malformed DXIL bytecode with correct signature could:
- Cause GPU hangs
- Result in shader compilation failures at runtime
- Lead to unexpected shader behavior or rendering errors

## Location
- **File**: `src/igl/d3d12/ShaderModule.cpp`
- **Lines**: 159-186
- **Function**: `ShaderModule::validateBytecode()`

## Root Cause
The validation is a stub implementation that only checks for presence of the DXIL magic bytes, not actual DXIL compliance. Proper validation requires using the DirectX Shader Compiler's validation API.

## Impact
- Invalid DXIL bytecode accepted without error detection
- Runtime shader compilation failures not caught at shader creation
- Potential GPU hangs or rendering corruption
- Difficult to diagnose shader issues since they appear valid during creation

## Fix Approach
Use DirectX Shader Compiler's `IDxcValidator::Validate()` API for proper DXIL validation:

1. Get the DXC Validator from the DXC library
2. Pass bytecode to `IDxcValidator::Validate()`
3. Check validation result
4. Report validation errors properly

### Implementation Steps
1. Include DXC validator interface:
   ```cpp
   #include "dxc/dxcapi.h"
   #include "dxc/Support/dxcapi.use.h"
   ```

2. Implement proper validation:
   ```cpp
   bool ShaderModule::validateBytecode(const std::vector<uint8_t>& bytecode) {
       // Create DXC library and validator
       CComPtr<IDxcLibrary> library;
       CComPtr<IDxcValidator> validator;

       // Get validator from library
       HrCheck(library->CreateBlobFromPinned(
           bytecode.data(),
           static_cast<uint32_t>(bytecode.size()),
           DXC_CP_ACP,
           &blob
       ));

       CComPtr<IDxcOperationResult> result;
       HrCheck(validator->Validate(blob, DXC_VALIDATOR_FLAGS_DEFAULT, &result));

       // Check validation status
       HRESULT validationStatus;
       HrCheck(result->GetStatus(&validationStatus));

       return SUCCEEDED(validationStatus);
   }
   ```

3. Return failure if validation errors exist
4. Log detailed error messages from validator

## Testing
- Create unit tests with valid and invalid DXIL bytecode
- Verify valid DXIL passes validation
- Verify corrupted DXIL is rejected with error
- Test with various DXIL versions and formats
- Run regression tests on all existing shaders
