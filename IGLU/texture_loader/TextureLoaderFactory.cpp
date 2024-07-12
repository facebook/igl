/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <IGLU/texture_loader/TextureLoaderFactory.h>

namespace iglu::textureloader {

TextureLoaderFactory::TextureLoaderFactory(
    std::vector<std::unique_ptr<ITextureLoaderFactory>>&& factories) :
  factories_(std::move(factories)), headerLength_(0) {
  for (const auto& factory : factories_) {
    headerLength_ = std::max(headerLength_, factory->headerLength());
  }
}

uint32_t TextureLoaderFactory::headerLength() const noexcept {
  return headerLength_;
}

bool TextureLoaderFactory::canCreateInternal(DataReader headerReader,
                                             igl::Result* IGL_NULLABLE outResult) const noexcept {
  for (const auto& factory : factories_) {
    if (factory->canCreate(headerReader, nullptr)) {
      return true;
    }
  }

  igl::Result::setResult(outResult, igl::Result::Code::RuntimeError, "No factory found.");
  return false;
}

std::unique_ptr<ITextureLoader> TextureLoaderFactory::tryCreateInternal(
    DataReader reader,
    igl::TextureFormat preferredFormat,
    igl::Result* IGL_NULLABLE outResult) const noexcept {
  for (const auto& factory : factories_) {
    auto loader = factory->tryCreate(reader, preferredFormat, nullptr);
    if (loader) {
      return loader;
    }
  }
  igl::Result::setResult(outResult, igl::Result::Code::RuntimeError, "No factory found.");
  return nullptr;
}

} // namespace iglu::textureloader
