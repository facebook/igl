/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// @MARK:COVERAGE_EXCLUDE_FILE

// Ported from the LightweightVK sample app `008_MeshShaderFireworks.cpp`
// (https://github.com/corporateshark/lightweightvk). Uses vertex shader
// billboarding with 4 vertices per particle instead of mesh shaders.

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <random>
#include <vector>
#include <shell/shared/platform/Platform.h>
#include <shell/shared/renderSession/RenderSession.h>
#include <igl/IGL.h>

namespace igl::shell {

class FireworksSession : public RenderSession {
 public:
  explicit FireworksSession(std::shared_ptr<Platform> platform) :
    RenderSession(std::move(platform)) {}
  void initialize() noexcept override;
  void update(SurfaceTextures surfaceTextures) noexcept override;

 private:
// @fb-only
  // @fb-only
// @fb-only
  static constexpr int32_t kMaxParticles = 16384;
// @fb-only
  static constexpr int32_t kParticleTextureSize = 64;

  enum class ParticleStateMessage : uint8_t {
    None = 0,
    Kill = 1,
    Emission = 2,
  };

  struct Particle {
    glm::vec3 pos{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 baseColor{0.0f};
    glm::vec3 currentColor{0.0f};
    int32_t ttl{0};
    int32_t initialLifetime{1};
    bool alive{false};
    bool flare{false};
    bool spawnExplosion{false};
    bool fadingOut{false};
    bool emission{false};

    ParticleStateMessage step(const glm::vec3& gravity);
  };

  struct ParticleSystem {
    std::vector<Particle> particles;
    std::vector<Particle> particlesStack;
    int32_t totalParticles{0};
    int32_t queuedParticles{0};

    ParticleSystem() : particles(kMaxParticles), particlesStack(kMaxParticles) {}

    void nextFrame(const glm::vec3& gravity, const glm::vec3& viewerPos, std::mt19937& rng);
    void addParticle(const Particle& particle);
    void addExplosion(const glm::vec3& pos, const glm::vec3& viewerPos, std::mt19937& rng);
  };

  struct GpuVertex {
    glm::vec3 pos;
    glm::vec3 color;
    float flare{0.0f};
  };

  std::unique_ptr<IShaderStages> getShaderStagesForBackend(IDevice& device, bool stereoRendering);
  void generateParticleTexture(std::vector<uint8_t>& image);

  RenderPassDesc renderPass_;
  std::shared_ptr<IRenderPipelineState> renderPipelineState_;
  std::shared_ptr<IVertexInputState> vertexInput_;
  std::shared_ptr<IBuffer> vertexBuffer_;
  std::shared_ptr<IBuffer> indexBuffer_;
  std::shared_ptr<ITexture> particleTexture_;
  std::shared_ptr<ISamplerState> sampler_;
  std::shared_ptr<IBuffer> uniformBuffer_;

  std::unique_ptr<ParticleSystem> particleSystem_;
  std::vector<GpuVertex> gpuVertices_;
// @fb-only
  // @fb-only
// @fb-only
  std::mt19937 rng_{42};
  double accTime_{0.0};
  glm::mat4 sceneModelMatrix_{1.0f};
  bool sceneAnchored_{false};
  static constexpr float kTimeQuantum = 0.02f;
};

} // namespace igl::shell
