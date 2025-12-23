/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12ImmediateCommands.h>

#include <igl/Assert.h>
#include <igl/d3d12/Common.h>
#include <igl/d3d12/D3D12FenceWaiter.h>

namespace igl::d3d12 {

D3D12ImmediateCommands::D3D12ImmediateCommands(ID3D12Device* device,
                                               ID3D12CommandQueue* queue,
                                               ID3D12Fence* fence,
                                               IFenceProvider* fenceProvider) :
  device_(device), queue_(queue), fence_(fence), fenceProvider_(fenceProvider) {
  IGL_DEBUG_ASSERT(device_);
  IGL_DEBUG_ASSERT(queue_);
  IGL_DEBUG_ASSERT(fence_);
  IGL_DEBUG_ASSERT(fenceProvider_);

  IGL_D3D12_LOG_VERBOSE("D3D12ImmediateCommands: Initialized (using shared fence timeline)\n");
}

D3D12ImmediateCommands::~D3D12ImmediateCommands() {
  // Wait for all in-flight operations to complete
  if (fence_) {
    for (const auto& entry : inFlightAllocators_) {
      if (fence_->GetCompletedValue() < entry.fenceValue) {
        FenceWaiter waiter(fence_, entry.fenceValue);
        Result waitResult = waiter.wait();
        if (!waitResult.isOk()) {
          IGL_LOG_ERROR(
              "D3D12ImmediateCommands::~D3D12ImmediateCommands() - Fence wait failed during "
              "cleanup: %s\n",
              waitResult.message.c_str());
        }
      }
    }
  }

  IGL_D3D12_LOG_VERBOSE("D3D12ImmediateCommands: Destroyed\n");
}

ID3D12GraphicsCommandList* D3D12ImmediateCommands::begin(Result* outResult) {
  std::lock_guard<std::mutex> lock(poolMutex_);

  // Reclaim completed allocators first
  reclaimCompletedAllocators();

  // Get or create an allocator
  Result result = getOrCreateAllocator(&currentAllocator_);
  if (!result.isOk()) {
    Result::setResult(outResult, result);
    return nullptr;
  }

  // Reset the allocator for reuse
  HRESULT hr = currentAllocator_->Reset();
  if (FAILED(hr)) {
    Result::setResult(outResult,
                      Result{Result::Code::RuntimeError, "Failed to reset command allocator"});
    return nullptr;
  }

  // Create or reset command list
  if (!cmdList_.Get()) {
    hr = device_->CreateCommandList(0,
                                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                                    currentAllocator_.Get(),
                                    nullptr,
                                    IID_PPV_ARGS(cmdList_.GetAddressOf()));
    if (FAILED(hr)) {
      Result::setResult(outResult,
                        Result{Result::Code::RuntimeError, "Failed to create command list"});
      return nullptr;
    }
  } else {
    hr = cmdList_->Reset(currentAllocator_.Get(), nullptr);
    if (FAILED(hr)) {
      Result::setResult(outResult,
                        Result{Result::Code::RuntimeError, "Failed to reset command list"});
      return nullptr;
    }
  }

  Result::setOk(outResult);
  return cmdList_.Get();
}

uint64_t D3D12ImmediateCommands::submit(bool wait, Result* outResult) {
  if (!cmdList_.Get()) {
    Result::setResult(outResult, Result{Result::Code::RuntimeError, "No active command list"});
    return 0;
  }

  // Close the command list
  HRESULT hr = cmdList_->Close();
  if (FAILED(hr)) {
    Result::setResult(outResult,
                      Result{Result::Code::RuntimeError, "Failed to close command list"});
    return 0;
  }

  // Execute command list
  ID3D12CommandList* lists[] = {cmdList_.Get()};
  queue_->ExecuteCommandLists(1, lists);

  // Get next fence value from shared timeline
  const uint64_t fenceValue = fenceProvider_->getNextFenceValue();

  // Signal fence on shared timeline
  hr = queue_->Signal(fence_, fenceValue);
  if (FAILED(hr)) {
    Result::setResult(outResult, Result{Result::Code::RuntimeError, "Failed to signal fence"});
    return 0;
  }

  // Move current allocator to in-flight list
  {
    std::lock_guard<std::mutex> lock(poolMutex_);
    inFlightAllocators_.push_back({currentAllocator_, fenceValue});
    currentAllocator_.Reset();
  }

  // Wait if requested
  if (wait) {
    Result waitResult = waitForFence(fenceValue);
    if (!waitResult.isOk()) {
      Result::setResult(outResult, waitResult);
      return 0; // Return 0 to signal failure
    }
  }

  Result::setOk(outResult);
  return fenceValue;
}

bool D3D12ImmediateCommands::isComplete(uint64_t fenceValue) const {
  if (!fence_) {
    return false;
  }
  return fence_->GetCompletedValue() >= fenceValue;
}

Result D3D12ImmediateCommands::waitForFence(uint64_t fenceValue) {
  if (!fence_) {
    return Result{Result::Code::RuntimeError, "Fence is null"};
  }

  if (isComplete(fenceValue)) {
    return Result{};
  }

  FenceWaiter waiter(fence_, fenceValue);
  return waiter.wait(); // Directly return the detailed Result
}

void D3D12ImmediateCommands::reclaimCompletedAllocators() {
  // Note: Internal helper called by begin() with poolMutex_ already held
  if (!fence_) {
    return;
  }

  const uint64_t completedValue = fence_->GetCompletedValue();

  // Move completed allocators from in-flight to available
  auto it = inFlightAllocators_.begin();
  while (it != inFlightAllocators_.end()) {
    if (it->fenceValue <= completedValue) {
      availableAllocators_.push_back({it->allocator, 0});
      it = inFlightAllocators_.erase(it);
    } else {
      ++it;
    }
  }
}

Result D3D12ImmediateCommands::getOrCreateAllocator(
    igl::d3d12::ComPtr<ID3D12CommandAllocator>* outAllocator) {
  // Try to reuse an available allocator
  if (!availableAllocators_.empty()) {
    *outAllocator = availableAllocators_.back().allocator;
    availableAllocators_.pop_back();
    return Result{};
  }

  // Create new allocator
  HRESULT hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                               IID_PPV_ARGS(outAllocator->GetAddressOf()));

  if (FAILED(hr)) {
    return Result{Result::Code::RuntimeError, "Failed to create command allocator"};
  }

  IGL_D3D12_LOG_VERBOSE("D3D12ImmediateCommands: Created new command allocator (pool size: %zu)\n",
                        availableAllocators_.size() + inFlightAllocators_.size() + 1);

  return Result{};
}

} // namespace igl::d3d12
