#!/bin/bash

SESSIONS=(
  "BasicFramebufferSession"
  "HelloWorldSession"
  "ThreeCubesRenderSession"
  "Textured3DCubeSession"
  "DrawInstancedSession"
  "MRTSession"
  "ComputeSession"
  "ImguiSession"
)

mkdir -p artifacts/test_results/regression

echo "=== D3D12 Regression Test Suite ===" | tee artifacts/test_results/regression_summary.txt
echo "Testing ${#SESSIONS[@]} sessions..." | tee -a artifacts/test_results/regression_summary.txt
echo "" | tee -a artifacts/test_results/regression_summary.txt

PASSED=0
FAILED=0

for session in "${SESSIONS[@]}"; do
  echo "Testing $session..." | tee -a artifacts/test_results/regression_summary.txt
  
  ./build/shell/Debug/${session}_d3d12.exe \
    --viewport-size 640x360 \
    --screenshot-number 0 \
    --screenshot-file "artifacts/test_results/regression/${session}.png" \
    > "artifacts/test_results/regression/${session}.log" 2>&1
  
  if [ $? -eq 0 ] && [ -f "artifacts/test_results/regression/${session}.png" ]; then
    echo "  ✓ PASSED" | tee -a artifacts/test_results/regression_summary.txt
    PASSED=$((PASSED + 1))
  else
    echo "  ✗ FAILED" | tee -a artifacts/test_results/regression_summary.txt
    FAILED=$((FAILED + 1))
  fi
done

echo "" | tee -a artifacts/test_results/regression_summary.txt
echo "=== Results ===" | tee -a artifacts/test_results/regression_summary.txt
echo "PASSED: $PASSED / ${#SESSIONS[@]}" | tee -a artifacts/test_results/regression_summary.txt
echo "FAILED: $FAILED / ${#SESSIONS[@]}" | tee -a artifacts/test_results/regression_summary.txt

if [ $FAILED -eq 0 ]; then
  echo "✓ ALL TESTS PASSED" | tee -a artifacts/test_results/regression_summary.txt
  exit 0
else
  echo "✗ SOME TESTS FAILED" | tee -a artifacts/test_results/regression_summary.txt
  exit 1
fi
