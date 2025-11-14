# I-001: Vertex Attribute Limit Inconsistency Across Backends

**Severity:** Critical
**Category:** Cross-Platform Portability & API Inconsistency
**Status:** Open
**Related Issues:** Common.h hardcoded limits vs. dynamic backend capabilities

---

## Problem Statement

The hardcoded constant `IGL_VERTEX_ATTRIBUTES_MAX = 24` in `Common.h` creates **critical inconsistencies** across backends, causing applications to behave differently depending on the platform. Backend APIs report different maximum vertex attribute counts through `getFeatureLimits()`, but the shared vertex input structures are constrained by the hardcoded limit, leading to **validation failures, runtime errors, and cross-platform incompatibilities**.

**The Impact:**
- Applications using >24 vertex attributes fail validation on some backends
- D3D12 API supports 32 attributes but IGL limits to 24 (25% capacity wasted)
- Vulkan dynamically queries `maxVertexInputAttributes` (typically 16-32) but is constrained to 24
- Metal supports 31 attributes but is limited to 24
- OpenGL queries `GL_MAX_VERTEX_ATTRIBS` (16-32) but array size restricts to 24
- Cross-platform apps see inconsistent behavior - code that works on one backend fails on another
- Validation errors when backend reports 32 but structures only support 24
- Advanced rendering techniques (e.g., skeletal animation with many bone weights, particle systems with rich attributes) are artificially limited

**Real-World Scenario:**
```cpp
// Application creates a vertex layout with 28 attributes (valid on D3D12/Vulkan)
VertexInputStateDesc inputDesc;
inputDesc.numAttributes = 28;  // Within D3D12's 32 limit

// Backend reports 32 is supported
size_t maxAttribs;
device->getFeatureLimits(DeviceFeatureLimits::MaxVertexInputAttributes, &maxAttribs);
// maxAttribs = 32 on D3D12, 31 on Metal, varies on Vulkan/OpenGL

// BUT: VertexInputStateDesc::attributes array is only size 24!
// Attributes 24-27 are SILENTLY DROPPED or cause buffer overrun!
struct VertexInputStateDesc {
  VertexAttribute attributes[IGL_VERTEX_ATTRIBUTES_MAX];  // Only 24 slots!
  // ...
};

// Result: VALIDATION FAILURE, incorrect rendering, or crash
```

---

## Root Cause Analysis

### Current Implementation - The Mismatch:

**File:** `src/igl/Common.h:31`
```cpp
// Hardcoded limit applied to ALL backends
constexpr uint32_t IGL_VERTEX_ATTRIBUTES_MAX = 24;
```

**File:** `src/igl/VertexInputState.h:130`
```cpp
struct VertexInputStateDesc {
  // ...
  VertexAttribute attributes[IGL_VERTEX_ATTRIBUTES_MAX];  // Fixed array of 24
  // ...
};
```

### Backend-Specific Limits Reported by `getFeatureLimits()`:

#### 1. D3D12 Backend (`src/igl/d3d12/Device.cpp:2478-2481`)
```cpp
case DeviceFeatureLimits::MaxVertexInputAttributes:
  // D3D12 max vertex input slots
  result = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; // 32
  return true;
```
**API Support:** 32 attributes (D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
**IGL Limit:** 24 (33% more available but unusable!)

#### 2. Vulkan Backend (`src/igl/vulkan/Device.cpp:792-794`)
```cpp
case DeviceFeatureLimits::MaxVertexInputAttributes:
  result = limits.maxVertexInputAttributes;  // Queried from VkPhysicalDeviceLimits
  return true;
```
**API Support:** Dynamic (typically 16-32 depending on GPU)
**IGL Limit:** 24 (may exceed or be less than device capability)

#### 3. Metal Backend (`src/igl/metal/DeviceFeatureSet.mm:296-298`)
```cpp
case DeviceFeatureLimits::MaxVertexInputAttributes:
  result = 31;  // Metal feature set tables
  return true;
```
**API Support:** 31 attributes (Metal Feature Set Tables)
**IGL Limit:** 24 (23% more available but unusable!)

#### 4. OpenGL Backend (`src/igl/opengl/DeviceFeatureSet.cpp:1169-1172`)
```cpp
case DeviceFeatureLimits::MaxVertexInputAttributes:
  glContext_.getIntegerv(GL_MAX_VERTEX_ATTRIBS, &tsize);
  result = (size_t)tsize;
  return true;
```
**API Support:** Dynamic (typically 16-32 depending on implementation)
**IGL Limit:** 24 (may waste or artificially restrict capabilities)

### Why This Is Wrong:

**The Fundamental Problem:**
Backend APIs provide accurate, dynamic capability reporting through `getFeatureLimits()`, but the shared interface structures use a **smaller, hardcoded limit**. This creates a **lie** - the API tells applications "you can use 32 attributes" but the structures silently fail beyond 24.

**Validation Mismatch:**
```cpp
// Backend validation in Metal (src/igl/metal/Device.mm:241)
if (desc.numAttributes > IGL_VERTEX_ATTRIBUTES_MAX) {
  Result::setResult(outResult, Result::Code::ArgumentOutOfRange,
                    "numAttributes exceeds IGL_VERTEX_ATTRIBUTES_MAX");
  return nullptr;
}
```
This validation uses `IGL_VERTEX_ATTRIBUTES_MAX = 24`, but `getFeatureLimits()` reports 31!

**Cross-Platform Inconsistency:**
- App developed on OpenGL (16 attribs) works fine
- Same app ported to D3D12 (32 attribs) - developer tries to use 28 attributes
- `getFeatureLimits()` says 32 is OK, but runtime fails with buffer overrun
- No compile-time error, no clear validation message

---

## Official Documentation References

### D3D12 Documentation:
1. **Input Assembler Stage**:
   - [Microsoft D3D12 Input Assembler Documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/input-assembler-stage)
   - Quote: "The Input Assembler (IA) stage reads vertex data from memory and assembles primitives for use by the vertex shader."

2. **D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT**:
   - [D3D12 Constants](https://learn.microsoft.com/en-us/windows/win32/direct3d12/constants)
   - Value: `32` (defined in d3d12.h)
   - Quote: "Maximum number of vertex buffer slots that can be bound to the Input Assembler."

3. **D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT**:
   - Value: `32` (defined in d3d12.h)
   - Quote: "Maximum number of elements in a vertex input structure."

### Vulkan Documentation:
1. **VkPhysicalDeviceLimits**:
   - [Vulkan Spec - Device Limits](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceLimits.html)
   - Field: `maxVertexInputAttributes`
   - Quote: "Maximum number of vertex input attributes that can be specified for a graphics pipeline. These are described in the array of VkVertexInputAttributeDescription structures."
   - Typical Value: 16-32 (device-dependent)

2. **Vulkan Validation Layers**:
   - [Validation Layer Guide](https://vulkan.lunarg.com/doc/view/latest/windows/validation_layers.html)
   - Quote: "Applications MUST NOT exceed `maxVertexInputAttributes` or validation errors will occur."

### Metal Documentation:
1. **Metal Feature Set Tables**:
   - [Apple Metal Feature Set Tables](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf)
   - Table: "Maximum number of vertex attributes"
   - Value: `31` for all modern Metal GPUs (A7 and later)

2. **MTLVertexDescriptor**:
   - [Apple MTLVertexDescriptor](https://developer.apple.com/documentation/metal/mtlvertexdescriptor)
   - Quote: "Configure vertex attributes using the attributes array. The maximum number of attributes is 31."

### OpenGL Documentation:
1. **GL_MAX_VERTEX_ATTRIBS**:
   - [OpenGL ES 3.0 Reference](https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glGet.xhtml)
   - [OpenGL 4.6 Core Profile](https://www.khronos.org/opengl/wiki/Vertex_Specification)
   - Quote: "GL_MAX_VERTEX_ATTRIBS returns one value, the maximum number of 4-component generic vertex attributes accessible to a vertex shader."
   - Typical Value: 16-32 (implementation-dependent)

---

## Code Location Strategy

### Files to Modify:

1. **Common.h** (Update constant):
   - Location: `src/igl/Common.h:31`
   - Search for: `constexpr uint32_t IGL_VERTEX_ATTRIBUTES_MAX = 24;`
   - Context: Global IGL constants
   - Action: Change from 24 to 32 (maximum of all backends)

2. **VertexInputState.h** (Verify usage):
   - Location: `src/igl/VertexInputState.h:130`
   - Search for: `VertexAttribute attributes[IGL_VERTEX_ATTRIBUTES_MAX];`
   - Context: Fixed-size array in VertexInputStateDesc
   - Action: Verify array can accommodate 32 elements (will increase struct size by 33%)

3. **D3D12 Device.cpp** (Add validation comment):
   - Location: `src/igl/d3d12/Device.cpp:2478-2481`
   - Search for: `case DeviceFeatureLimits::MaxVertexInputAttributes:`
   - Context: getFeatureLimits() implementation
   - Action: Add static_assert to validate IGL_VERTEX_ATTRIBUTES_MAX >= D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT

4. **Metal Device.mm** (Update validation):
   - Location: `src/igl/metal/Device.mm:241`
   - Search for: `if (desc.numAttributes > IGL_VERTEX_ATTRIBUTES_MAX)`
   - Context: createRenderPipeline validation
   - Action: Verify validation logic still correct after constant change

5. **Vulkan RenderPipelineState.h** (Verify array size):
   - Location: `src/igl/vulkan/RenderPipelineState.h:184`
   - Search for: `std::array<VkVertexInputAttributeDescription, IGL_VERTEX_ATTRIBUTES_MAX>`
   - Context: Vulkan vertex input attribute array
   - Action: Ensure array can hold 32 elements

### Files to Review (No Changes Expected):

1. **OpenGL DeviceFeatureSet.cpp** (`src/igl/opengl/DeviceFeatureSet.cpp:1169-1172`)
   - Already queries `GL_MAX_VERTEX_ATTRIBS` dynamically

2. **Test Files** (`src/igl/tests/ogl/VertexInputState.cpp`, `src/igl/tests/metal/VertexInputState.mm`)
   - Verify tests still pass with larger limit

---

## Detection Strategy

### How to Reproduce the Inconsistency:

#### Test 1: Query Backend Capabilities
```cpp
// Test all backends report correct limits
auto device = createDevice(BackendType::D3D12);

size_t maxAttribs;
device->getFeatureLimits(DeviceFeatureLimits::MaxVertexInputAttributes, &maxAttribs);

// CURRENT: maxAttribs = 32 (D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
// But IGL_VERTEX_ATTRIBUTES_MAX = 24
// MISMATCH: 32 vs 24

// Verify constant matches or is smaller than backend limit
ASSERT_LE(IGL_VERTEX_ATTRIBUTES_MAX, maxAttribs)
    << "IGL limit exceeds backend capability!";

// Verify constant doesn't waste more than 25% of backend capacity
ASSERT_GE(IGL_VERTEX_ATTRIBUTES_MAX, maxAttribs * 0.75)
    << "IGL limit wastes backend capacity!";
```

#### Test 2: Create Pipeline with Maximum Attributes
```cpp
// Create vertex layout with as many attributes as backend supports
VertexInputStateDesc inputDesc;

size_t maxAttribs;
device->getFeatureLimits(DeviceFeatureLimits::MaxVertexInputAttributes, &maxAttribs);

// Try to use all attributes backend supports
inputDesc.numAttributes = maxAttribs;  // 32 on D3D12

for (size_t i = 0; i < maxAttribs; i++) {
  inputDesc.attributes[i].location = i;
  inputDesc.attributes[i].format = VertexAttributeFormat::Float4;
  inputDesc.attributes[i].offset = i * 16;
  inputDesc.attributes[i].bufferIndex = 0;
  inputDesc.inputBindings[i].stride = 16;
}

// CURRENT: FAILS with buffer overrun if maxAttribs > 24
// attributes[24..31] write beyond array bounds!

Result result;
auto pipelineState = device->createRenderPipeline(desc, &result);

// Should succeed if backend supports it
ASSERT_TRUE(result.isOk())
    << "Failed to create pipeline with " << maxAttribs << " attributes: "
    << result.message;
```

#### Test 3: Cross-Backend Consistency
```cpp
// Ensure all backends can handle IGL_VERTEX_ATTRIBUTES_MAX
std::vector<BackendType> backends = {
  BackendType::D3D12,
  BackendType::Vulkan,
  BackendType::Metal,
  BackendType::OpenGL
};

for (auto backendType : backends) {
  auto device = createDevice(backendType);

  size_t maxAttribs;
  device->getFeatureLimits(DeviceFeatureLimits::MaxVertexInputAttributes, &maxAttribs);

  // All backends should support at least IGL_VERTEX_ATTRIBUTES_MAX
  ASSERT_GE(maxAttribs, IGL_VERTEX_ATTRIBUTES_MAX)
      << "Backend " << toString(backendType)
      << " reports " << maxAttribs << " but IGL requires " << IGL_VERTEX_ATTRIBUTES_MAX;

  // Create pipeline with IGL_VERTEX_ATTRIBUTES_MAX attributes
  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = IGL_VERTEX_ATTRIBUTES_MAX;
  // ... configure attributes ...

  Result result;
  auto pipelineState = device->createRenderPipeline(desc, &result);

  ASSERT_TRUE(result.isOk())
      << "Backend " << toString(backendType) << " failed with IGL_VERTEX_ATTRIBUTES_MAX";
}
```

### Verification After Fix:

1. **IGL_VERTEX_ATTRIBUTES_MAX = 32** (matches minimum of all backends)
2. All backends report >= 32 attributes via `getFeatureLimits()`
3. Applications can create pipelines with up to 32 attributes on all backends
4. No validation errors or buffer overruns
5. Cross-platform code behaves consistently

---

## Fix Guidance

### Step-by-Step Implementation:

#### Step 1: Update IGL_VERTEX_ATTRIBUTES_MAX Constant

**File:** `src/igl/Common.h`

**Locate:** Line 31
```cpp
constexpr uint32_t IGL_VERTEX_ATTRIBUTES_MAX = 24;
```

**Change to:**
```cpp
// Maximum vertex attributes across all backends
// - D3D12: D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT = 32
// - Vulkan: VkPhysicalDeviceLimits::maxVertexInputAttributes (typically >= 16, commonly 32)
// - Metal: 31 (Metal Feature Set Tables)
// - OpenGL: GL_MAX_VERTEX_ATTRIBS (typically >= 16)
// Setting to 32 ensures compatibility with D3D12 (the most restrictive modern API)
constexpr uint32_t IGL_VERTEX_ATTRIBUTES_MAX = 32;
```

**Impact:** Increases `VertexInputStateDesc` struct size by ~33% (24 → 32 elements)

---

#### Step 2: Add Compile-Time Validation in D3D12

**File:** `src/igl/d3d12/Device.cpp`

**Locate:** Near line 2478 (inside `getFeatureLimits()` switch statement)

**Add after includes or at beginning of getFeatureLimits():**
```cpp
bool Device::getFeatureLimits(DeviceFeatureLimits featureLimits, size_t& result) const {
  // Compile-time validation: IGL constant must not exceed D3D12 API limit
  static_assert(IGL_VERTEX_ATTRIBUTES_MAX <= D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
                "IGL_VERTEX_ATTRIBUTES_MAX exceeds D3D12 vertex input limit");

  switch (featureLimits) {
    case DeviceFeatureLimits::MaxVertexInputAttributes:
      // D3D12 max vertex input slots (32)
      result = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
      IGL_ASSERT(IGL_VERTEX_ATTRIBUTES_MAX <= result);  // Runtime validation
      return true;
    // ... rest of cases ...
  }
}
```

**Rationale:** Catches any future regressions where IGL limit exceeds D3D12 capability

---

#### Step 3: Add Validation in Metal Backend

**File:** `src/igl/metal/DeviceFeatureSet.mm`

**Locate:** Line 296-298
```cpp
case DeviceFeatureLimits::MaxVertexInputAttributes:
  result = 31;
  return true;
```

**Change to:**
```cpp
case DeviceFeatureLimits::MaxVertexInputAttributes:
  result = 31;  // Metal supports 31 vertex attributes (all modern devices)
  IGL_ASSERT(IGL_VERTEX_ATTRIBUTES_MAX <= 31);  // Validate IGL constant within Metal limit
  return true;
```

**File:** `src/igl/metal/Device.mm`

**Locate:** Line 241 (validation in createRenderPipeline)
```cpp
if (desc.numAttributes > IGL_VERTEX_ATTRIBUTES_MAX) {
  Result::setResult(outResult, Result::Code::ArgumentOutOfRange,
                    "numAttributes exceeds IGL_VERTEX_ATTRIBUTES_MAX");
  return nullptr;
}
```

**Update comment:**
```cpp
// Validate against IGL constant (32) - Metal supports 31, so this is safe
if (desc.numAttributes > IGL_VERTEX_ATTRIBUTES_MAX) {
  Result::setResult(outResult, Result::Code::ArgumentOutOfRange,
                    "numAttributes exceeds IGL_VERTEX_ATTRIBUTES_MAX (32)");
  return nullptr;
}

// Additional check: Metal-specific limit
if (desc.numAttributes > 31) {
  Result::setResult(outResult, Result::Code::ArgumentOutOfRange,
                    "numAttributes exceeds Metal limit (31)");
  return nullptr;
}
```

---

#### Step 4: Validate Vulkan Backend Compatibility

**File:** `src/igl/vulkan/Device.cpp`

**Locate:** Line 792-794
```cpp
case DeviceFeatureLimits::MaxVertexInputAttributes:
  result = limits.maxVertexInputAttributes;
  return true;
```

**Add validation:**
```cpp
case DeviceFeatureLimits::MaxVertexInputAttributes:
  result = limits.maxVertexInputAttributes;  // Query from device
  // Warn if device doesn't support IGL_VERTEX_ATTRIBUTES_MAX
  if (result < IGL_VERTEX_ATTRIBUTES_MAX) {
    IGL_LOG_ERROR("Vulkan device reports %zu vertex attributes, less than IGL_VERTEX_ATTRIBUTES_MAX (%u)\n",
                  result, IGL_VERTEX_ATTRIBUTES_MAX);
  }
  return true;
```

**File:** `src/igl/vulkan/RenderPipelineState.h`

**Locate:** Line 184
```cpp
std::array<VkVertexInputAttributeDescription, IGL_VERTEX_ATTRIBUTES_MAX> vkAttributes_{};
```

**Verify:** No changes needed - `std::array` will automatically resize from 24 to 32 elements

---

#### Step 5: Validate OpenGL Backend Compatibility

**File:** `src/igl/opengl/DeviceFeatureSet.cpp`

**Locate:** Line 1169-1172
```cpp
case DeviceFeatureLimits::MaxVertexInputAttributes:
  glContext_.getIntegerv(GL_MAX_VERTEX_ATTRIBS, &tsize);
  result = (size_t)tsize;
  return true;
```

**Add validation:**
```cpp
case DeviceFeatureLimits::MaxVertexInputAttributes:
  glContext_.getIntegerv(GL_MAX_VERTEX_ATTRIBS, &tsize);
  result = (size_t)tsize;
  // Warn if OpenGL implementation doesn't support IGL_VERTEX_ATTRIBUTES_MAX
  if (result < IGL_VERTEX_ATTRIBUTES_MAX) {
    IGL_LOG_ERROR("OpenGL reports GL_MAX_VERTEX_ATTRIBS = %d, less than IGL_VERTEX_ATTRIBUTES_MAX (%u)\n",
                  tsize, IGL_VERTEX_ATTRIBUTES_MAX);
  }
  return true;
```

**Rationale:** Some older OpenGL implementations may only support 16 attributes. Log error but don't fail - let validation happen at pipeline creation time.

---

#### Step 6: Update Documentation

**File:** `src/igl/VertexInputState.h`

**Locate:** Line 130
```cpp
VertexAttribute attributes[IGL_VERTEX_ATTRIBUTES_MAX];
```

**Add comment:**
```cpp
// Maximum vertex attributes array (size = 32)
// Sized to accommodate D3D12's maximum (D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT = 32)
// Individual backends may support fewer (e.g., Metal supports 31)
VertexAttribute attributes[IGL_VERTEX_ATTRIBUTES_MAX];
```

---

## Testing Requirements

### Unit Tests (MANDATORY - Must Pass Before Proceeding):

```bash
# Run all cross-platform vertex input tests
./build/Debug/IGLTests.exe --gtest_filter="*VertexInput*"

# Backend-specific tests
./build/Debug/IGLTests.exe --gtest_filter="*D3D12*VertexInput*"
./build/Debug/IGLTests.exe --gtest_filter="*Vulkan*VertexInput*"
./build/Debug/IGLTests.exe --gtest_filter="*Metal*VertexInput*"
./build/Debug/IGLTests.exe --gtest_filter="*OpenGL*VertexInput*"
```

**Expected Results:**
- All existing tests continue to pass
- Tests that create pipelines with many attributes now succeed on all backends

### New Test Cases Required:

#### Test 1: Verify IGL_VERTEX_ATTRIBUTES_MAX Consistency

**File:** `src/igl/tests/Device.cpp` (or new cross-platform test file)

```cpp
TEST(DeviceLimits, VertexAttributeMaxConsistency) {
  auto device = createDevice();

  size_t maxAttribs;
  bool result = device->getFeatureLimits(DeviceFeatureLimits::MaxVertexInputAttributes, &maxAttribs);

  ASSERT_TRUE(result) << "getFeatureLimits failed";

  // Backend should support at least IGL_VERTEX_ATTRIBUTES_MAX
  EXPECT_GE(maxAttribs, IGL_VERTEX_ATTRIBUTES_MAX)
      << "Backend reports " << maxAttribs << " attributes, less than IGL_VERTEX_ATTRIBUTES_MAX ("
      << IGL_VERTEX_ATTRIBUTES_MAX << ")";

  // Backend should not waste more than 25% of its capacity
  EXPECT_GE(IGL_VERTEX_ATTRIBUTES_MAX, maxAttribs * 0.75)
      << "IGL_VERTEX_ATTRIBUTES_MAX (" << IGL_VERTEX_ATTRIBUTES_MAX << ") wastes backend capacity ("
      << maxAttribs << ")";
}
```

#### Test 2: Create Pipeline with Maximum Attributes

```cpp
TEST(RenderPipeline, MaxVertexAttributesSupport) {
  auto device = createDevice();

  // Create vertex shader with many attributes
  std::string vertexShader = generateVertexShaderWithNAttributes(IGL_VERTEX_ATTRIBUTES_MAX);

  // Create vertex input layout with maximum attributes
  VertexInputStateDesc inputDesc;
  inputDesc.numAttributes = IGL_VERTEX_ATTRIBUTES_MAX;
  inputDesc.numInputBindings = 1;

  for (uint32_t i = 0; i < IGL_VERTEX_ATTRIBUTES_MAX; i++) {
    inputDesc.attributes[i].location = i;
    inputDesc.attributes[i].format = VertexAttributeFormat::Float1;
    inputDesc.attributes[i].offset = i * sizeof(float);
    inputDesc.attributes[i].bufferIndex = 0;
  }

  inputDesc.inputBindings[0].stride = IGL_VERTEX_ATTRIBUTES_MAX * sizeof(float);

  // Create render pipeline
  RenderPipelineDesc pipelineDesc;
  pipelineDesc.vertexInputState = inputDesc;
  // ... configure shader stages ...

  Result result;
  auto pipelineState = device->createRenderPipeline(pipelineDesc, &result);

  ASSERT_TRUE(result.isOk()) << "Failed to create pipeline with " << IGL_VERTEX_ATTRIBUTES_MAX
                             << " attributes: " << result.message;
  ASSERT_NE(pipelineState, nullptr);
}
```

#### Test 3: Cross-Backend Maximum Attribute Test

```cpp
TEST(CrossPlatform, VertexAttributeLimitConsistency) {
  std::vector<BackendType> backends = {
#if IGL_BACKEND_D3D12
    BackendType::D3D12,
#endif
#if IGL_BACKEND_VULKAN
    BackendType::Vulkan,
#endif
#if IGL_BACKEND_METAL
    BackendType::Metal,
#endif
#if IGL_BACKEND_OPENGL
    BackendType::OpenGL,
#endif
  };

  for (auto backendType : backends) {
    auto device = createDevice(backendType);

    size_t maxAttribs;
    device->getFeatureLimits(DeviceFeatureLimits::MaxVertexInputAttributes, &maxAttribs);

    // All backends should support IGL_VERTEX_ATTRIBUTES_MAX
    // (or at least warn if they don't)
    if (maxAttribs < IGL_VERTEX_ATTRIBUTES_MAX) {
      GTEST_SKIP() << "Backend " << toString(backendType) << " reports only " << maxAttribs
                   << " attributes, less than IGL_VERTEX_ATTRIBUTES_MAX (" << IGL_VERTEX_ATTRIBUTES_MAX << ")";
    }

    // Verify pipeline creation works with IGL_VERTEX_ATTRIBUTES_MAX
    VertexInputStateDesc inputDesc;
    inputDesc.numAttributes = IGL_VERTEX_ATTRIBUTES_MAX;
    // ... setup attributes ...

    Result result;
    auto pipelineState = device->createRenderPipeline(desc, &result);

    EXPECT_TRUE(result.isOk())
        << "Backend " << toString(backendType) << " failed with IGL_VERTEX_ATTRIBUTES_MAX: "
        << result.message;
  }
}
```

### Render Sessions (MANDATORY - Must Pass Before Proceeding):

```bash
# All sessions should continue working
./test_all_sessions.bat
```

**Expected Changes:**
- No behavior change for existing sessions (all use <24 attributes)
- Memory usage may increase slightly due to larger VertexInputStateDesc structs

**Modifications Allowed:**
- None required

---

## Definition of Done

### Completion Criteria:

- [ ] `IGL_VERTEX_ATTRIBUTES_MAX` changed from 24 to 32 in Common.h
- [ ] All unit tests pass (including new vertex attribute limit tests)
- [ ] All render sessions pass
- [ ] Compile-time assertions added in D3D12 backend to validate constant
- [ ] Runtime validation added in Vulkan/OpenGL backends to warn if device doesn't support 32 attributes
- [ ] Metal backend updated with additional validation
- [ ] All backends report consistent limits via `getFeatureLimits()`
- [ ] Applications can create pipelines with up to 32 attributes on D3D12
- [ ] No validation errors or buffer overruns
- [ ] Documentation updated in VertexInputState.h

### User Confirmation Required:

⚠️ **STOP - Do NOT proceed to next task until user confirms:**

1. All cross-platform vertex input tests pass
2. Backend-specific tests pass on D3D12, Vulkan, Metal, and OpenGL
3. New maximum attribute tests pass
4. All render sessions continue working

**Post in chat:**
```
I-001 Fix Complete - Ready for Review
- Unit tests: PASS
- Render sessions: PASS
- IGL_VERTEX_ATTRIBUTES_MAX: 24 → 32
- D3D12: Supports all 32 attributes (validated)
- Vulkan: Validated against maxVertexInputAttributes
- Metal: Validated against 31 attribute limit
- OpenGL: Validated against GL_MAX_VERTEX_ATTRIBS

Awaiting confirmation to proceed.
```

---

## Related Issues

### Depends On:
- None (independent fix)

### Blocks:
- Advanced rendering techniques requiring many vertex attributes
- Cross-platform applications using skeletal animation with rich bone data
- Particle systems with many per-vertex attributes
- Validation consistency improvements

### Related:
- **I-002** (Other cross-platform limit inconsistencies)
- **C-001** (Descriptor validation improvements)
- Performance optimizations for vertex input processing

---

## Implementation Priority

**Priority:** P0 - CRITICAL (Cross-Platform Portability Blocker)
**Estimated Effort:** 2-3 hours
**Risk:** Low (well-defined change, extensive testing possible)
**Impact:** VERY HIGH - Fixes critical cross-platform inconsistency, enables advanced rendering techniques

**Rationale:**
- This is a **data-driven issue** with clear documentation across all APIs
- The fix is **low-risk**: changing a constant and adding validation
- The impact is **high**: enables 33% more vertex attributes on D3D12/Metal
- Fixes **critical inconsistency** between `getFeatureLimits()` and actual structure sizes
- Prevents **silent failures** where backend reports capacity but structures can't use it

---

## References

### D3D12:
- [Microsoft D3D12 Input Assembler Stage](https://learn.microsoft.com/en-us/windows/win32/direct3d12/input-assembler-stage)
- [D3D12 Constants Reference](https://learn.microsoft.com/en-us/windows/win32/direct3d12/constants)
- D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT = 32

### Vulkan:
- [VkPhysicalDeviceLimits](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceLimits.html)
- [Vulkan Vertex Input Attributes](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#fxvertex-input)
- maxVertexInputAttributes (typically 16-32)

### Metal:
- [Apple Metal Feature Set Tables](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf)
- [MTLVertexDescriptor](https://developer.apple.com/documentation/metal/mtlvertexdescriptor)
- Maximum vertex attributes: 31

### OpenGL:
- [OpenGL ES 3.0 - glGet Reference](https://www.khronos.org/registry/OpenGL-Refpages/es3.0/html/glGet.xhtml)
- [OpenGL Wiki - Vertex Specification](https://www.khronos.org/opengl/wiki/Vertex_Specification)
- GL_MAX_VERTEX_ATTRIBS (typically 16-32)

---

**Task Created:** 2025-11-12
**Last Updated:** 2025-11-12
**Owner:** TBD
**Reviewer:** TBD
