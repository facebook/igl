/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/d3d12/SamplerState.h>
#include <functional>

namespace igl::d3d12 {

size_t SamplerState::hash() const noexcept {
  size_t h = 0;

  // Hash all D3D12_SAMPLER_DESC fields using the same technique as Device.cpp
  // Magic constant 0x9e3779b9 is the golden ratio used for hash mixing
  h ^= std::hash<int>{}(static_cast<int>(desc_.Filter)) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<int>{}(static_cast<int>(desc_.AddressU)) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<int>{}(static_cast<int>(desc_.AddressV)) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<int>{}(static_cast<int>(desc_.AddressW)) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<float>{}(desc_.MipLODBias) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<uint32_t>{}(desc_.MaxAnisotropy) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<int>{}(static_cast<int>(desc_.ComparisonFunc)) + 0x9e3779b9 + (h << 6) + (h >> 2);

  // Hash border color array
  h ^= std::hash<float>{}(desc_.BorderColor[0]) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<float>{}(desc_.BorderColor[1]) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<float>{}(desc_.BorderColor[2]) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<float>{}(desc_.BorderColor[3]) + 0x9e3779b9 + (h << 6) + (h >> 2);

  h ^= std::hash<float>{}(desc_.MinLOD) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<float>{}(desc_.MaxLOD) + 0x9e3779b9 + (h << 6) + (h >> 2);

  return h;
}

bool SamplerState::operator==(const SamplerState& rhs) const noexcept {
  // Compare all D3D12_SAMPLER_DESC fields
  return desc_.Filter == rhs.desc_.Filter &&
         desc_.AddressU == rhs.desc_.AddressU &&
         desc_.AddressV == rhs.desc_.AddressV &&
         desc_.AddressW == rhs.desc_.AddressW &&
         desc_.MipLODBias == rhs.desc_.MipLODBias &&
         desc_.MaxAnisotropy == rhs.desc_.MaxAnisotropy &&
         desc_.ComparisonFunc == rhs.desc_.ComparisonFunc &&
         desc_.BorderColor[0] == rhs.desc_.BorderColor[0] &&
         desc_.BorderColor[1] == rhs.desc_.BorderColor[1] &&
         desc_.BorderColor[2] == rhs.desc_.BorderColor[2] &&
         desc_.BorderColor[3] == rhs.desc_.BorderColor[3] &&
         desc_.MinLOD == rhs.desc_.MinLOD &&
         desc_.MaxLOD == rhs.desc_.MaxLOD;
}

} // namespace igl::d3d12
