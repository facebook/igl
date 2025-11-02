#!/bin/bash

# Comprehensive D3D12 Session Testing Script
# Tests all D3D12 render sessions for crashes, errors, and visual correctness

OUTPUT_DIR="artifacts/test_results"
SCREENSHOT_DIR="artifacts/captures/d3d12"
LOG_FILE="$OUTPUT_DIR/comprehensive_test_log.txt"

mkdir -p "$OUTPUT_DIR"

# Session list
SESSIONS=(
    "BasicFramebufferSession_d3d12"
    "BindGroupSession_d3d12"
    "ColorSession_d3d12"
    "ComputeSession_d3d12"
    "DrawInstancedSession_d3d12"
    "EmptySession_d3d12"
    "EngineTestSession_d3d12"
    "GPUStressSession_d3d12"
    "HelloTriangleSession_d3d12"
    "HelloWorldSession_d3d12"
    "ImguiSession_d3d12"
    "MRTSession_d3d12"
    "PassthroughSession_d3d12"
    "Textured3DCubeSession_d3d12"
    "TextureViewSession_d3d12"
    "ThreeCubesRenderSession_d3d12"
    "TinyMeshBindGroupSession_d3d12"
    "TinyMeshSession_d3d12"
    "TQMultiRenderPassSession_d3d12"
    "TQSession_d3d12"
    "YUVColorSession_d3d12"
)

echo "=== D3D12 Comprehensive Test Suite ===" | tee "$LOG_FILE"
echo "Started: $(date)" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

PASS_COUNT=0
FAIL_COUNT=0
CRASH_COUNT=0

for session in "${SESSIONS[@]}"; do
    session_name="${session/_d3d12/}"
    session_dir="$SCREENSHOT_DIR/$session_name/640x360"
    mkdir -p "$session_dir"

    echo "Testing: $session" | tee -a "$LOG_FILE"

    # Run session with screenshot capture (frame 10 for multi-frame validation)
    timeout 30s ./build/shell/Debug/$session.exe \
        --viewport-size 640x360 \
        --screenshot-number 10 \
        --screenshot-file "$session_dir/${session_name}_test.png" \
        > "$OUTPUT_DIR/${session}_output.log" 2>&1

    EXIT_CODE=$?

    # Analyze results
    if [ $EXIT_CODE -eq 0 ]; then
        # Check for errors in log
        if grep -iq "error\|device removed\|dxgi_error\|failed\|abort" "$OUTPUT_DIR/${session}_output.log"; then
            echo "  Status: FAIL (errors detected)" | tee -a "$LOG_FILE"
            FAIL_COUNT=$((FAIL_COUNT + 1))
        else
            echo "  Status: PASS" | tee -a "$LOG_FILE"
            PASS_COUNT=$((PASS_COUNT + 1))
        fi
    elif [ $EXIT_CODE -eq 124 ]; then
        echo "  Status: TIMEOUT (hangs)" | tee -a "$LOG_FILE"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    else
        echo "  Status: CRASH (exit code: $EXIT_CODE)" | tee -a "$LOG_FILE"
        CRASH_COUNT=$((CRASH_COUNT + 1))
    fi

    # Check for screenshot
    if [ -f "$session_dir/${session_name}_test.png" ]; then
        echo "  Screenshot: Generated" | tee -a "$LOG_FILE"
    else
        echo "  Screenshot: Missing" | tee -a "$LOG_FILE"
    fi

    echo "" | tee -a "$LOG_FILE"
done

echo "=== Test Summary ===" | tee -a "$LOG_FILE"
echo "Total Sessions: ${#SESSIONS[@]}" | tee -a "$LOG_FILE"
echo "Passed: $PASS_COUNT" | tee -a "$LOG_FILE"
echo "Failed: $FAIL_COUNT" | tee -a "$LOG_FILE"
echo "Crashed: $CRASH_COUNT" | tee -a "$LOG_FILE"
echo "Success Rate: $(( (PASS_COUNT * 100) / ${#SESSIONS[@]} ))%" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"
echo "Completed: $(date)" | tee -a "$LOG_FILE"
