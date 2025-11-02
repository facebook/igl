#!/bin/bash
# Test all D3D12 render sessions for errors

sessions="EmptySession BasicFramebufferSession HelloWorldSession ThreeCubesRenderSession Textured3DCubeSession DrawInstancedSession MRTSession BindGroupSession"

for session in $sessions; do
  echo "=== Testing $session ==="
  timeout 10 ./build/shell/Debug/${session}_d3d12.exe --viewport-size 640x360 --screenshot-number 0 --screenshot-file /tmp/${session}.png 2>&1 | grep -E "Error|Device removed|ABORT|should NOT be reached" | head -3
  echo ""
done
