/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <shell/shared/renderSession/RenderSessionLoader.hpp>

#include <string>
#include <unordered_map>

namespace igl::shell {

class RenderSessionRegistry {
 public:
  bool contains(const std::string& loaderName) const noexcept;
  bool empty() const noexcept;
  size_t size() const noexcept;

  RenderSessionLoader& findLoader(const std::string& loaderName) noexcept;
  void registerLoader(const std::string& loaderName, RenderSessionLoader value) noexcept;

  const std::string& defaultLoaderName() const noexcept;
  void setDefaultLoaderName(std::string loaderName) noexcept;

 private:
  std::unordered_map<std::string, RenderSessionLoader> loaders_;
  std::string defaultLoaderName_;
};

} // namespace igl::shell
