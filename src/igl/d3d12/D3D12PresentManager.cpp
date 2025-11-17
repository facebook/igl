/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/D3D12PresentManager.h>
#include <igl/d3d12/D3D12Context.h>
#include <d3d12sdklayers.h>
#include <vector>

namespace igl::d3d12 {

bool PresentManager::present() {
  auto* swapChain = context_.getSwapChain();
  if (!swapChain) {
    return true; // No swapchain, nothing to present
  }

  auto* device = context_.getDevice();

  // Check device status before presenting
  if (!checkDeviceStatus("before Present")) {
    return false;
  }

  // Configure VSync via environment variable
  UINT syncInterval = 1;
  UINT presentFlags = 0;
  {
    char buf[8] = {};
    if (GetEnvironmentVariableA("IGL_D3D12_VSYNC", buf, sizeof(buf)) > 0) {
      if (buf[0] == '0') {
        syncInterval = 0;
        if (context_.isTearingSupported()) {
          presentFlags |= DXGI_PRESENT_ALLOW_TEARING;
        }
      }
    }
  }

  // Present
  HRESULT presentHr = swapChain->Present(syncInterval, presentFlags);
  if (FAILED(presentHr)) {
    IGL_LOG_ERROR("PresentManager: Present failed: 0x%08X\n", static_cast<unsigned>(presentHr));

    // Check if device was removed during Present
    HRESULT deviceStatus = device->GetDeviceRemovedReason();
    if (FAILED(deviceStatus)) {
      IGL_LOG_ERROR("PresentManager: DEVICE REMOVED during Present! Reason: 0x%08X\n",
                    static_cast<unsigned>(deviceStatus));
      logInfoQueueMessages(device);
      logDredInfo(device);
      IGL_DEBUG_ASSERT(false);
    } else {
      IGL_LOG_ERROR("PresentManager: Present failed but device reports OK; check swapchain/window state\n");
    }
    // Present failed - return false regardless of whether device was removed
    return false;
  }

#ifdef IGL_DEBUG
  IGL_D3D12_LOG_VERBOSE("PresentManager: Present OK\n");
#endif

  // Check device status after Present
  if (!checkDeviceStatus("after Present")) {
    return false;
  }

  return true;
}

bool PresentManager::checkDeviceStatus(const char* contextStr) {
  auto* device = context_.getDevice();
  HRESULT deviceStatus = device->GetDeviceRemovedReason();

  if (FAILED(deviceStatus)) {
    IGL_LOG_ERROR("PresentManager: DEVICE REMOVED %s! Reason: 0x%08X\n",
                  contextStr, static_cast<unsigned>(deviceStatus));
    logInfoQueueMessages(device);
    logDredInfo(device);
    IGL_DEBUG_ASSERT(false);
    return false;
  }

  return true;
}

void PresentManager::logInfoQueueMessages(ID3D12Device* device) {
  Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
  if (FAILED(device->QueryInterface(IID_PPV_ARGS(infoQueue.GetAddressOf())))) {
    return;
  }

  UINT64 numMessages = infoQueue->GetNumStoredMessages();
  IGL_D3D12_LOG_VERBOSE("D3D12 Info Queue has %llu messages:\n", numMessages);
  for (UINT64 i = 0; i < numMessages; ++i) {
    SIZE_T messageLength = 0;
    infoQueue->GetMessage(i, nullptr, &messageLength);
    if (messageLength == 0) {
      continue;
    }
    // Use RAII vector instead of malloc/free
    std::vector<uint8_t> messageBuffer(messageLength);
    auto* message = reinterpret_cast<D3D12_MESSAGE*>(messageBuffer.data());
    if (SUCCEEDED(infoQueue->GetMessage(i, message, &messageLength))) {
      const char* severityStr = "UNKNOWN";
      switch (message->Severity) {
        case D3D12_MESSAGE_SEVERITY_CORRUPTION: severityStr = "CORRUPTION"; break;
        case D3D12_MESSAGE_SEVERITY_ERROR: severityStr = "ERROR"; break;
        case D3D12_MESSAGE_SEVERITY_WARNING: severityStr = "WARNING"; break;
        case D3D12_MESSAGE_SEVERITY_INFO: severityStr = "INFO"; break;
        case D3D12_MESSAGE_SEVERITY_MESSAGE: severityStr = "MESSAGE"; break;
      }
      IGL_D3D12_LOG_VERBOSE("  [%s] %s\n", severityStr, message->pDescription);
    }
    // messageBuffer automatically freed at end of scope
  }
}

void PresentManager::logDredInfo(ID3D12Device* device) {
#if defined(__ID3D12DeviceRemovedExtendedData1_INTERFACE_DEFINED__)
  Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
  if (FAILED(device->QueryInterface(IID_PPV_ARGS(dred.GetAddressOf())))) {
    IGL_D3D12_LOG_VERBOSE("DRED: ID3D12DeviceRemovedExtendedData1 not available.\n");
    return;
  }

  D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs = {};
  if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs)) && breadcrumbs.pHeadAutoBreadcrumbNode) {
    IGL_LOG_ERROR("DRED AutoBreadcrumbs (most recent first):\n");
    const D3D12_AUTO_BREADCRUMB_NODE1* node = breadcrumbs.pHeadAutoBreadcrumbNode;
    uint32_t nodeIndex = 0;
    constexpr uint32_t kMaxNodesToPrint = 16;
    while (node && nodeIndex < kMaxNodesToPrint) {
      const char* listName = node->pCommandListDebugNameA ? node->pCommandListDebugNameA : "<unnamed>";
      const char* queueName = node->pCommandQueueDebugNameA ? node->pCommandQueueDebugNameA : "<unnamed>";
      IGL_LOG_ERROR("  Node #%u: CommandList=%p (%s) CommandQueue=%p (%s) Breadcrumbs=%u completed=%u\n",
                    nodeIndex,
                    node->pCommandList,
                    listName,
                    node->pCommandQueue,
                    queueName,
                    node->BreadcrumbCount,
                    node->pLastBreadcrumbValue ? *node->pLastBreadcrumbValue : 0);
      if (node->pCommandHistory && node->BreadcrumbCount > 0) {
        D3D12_AUTO_BREADCRUMB_OP lastOp = node->pCommandHistory[node->BreadcrumbCount - 1];
        IGL_LOG_ERROR("    Last command: %d (history count=%u)\n", static_cast<int>(lastOp), node->BreadcrumbCount);
      }
      node = node->pNext;
      ++nodeIndex;
    }
    if (node) {
      IGL_LOG_ERROR("  ... additional breadcrumbs omitted ...\n");
    }
  } else {
    IGL_D3D12_LOG_VERBOSE("DRED: No auto breadcrumbs captured.\n");
  }

  D3D12_DRED_PAGE_FAULT_OUTPUT1 pageFault = {};
  if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&pageFault)) && pageFault.PageFaultVA != 0) {
    IGL_LOG_ERROR("DRED PageFault: VA=0x%016llx\n", pageFault.PageFaultVA);
    if (pageFault.pHeadExistingAllocationNode) {
      const auto* alloc = pageFault.pHeadExistingAllocationNode;
      IGL_LOG_ERROR("  Existing allocation: Object=%p Name=%s Type=%u\n",
                    alloc->pObject,
                    alloc->ObjectNameA ? alloc->ObjectNameA : "<unnamed>",
                    static_cast<unsigned>(alloc->AllocationType));
    }
    if (pageFault.pHeadRecentFreedAllocationNode) {
      const auto* freed = pageFault.pHeadRecentFreedAllocationNode;
      IGL_LOG_ERROR("  Recently freed allocation: Object=%p Name=%s Type=%u\n",
                    freed->pObject,
                    freed->ObjectNameA ? freed->ObjectNameA : "<unnamed>",
                    static_cast<unsigned>(freed->AllocationType));
    }
  } else {
    IGL_D3D12_LOG_VERBOSE("DRED: No page fault data available.\n");
  }
#else
  (void)device;
  IGL_D3D12_LOG_VERBOSE("DRED: Extended data interfaces not available on this SDK.\n");
#endif
}

} // namespace igl::d3d12
