/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12FenceWaiter.h>

namespace igl::d3d12 {

FenceWaiter::FenceWaiter(ID3D12Fence* fence, UINT64 targetValue)
    : fence_(fence), targetValue_(targetValue) {
  if (!fence_) {
    IGL_LOG_ERROR("FenceWaiter: null fence provided\n");
    return;
  }

  event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!event_) {
    IGL_LOG_ERROR("FenceWaiter: Failed to create event handle\n");
    return;
  }

  HRESULT hr = fence_->SetEventOnCompletion(targetValue_, event_);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("FenceWaiter: SetEventOnCompletion failed: 0x%08X\n", static_cast<unsigned>(hr));
    CloseHandle(event_);
    event_ = nullptr;
    return;
  }

  setupSucceeded_ = true;
}

FenceWaiter::~FenceWaiter() {
  if (event_) {
    CloseHandle(event_);
  }
}

bool FenceWaiter::isComplete() const {
  return fence_ && fence_->GetCompletedValue() >= targetValue_;
}

bool FenceWaiter::wait(DWORD timeoutMs) {
  if (!setupSucceeded_ || !event_) {
    return false;
  }

  // D-003: Re-check fence after SetEventOnCompletion to avoid TOCTOU race
  if (isComplete()) {
    return true;
  }

  DWORD waitResult = WaitForSingleObject(event_, timeoutMs);

  if (waitResult == WAIT_OBJECT_0) {
    // Verify fence actually reached target value
    UINT64 completedValue = fence_->GetCompletedValue();
    if (completedValue < targetValue_) {
      IGL_LOG_ERROR("FenceWaiter: Wait returned but fence incomplete (expected=%llu, got=%llu)\n",
                    targetValue_, completedValue);

      // CRITICAL: This indicates a GPU/driver issue (event signaled but fence not updated)
      // For INFINITE timeout, try bounded recovery; otherwise honor the timeout contract
      if (timeoutMs == INFINITE) {
        // Bounded spin as last resort for INFINITE waits only
        const int maxSpins = 10000;
        int spins = 0;
        for (; spins < maxSpins && fence_->GetCompletedValue() < targetValue_; ++spins) {
          Sleep(1);
        }

        if (fence_->GetCompletedValue() >= targetValue_) {
          IGL_D3D12_LOG_VERBOSE("FenceWaiter: Fence completed after %d recovery spins\n", spins);
          return true;
        }

        IGL_LOG_ERROR("FenceWaiter: Fence still incomplete after %d bounded spins; returning false\n",
                      maxSpins);
      }

      // Honor timeout contract: event signaled but fence incomplete = failure
      return false;
    }
    return true;
  } else if (waitResult == WAIT_TIMEOUT) {
    IGL_LOG_ERROR("FenceWaiter: Timeout waiting for fence %llu (completed=%llu)\n",
                  targetValue_, fence_->GetCompletedValue());
    return false;
  } else {
    IGL_LOG_ERROR("FenceWaiter: Wait failed with result 0x%08X\n", waitResult);
    return false;
  }
}

} // namespace igl::d3d12
