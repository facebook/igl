/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/RenderPass.h>
#include <igl/Texture.h>
#include <igl/d3d12/Common.h>

// Translation of IGL render pass actions into D3D12 render pass descriptions. Everything here is a
// pure function of its arguments, so it can be tested without a device.

namespace igl::d3d12 {

[[nodiscard]] inline const char* renderPassModeName(RenderPassMode mode) {
  switch (mode) {
  case RenderPassMode::Auto:
    return "auto";
  case RenderPassMode::ForceOn:
    return "on";
  case RenderPassMode::ForceOff:
    return "off";
  }
  IGL_UNREACHABLE_RETURN("auto")
}

// Whether render command encoders record BeginRenderPass()/EndRenderPass() instead of
// OMSetRenderTargets(). Auto needs OPTIONS18 to report RenderPassesValid; `renderPassesValidKnown`
// is false when that query failed, and Auto then stays off.
[[nodiscard]] inline bool shouldUseRenderPassApi(RenderPassMode mode,
                                                 D3D12_RENDER_PASS_TIER tier,
                                                 bool tileBasedRenderer,
                                                 bool renderPassesValidKnown,
                                                 bool renderPassesValid) {
  switch (mode) {
  case RenderPassMode::ForceOn:
    return true;
  case RenderPassMode::ForceOff:
    return false;
  case RenderPassMode::Auto:
    return tier >= D3D12_RENDER_PASS_TIER_1 && tileBasedRenderer && renderPassesValidKnown &&
           renderPassesValid;
  }
  IGL_UNREACHABLE_RETURN(false)
}

[[nodiscard]] inline D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE toBeginningAccessType(
    LoadAction loadAction) {
  switch (loadAction) {
  case LoadAction::Clear:
    return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
  case LoadAction::Load:
    return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
  case LoadAction::DontCare:
    return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
  }
  IGL_UNREACHABLE_RETURN(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD)
}

// MsaaResolve only resolves when there is a resolve texture; without one nothing consumes the
// multisampled contents.
[[nodiscard]] inline D3D12_RENDER_PASS_ENDING_ACCESS_TYPE toEndingAccessType(
    StoreAction storeAction,
    bool hasResolveTexture) {
  switch (storeAction) {
  case StoreAction::Store:
    return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
  case StoreAction::DontCare:
    return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
  case StoreAction::MsaaResolve:
    return hasResolveTexture ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE
                             : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
  }
  IGL_UNREACHABLE_RETURN(D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD)
}

// Depth is resolved after the pass, which needs the multisampled contents to survive it.
[[nodiscard]] inline D3D12_RENDER_PASS_ENDING_ACCESS_TYPE toDepthStencilEndingAccessType(
    StoreAction storeAction,
    bool resolvedAfterPass) {
  switch (storeAction) {
  case StoreAction::Store:
    return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
  case StoreAction::DontCare:
    return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
  case StoreAction::MsaaResolve:
    return resolvedAfterPass ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE
                             : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
  }
  IGL_UNREACHABLE_RETURN(D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD)
}

// D3D12_RESOLVE_MODE_AVERAGE is not defined for integer formats; those resolve after the pass.
[[nodiscard]] inline bool canResolveInRenderPass(TextureFormat resolveFormat) {
  const auto properties = TextureFormatProperties::fromTextureFormat(resolveFormat);
  return properties.hasColor() && !properties.isInteger() && !properties.isCompressed();
}

// `desc` is null for a framebuffer attachment without an entry in
// RenderPassDesc::colorAttachments; it keeps its contents, as it does without the render pass API.
// An ending access of D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE leaves the resolve parameters
// to the caller.
[[nodiscard]] inline D3D12_RENDER_PASS_RENDER_TARGET_DESC makeColorRenderTargetDesc(
    D3D12_CPU_DESCRIPTOR_HANDLE rtv,
    const RenderPassDesc::AttachmentDesc* IGL_NULLABLE desc,
    DXGI_FORMAT rtvFormat,
    bool hasResolveTexture) {
  D3D12_RENDER_PASS_RENDER_TARGET_DESC target = {};
  target.cpuDescriptor = rtv;
  if (!desc) {
    target.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
    target.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    return target;
  }
  target.BeginningAccess.Type = toBeginningAccessType(desc->loadAction);
  if (target.BeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR) {
    target.BeginningAccess.Clear.ClearValue.Format = rtvFormat;
    target.BeginningAccess.Clear.ClearValue.Color[0] = desc->clearColor.r;
    target.BeginningAccess.Clear.ClearValue.Color[1] = desc->clearColor.g;
    target.BeginningAccess.Clear.ClearValue.Color[2] = desc->clearColor.b;
    target.BeginningAccess.Clear.ClearValue.Color[3] = desc->clearColor.a;
  }
  target.EndingAccess.Type = toEndingAccessType(desc->storeAction, hasResolveTexture);
  return target;
}

// The stencil accesses are D3D12_RENDER_PASS_*_ACCESS_TYPE_NO_ACCESS when the depth format has no
// stencil plane. `resolvedAfterPass` tells whether depth is resolved with ResolveSubresource()
// once the pass has ended.
[[nodiscard]] inline D3D12_RENDER_PASS_DEPTH_STENCIL_DESC makeDepthStencilDesc(
    D3D12_CPU_DESCRIPTOR_HANDLE dsv,
    const RenderPassDesc::AttachmentDesc& depth,
    const RenderPassDesc::AttachmentDesc& stencil,
    TextureFormat depthFormat,
    DXGI_FORMAT dsvFormat,
    bool resolvedAfterPass) {
  D3D12_RENDER_PASS_DEPTH_STENCIL_DESC target = {};
  target.cpuDescriptor = dsv;

  target.DepthBeginningAccess.Type = toBeginningAccessType(depth.loadAction);
  if (target.DepthBeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR) {
    target.DepthBeginningAccess.Clear.ClearValue.Format = dsvFormat;
    target.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = depth.clearDepth;
  }
  target.DepthEndingAccess.Type =
      toDepthStencilEndingAccessType(depth.storeAction, resolvedAfterPass);

  if (!TextureFormatProperties::fromTextureFormat(depthFormat).hasStencil()) {
    target.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
    target.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
    return target;
  }
  target.StencilBeginningAccess.Type = toBeginningAccessType(stencil.loadAction);
  if (target.StencilBeginningAccess.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR) {
    target.StencilBeginningAccess.Clear.ClearValue.Format = dsvFormat;
    target.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil =
        static_cast<UINT8>(stencil.clearStencil);
  }
  target.StencilEndingAccess.Type = toDepthStencilEndingAccessType(stencil.storeAction, false);
  return target;
}

// Description of a pass that continues the contents of the previous one. It differs only in the
// beginning accesses, which preserve instead of clearing or discarding.
[[nodiscard]] inline D3D12_RENDER_PASS_RENDER_TARGET_DESC asContinuation(
    const D3D12_RENDER_PASS_RENDER_TARGET_DESC& target) {
  D3D12_RENDER_PASS_RENDER_TARGET_DESC resumed = target;
  resumed.BeginningAccess = {};
  resumed.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
  return resumed;
}

[[nodiscard]] inline D3D12_RENDER_PASS_DEPTH_STENCIL_DESC asContinuation(
    const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC& target) {
  D3D12_RENDER_PASS_DEPTH_STENCIL_DESC resumed = target;
  if (resumed.DepthBeginningAccess.Type != D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS) {
    resumed.DepthBeginningAccess = {};
    resumed.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
  }
  if (resumed.StencilBeginningAccess.Type != D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS) {
    resumed.StencilBeginningAccess = {};
    resumed.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
  }
  return resumed;
}

} // namespace igl::d3d12
