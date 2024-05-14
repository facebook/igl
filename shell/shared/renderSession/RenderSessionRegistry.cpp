/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/renderSession/RenderSessionRegistry.hpp>

#include <igl/Common.h>
#include <unordered_map>

namespace igl::shell {

bool RenderSessionRegistry::contains(const std::string& loaderName) const noexcept {
  return loaders_.find(loaderName) != loaders_.cend();
}

bool RenderSessionRegistry::empty() const noexcept {
  return loaders_.empty();
}

size_t RenderSessionRegistry::size() const noexcept {
  return loaders_.size();
}

RenderSessionLoader& RenderSessionRegistry::findLoader(const std::string& loaderName) noexcept {
  if (auto it = loaders_.find(loaderName); IGL_VERIFY(it != loaders_.end())) {
    return it->second;
  }
  IGL_LOG_ERROR("Could not find RenderSessionLoader for (%s)\n", loaderName.c_str());
  static RenderSessionLoader sSentinelLoader;
  return sSentinelLoader;
}

void RenderSessionRegistry::registerLoader(const std::string& loaderName,
                                           RenderSessionLoader value) noexcept {
  loaders_[loaderName] = std::move(value);
}

const std::string& RenderSessionRegistry::defaultLoaderName() const noexcept {
  return defaultLoaderName_;
}

void RenderSessionRegistry::setDefaultLoaderName(std::string loaderName) noexcept {
  defaultLoaderName_ = std::move(loaderName);
}

} // namespace igl::shell
