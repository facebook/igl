/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "RenderSnapshotTests.h"

#include <shell/renderSessions/BasicFramebufferSession.h>
#include <shell/renderSessions/BindlessBufferSession.h>
#include <shell/renderSessions/BufferMappingSession.h>
#include <shell/renderSessions/CheckerboardMipmapSession.h>
#include <shell/renderSessions/ClothSimulationSession.h>
#include <shell/renderSessions/ColorSession.h>
#include <shell/renderSessions/ComputeSession.h>
#include <shell/renderSessions/CopyOperationsSession.h>
#include <shell/renderSessions/DepthBiasSession.h>
#include <shell/renderSessions/DrawIndirectSession.h>
#include <shell/renderSessions/DrawInstancedSession.h>
#include <shell/renderSessions/FireworksSession.h>
#include <shell/renderSessions/FluidSimulationSession.h>
#include <shell/renderSessions/GPUTimerSession.h>
#include <shell/renderSessions/GraphSampleSession.h>
#include <shell/renderSessions/HelloSparkSLSession.h>
#include <shell/renderSessions/HelloWorldSession.h>
#include <shell/renderSessions/ImguiSession.h>
#include <shell/renderSessions/MRTSession.h>
#include <shell/renderSessions/MSAASession.h>
#include <shell/renderSessions/MeshShaderTriangleSession.h>
#include <shell/renderSessions/MultiDrawIndexedIndirectSession.h>
#include <shell/renderSessions/ScissorTestSession.h>
#include <shell/renderSessions/SpecConstantsSession.h>
#include <shell/renderSessions/SrgbMipmapGammaSession.h>
#include <shell/renderSessions/StencilOutlineSession.h>
#include <shell/renderSessions/TQMultiRenderPassSession.h>
#include <shell/renderSessions/TQSession.h>
#include <shell/renderSessions/Texture3DSession.h>
#include <shell/renderSessions/TextureAccessorSession.h>
#include <shell/renderSessions/TextureRotationSession.h>
#include <shell/renderSessions/TextureViewSession.h>
#include <shell/renderSessions/Textured3DCubeSession.h>
#include <shell/renderSessions/TinyMeshBindGroupSession.h>
#include <shell/renderSessions/TinyMeshSession.h>
#include <shell/renderSessions/UniformArrayTestSession.h>
#include <shell/renderSessions/UniformPackedTestSession.h>
#include <shell/renderSessions/UniformTestSession.h>
#include <shell/renderSessions/WireframeSession.h>
#include <shell/renderSessions/YUVColorSession.h>

namespace igl::shell {

TEST_F(RenderSnapshotTests, HelloWorldSession) {
  HelloWorldSession session(platform_);
  render(session, "HelloWorldSession");
}

TEST_F(RenderSnapshotTests, ColorSession) {
  ColorSession session(platform_);
  render(session, "ColorSession");
}

TEST_F(RenderSnapshotTests, TQSession) {
  TQSession session(platform_);
  render(session, "TQSession");
}

TEST_F(RenderSnapshotTests, CheckerboardMipmapSession) {
  CheckerboardMipmapSession session(platform_);
  render(session, "CheckerboardMipmapSession");
}

TEST_F(RenderSnapshotTests, DrawInstancedSession) {
  DrawInstancedSession session(platform_);
  render(session, "DrawInstancedSession");
}

TEST_F(RenderSnapshotTests, MRTSession) {
  MRTSession session(platform_);
  render(session, "MRTSession");
}

TEST_F(RenderSnapshotTests, TQMultiRenderPassSession) {
  TQMultiRenderPassSession session(platform_);
  render(session, "TQMultiRenderPassSession");
}

TEST_F(RenderSnapshotTests, Texture3DSession) {
  Texture3DSession session(platform_);
  render(session, "Texture3DSession");
}

TEST_F(RenderSnapshotTests, Textured3DCubeSession) {
  Textured3DCubeSession session(platform_);
  render(session, "Textured3DCubeSession");
}

// The inset shows frame 5 as read back by the texture accessor at frame 10.
TEST_F(RenderSnapshotTests, TextureAccessorSession) {
  TextureAccessorSession session(platform_);
  render(session, "TextureAccessorSession", 12);
}

TEST_F(RenderSnapshotTests, MSAASession) {
  MSAASession session(platform_);
  render(session, "MSAASession");
}

TEST_F(RenderSnapshotTests, ComputeSession) {
  if (!platform_->getDevice().hasFeature(DeviceFeatures::Compute)) {
    GTEST_SKIP() << "Compute is not supported";
  }
  ComputeSession session(platform_);
  render(session, "ComputeSession");
}

TEST_F(RenderSnapshotTests, GraphSampleSession) {
  GraphSampleSession session(platform_);
  render(session, "GraphSampleSession");
}

TEST_F(RenderSnapshotTests, TextureRotationSession) {
  TextureRotationSession session(platform_);
  render(session, "TextureRotationSession");
}

TEST_F(RenderSnapshotTests, UniformTestSession) {
  UniformTestSession session(platform_);
  render(session, "UniformTestSession");
}

TEST_F(RenderSnapshotTests, UniformPackedTestSession) {
  UniformPackedTestSession session(platform_);
  render(session, "UniformPackedTestSession");
}

TEST_F(RenderSnapshotTests, ImguiSession) {
  ImguiSession session(platform_);
  // ImGui lays windows out in the first frame and draws them from the second.
  render(session, "ImguiSession", 2);
}

TEST_F(RenderSnapshotTests, UniformArrayTestSession) {
  UniformArrayTestSession session(platform_);
  render(session, "UniformArrayTestSession");
}

TEST_F(RenderSnapshotTests, BasicFramebufferSession) {
  BasicFramebufferSession session(platform_);
  render(session, "BasicFramebufferSession");
}

TEST_F(RenderSnapshotTests, BindlessBufferSession) {
  if (backendIs({BackendType::OpenGL})) {
    GTEST_SKIP()
        << "BindlessBufferSession needs indirect/storage buffers the macOS OpenGL backend lacks";
  }
  BindlessBufferSession session(platform_);
  render(session, "BindlessBufferSession");
}

TEST_F(RenderSnapshotTests, BufferMappingSession) {
  BufferMappingSession session(platform_);
  render(session, "BufferMappingSession");
}

TEST_F(RenderSnapshotTests, ClothSimulationSession) {
  if (backendIs({BackendType::OpenGL})) {
    GTEST_SKIP() << "ClothSimulationSession needs compute, which macOS OpenGL lacks";
  }
  ClothSimulationSession session(platform_);
  render(session, "ClothSimulationSession");
}

TEST_F(RenderSnapshotTests, CopyOperationsSession) {
  if (backendIs({BackendType::OpenGL})) {
    GTEST_SKIP() << "CopyOperationsSession fails on macOS OpenGL (GL_INVALID_ENUM in bindBuffer)";
  }
  CopyOperationsSession session(platform_);
  render(session, "CopyOperationsSession");
}

TEST_F(RenderSnapshotTests, DepthBiasSession) {
  DepthBiasSession session(platform_);
  render(session, "DepthBiasSession");
}

TEST_F(RenderSnapshotTests, DrawIndirectSession) {
  if (backendIs({BackendType::OpenGL})) {
    GTEST_SKIP()
        << "DrawIndirectSession needs indirect/storage buffers the macOS OpenGL backend lacks";
  }
  DrawIndirectSession session(platform_);
  render(session, "DrawIndirectSession");
}

TEST_F(RenderSnapshotTests, FireworksSession) {
  FireworksSession session(platform_);
  render(session, "FireworksSession", 30);
}

TEST_F(RenderSnapshotTests, FluidSimulationSession) {
  if (backendIs({BackendType::OpenGL})) {
    GTEST_SKIP() << "FluidSimulationSession needs compute, which macOS OpenGL lacks";
  }
  FluidSimulationSession session(platform_);
  render(session, "FluidSimulationSession", 3);
}

TEST_F(RenderSnapshotTests, GPUTimerSession) {
  if (backendIs({BackendType::OpenGL})) {
    GTEST_SKIP() << "GPUTimerSession fails on macOS OpenGL (GL_INVALID_OPERATION querying timers)";
  }
  GPUTimerSession session(platform_);
  render(session, "GPUTimerSession", 3);
}

TEST_F(RenderSnapshotTests, HelloSparkSLSession) {
  HelloSparkSLSession session(platform_);
  render(session, "HelloSparkSLSession");
}

TEST_F(RenderSnapshotTests, MeshShaderTriangleSession) {
  if (backendIs({BackendType::OpenGL})) {
    GTEST_SKIP() << "MeshShaderTriangleSession needs mesh shaders (WebGPU has a vertex fallback)";
  }
  MeshShaderTriangleSession session(platform_);
  render(session, "MeshShaderTriangleSession");
}

TEST_F(RenderSnapshotTests, MultiDrawIndexedIndirectSession) {
  if (backendIs({BackendType::OpenGL})) {
    GTEST_SKIP()
        << "MultiDrawIndexedIndirectSession needs indirect buffers the macOS OpenGL backend lacks";
  }
  MultiDrawIndexedIndirectSession session(platform_);
  render(session, "MultiDrawIndexedIndirectSession");
}

TEST_F(RenderSnapshotTests, ScissorTestSession) {
  ScissorTestSession session(platform_);
  render(session, "ScissorTestSession");
}

TEST_F(RenderSnapshotTests, SpecConstantsSession) {
  SpecConstantsSession session(platform_);
  render(session, "SpecConstantsSession");
}

TEST_F(RenderSnapshotTests, SrgbMipmapGammaSession) {
  SrgbMipmapGammaSession session(platform_);
  render(session, "SrgbMipmapGammaSession");
}

TEST_F(RenderSnapshotTests, StencilOutlineSession) {
  useDepthStencilTexture();
  StencilOutlineSession session(platform_);
  render(session, "StencilOutlineSession");
}

TEST_F(RenderSnapshotTests, TextureViewSession) {
  if (backendIs({BackendType::Metal, BackendType::OpenGL})) {
    GTEST_SKIP() << "TextureViewSession needs DeviceFeatures::TextureViews";
  }
  TextureViewSession session(platform_);
  render(session, "TextureViewSession", 2);
}

TEST_F(RenderSnapshotTests, TinyMeshBindGroupSession) {
  if (backendIs({BackendType::OpenGL})) {
    GTEST_SKIP() << "TinyMeshBindGroupSession has GLSL that macOS OpenGL rejects";
  }
  TinyMeshBindGroupSession session(platform_);
  render(session, "TinyMeshBindGroupSession", 2);
}

TEST_F(RenderSnapshotTests, TinyMeshSession) {
  if (backendIs({BackendType::OpenGL})) {
    GTEST_SKIP() << "TinyMeshSession has GLSL that macOS OpenGL rejects";
  }
  TinyMeshSession session(platform_);
  render(session, "TinyMeshSession", 2);
}

TEST_F(RenderSnapshotTests, WireframeSession) {
  WireframeSession session(platform_);
  render(session, "WireframeSession");
}

TEST_F(RenderSnapshotTests, YUVColorSession) {
  if (backendIs({BackendType::Metal, BackendType::OpenGL})) {
    GTEST_SKIP() << "YUVColorSession has no Metal path; OpenGL cannot sample the planes on macOS";
  }
  YUVColorSession session(platform_);
  render(session, "YUVColorSession");
}

} // namespace igl::shell
