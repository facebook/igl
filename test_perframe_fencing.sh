#!/bin/bash
# Test script for per-frame fencing implementation

mkdir -p artifacts/test_perframe

SESSIONS=(
  "BasicFramebufferSession"
  "HelloWorldSession"
  "DrawInstancedSession"
  "Textured3DCubeSession"
  "ThreeCubesRenderSession"
  "TQMultiRenderPassSession"
  "MRTSession"
  "EmptySession"
  "ComputeSession"
)

echo "=== Testing Per-Frame Fencing Implementation ==="
echo "Date: $(date)"
echo ""

PASSED=0
FAILED=0

for session in "${SESSIONS[@]}"; do
  echo "Testing $session..."
  ./build/shell/Debug/${session}_d3d12.exe \
    --viewport-size 640x360 \
    --screenshot-number 0 \
    --screenshot-file artifacts/test_perframe/${session}.png \
    > artifacts/test_perframe/${session}.log 2>&1

  if [ $? -eq 0 ]; then
    echo "  ✅ PASS"
    ((PASSED++))
  else
    echo "  ❌ FAIL"
    ((FAILED++))
  fi
done

echo ""
echo "=== Test Results ==="
echo "Passed: $PASSED/${#SESSIONS[@]}"
echo "Failed: $FAILED/${#SESSIONS[@]}"
echo ""

if [ $FAILED -eq 0 ]; then
  echo "✅ All tests passed!"
  exit 0
else
  echo "❌ Some tests failed"
  exit 1
fi
