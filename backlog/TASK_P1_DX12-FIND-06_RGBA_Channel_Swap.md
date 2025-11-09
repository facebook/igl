# TASK_P1_DX12-FIND-06: Fix RGBA Texture Channel Swap Bug

**Priority:** P1 - High (**URGENT** - Visual Corruption)
**Estimated Effort:** 2-3 hours
**Status:** Open
**Issue ID:** DX12-FIND-06 (from Codex Audit)

---

## Problem Statement

The `Texture::upload()` function incorrectly swaps R and B channels for `RGBA_UNorm8` and `RGBA_SRGB` textures, even though DXGI formats store RGBA data directly without requiring channel swapping. This corrupts all RGBA texture uploads, turning red pixels blue and vice versa.

### Current Behavior
- `needsRGBAChannelSwap()` returns `true` for `RGBA_UNorm8` and `RGBA_SRGB`
- Upload code swaps R ↔ B bytes
- Textures created with `DXGI_FORMAT_R8G8B8A8_*` (correct format)
- **Result:** RGBA input data becomes BGRA on GPU

### Expected Behavior
- RGBA formats should NOT swap channels
- Only BGRA formats should swap (if input data is RGBA but format is BGRA)
- Or: Match input data format to DXGI format without swapping

---

## Evidence and Code Location

**File:** `src/igl/d3d12/Texture.cpp`

**Search Pattern 1 - needsRGBAChannelSwap() Function:**
Find function around line 13-25 (from Codex):
```cpp
bool needsRGBAChannelSwap(TextureFormat format) {
    // PROBLEM: Returns true for RGBA formats
    return format == TextureFormat::RGBA_UNorm8 ||
           format == TextureFormat::RGBA_SRGB;
    // ← This is WRONG! RGBA formats don't need swapping
}
```

**Search Pattern 2 - Upload Code Using This Function:**
Find `Texture::upload()` function around line 213-242:
```cpp
if (needsRGBAChannelSwap(format_)) {
    // Swap R and B channels
    swapRGBAToBGRA(data);  // ← Incorrectly swaps RGBA textures
}
```

**Search Keywords:**
- `needsRGBAChannelSwap`
- `swapRGBAToBGRA`
- `RGBA_UNorm8`
- `RGBA_SRGB`
- Channel swap
- `Texture::upload`

---

## Impact

**Severity:** High - **URGENT VISUAL BUG**
**Visibility:** All RGBA textures corrupted
**User Impact:** Red/blue color channels swapped in all RGBA content

**Affected Content:**
- All RGBA textures (most common format)
- UI textures
- Color-critical content (skin tones, branding, etc.)
- Screenshots, renders, all visual output

**Why This is Critical:**
- Immediately visible to users
- Affects all RGBA texture usage
- Easy to notice (red becomes blue)
- Breaks visual fidelity

---

## Official References

### Microsoft Documentation

1. **DXGI Formats** - [DXGI_FORMAT](https://learn.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format)
   - `DXGI_FORMAT_R8G8B8A8_UNORM`: Red in byte 0, Green in byte 1, Blue in byte 2, Alpha in byte 3
   - `DXGI_FORMAT_B8G8R8A8_UNORM`: Blue in byte 0, Green in byte 1, Red in byte 2, Alpha in byte 3

2. **Channel Order:**
   - RGBA formats: R is first byte, B is third byte
   - BGRA formats: B is first byte, R is third byte
   - **If data is RGBA and format is R8G8B8A8, NO SWAP needed**

### Correct Logic

**When to swap:**
- Input data format != DXGI format
  - Example: Input is BGRA, DXGI format is `R8G8B8A8` → Swap
  - Example: Input is RGBA, DXGI format is `B8G8R8A8` → Swap

**When NOT to swap:**
- Input is RGBA, DXGI format is `R8G8B8A8` → NO swap
- Input is BGRA, DXGI format is `B8G8R8A8` → NO swap

---

## Implementation Guidance

### High-Level Approach

**Option 1: Remove Swap for RGBA (Simplest)**
- If all RGBA input data is actually RGBA, just don't swap
- Change `needsRGBAChannelSwap()` to return `false` for RGBA formats

**Option 2: Fix Logic (Correct)**
- Determine actual input data format (RGBA vs BGRA)
- Compare with DXGI format
- Swap only when mismatch

**Recommended: Option 1 (Simple Fix)**
- Assumes IGL always provides data matching format name
- `TextureFormat::RGBA_UNorm8` → data is RGBA
- `TextureFormat::BGRA_UNorm8` → data is BGRA

### Detailed Steps

**Step 1: Locate needsRGBAChannelSwap() Function**

File: `src/igl/d3d12/Texture.cpp` (around line 13-25)

**Step 2: Fix the Function**

Before (BROKEN):
```cpp
bool needsRGBAChannelSwap(TextureFormat format) {
    return format == TextureFormat::RGBA_UNorm8 ||
           format == TextureFormat::RGBA_SRGB;
}
```

After (FIXED):
```cpp
bool needsRGBAChannelSwap(TextureFormat format) {
    // Only swap if input data is BGRA but we need RGBA (or vice versa)
    // Since IGL TextureFormat names match data layout, RGBA formats don't need swapping
    // when creating R8G8B8A8 DXGI textures

    // Swap only for BGRA formats if needed (check DXGI format mapping)
    // For now, assume RGBA input → R8G8B8A8 DXGI format = NO SWAP
    return false;  // ← Simple fix: don't swap RGBA

    // Alternative: Only swap for actual BGRA formats
    // return format == TextureFormat::BGRA_UNorm8 ||
    //        format == TextureFormat::BGRA_SRGB;
}
```

**Step 3: Verify DXGI Format Mapping**

Find where `TextureFormat` maps to `DXGI_FORMAT`:
```cpp
DXGI_FORMAT toDXGIFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::RGBA_UNorm8:
            return DXGI_FORMAT_R8G8B8A8_UNORM;  // ✓ Correct mapping
        case TextureFormat::RGBA_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;  // ✓ Correct
        case TextureFormat::BGRA_UNorm8:
            return DXGI_FORMAT_B8G8R8A8_UNORM;  // ✓ Correct
        // ...
    }
}
```

If RGBA maps to R8G8B8A8 (correct), then RGBA input data doesn't need swapping.

**Step 4: Test Fix**

Simple test:
```cpp
// Upload RGBA texture with pure red (255, 0, 0, 255)
uint8_t redPixel[] = {255, 0, 0, 255};  // R=255, G=0, B=0, A=255

// Upload to RGBA_UNorm8 texture
texture->upload(..., redPixel, ...);

// Readback and verify: Should still be (255, 0, 0, 255), not (0, 0, 255, 255)
```

**Step 5: Consider BGRA Format**

If `TextureFormat::BGRA_UNorm8` exists:
```cpp
bool needsRGBAChannelSwap(TextureFormat format) {
    // Check if this is a BGRA format but we're creating R8G8B8A8 DXGI texture
    // OR if RGBA format but creating B8G8R8A8 DXGI texture

    // Simple approach: assume no swap needed if format name matches DXGI layout
    return false;
}
```

---

## Testing Requirements

### Mandatory Tests
```bash
test_unittests.bat
test_all_sessions.bat
```

**Critical Sessions:**
- **Textured3DCubeSession** - Uses textures, check colors
- **TQSession** - Texture quality
- **MRTSession** - Multiple render targets with textures
- Any session with textured rendering

### Validation Steps

1. **Visual Validation** (**CRITICAL**)
   - Run textured sessions
   - Check if red is red (not blue)
   - Verify all colors correct
   - Compare screenshots before/after fix

2. **Pixel-Perfect Validation**
   - Upload texture with known RGBA pattern
   - Read back pixel data
   - Verify channels in correct order

3. **Test Matrix:**
   - RGBA textures → Should show correctly
   - BGRA textures (if supported) → Should show correctly
   - Various texture formats

4. **Regression Check:**
   - All sessions pass
   - No new visual artifacts
   - Colors match expected output

---

## Success Criteria

- [ ] `needsRGBAChannelSwap()` returns correct value for RGBA formats
- [ ] RGBA textures display with correct colors (red is red, blue is blue)
- [ ] No channel swapping for R8G8B8A8 DXGI formats with RGBA input
- [ ] All unit tests pass
- [ ] All render sessions pass
- [ ] Visual verification: Colors are correct
- [ ] Screenshots match expected output
- [ ] User confirms colors are correct (visual inspection)

---

## Dependencies

**Depends On:**
- None (isolated bug fix)

**Blocks:**
- Visual quality of all textured rendering
- Correct color reproduction
- Production readiness

---

## Restrictions

1. **Test Immutability:** ❌ DO NOT modify test scripts
2. **User Confirmation Required:** ⚠️ **VISUAL INSPECTION MANDATORY**
3. **Visual Validation:** Must compare before/after screenshots
4. **Code Scope:** ✅ Modify `Texture.cpp` only

---

**Estimated Timeline:** 2-3 hours
**Risk Level:** Low (simple logic fix, easy to validate visually)
**Validation Effort:** 1-2 hours (visual inspection of sessions)

**URGENCY NOTE:** This is a highly visible bug. Should be fixed early for visual correctness.

---

*Task Created: 2025-11-08*
*Issue Source: DX12 Codex Audit - Finding 06*
