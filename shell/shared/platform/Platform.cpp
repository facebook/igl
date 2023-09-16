/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <shell/shared/extension/Extension.h>
#include <shell/shared/extension/ExtensionLoader.h>
#include <shell/shared/imageLoader/ImageLoader.h>
#include <shell/shared/platform/Platform.h>

namespace igl::shell {

Platform::~Platform() = default;

Extension* Platform::createAndInitializeExtension(const char* name) noexcept {
  return extensionLoader_.createAndInitialize(name, *this);
}

InputDispatcher& Platform::getInputDispatcher() noexcept {
  return inputDispatcher_;
}

std::shared_ptr<ITexture> Platform::loadTexture(const char* filename,
                                                bool calculateMipmapLevels,
                                                igl::TextureFormat format,
                                                igl::TextureDesc::TextureUsageBits usage) {
  auto imageData = getImageLoader().loadImageData(filename);
  igl::TextureDesc texDesc =
      igl::TextureDesc::new2D(format, imageData.desc.width, imageData.desc.height, usage);
  texDesc.numMipLevels =
      calculateMipmapLevels ? igl::TextureDesc::calcNumMipLevels(texDesc.width, texDesc.height) : 1;

  Result res;
  auto tex = getDevice().createTexture(texDesc, &res);
  IGL_ASSERT_MSG(res.isOk(), res.message.c_str());
  IGL_ASSERT_MSG(tex != nullptr, "createTexture returned null for some reason");
  tex->upload(tex->getFullRange(), imageData.data->data());
  return tex;
}

} // namespace igl::shell
