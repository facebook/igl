/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/renderSessions/ColorSession.h>
#include <shell/renderSessions/ComputeSession.h>
#include <shell/renderSessions/EmptySession.h>
#include <shell/renderSessions/GraphSampleSession.h>
#include <shell/renderSessions/MRTSession.h>
#include <shell/renderSessions/MSAASession.h>
#include <shell/renderSessions/ResourceTrackerSession.h>
#include <shell/renderSessions/ShaderCompilationTestSession.h>
#include <shell/renderSessions/TQMultiRenderPassSession.h>
#include <shell/renderSessions/TQSession.h>
#include <shell/renderSessions/TextureAccessorSession.h>
#include <shell/renderSessions/TextureRotationSession.h>
#include <shell/renderSessions/Textured3DCubeSession.h>
#include <shell/renderSessions/UniformArrayTestSession.h>
#include <shell/renderSessions/UniformPackedTestSession.h>
#include <shell/renderSessions/UniformTestSession.h>
#include <shell/shared/testShell/TestShell.h>

class IGLSampleTests : public igl::shell::TestShell {};

TEST_F(IGLSampleTests, ColorSession) {
  igl::shell::ColorSession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, EmptySession) {
  igl::shell::EmptySession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, GraphSampleSession) {
  igl::shell::GraphSampleSession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, MSAASession) {
  igl::shell::MSAASession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, TextureAccessorSession) {
  igl::shell::TextureAccessorSession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, TextureRotationSession) {
  igl::shell::TextureRotationSession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, Textured3DCubeSession) {
  igl::shell::Textured3DCubeSession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, TQSession) {
  igl::shell::TQSession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, ComputeSession) {
  igl::shell::ComputeSession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, MRTSession) {
  igl::shell::MRTSession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, TQMultiRenderPassSession) {
  igl::shell::TQMultiRenderPassSession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, UniformTestSession) {
  igl::shell::UniformTestSession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, UniformPackedTestSession) {
  igl::shell::UniformPackedTestSession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, UniformArrayTestSession) {
  igl::shell::UniformArrayTestSession test(platform_);
  run(test, 1);
}

TEST_F(IGLSampleTests, ResourceTrackerSession) {
#if IGL_PLATFORM_ANDROID
  constexpr size_t kNumBytesDefaultImage = 256;
  igl::shell::ResourceTrackerSession test(platform_, kNumBytesDefaultImage);
#else
  igl::shell::ResourceTrackerSession test(platform_);
#endif
  run(test, 1);
}

TEST_F(IGLSampleTests, ShaderCompilationTestSession) {
  igl::shell::ShaderCompilationTestSession test(platform_);
  run(test, 1);
}
