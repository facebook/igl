/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>

#include <igl/Common.h>
#include <igl/SamplerState.h>
#include <igl/d3d12/SamplerState.h>

namespace igl::d3d12 {

struct SamplerCacheStats {
  size_t cacheHits = 0;
  size_t cacheMisses = 0;
  size_t activeSamplers = 0;
  float hitRate = 0.0f;
};

class D3D12SamplerCache {
 public:
  D3D12SamplerCache() = default;

  [[nodiscard]] std::shared_ptr<ISamplerState> createSamplerState(
      const SamplerStateDesc& desc,
      Result* IGL_NULLABLE outResult) const {
    const size_t samplerHash = std::hash<SamplerStateDesc>{}(desc);

    {
      std::lock_guard<std::mutex> lock(samplerCacheMutex_);

      auto it = samplerCache_.find(samplerHash);
      if (it != samplerCache_.end()) {
        std::shared_ptr<SamplerState> existingSampler = it->second.lock();

        if (existingSampler) {
          samplerCacheHits_++;
          const size_t totalRequests =
              samplerCacheHits_ + samplerCacheMisses_;
          IGL_D3D12_LOG_VERBOSE(
              "D3D12SamplerCache::createSamplerState: Cache HIT "
              "(hash=0x%zx, hits=%zu, misses=%zu, hit rate=%.1f%%)\n",
              samplerHash,
              samplerCacheHits_,
              samplerCacheMisses_,
              totalRequests > 0
                  ? 100.0 * samplerCacheHits_ /
                        static_cast<double>(totalRequests)
                  : 0.0);
          Result::setOk(outResult);
          // Upcast shared_ptr<SamplerState> -> shared_ptr<ISamplerState>
          return existingSampler;
        } else {
          samplerCache_.erase(it);
        }
      }
    }

    D3D12_SAMPLER_DESC samplerDesc = {};

    auto toD3D12Address = [](SamplerAddressMode m) {
      switch (m) {
      case SamplerAddressMode::Repeat:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
      case SamplerAddressMode::MirrorRepeat:
        return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
      case SamplerAddressMode::Clamp:
        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
      default:
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
      }
    };

    auto toD3D12Compare = [](CompareFunction f) {
      switch (f) {
      case CompareFunction::Less:
        return D3D12_COMPARISON_FUNC_LESS;
      case CompareFunction::LessEqual:
        return D3D12_COMPARISON_FUNC_LESS_EQUAL;
      case CompareFunction::Greater:
        return D3D12_COMPARISON_FUNC_GREATER;
      case CompareFunction::GreaterEqual:
        return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
      case CompareFunction::Equal:
        return D3D12_COMPARISON_FUNC_EQUAL;
      case CompareFunction::NotEqual:
        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
      case CompareFunction::AlwaysPass:
        return D3D12_COMPARISON_FUNC_ALWAYS;
      case CompareFunction::Never:
        return D3D12_COMPARISON_FUNC_NEVER;
      default:
        return D3D12_COMPARISON_FUNC_NEVER;
      }
    };

    const bool useComparison = desc.depthCompareEnabled;

    const bool minLinear = (desc.minFilter != SamplerMinMagFilter::Nearest);
    const bool magLinear = (desc.magFilter != SamplerMinMagFilter::Nearest);
    const bool mipLinear = (desc.mipFilter == SamplerMipFilter::Linear);
    const bool anisotropic = (desc.maxAnisotropic > 1);

    if (anisotropic) {
      samplerDesc.Filter = useComparison
          ? D3D12_FILTER_COMPARISON_ANISOTROPIC
          : D3D12_FILTER_ANISOTROPIC;
      samplerDesc.MaxAnisotropy =
          std::min<uint32_t>(desc.maxAnisotropic, 16);
    } else {
      D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
      if (minLinear && magLinear && mipLinear) {
        filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
      } else if (minLinear && magLinear && !mipLinear) {
        filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
      } else if (minLinear && !magLinear && mipLinear) {
        filter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
      } else if (minLinear && !magLinear && !mipLinear) {
        filter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
      } else if (!minLinear && magLinear && mipLinear) {
        filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
      } else if (!minLinear && magLinear && !mipLinear) {
        filter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
      } else if (!minLinear && !magLinear && mipLinear) {
        filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
      }

      if (useComparison) {
        filter = static_cast<D3D12_FILTER>(
            filter | D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT -
                         D3D12_FILTER_MIN_MAG_MIP_POINT);
      }
      samplerDesc.Filter = filter;
      samplerDesc.MaxAnisotropy = 1;
    }

    samplerDesc.AddressU = toD3D12Address(desc.addressModeU);
    samplerDesc.AddressV = toD3D12Address(desc.addressModeV);
    samplerDesc.AddressW = toD3D12Address(desc.addressModeW);
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.ComparisonFunc =
        useComparison ? toD3D12Compare(desc.depthCompareFunction)
                      : D3D12_COMPARISON_FUNC_ALWAYS;
    samplerDesc.BorderColor[0] = 0.0f;
    samplerDesc.BorderColor[1] = 0.0f;
    samplerDesc.BorderColor[2] = 0.0f;
    samplerDesc.BorderColor[3] = 0.0f;
    samplerDesc.MinLOD = static_cast<float>(desc.mipLodMin);
    samplerDesc.MaxLOD = static_cast<float>(desc.mipLodMax);

    auto concreteSampler = std::make_shared<SamplerState>(samplerDesc);
    std::shared_ptr<ISamplerState> samplerState =
        std::static_pointer_cast<ISamplerState>(concreteSampler);

    {
      std::lock_guard<std::mutex> lock(samplerCacheMutex_);
      samplerCache_[samplerHash] = concreteSampler;
      samplerCacheMisses_++;
      IGL_D3D12_LOG_VERBOSE(
          "D3D12SamplerCache::createSamplerState: Cache MISS "
          "(hash=0x%zx, total misses=%zu)\n",
          samplerHash,
          samplerCacheMisses_);
    }

    Result::setOk(outResult);
    return samplerState;
  }

  [[nodiscard]] SamplerCacheStats getStats() const {
    std::lock_guard<std::mutex> lock(samplerCacheMutex_);

    SamplerCacheStats stats;
    stats.cacheHits = samplerCacheHits_;
    stats.cacheMisses = samplerCacheMisses_;

    stats.activeSamplers = 0;
    for (const auto& [hash, weakPtr] : samplerCache_) {
      (void)hash;
      if (!weakPtr.expired()) {
        stats.activeSamplers++;
      }
    }

    const size_t totalRequests = stats.cacheHits + stats.cacheMisses;
    if (totalRequests > 0) {
      stats.hitRate = 100.0f * static_cast<float>(stats.cacheHits) /
                      static_cast<float>(totalRequests);
    }

    return stats;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(samplerCacheMutex_);
    samplerCache_.clear();
    samplerCacheHits_ = 0;
    samplerCacheMisses_ = 0;
  }

 private:
  mutable std::unordered_map<size_t, std::weak_ptr<SamplerState>> samplerCache_;
  mutable std::mutex samplerCacheMutex_;
  mutable size_t samplerCacheHits_ = 0;
  mutable size_t samplerCacheMisses_ = 0;
};

} // namespace igl::d3d12
