/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12FenceWaiter.h>

namespace igl::d3d12 {

FenceWaiter::FenceWaiter(ID3D12Fence* fence, UINT64 targetValue) :
  fence_(fence), targetValue_(targetValue) {
  if (!fence_) {
    IGL_LOG_ERROR("FenceWaiter: null fence provided\n");
    setupErrorCode_ = Result::Code::ArgumentNull;
    setupErrorMessage_ = "Null fence provided to FenceWaiter";
    return;
  }

  event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!event_) {
    const DWORD lastError = GetLastError();
    IGL_LOG_ERROR("FenceWaiter: Failed to create event handle (LastError=0x%08X)\n", lastError);
    setupErrorCode_ = Result::Code::InvalidOperation;
    char buf[128];
    snprintf(buf, sizeof(buf), "CreateEvent failed (OS error 0x%08X)", lastError);
    setupErrorMessage_ = buf;
    return;
  }

  HRESULT hr = fence_->SetEventOnCompletion(targetValue_, event_);
  if (FAILED(hr)) {
    IGL_LOG_ERROR("FenceWaiter: SetEventOnCompletion failed: 0x%08X\n", static_cast<unsigned>(hr));
    CloseHandle(event_);
    event_ = nullptr;
    setupErrorCode_ = Result::Code::InvalidOperation;
    char buf[128];
    snprintf(buf,
             sizeof(buf),
             "SetEventOnCompletion failed (HRESULT=0x%08X)",
             static_cast<unsigned>(hr));
    setupErrorMessage_ = buf;
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

Result FenceWaiter::wait(DWORD timeoutMs) {
  // Check if setup succeeded (constructor completed event creation and SetEventOnCompletion)
  if (!setupSucceeded_ || !event_) {
    return Result(setupErrorCode_, setupErrorMessage_);
  }

  // D-003: Re-check fence after SetEventOnCompletion to avoid TOCTOU race
  if (isComplete()) {
    return Result(); // Already complete, no wait needed
  }

  DWORD waitResult = WaitForSingleObject(event_, timeoutMs);

  if (waitResult == WAIT_OBJECT_0) {
    // Verify fence actually reached target value
    UINT64 completedValue = fence_->GetCompletedValue();
    if (completedValue < targetValue_) {
      IGL_LOG_ERROR("FenceWaiter: Wait returned but fence incomplete (expected=%llu, got=%llu)\n",
                    targetValue_,
                    completedValue);

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
          return Result(); // Success after recovery
        }

        IGL_LOG_ERROR("FenceWaiter: Fence still incomplete after %d bounded spins\n", maxSpins);
      }

      // Honor timeout contract: event signaled but fence incomplete = failure
      return Result(Result::Code::RuntimeError,
                    "Fence incomplete after wait (possible GPU hang or driver issue)");
    }
    return Result(); // Success
  } else if (waitResult == WAIT_TIMEOUT) {
    const UINT64 completedValue = fence_ ? fence_->GetCompletedValue() : 0;
    IGL_LOG_ERROR("FenceWaiter: Timeout waiting for fence %llu (completed=%llu)\n",
                  targetValue_,
                  completedValue);
    return Result(Result::Code::RuntimeError, "Fence wait timed out (possible GPU hang)");
  } else {
    const DWORD lastError = GetLastError();
    IGL_LOG_ERROR(
        "FenceWaiter: Wait failed with result 0x%08X (LastError=0x%08X)\n", waitResult, lastError);
    char buf[128];
    snprintf(buf,
             sizeof(buf),
             "WaitForSingleObject failed (result=0x%08X, OS error=0x%08X)",
             waitResult,
             lastError);
    return Result(Result::Code::RuntimeError, buf);
  }
}

bool FenceWaiter::isTimeoutError(const Result& result) {
  return !result.isOk() && result.message.find("timed out") != std::string::npos;
}

} // namespace igl::d3d12
